/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: RKNN 图像推理示例插件，参考：https://github.com/rockchip-linux/rknpu2/tree/master/examples/rknn_yolov5_demo 实现
 */

/* 包含定义插件必须的头文件 */
#include <string>
#include <cstring>

#include "opencv2/core/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"

#include "RgaUtils.h"
#include "im2d.h"
#include "rga.h"

#include "rknn_infer_api.h"
#include "utils_log.h"

#include "postprocess.h"
#include "mpp_video_decoder.h"
#include "mpp_video_encoder.h"

const float    nms_threshold      = NMS_THRESH;
const float    box_conf_threshold = BOX_THRESH;

// 插件全局配置信息，由调度程序给插件传来的信息
PluginConfigSet g_plugin_config_set;

std::vector<MppVideoEncoder *> g_mpp_video_encoders;

// 输入线程私有数据
struct PluginInputData {
    // 每个线程定制解码器
    MppVideoDecoder *mpp_video_decoder;
};

// 输出线程私有数据
struct PluginOutputData {
    // 每个线程定制输出
    std::string image_path;
};

// 插件输入输出线程同步数据
struct PluginSyncData {
    // 帧缓存
    DecoderMppFrame frame = {};
    MppVideoDecoder *mpp_video_decoder = nullptr;

    int input_thread = -1;

    // 模型出入结构
    uint32_t input_channel = 3;
    uint32_t input_width   = 0;
    uint32_t input_height  = 0;
};

static int get_config(PluginConfigGet *plugin_config){
    // 输入线程个数
    plugin_config->input_thread_nums = 2;
    // 输出线程个数
    plugin_config->output_thread_nums = 2;
    // 是否需要输出float类型的输出结果
    plugin_config->output_want_float = false;

    d_rknn_plugin_info("post process config: box_conf_threshold = %.2f, nms_threshold = %.2f", box_conf_threshold, nms_threshold);

    // 模型输出编码器定义
    auto *mpp_video_encoder_1 = new MppVideoEncoder("out_1.h264");
    auto *mpp_video_encoder_2 = new MppVideoEncoder("out_2.h264");
    g_mpp_video_encoders.push_back(mpp_video_encoder_1);
    g_mpp_video_encoders.push_back(mpp_video_encoder_2);
    return 0;
}

static int set_config(PluginConfigSet *plugin_config){
    // 注意拷贝构造函数
    memcpy(&g_plugin_config_set, plugin_config, sizeof(PluginConfigSet));
    d_rknn_plugin_info("plugin config set success")
    return 0;
}

static int rknn_plugin_init(struct ThreadData *td) {
    // 设置输入线程的输入源
    if(td->thread_type == THREAD_TYPE_INPUT) {
        td->plugin_private_data = new PluginInputData();
        auto *pri_data = (PluginInputData *)td->plugin_private_data;
        if(td->thread_id == 0){
            pri_data->mpp_video_decoder = new MppVideoDecoder("1080p.264");
        }else if(td->thread_id == 1) {
            pri_data->mpp_video_decoder = new MppVideoDecoder("1080p.264");
        }
    }else{
        // 设置输出线程的输出源
        td->plugin_private_data = new PluginOutputData();
        auto *pri_data = (PluginOutputData *)td->plugin_private_data;
        if(td->thread_id == 0) {
            pri_data->image_path = "thread_1_out.jpg";
        }else if(td->thread_id == 1) {
            pri_data->image_path = "thread_2_out.jpg";
        }
    }
    return 0;
}

static int rknn_plugin_uninit(struct ThreadData *td) {
    // 释放输入线程的输入源
    if(td->thread_type == THREAD_TYPE_INPUT) {
        auto *pri_data = (PluginInputData *)td->plugin_private_data;
        delete pri_data;
    }
    return 0;
}

static int rknn_plugin_input(struct ThreadData *td, struct InputUnit *input_unit) {
    // 根据数据源采集数据，使用 动态 的内存做封装
    auto *pri_data = (PluginInputData *)td->plugin_private_data;
    td->plugin_sync_data = new PluginSyncData();
    auto *sync_data = (PluginSyncData *)td->plugin_sync_data;
    sync_data->mpp_video_decoder = pri_data->mpp_video_decoder;
    sync_data->input_thread = td->thread_id;

    // Load frame
    if (pri_data->mpp_video_decoder->get_next_frame(sync_data->frame) < 0) {
        d_rknn_plugin_error("get_frame fail!");
        return -1;
    }

    if (g_plugin_config_set.input_attr[0].fmt == RKNN_TENSOR_NCHW) {
        d_rknn_plugin_info("model is NCHW input fmt");
        sync_data->input_channel = g_plugin_config_set.input_attr[0].dims[1];
        sync_data->input_height  = g_plugin_config_set.input_attr[0].dims[2];
        sync_data->input_width   = g_plugin_config_set.input_attr[0].dims[3];
    } else {
        d_rknn_plugin_info("model is NHWC input fmt");
        sync_data->input_height  = g_plugin_config_set.input_attr[0].dims[1];
        sync_data->input_width   = g_plugin_config_set.input_attr[0].dims[2];
        sync_data->input_channel = g_plugin_config_set.input_attr[0].dims[3];
    }
    d_rknn_plugin_info("model input input_height=%d, input_width=%d, input_channel=%d", sync_data->input_height, sync_data->input_width, sync_data->input_channel);

    input_unit->n_inputs = g_plugin_config_set.io_num.n_input;
    input_unit->inputs = (rknn_input*)malloc(input_unit->n_inputs * sizeof(rknn_input));
    memset(input_unit->inputs, 0, input_unit->n_inputs * sizeof(rknn_input));
    input_unit->inputs[0].index        = 0;
    input_unit->inputs[0].type         = RKNN_TENSOR_UINT8;
    input_unit->inputs[0].size         = sync_data->input_width * sync_data->input_height * sync_data->input_channel;
    input_unit->inputs[0].fmt          = RKNN_TENSOR_NHWC;
    input_unit->inputs[0].pass_through = 0;
    input_unit->inputs[0].buf = new uint8_t[sync_data->input_width * sync_data->input_height * sync_data->input_channel];

    d_rknn_plugin_info("resize with RGA!");

    // init rga context
    rga_buffer_t src;
    rga_buffer_t dst;
    im_rect      src_rect;
    im_rect      dst_rect;
    memset(&src_rect, 0, sizeof(src_rect));
    memset(&dst_rect, 0, sizeof(dst_rect));
    memset(&src, 0, sizeof(src));
    memset(&dst, 0, sizeof(dst));

    void* resize_buf = input_unit->inputs[0].buf;
    memset(resize_buf, 0x00, sync_data->input_width * sync_data->input_height * sync_data->input_channel);

    src = wrapbuffer_virtualaddr((void*)sync_data->frame.data_buf,
                                 sync_data->frame.hor_width, sync_data->frame.ver_height,
                                 sync_data->frame.mpp_frame_format,
                                 (int)sync_data->frame.hor_stride, (int)sync_data->frame.ver_stride);
    dst = wrapbuffer_virtualaddr((void*)resize_buf, sync_data->input_width, sync_data->input_height, RK_FORMAT_RGB_888);
    int ret = imcheck(src, dst, src_rect, dst_rect);
    if (IM_STATUS_NOERROR != ret) {
        d_rknn_plugin_info("%d, check error! %s", __LINE__, imStrError((IM_STATUS)ret));
        return -1;
    }
    IM_STATUS STATUS = imresize(src, dst);
    if (IM_STATUS_NOERROR != STATUS) {
        d_rknn_plugin_info("%d, resize error! %s", __LINE__, imStrError(STATUS));
        return -1;
    }
    return 0;
}

static int rknn_plugin_input_release(struct ThreadData *td, struct InputUnit *input_unit) {
    // 释放 rknn_plugin_input 中申请的内存
    for(uint32_t idx = 0; idx < input_unit->n_inputs; idx++){
        delete[] (uint8_t*)input_unit->inputs[0].buf;
        input_unit->inputs[0].buf = nullptr;
    }
    free(input_unit->inputs);
    return 0;
}

static int rknn_plugin_output(struct ThreadData *td, struct OutputUnit *output_unit) {
    // 处理输出数据
    d_rknn_plugin_info("plugin print output data, thread_id: %d", td->thread_id)
    auto *pri_data = (PluginOutputData *)td->plugin_private_data;
    auto *sync_data = (PluginSyncData *)td->plugin_sync_data;

    // post process
    float scale_w = (float)sync_data->input_width / (float)sync_data->frame.hor_width;
    float scale_h = (float)sync_data->input_height / (float)sync_data->frame.ver_height;
    d_rknn_plugin_info("scale_w=%f, scale_h=%f", scale_w, scale_h);

    detect_result_group_t detect_result_group;
    std::vector<float>    out_scales;
    std::vector<int32_t>  out_zps;
    for (int i = 0; i < output_unit->n_outputs; ++i) {
        out_scales.push_back(g_plugin_config_set.output_attr[i].scale);
        out_zps.push_back(g_plugin_config_set.output_attr[i].zp);
    }
    post_process(
            (int8_t*)output_unit->outputs[0].buf,
            (int8_t*)output_unit->outputs[1].buf,
            (int8_t*)output_unit->outputs[2].buf,
            (int)sync_data->input_height, (int)sync_data->input_width,
            box_conf_threshold, nms_threshold,
            scale_w, scale_h,
            out_zps, out_scales, &detect_result_group);

//    // Copy To another buffer avoid to modify mpp decoder buffer
//    rga_buffer_t frame_buffer;
//    rga_buffer_t draw_buffer;
//    uint8_t out_buf = new uint8_t[sync_data->input_width * sync_data->input_height * sync_data->input_channel];
//    frame_buffer = wrapbuffer_virtualaddr((void*)sync_data->frame.data_buf, sync_data->orig_img.cols, sync_data->orig_img.rows, RK_FORMAT_RGB_888);
//    draw_buffer = wrapbuffer_virtualaddr((void*)resize_buf, sync_data->input_width, sync_data->input_height, RK_FORMAT_RGB_888);
//    imcopy(origin, src);
//
//    // Draw objects
//    for (int i = 0; i < detect_result.count; i++) {
//        detect_result_t* det_result = &(detect_result.results[i]);
//        printf("%s @ (%d %d %d %d) %f\n", det_result->name, det_result->box.left, det_result->box.top,
//               det_result->box.right, det_result->box.bottom, det_result->prop);
//        int x1 = det_result->box.left;
//        int y1 = det_result->box.top;
//        int x2 = det_result->box.right;
//        int y2 = det_result->box.bottom;
//        draw_rectangle_yuv420sp((unsigned char*)mpp_frame_addr, width_stride, height_stride, x1, y1, x2-x1+1, y2-y1+1, 0x00FF0000, 4);
//    }
    // 编码器输出

    // 释放输出

    sync_data->mpp_video_decoder->release_frame(sync_data->frame);
    sync_data->mpp_video_decoder = nullptr;
    delete sync_data;
    td->plugin_sync_data = nullptr;
    return 0;
}

// 注册所有本插件的相关函数到插件结构体中
// 注意：插件名称和结构体定义名称必须和插件动态库名称一致
static struct PluginStruct rknn_yolo_v5 = {
        .plugin_name 		= "rknn_yolo_v5",
        .plugin_version 	= 1,
        .get_config         = get_config,
        .set_config         = set_config,
        .init				= rknn_plugin_init,
        .uninit 			= rknn_plugin_uninit,
        .rknn_input 		= rknn_plugin_input,
        .rknn_input_release = rknn_plugin_input_release,
        .rknn_output		= rknn_plugin_output,
};

// 插件动态库在加载时会自动调用该函数
static void plugin_init plugin_auto_register(){
    d_rknn_plugin_info("auto register plugin %p, name: %s", &rknn_yolo_v5, rknn_yolo_v5.plugin_name)
    plugin_register(&rknn_yolo_v5);
}

// 插件动态库在关闭时会自动调用该函数
static void plugin_exit plugin_auto_unregister(){
    d_rknn_plugin_info("auto unregister plugin %p, name: %s", &rknn_yolo_v5, rknn_yolo_v5.plugin_name)
    deinitPostProcess();

    // 清除编码器
    for(auto &it : g_mpp_video_encoders){
        delete it;
    }
    plugin_unregister(&rknn_yolo_v5);
}

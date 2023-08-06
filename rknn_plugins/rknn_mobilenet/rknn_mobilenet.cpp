/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: RKNN 图像推理示例插件，参考：https://github.com/rockchip-linux/rknpu2/tree/master/examples/rknn_mobilenet_demo 实现
 */

/* 包含定义插件必须的头文件 */
#include <string>
#include <cstring>

#include "opencv2/core/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"

#include "rknn_infer_api.h"
#include "plugin_common.h"
#include "utils_log.h"

// 插件全局配置信息，由调度程序给插件传来的信息
PluginConfigSet g_plugin_config_set;

// 插件私有变量定义
struct PluginInputData {
    // 输入源
    std::string image_path;
};

const int MODEL_IN_WIDTH    = 224;
const int MODEL_IN_HEIGHT   = 224;
const int MODEL_IN_CHANNELS = 3;

static int get_config(PluginConfigGet *plugin_config){
    // 输入线程个数
    plugin_config->input_thread_nums = 2;
    // 输出线程个数
    plugin_config->output_thread_nums = 2;
    // 是否需要输出float类型的输出结果
    plugin_config->output_want_float = true;
    return 0;
}

static int set_config(PluginConfigSet *plugin_config){
    // 注意拷贝构造函数
    memcpy(&g_plugin_config_set, plugin_config, sizeof(PluginConfigSet));
    d_rknn_plugin_info("plugin config set success")
    return 0;
}

static int rknn_plugin_init(struct ThreadData *td) {
    if(td->thread_type == THREAD_TYPE_INPUT) {
        // 数据读取线程初始化
        td->plugin_private_data = new PluginInputData();
        auto *pri_data = (PluginInputData *)td->plugin_private_data;
        if(td->thread_id == 0){
            pri_data->image_path = "model/dog_224x224.jpg";
        }else if(td->thread_id == 1) {
            pri_data->image_path = "model/cat_224x224.jpg";
        }
    }
    return 0;
}

static int rknn_plugin_uninit(struct ThreadData *td) {
    if(td->thread_type == THREAD_TYPE_INPUT) {
        // 数据读取线程反初始化
        auto *pri_data = (PluginInputData *)td->plugin_private_data;
        delete pri_data;
    }
    return 0;
}

static int rknn_plugin_input(struct ThreadData *td, struct InputUnit *input_unit) {
    // 根据td来收集输入源（多线程收集）
    d_rknn_plugin_debug("plugin read input data")
    auto *pri_data = (PluginInputData *)td->plugin_private_data;

    // Load image
    cv::Mat orig_img = imread(pri_data->image_path, cv::IMREAD_COLOR);
    if (!orig_img.data) {
        d_rknn_plugin_error("cv::imread %s fail!", pri_data->image_path.c_str());
        return -1;
    }

    cv::Mat orig_img_rgb;
    cv::cvtColor(orig_img, orig_img_rgb, cv::COLOR_BGR2RGB);

    cv::Mat img = orig_img_rgb.clone();
    if (orig_img.cols != MODEL_IN_WIDTH || orig_img.rows != MODEL_IN_HEIGHT) {
        d_rknn_plugin_warn("resize %d %d to %d %d", orig_img.cols, orig_img.rows, MODEL_IN_WIDTH, MODEL_IN_HEIGHT);
        cv::resize(orig_img, img, cv::Size(MODEL_IN_WIDTH, MODEL_IN_HEIGHT), 0, 0, cv::INTER_LINEAR);
    }

    input_unit->n_inputs = g_plugin_config_set.io_num.n_input;
    input_unit->inputs = (rknn_input*)malloc(input_unit->n_inputs * sizeof(rknn_input));
    memset(input_unit->inputs, 0, input_unit->n_inputs * sizeof(rknn_input));

    input_unit->inputs[0].index = 0;
    input_unit->inputs[0].type  = RKNN_TENSOR_UINT8;
    input_unit->inputs[0].size  = img.cols * img.rows * img.channels() * sizeof(uint8_t);
    input_unit->inputs[0].fmt   = RKNN_TENSOR_NHWC;
    input_unit->inputs[0].buf = new uint8_t[img.cols * img.rows * img.channels()];
    memcpy(input_unit->inputs[0].buf, img.data, input_unit->inputs[0].size);
    return 0;
}

static int rknn_plugin_input_release(struct ThreadData *td, struct InputUnit *input_unit) {
    // 释放输入源
    for(uint32_t idx = 0; idx < input_unit->n_inputs; idx++){
        delete[] (uint8_t*)input_unit->inputs[0].buf;
        input_unit->inputs[0].buf = nullptr;
    }
    free(input_unit->inputs);
    return 0;
}

static int rknn_plugin_output(struct ThreadData *td, struct OutputUnit *output_unit) {
    d_rknn_plugin_info("plugin print output data, thread_id: %d", td->thread_id)
    // 收集测试结果
    for (int i = 0; i < output_unit->n_outputs; i++) {
        uint32_t max_class[5];
        float max_prob[5];
        rknn_plugin_get_top(
                (float *) output_unit->outputs[i].buf,
                max_prob,
                max_class,
                output_unit->outputs[i].size / 4,
                5);

        d_rknn_plugin_info(" --- Top5 ---");
        for (int j = 0; j < 5; j++) {
            d_rknn_plugin_info("%3d: %8.6f\n", max_class[j], max_prob[j]);
        }
    }
    return 0;
}

// 注册所有本插件的相关函数到插件结构体中
// 注意：插件名称和结构体定义名称必须和插件动态库名称一致
static struct PluginStruct rknn_mobilenet = {
        .plugin_name 		= "rknn_mobilenet",
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
    d_rknn_plugin_info("auto register plugin %p, name: %s", &rknn_mobilenet, rknn_mobilenet.plugin_name)
    plugin_register(&rknn_mobilenet);
}

// 插件动态库在关闭时会自动调用该函数
static void plugin_exit plugin_auto_unregister(){
    d_rknn_plugin_info("auto unregister plugin %p, name: %s", &rknn_mobilenet, rknn_mobilenet.plugin_name)
    plugin_unregister(&rknn_mobilenet);
}

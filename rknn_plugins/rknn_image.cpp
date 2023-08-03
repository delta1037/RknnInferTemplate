/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: RKNN 图像推理示例插件，参考 https://github.com/rockchip-linux/rknpu2/tree/master/examples/rknn_mobilenet_demo 实现
 */
/* 包含定义插件必须的头文件 */
#include <string>
#include <cstring>
#include "opencv2/core/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"

#include "rknn_infer_api.h"
#include "utils_log.h"

// 插件私有变量定义
struct plugin_private_data {

};

const int MODEL_IN_WIDTH    = 224;
const int MODEL_IN_HEIGHT   = 224;
const int MODEL_IN_CHANNELS = 3;


static int rknn_GetTop(const float* pfProb, float* pfMaxProb, uint32_t* pMaxClass, uint32_t outputCount, uint32_t topNum){
    uint32_t i, j;

#define MAX_TOP_NUM 20
    if (topNum > MAX_TOP_NUM)
        return 0;

    memset(pfMaxProb, 0, sizeof(float) * topNum);
    memset(pMaxClass, 0xff, sizeof(float) * topNum);

    for (j = 0; j < topNum; j++) {
        for (i = 0; i < outputCount; i++) {
            if ((i == *(pMaxClass + 0)) || (i == *(pMaxClass + 1)) || (i == *(pMaxClass + 2)) || (i == *(pMaxClass + 3)) ||
                (i == *(pMaxClass + 4))) {
                continue;
            }

            if (pfProb[i] > *(pfMaxProb + j)) {
                *(pfMaxProb + j) = pfProb[i];
                *(pMaxClass + j) = i;
            }
        }
    }

    return 1;
}

static int rknn_image_infer_config(uint32_t *input_nums, uint32_t *output_nums){
    *input_nums = 1;
    *output_nums = 1;
    return 0;
}

static int rknn_image_init(struct ThreadData *td) {
    return 0;
}

static int rknn_image_uninit(struct ThreadData *td) {
    return 0;
}

static int rknn_image_input(struct ThreadData *td, struct InputUnit *input) {
    // 根据td来收集输入源（多线程收集）
    std::string image_path = "bus.jpg";
    cv::Mat orig_img = imread(image_path.c_str(), cv::IMREAD_COLOR);
    if (!orig_img.data) {
        d_rknn_plugin_error("cv::imread %s fail!\n", img_path);
        return -1;
    }

    cv::Mat orig_img_rgb;
    cv::cvtColor(orig_img, orig_img_rgb, cv::COLOR_BGR2RGB);

    cv::Mat img = orig_img_rgb.clone();
    if (orig_img.cols != MODEL_IN_WIDTH || orig_img.rows != MODEL_IN_HEIGHT) {
        d_rknn_plugin_info("resize %d %d to %d %d\n", orig_img.cols, orig_img.rows, MODEL_IN_WIDTH, MODEL_IN_HEIGHT);
        cv::resize(orig_img, img, cv::Size(MODEL_IN_WIDTH, MODEL_IN_HEIGHT), 0, 0, cv::INTER_LINEAR);
    }

    input->n_inputs = 1;
    input->inputs = new rknn_input[input->n_inputs];
    memset(input->inputs, 0, input->n_inputs * sizeof(rknn_input));
    input->inputs[0].index = 0;
    input->inputs[0].type  = RKNN_TENSOR_UINT8;
    input->inputs[0].size  = img.cols * img.rows * img.channels() * sizeof(uint8_t);
    input->inputs[0].fmt   = RKNN_TENSOR_NHWC;
    input->inputs[0].buf   = img.data;
    return 0;
}

static int rknn_image_output(struct ThreadData *td, struct OutputUnit *output) {
    // 收集测试结果
    for (int i = 0; i < output->n_outputs; i++) {
        uint32_t MaxClass[5];
        float    fMaxProb[5];
        auto*    buffer = (float*)output->outputs[i].buf;
        uint32_t sz     = output->outputs[i].size / 4;

        rknn_GetTop(buffer, fMaxProb, MaxClass, sz, 5);

        d_rknn_plugin_info(" --- Top5 ---\n");
        for (int j = 0; j < 5; j++) {
            d_rknn_plugin_info("%3d: %8.6f\n", MaxClass[j], fMaxProb[j]);
        }
    }
    return 0;
}

// 注册所有本插件的相关函数到插件结构体中
// 注意：插件名称和结构体定义名称必须和插件动态库名称一致
const char *plugin_name = "plugin_rknn_image";
struct PluginStruct plugin_rknn_image = {
        .plugin_name 		= plugin_name,
        .plugin_version 	= 1,
        .init				= rknn_image_init,
        .uninit 			= rknn_image_uninit,
        .rknn_input 		= rknn_image_input,
        .rknn_output		= rknn_image_output,
};

// 插件动态库在加载时会自动调用该函数，因为plugin_init的原因
static void plugin_init plugin_auto_register(){
    plugin_register(&plugin_rknn_image);
}

// 插件动态库在关闭时会自动调用该函数
static void plugin_exit plugin_auto_unregister(){
    plugin_unregister(&plugin_rknn_image);
}

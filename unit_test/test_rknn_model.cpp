/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.04
 * @brief: RKNN model 的测试，参考样例 https://github.com/rockchip-linux/rknpu2/tree/master/examples/rknn_mobilenet_demo
 */
#include <string>
#include "rknn_model.h"
#include "utils_log.h"
#include "rknn_infer_api.h"

#include "opencv2/core/core.hpp"
#include "opencv2/imgcodecs.hpp"
#include "opencv2/imgproc.hpp"

const int MODEL_IN_WIDTH    = 224;
const int MODEL_IN_HEIGHT   = 224;
const int MODEL_IN_CHANNELS = 3;

static unsigned char* load_model(const char* filename, int* model_size)
{
    FILE* fp = fopen(filename, "rb");
    if (fp == nullptr) {
        printf("fopen %s fail!\n", filename);
        return NULL;
    }
    fseek(fp, 0, SEEK_END);
    int            model_len = ftell(fp);
    unsigned char* model     = (unsigned char*)malloc(model_len);
    fseek(fp, 0, SEEK_SET);
    if (model_len != fread(model, 1, model_len, fp)) {
        printf("fread %s fail!\n", filename);
        free(model);
        return NULL;
    }
    *model_size = model_len;
    if (fp) {
        fclose(fp);
    }
    return model;
}

static void dump_tensor_attr(rknn_tensor_attr* attr){
    printf("  index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d], n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
           "zp=%d, scale=%f\n",
           attr->index, attr->name, attr->n_dims, attr->dims[0], attr->dims[1], attr->dims[2], attr->dims[3],
           attr->n_elems, attr->size, get_format_string(attr->fmt), get_type_string(attr->type),
           get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}

static int rknn_plugin_GetTop(const float* pfProb, float* pfMaxProb, uint32_t* pMaxClass, uint32_t outputCount, uint32_t topNum){
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

int test_rknn_model(){
    std::string model_path = "model/RK3566_RK3568/mobilenet_v1.rknn";
    std::string image_path   = "model/dog_224x224.jpg";

    // Load image
    cv::Mat orig_img = imread(image_path, cv::IMREAD_COLOR);
    if (!orig_img.data) {
        printf("cv::imread %s fail!\n", image_path.c_str());
        return -1;
    }

    cv::Mat orig_img_rgb;
    cv::cvtColor(orig_img, orig_img_rgb, cv::COLOR_BGR2RGB);

    cv::Mat img = orig_img_rgb.clone();
    if (orig_img.cols != MODEL_IN_WIDTH || orig_img.rows != MODEL_IN_HEIGHT) {
        printf("resize %d %d to %d %d\n", orig_img.cols, orig_img.rows, MODEL_IN_WIDTH, MODEL_IN_HEIGHT);
        cv::resize(orig_img, img, cv::Size(MODEL_IN_WIDTH, MODEL_IN_HEIGHT), 0, 0, cv::INTER_LINEAR);
    }

    RknnModel model(model_path, true);

    auto *input_unit = new InputUnit();
    input_unit->n_inputs = 1;
    input_unit->inputs = (rknn_input*)malloc(input_unit->n_inputs * sizeof(rknn_input));
    memset(input_unit->inputs, 0, input_unit->n_inputs * sizeof(rknn_input));

    input_unit->inputs[0].index = 0;
    input_unit->inputs[0].type  = RKNN_TENSOR_UINT8;
    input_unit->inputs[0].size  = img.cols * img.rows * img.channels() * sizeof(uint8_t);
    input_unit->inputs[0].fmt   = RKNN_TENSOR_NHWC;
    input_unit->inputs[0].buf = new uint8_t[img.cols * img.rows * img.channels()];
    memcpy(input_unit->inputs[0].buf, img.data, input_unit->inputs[0].size);

    auto *output_unit = new OutputUnit();
    output_unit->n_outputs = 1;
    output_unit->outputs = (rknn_output*)malloc(output_unit->n_outputs * sizeof(rknn_output));
    memset(output_unit->outputs, 0, output_unit->n_outputs * sizeof(rknn_output));
    output_unit->outputs[0].want_float = 1;

    model.model_infer_sync(input_unit->n_inputs, input_unit->inputs, output_unit->n_outputs, output_unit->outputs);

    for (int i = 0; i < 1; i++) {
        uint32_t MaxClass[5];
        float    fMaxProb[5];
        auto*   buffer = (float*)output_unit->outputs[i].buf;
        uint32_t sz     = output_unit->outputs[i].size / 4;

        rknn_plugin_GetTop(buffer, fMaxProb, MaxClass, sz, 5);

        printf(" --- Top5 ---\n");
        for (int j = 0; j < 5; j++) {
            printf("%3d: %8.6f\n", MaxClass[j], fMaxProb[j]);
        }
    }
    model.model_infer_release(output_unit->n_outputs, output_unit->outputs);
    return 0;
}

int main(){
    test_rknn_model();
    return 0;
}
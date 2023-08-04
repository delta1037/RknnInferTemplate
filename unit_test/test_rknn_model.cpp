/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.04
 * @brief: RKNN model 的测试
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

static void dump_tensor_attr(rknn_tensor_attr* attr)
{
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

int test_class(){
    // 1. 初始化模型
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

    struct InputUnit *input_unit = new InputUnit();
    input_unit->n_inputs = 1;
    input_unit->inputs = (rknn_input*)malloc(input_unit->n_inputs * sizeof(rknn_input));
    memset(input_unit->inputs, 0, input_unit->n_inputs * sizeof(rknn_input));

    input_unit->inputs[0].index = 0;
    input_unit->inputs[0].type  = RKNN_TENSOR_UINT8;
    input_unit->inputs[0].size  = img.cols * img.rows * img.channels() * sizeof(uint8_t);
    input_unit->inputs[0].fmt   = RKNN_TENSOR_NHWC;
    input_unit->inputs[0].buf = new uint8_t[img.cols * img.rows * img.channels()];
    memcpy(input_unit->inputs[0].buf, img.data, input_unit->inputs[0].size);

    struct OutputUnit *output_unit = new OutputUnit();
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
    for (int i = 0; i < 1; i++) {
        uint32_t MaxClass[5];
        float fMaxProb[5];
        float *buffer = (float *) output_unit->outputs[i].buf;
        uint32_t sz = output_unit->outputs[i].size / 4;

        rknn_plugin_GetTop(buffer, fMaxProb, MaxClass, sz, 5);

        printf(" --- Top5 ---\n");
        for (int i = 0; i < 5; i++) {
            printf("%3d: %8.6f\n", MaxClass[i], fMaxProb[i]);
        }
    }
    model.model_infer_release(output_unit->n_outputs, output_unit->outputs);
    return 0;
}

int test_single(){
    rknn_context ctx = 0;
    int            ret;
    int            model_len = 0;
    unsigned char* model;

    const char* model_path = "model/RK3566_RK3568/mobilenet_v1.rknn";
    const char* img_path   = "model/dog_224x224.jpg";

    // Load image
    cv::Mat orig_img = imread(img_path, cv::IMREAD_COLOR);
    if (!orig_img.data) {
        printf("cv::imread %s fail!\n", img_path);
        return -1;
    }

    cv::Mat orig_img_rgb;
    cv::cvtColor(orig_img, orig_img_rgb, cv::COLOR_BGR2RGB);

    cv::Mat img = orig_img_rgb.clone();
    if (orig_img.cols != MODEL_IN_WIDTH || orig_img.rows != MODEL_IN_HEIGHT) {
        printf("resize %d %d to %d %d\n", orig_img.cols, orig_img.rows, MODEL_IN_WIDTH, MODEL_IN_HEIGHT);
        cv::resize(orig_img, img, cv::Size(MODEL_IN_WIDTH, MODEL_IN_HEIGHT), 0, 0, cv::INTER_LINEAR);
    }

    // Load RKNN Model
    model = load_model(model_path, &model_len);
    ret   = rknn_init(&ctx, model, model_len, 0, NULL);
    if (ret < 0) {
        printf("rknn_init fail! ret=%d\n", ret);
        return -1;
    }

    // Get Model Input Output Info
    rknn_input_output_num io_num;
    ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC) {
        printf("rknn_query fail! ret=%d\n", ret);
        return -1;
    }
    printf("model input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);

    printf("input tensors:\n");
    rknn_tensor_attr input_attrs[io_num.n_input];
    memset(input_attrs, 0, sizeof(input_attrs));
    for (int i = 0; i < io_num.n_input; i++) {
        input_attrs[i].index = i;
        ret                  = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            printf("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        dump_tensor_attr(&(input_attrs[i]));
    }

    printf("output tensors:\n");
    rknn_tensor_attr output_attrs[io_num.n_output];
    memset(output_attrs, 0, sizeof(output_attrs));
    for (int i = 0; i < io_num.n_output; i++) {
        output_attrs[i].index = i;
        ret                   = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            printf("rknn_query fail! ret=%d\n", ret);
            return -1;
        }
        dump_tensor_attr(&(output_attrs[i]));
    }

    struct InputUnit *input_unit = new InputUnit();
    input_unit->n_inputs = 1;
    input_unit->inputs = (rknn_input*)malloc(input_unit->n_inputs * sizeof(rknn_input));
    memset(input_unit->inputs, 0, input_unit->n_inputs * sizeof(rknn_input));

    input_unit->inputs[0].index = 0;
    input_unit->inputs[0].type  = RKNN_TENSOR_UINT8;
    input_unit->inputs[0].size  = img.cols * img.rows * img.channels() * sizeof(uint8_t);
    input_unit->inputs[0].fmt   = RKNN_TENSOR_NHWC;
    input_unit->inputs[0].buf = new uint8_t[img.cols * img.rows * img.channels()];
    memcpy(input_unit->inputs[0].buf, img.data, input_unit->inputs[0].size);

    d_rknn_model_info("rknn_inputs_set rk_model_ctx=%lu, n_inputs:%u, inputs:%p", ctx, input_unit->n_inputs, input_unit->inputs)
    d_rknn_model_info("rknn_inputs index: %d", input_unit->inputs[0].index)
    d_rknn_model_info("rknn_inputs type: %d", input_unit->inputs[0].type)
    d_rknn_model_info("rknn_inputs size: %d", input_unit->inputs[0].size)
    d_rknn_model_info("rknn_inputs fmt: %d", input_unit->inputs[0].fmt)
    ret = rknn_inputs_set(ctx, input_unit->n_inputs, input_unit->inputs);
    if (ret < 0) {
        printf("rknn_input_set fail! ret=%d\n", ret);
        return -1;
    }

    // Run
    printf("rknn_run\n");
    ret = rknn_run(ctx, nullptr);
    if (ret < 0) {
        printf("rknn_run fail! ret=%d\n", ret);
        return -1;
    }

    // Get Output
    rknn_output outputs[1];
    memset(outputs, 0, sizeof(outputs));
    outputs[0].want_float = 1;
    ret                   = rknn_outputs_get(ctx, 1, outputs, NULL);
    if (ret < 0) {
        printf("rknn_outputs_get fail! ret=%d\n", ret);
        return -1;
    }

    // Post Process
    for (int i = 0; i < io_num.n_output; i++) {
        uint32_t MaxClass[5];
        float    fMaxProb[5];
        float*   buffer = (float*)outputs[i].buf;
        uint32_t sz     = outputs[i].size / 4;

        rknn_plugin_GetTop(buffer, fMaxProb, MaxClass, sz, 5);

        printf(" --- Top5 ---\n");
        for (int i = 0; i < 5; i++) {
            printf("%3d: %8.6f\n", MaxClass[i], fMaxProb[i]);
        }
    }

    // Release rknn_outputs
    rknn_outputs_release(ctx, 1, outputs);

    // Release
    if (ctx > 0)
    {
        rknn_destroy(ctx);
    }
    if (model) {
        free(model);
    }
    return 0;
}

int test_func_load(const std::string &model_path, rknn_context &rk_model_ctx){
    // rknn 模型初始化
    int model_len = 0;
    unsigned char* m_model = load_model(model_path.c_str() , &model_len);
    if (m_model == nullptr){
        d_rknn_model_error("load m_model fail!")
        return -1;
    }
    int ret = rknn_init(&rk_model_ctx, m_model, model_len, 0, NULL);
    if(ret != 0) {
        d_rknn_model_error("rknn_init fail! ret=%d", ret)
        return -1;
    }

    // Get Model Input Output Info
//    rknn_input_output_num io_num;
//    ret = rknn_query(rk_model_ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
//    if (ret != RKNN_SUCC) {
//        d_rknn_model_error("rknn_query fail! ret=%d", ret)
//        return -1;
//    }
//    d_rknn_model_info("m_model input num: %d, output num: %d", io_num.n_input, io_num.n_output);
//
//    d_rknn_model_info("input tensors:");
//    rknn_tensor_attr input_attrs[io_num.n_input];
//    memset(input_attrs, 0, sizeof(input_attrs));
//    for (int i = 0; i < io_num.n_input; i++) {
//        input_attrs[i].index = i;
//        ret = rknn_query(rk_model_ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
//        if (ret != RKNN_SUCC) {
//            d_rknn_model_error("rknn_query fail! ret=%d", ret);
//            return -1;
//        }
//        dump_tensor_attr(&(input_attrs[i]));
//    }
//
//    d_rknn_model_info("output tensors:");
//    rknn_tensor_attr output_attrs[io_num.n_output];
//    memset(output_attrs, 0, sizeof(output_attrs));
//    for (int i = 0; i < io_num.n_output; i++) {
//        output_attrs[i].index = i;
//        ret = rknn_query(rk_model_ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
//        if (ret != RKNN_SUCC) {
//            d_rknn_model_error("rknn_query fail! ret=%d", ret);
//            return -1;
//        }
//        dump_tensor_attr(&(output_attrs[i]));
//    }
    d_rknn_model_info("rknn m_model init success! rk_model_ctx：%lu", rk_model_ctx)

    return 0;
}

RetStatus test_func_infer(rknn_context &rk_model_ctx, uint32_t n_inputs, rknn_input *inputs, uint32_t n_outputs, rknn_output *outputs) {
//    d_rknn_model_info("rknn_inputs_set rk_model_ctx=%lu, n_inputs:%u, inputs:%p", rk_model_ctx, n_inputs, inputs)
//    d_rknn_model_info("rknn_inputs index: %d", inputs[0].index)
//    d_rknn_model_info("rknn_inputs type: %d", inputs[0].type)
//    d_rknn_model_info("rknn_inputs size: %d", inputs[0].size)
//    d_rknn_model_info("rknn_inputs fmt: %d", inputs[0].fmt)
//     for(uint32_t t = 0; t < inputs[0].size; t++){
//         printf("%x", (char)(((char*)inputs[0].buf)[t]));
//     }
    int ret = rknn_inputs_set(rk_model_ctx, n_inputs, inputs);
    if (ret < 0) {
        d_rknn_model_error("rknn_input_set fail! ret=%d\n", ret);
        return RET_STATUS_FAILED;
    }

    d_rknn_model_info("rknn_run\n");
    ret = rknn_run(rk_model_ctx, NULL);
    if (ret < 0) {
        d_rknn_model_error("rknn_run fail! ret=%d\n", ret);
        return RET_STATUS_FAILED;
    }

    ret = rknn_outputs_get(rk_model_ctx, 1, outputs, NULL);
    if (ret < 0) {
        d_rknn_model_error("rknn_outputs_get fail! ret=%d\n", ret);
        return RET_STATUS_FAILED;
    }
    return RET_STATUS_SUCCESS;
}

int test_func(){
    std::string model_path = "model/RK3566_RK3568/mobilenet_v1.rknn";
    std::string image_path   = "model/dog_224x224.jpg";

    rknn_context rk_model_ctx = 0;
    test_func_load(model_path, rk_model_ctx);

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

    struct InputUnit *input_unit = new InputUnit();
    input_unit->n_inputs = 1;
    input_unit->inputs = (rknn_input*)malloc(input_unit->n_inputs * sizeof(rknn_input));
    memset(input_unit->inputs, 0, input_unit->n_inputs * sizeof(rknn_input));

    input_unit->inputs[0].index = 0;
    input_unit->inputs[0].type  = RKNN_TENSOR_UINT8;
    input_unit->inputs[0].size  = img.cols * img.rows * img.channels() * sizeof(uint8_t);
    input_unit->inputs[0].fmt   = RKNN_TENSOR_NHWC;
    input_unit->inputs[0].buf = new uint8_t[img.cols * img.rows * img.channels()];
    memcpy(input_unit->inputs[0].buf, img.data, input_unit->inputs[0].size);

//    rknn_output outputs[1];
//    memset(outputs, 0, sizeof(outputs));
//    outputs[0].want_float = 1;
    struct OutputUnit *output_unit = new OutputUnit();
    output_unit->n_outputs = 1;
    output_unit->outputs = (rknn_output*)malloc(output_unit->n_outputs * sizeof(rknn_output));
    memset(output_unit->outputs, 0, output_unit->n_outputs * sizeof(rknn_output));
    output_unit->outputs[0].want_float = 1;
    test_func_infer(rk_model_ctx, input_unit->n_inputs, input_unit->inputs, output_unit->n_outputs, output_unit->outputs);

    // Post Process
    for (int i = 0; i < 1; i++) {
        uint32_t MaxClass[5];
        float fMaxProb[5];
        float *buffer = (float *) output_unit->outputs[i].buf;
        uint32_t sz = output_unit->outputs[i].size / 4;

        rknn_plugin_GetTop(buffer, fMaxProb, MaxClass, sz, 5);

        printf(" --- Top5 ---\n");
        for (int i = 0; i < 5; i++) {
            printf("%3d: %8.6f\n", MaxClass[i], fMaxProb[i]);
        }
    }

    // Release rknn_outputs
    rknn_outputs_release(rk_model_ctx, 1, output_unit->outputs);

    // Release
    if (rk_model_ctx > 0) {
        rknn_destroy(rk_model_ctx);
    }
    return 0;
}

int test_class_new(){
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

    struct InputUnit *input_unit = new InputUnit();
    input_unit->n_inputs = 1;
    input_unit->inputs = (rknn_input*)malloc(input_unit->n_inputs * sizeof(rknn_input));
    memset(input_unit->inputs, 0, input_unit->n_inputs * sizeof(rknn_input));

    input_unit->inputs[0].index = 0;
    input_unit->inputs[0].type  = RKNN_TENSOR_UINT8;
    input_unit->inputs[0].size  = img.cols * img.rows * img.channels() * sizeof(uint8_t);
    input_unit->inputs[0].fmt   = RKNN_TENSOR_NHWC;
    input_unit->inputs[0].buf = new uint8_t[img.cols * img.rows * img.channels()];
    memcpy(input_unit->inputs[0].buf, img.data, input_unit->inputs[0].size);

    struct OutputUnit *output_unit = new OutputUnit();
    output_unit->n_outputs = 1;
    output_unit->outputs = (rknn_output*)malloc(output_unit->n_outputs * sizeof(rknn_output));
    memset(output_unit->outputs, 0, output_unit->n_outputs * sizeof(rknn_output));
    output_unit->outputs[0].want_float = 1;

    RknnModel model(model_path, true);
    model.model_infer_sync(input_unit->n_inputs, input_unit->inputs, output_unit->n_outputs, output_unit->outputs);

    // Post Process
    for (int i = 0; i < 1; i++) {
        uint32_t MaxClass[5];
        float fMaxProb[5];
        float *buffer = (float *) output_unit->outputs[i].buf;
        uint32_t sz = output_unit->outputs[i].size / 4;

        rknn_plugin_GetTop(buffer, fMaxProb, MaxClass, sz, 5);

        printf(" --- Top5 ---\n");
        for (int i = 0; i < 5; i++) {
            printf("%3d: %8.6f\n", MaxClass[i], fMaxProb[i]);
        }
    }

    // Release rknn_outputs
    model.model_infer_release(output_unit->n_outputs, output_unit->outputs);
//    rknn_outputs_release(rk_model_ctx, 1, output_unit->outputs);

    // Release
//    if (rk_model_ctx > 0) {
//        rknn_destroy(rk_model_ctx);
//    }
    return 0;
}

int main(){
//    test_class();

//    test_single();

//    test_func();

    test_class_new();

    return 0;
}
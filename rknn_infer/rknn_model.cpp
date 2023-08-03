/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: RKNN 模型接口封装
 */
#include <string>
#include <cstring>
#include "rknn_model.h"
#include "utils_log.h"

static unsigned char* load_model(const char* filename, int* model_size){
    FILE* fp = fopen(filename, "rb");
    if (fp == nullptr) {
        printf("fopen %s fail!\n", filename);
        return nullptr;
    }
    fseek(fp, 0, SEEK_END);
    int model_len = (int)ftell(fp);
    auto* model     = (unsigned char*)malloc(model_len);
    fseek(fp, 0, SEEK_SET);
    if (model_len != fread(model, 1, model_len, fp)) {
        printf("fread %s fail!\n", filename);
        free(model);
        return nullptr;
    }
    *model_size = model_len;
    fclose(fp);
    return model;
}

static void dump_tensor_attr(rknn_tensor_attr* attr)
{
    d_rknn_model_info("index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d], n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
           "zp=%d, scale=%f\n",
           attr->index, attr->name, attr->n_dims, attr->dims[0], attr->dims[1], attr->dims[2], attr->dims[3],
           attr->n_elems, attr->size, get_format_string(attr->fmt), get_type_string(attr->type),
           get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}

RknnModel::RknnModel(const std::string &model_path, bool show_model) {
    // 初始化变量
    init = false;
    ctx = 0;

    // rknn 模型初始化
    int model_len = 0;
    m_model = load_model(model_path.c_str() , &model_len);
    if (m_model == nullptr){
        d_rknn_model_error("load m_model fail!")
        return;
    }
    int ret = rknn_init(&ctx, m_model, model_len, RKNN_FLAG_PRIOR_HIGH, nullptr);
    if(ret != 0) {
        d_rknn_model_error("rknn_init fail! ret=%d", ret)
        return;
    }

    if(show_model){
        // Get Model Input Output Info
        rknn_input_output_num io_num;
        ret = rknn_query(ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
        if (ret != RKNN_SUCC) {
            d_rknn_model_error("rknn_query fail! ret=%d\n", ret)
            return;
        }
        d_rknn_model_info("m_model input num: %d, output num: %d\n", io_num.n_input, io_num.n_output);

        d_rknn_model_info("input tensors:\n");
        rknn_tensor_attr input_attrs[io_num.n_input];
        memset(input_attrs, 0, sizeof(input_attrs));
        for (int i = 0; i < io_num.n_input; i++) {
            input_attrs[i].index = i;
            ret = rknn_query(ctx, RKNN_QUERY_INPUT_ATTR, &(input_attrs[i]), sizeof(rknn_tensor_attr));
            if (ret != RKNN_SUCC) {
                d_rknn_model_error("rknn_query fail! ret=%d\n", ret);
                return;
            }
            dump_tensor_attr(&(input_attrs[i]));
        }

        d_rknn_model_info("output tensors:\n");
        rknn_tensor_attr output_attrs[io_num.n_output];
        memset(output_attrs, 0, sizeof(output_attrs));
        for (int i = 0; i < io_num.n_output; i++) {
            output_attrs[i].index = i;
            ret = rknn_query(ctx, RKNN_QUERY_OUTPUT_ATTR, &(output_attrs[i]), sizeof(rknn_tensor_attr));
            if (ret != RKNN_SUCC) {
                d_rknn_model_error("rknn_query fail! ret=%d\n", ret);
                return;
            }
            dump_tensor_attr(&(output_attrs[i]));
        }
    }
    d_rknn_model_info("rknn m_model init success!")
    init = true;
}

RknnModel RknnModel::model_infer_dup() const {
    return RknnModel(this->ctx);
}

RknnModel::RknnModel(rknn_context ctx) {
    // 初始化变量
    init = false;
    this->ctx = 0;

    // 复制 rknn 模型， 做权重复用
    int ret = rknn_dup_context(&ctx, &this->ctx);
    if (ret != RKNN_SUCC){
        d_rknn_model_error("rknn_dup_context fail! ret=%d", ret)
        return;
    }
    init = true;
}


RknnModel::~RknnModel() {
    // 销毁 rknn 模型
    if(ctx != 0){
        rknn_destroy(ctx);
    }
    if(m_model != nullptr){
        free(m_model);
    }
}

bool RknnModel::check_init() const {
    return init;
}

RetStatus RknnModel::model_infer_sync(
        uint32_t n_inputs, rknn_input *inputs,
        uint32_t n_outputs, rknn_output *outputs
        ) const {
    // 设置输入
    int ret = rknn_inputs_set(ctx, n_inputs, inputs);
    if (ret != RKNN_SUCC){
        d_rknn_model_error("rknn_inputs_set fail! ret=%d", ret)
        return RetStatus::RET_STATUS_FAILED;
    }

    // 运行模型
    ret = rknn_run(ctx, nullptr);
    if (ret != RKNN_SUCC){
        d_rknn_model_error("rknn_run fail! ret=%d", ret)
        return RetStatus::RET_STATUS_FAILED;
    }

    // 获取输出
    ret = rknn_outputs_get(ctx, n_outputs, outputs, nullptr);
    if(ret != RKNN_SUCC){
        d_rknn_model_error("rknn_outputs_get fail! ret=%d", ret)
        return RetStatus::RET_STATUS_FAILED;
    }
    return RetStatus::RET_STATUS_SUCCESS;
}

RetStatus RknnModel::model_infer_release(uint32_t n_outputs, rknn_output *outputs) const {
    int ret = rknn_outputs_release(ctx, n_outputs, outputs);
    if(ret != RKNN_SUCC){
        d_rknn_model_error("rknn_outputs_release fail! ret=%d", ret)
    }
    return RetStatus::RET_STATUS_SUCCESS;
}


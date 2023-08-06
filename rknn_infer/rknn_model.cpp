/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: RKNN 模型接口封装
 */
#include <string>
#include <cstring>
#include <cstdio>
#include "rknn_model.h"
#include "utils_log.h"

static unsigned char* load_model(const char* filename, int* model_size) {
    FILE* fp = fopen(filename, "rb");
    if (fp == nullptr) {
        printf("fopen %s fail!\n", filename);
        return nullptr;
    }
    fseek(fp, 0, SEEK_END);
    int model_len = ftell(fp);
    auto* model = (unsigned char*)malloc(model_len);
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

static void dump_tensor_attr(rknn_tensor_attr* attr) {
    d_rknn_model_info("index=%d, name=%s, n_dims=%d, dims=[%d, %d, %d, %d], n_elems=%d, size=%d, fmt=%s, type=%s, qnt_type=%s, "
           "zp=%d, scale=%f",
           attr->index, attr->name, attr->n_dims, attr->dims[0], attr->dims[1], attr->dims[2], attr->dims[3],
           attr->n_elems, attr->size, get_format_string(attr->fmt), get_type_string(attr->type),
           get_qnt_type_string(attr->qnt_type), attr->zp, attr->scale);
}

RknnModel::RknnModel(const std::string &model_path, PluginConfigSet &plugin_config_set, bool show_model):m_plugin_config_set(plugin_config_set) {
    // 初始化变量
    init = false;
    rk_model_ctx = 0;

    // rknn 模型初始化
    int model_len = 0;
    m_model = load_model(model_path.c_str() , &model_len);
    if (m_model == nullptr){
        d_rknn_model_error("load m_model fail!")
        return;
    }
    int ret = rknn_init(&rk_model_ctx, m_model, model_len, 0, nullptr);
    if(ret != 0) {
        d_rknn_model_error("rknn_init fail! ret=%d", ret)
        return;
    }

    // 模型信息查询
    rknn_sdk_version version;
    ret = rknn_query(rk_model_ctx, RKNN_QUERY_SDK_VERSION, &version, sizeof(rknn_sdk_version));
    if (ret < 0) {
        d_rknn_model_error("rknn_init error ret=%d\n", ret);
        return;
    }
    memcpy(&plugin_config_set.sdk_version, &version, sizeof(rknn_sdk_version));
    CHECK(show_model, true, d_rknn_model_info("sdk version: %s driver version: %s\n", version.api_version, version.drv_version));

    rknn_input_output_num io_num;
    ret = rknn_query(rk_model_ctx, RKNN_QUERY_IN_OUT_NUM, &io_num, sizeof(io_num));
    if (ret != RKNN_SUCC) {
        d_rknn_model_error("rknn_query fail! ret=%d", ret)
        return;
    }
    plugin_config_set.io_num = io_num;
    CHECK(show_model, true, d_rknn_model_info("m_model input num: %d, output num: %d", io_num.n_input, io_num.n_output))

    CHECK(show_model, true, d_rknn_model_info("input tensors:"))
    plugin_config_set.input_attr = new rknn_tensor_attr[io_num.n_input];
    memset(plugin_config_set.input_attr, 0, sizeof(rknn_tensor_attr) * io_num.n_input);
    for (int i = 0; i < io_num.n_input; i++) {
        plugin_config_set.input_attr[i].index = i;
        ret = rknn_query(rk_model_ctx, RKNN_QUERY_INPUT_ATTR, &(plugin_config_set.input_attr[i]), sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            d_rknn_model_error("rknn_query fail! ret=%d", ret);
            return;
        }
        CHECK(show_model, true, dump_tensor_attr(&(plugin_config_set.input_attr[i]));)
    }

    CHECK(show_model, true, d_rknn_model_info("output tensors:"))
    plugin_config_set.output_attr = new rknn_tensor_attr[io_num.n_output];
    memset(plugin_config_set.output_attr, 0, sizeof(rknn_tensor_attr) * io_num.n_output);
    for (int i = 0; i < io_num.n_output; i++) {
        plugin_config_set.output_attr[i].index = i;
        ret = rknn_query(rk_model_ctx, RKNN_QUERY_OUTPUT_ATTR, &(plugin_config_set.output_attr[i]), sizeof(rknn_tensor_attr));
        if (ret != RKNN_SUCC) {
            d_rknn_model_error("rknn_query fail! ret=%d", ret);
            return;
        }
        CHECK(show_model, true, dump_tensor_attr(&(plugin_config_set.output_attr[i]));)
    }
    d_rknn_model_info("rknn m_model init success! rk_model_ctx：%lu", rk_model_ctx)
    is_dup = true;
    init = true;
}

RknnModel *RknnModel::model_infer_dup() const {
    return new RknnModel(this->rk_model_ctx, this->m_plugin_config_set);
}

RknnModel::RknnModel(rknn_context ctx, PluginConfigSet &plugin_config_set): m_plugin_config_set(plugin_config_set){
    // 初始化变量
    init = false;
    this->rk_model_ctx = 0;
    this->m_model = nullptr;
    this->is_dup = true;

    // 复制 rknn 模型， 做权重复用
    int ret = rknn_dup_context(&ctx, &this->rk_model_ctx);
    if (ret != RKNN_SUCC){
        d_rknn_model_error("rknn dup model fail! ret=%d", ret)
        return;
    }
    d_rknn_model_error("rknn dup model success!")
    init = true;
}


RknnModel::~RknnModel() {
    // 销毁 rknn 模型
    if(rk_model_ctx != 0){
        rknn_destroy(rk_model_ctx);
        rk_model_ctx = 0;
    }
    if(m_model != nullptr){
        free(m_model);
    }

    // 销毁配置信息
    if(! is_dup){
        if(m_plugin_config_set.input_attr != nullptr){
            delete[] m_plugin_config_set.input_attr;
            m_plugin_config_set.input_attr = nullptr;
        }
        if(m_plugin_config_set.output_attr != nullptr){
            delete[] m_plugin_config_set.output_attr;
            m_plugin_config_set.output_attr = nullptr;
        }
    }
}

bool RknnModel::check_init() const {
    return init;
}

RetStatus RknnModel::model_infer_sync(
        uint32_t n_inputs, rknn_input *inputs,
        uint32_t n_outputs, rknn_output *outputs
        ) const {
    d_rknn_model_debug("rknn_inputs_set rk_model_ctx=%lu, n_inputs:%u, inputs:%p", rk_model_ctx, n_inputs, inputs)
    d_rknn_model_debug("rknn_inputs index: %d", inputs[0].index)
    d_rknn_model_debug("rknn_inputs type: %d", inputs[0].type)
    d_rknn_model_debug("rknn_inputs size: %d", inputs[0].size)
    d_rknn_model_debug("rknn_inputs fmt: %d", inputs[0].fmt)
    int ret = rknn_inputs_set(rk_model_ctx, n_inputs, inputs);
    if (ret < 0) {
        d_rknn_model_error("rknn_input_set fail! ret=%d", ret);
        return RET_STATUS_FAILED;
    }

    d_rknn_model_debug("rknn_run");
    ret = rknn_run(rk_model_ctx, nullptr);
    if (ret < 0) {
        d_rknn_model_error("rknn_run fail! ret=%d", ret);
        return RET_STATUS_FAILED;
    }

    ret = rknn_outputs_get(rk_model_ctx, n_outputs, outputs, nullptr);
    if (ret < 0) {
        d_rknn_model_error("rknn_outputs_get fail! ret=%d", ret);
        return RET_STATUS_FAILED;
    }
    return RET_STATUS_SUCCESS;
}

RetStatus RknnModel::model_infer_release(uint32_t n_outputs, rknn_output *outputs) const {
    int ret = rknn_outputs_release(rk_model_ctx, n_outputs, outputs);
    if(ret != RKNN_SUCC){
        d_rknn_model_error("rknn_outputs_release fail! ret=%d", ret)
    }
    return RetStatus::RET_STATUS_SUCCESS;
}


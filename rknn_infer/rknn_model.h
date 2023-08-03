/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: RKNN 模型接口封装
 */
#ifndef PLUGIN_RKNN_IMAGE_RKNN_MODEL_H
#define PLUGIN_RKNN_IMAGE_RKNN_MODEL_H

#include "rknn_api.h"
#include "rknn_matmul_api.h"
#include "utils.h"

class RknnModel {
public:
    explicit RknnModel(const std::string &model_path, bool show_model=false);
    ~RknnModel();

    // 检查初始化
    [[nodiscard]] bool check_init() const;

    // 模型推理
    RetStatus model_infer_sync(uint32_t n_inputs, rknn_input *inputs, uint32_t n_outputs, rknn_output *outputs) const;
    // 释放推理资源
    RetStatus model_infer_release(uint32_t n_outputs, rknn_output *outputs) const;

    // 模型复用
    [[nodiscard]] RknnModel model_infer_dup() const;

private:
    // 内部模型上下文拷贝接口
    explicit RknnModel(rknn_context ctx);
private:
    // 初始化记录
    bool init;
    rknn_context ctx;
    unsigned char* m_model;
};


#endif //PLUGIN_RKNN_IMAGE_RKNN_MODEL_H

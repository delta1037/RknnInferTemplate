/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: 推理调度头文件
 */
#ifndef RKNN_INFER_RKNN_INFER_H
#define RKNN_INFER_RKNN_INFER_H
#include <thread>
#include <mutex>
#include <list>
#include <vector>
#include <condition_variable>
#include "rknn_model.h"
#include "rknn_infer_api.h"
#include "plugin_ctrl.h"

struct QueuePack{
#ifdef PERFORMANCE_STATISTIC
    time_unit s_pack_record_ms;
#endif
    InputUnit* input_unit;
};
#ifdef PERFORMANCE_STATISTIC
struct StaticStruct{
    // 模型初始化统计
    time_unit s_model_init_count;
    time_unit s_model_init_ms;
    // 模型推理统计
    std::mutex s_model_infer_mutex;
    time_unit s_model_infer_count;
    time_unit s_model_infer_ms;
    // 模型释放资源统计
    std::mutex s_model_release_mutex;
    time_unit s_model_release_count;
    time_unit s_model_release_ms;
    // 插件初始化统计
    std::mutex s_plugin_init_mutex;
    time_unit s_plugin_init_count;
    time_unit s_plugin_init_ms;
    // 插件反初始化统计
    std::mutex s_plugin_uninit_mutex;
    time_unit s_plugin_uninit_count;
    time_unit s_plugin_uninit_ms;
    // 插件输入调用统计
    std::mutex s_plugin_input_mutex;
    time_unit s_plugin_input_count;
    time_unit s_plugin_input_ms;
    // 插件输入释放统计
    std::mutex s_plugin_input_release_mutex;
    time_unit s_plugin_input_release_count;
    time_unit s_plugin_input_release_ms;
    // 插件输出统计
    std::mutex s_plugin_output_mutex;
    time_unit s_plugin_output_count;
    time_unit s_plugin_output_ms;
    // 队列调度统计
    std::mutex s_queue_mutex;
    time_unit s_queue_count;
    time_unit s_queue_ms;

    StaticStruct(){
        s_model_init_count = 0;
        s_model_init_ms = 0;

        s_model_infer_count = 0;
        s_model_infer_ms = 0;

        s_model_release_count = 0;
        s_model_release_ms = 0;

        s_plugin_init_count = 0;
        s_plugin_init_ms = 0;

        s_plugin_uninit_count = 0;
        s_plugin_uninit_ms = 0;

        s_plugin_input_count = 0;
        s_plugin_input_ms = 0;

        s_plugin_input_release_count = 0;
        s_plugin_input_release_ms = 0;

        s_plugin_output_count = 0;
        s_plugin_output_ms = 0;

        s_queue_count = 0;
        s_queue_ms = 0;
    }
};
#endif
class RknnInfer {
public:
    explicit RknnInfer(const std::string &model_name, const std::string &plugin_name);
    RetStatus stop();

    // 检查初始化
    [[nodiscard]] bool check_init() const;
#ifdef PERFORMANCE_STATISTIC
    void print_statistic() const;
#endif
private:
    // 获取输入
    RetStatus get_input_unit(QueuePack &pack);
    // 填入输入
    RetStatus put_input_unit(QueuePack &pack);

    // 输入处理线程
    void input_data_thread(uint32_t idx);
    // 输出处理线程
    void infer_proc_thread(uint32_t idx);
private:
    bool m_init;
#ifdef PERFORMANCE_STATISTIC
    StaticStruct m_statistic;
#endif
    // 调度队列
    std::vector<std::thread> m_infer_proc_ctrl;
    std::vector<ThreadData> m_infer_proc_meta;
    // 输入调度
    std::vector<RknnModel*> m_rknn_models;

    std::vector<std::thread> m_input_data_ctrl;
    std::vector<ThreadData> m_input_data_meta;

    // 推理调度（和输出）
    mutable std::mutex m_infer_queue_mutex;
    std::condition_variable m_infer_queue_not_empty;
    struct std::list<QueuePack> m_infer_queue;
};

#endif //RKNN_INFER_RKNN_INFER_H

/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: 推理调度实现
 */
#include <cstring>
#include "rknn_infer.h"
#include "utils_log.h"

extern bool g_system_running;

RknnInfer::RknnInfer(const std::string &model_name) {
    // 初始化变量
    m_init = false;
    // 加载插件
    PluginStruct *plugin = get_plugin((char *) model_name.c_str());
    if (plugin == nullptr) {
        d_rknn_infer_error("load plugin failed")
        return;
    }

    // 获取插件配置
    uint32_t input_thread_nums = 1;
    uint32_t output_thread_nums = 1;
    if (0 != plugin->rknn_infer_config(&input_thread_nums, &output_thread_nums)) {
        d_rknn_infer_error("rknn_infer_config failed")
        return;
    }

    // 初始化模型
#ifdef PERFORMANCE_STATISTIC
    time_unit t_model_init = get_time_of_ms();
#endif
    for(int idx = 0; idx < output_thread_nums; ++idx){
        if(idx == 0){
            m_rknn_model.emplace_back(RknnModel(model_name));
        }else{
            m_rknn_model.emplace_back(m_rknn_model[0].model_infer_dup());
        }
    }
#ifdef PERFORMANCE_STATISTIC
    m_statistic.s_model_init_count = output_thread_nums;
    m_statistic.s_model_init_ms = get_time_of_ms() - t_model_init;
#endif
    for(int idx = 0; idx < output_thread_nums; ++idx){
        if(!m_rknn_model[idx].check_init()){
            d_rknn_infer_error("rknn model %d init failed", idx)
            return;
        }
    }

    // 启动输出处理线程
    for (int idx = 0; idx < output_thread_nums; ++idx) {
        ThreadData td_data{};
        td_data.plugin = plugin;
        td_data.thread_id = idx;
        td_data.thread_type = THREAD_TYPE_OUTPUT;
        m_infer_proc_meta.push_back(td_data);
        m_infer_proc_ctrl.emplace_back([this, idx] { infer_proc_thread(idx); });
    }

    // 启动输入接收线程
    for (int idx = 0; idx < input_thread_nums; ++idx) {
        ThreadData td_data{};
        td_data.plugin = plugin;
        td_data.thread_id = idx;
        td_data.thread_type = THREAD_TYPE_INPUT;
        m_input_data_meta.push_back(td_data);
        m_input_data_ctrl.emplace_back([this, idx] { input_data_thread(idx); });
    }

    m_init = true;
}

RetStatus RknnInfer::stop() {
    // 先数据线程
    for (auto &item : m_input_data_ctrl) {
        item.join();
    }
    // 停推理线程
    for (auto &item : m_infer_proc_ctrl) {
        item.join();
    }
    return RET_STATUS_SUCCESS;
}


bool RknnInfer::check_init() const {
    return m_init;
}

RetStatus RknnInfer::get_input_unit(QueuePack &pack) {
    std::unique_lock<std::mutex> proc_queue_lock(m_infer_queue_mutex);
    while (m_infer_queue.empty()) {
        m_infer_queue_not_empty.wait(proc_queue_lock);  //如果队列为空，线程就在此阻塞挂起，等待唤醒
    }
    if(!m_infer_queue.empty()){
        // 从头部开始拿
        pack = m_infer_queue.front();
        m_infer_queue.pop_front();
        return RetStatus::RET_STATUS_SUCCESS;
    }else{
        return RetStatus::RET_STATUS_FAILED;
    }
}

RetStatus RknnInfer::put_input_unit(QueuePack &pack) {
    std::unique_lock<std::mutex> proc_queue_lock(m_infer_queue_mutex);
    m_infer_queue.push_back(pack);
    m_infer_queue_not_empty.notify_one();
    return RetStatus::RET_STATUS_SUCCESS;
}

void RknnInfer::input_data_thread(uint32_t idx) {
    auto &td_data = m_input_data_meta[idx];
    // 插件初始化
#ifdef PERFORMANCE_STATISTIC
    time_unit t_plugin_init = get_time_of_ms();
#endif
    if (0 != td_data.plugin->init(&td_data)) {
        d_rknn_infer_error("rknn_infer_init failed")
        return;
    }
#ifdef PERFORMANCE_STATISTIC
    {
        std::lock_guard<std::mutex> proc_queue_lock(m_statistic.s_plugin_init_mutex);
        m_statistic.s_plugin_init_count++;
        m_statistic.s_plugin_init_ms += get_time_of_ms() - t_plugin_init;
    }
#endif

    while(g_system_running){
        // 收集数据
#ifdef PERFORMANCE_STATISTIC
        time_unit t_plugin_input_ms = get_time_of_ms();
#endif
        auto unit = new InputUnit();
        if (0 != td_data.plugin->rknn_input(&td_data, unit)) {
            d_rknn_infer_error("rknn_infer_get_input_data failed")
            continue;
        }
#ifdef PERFORMANCE_STATISTIC
        {
            std::lock_guard<std::mutex> proc_queue_lock(m_statistic.s_plugin_input_mutex);
            m_statistic.s_plugin_input_count++;
            m_statistic.s_plugin_input_ms += get_time_of_ms() - t_plugin_input_ms;
        }
#endif

        // 放入队列
        QueuePack pack{};
#ifdef PERFORMANCE_STATISTIC
        pack.s_pack_record_ms = get_time_of_ms();
#endif
        pack.input_unit = unit;
        put_input_unit(pack);
    }

    // 插件反初始化
#ifdef PERFORMANCE_STATISTIC
    time_unit t_plugin_uninit = get_time_of_ms();
#endif
    if (0 != td_data.plugin->uninit(&td_data)) {
        d_rknn_infer_error("rknn_infer uninit failed")
        return;
    }
#ifdef PERFORMANCE_STATISTIC
    {
        std::lock_guard<std::mutex> proc_queue_lock(m_statistic.s_plugin_uninit_mutex);
        m_statistic.s_plugin_uninit_count++;
        m_statistic.s_plugin_uninit_ms += get_time_of_ms() - t_plugin_uninit;
    }
#endif
}

void RknnInfer::infer_proc_thread(uint32_t idx) {
    auto &td_data = m_infer_proc_meta[idx];
    // 插件初始化
#ifdef PERFORMANCE_STATISTIC
    time_unit t_plugin_init = get_time_of_ms();
#endif
    if (0 != td_data.plugin->init(&td_data)) {
        d_rknn_infer_error("rknn_infer_init failed")
        return;
    }
#ifdef PERFORMANCE_STATISTIC
    {
        std::lock_guard<std::mutex> proc_queue_lock(m_statistic.s_plugin_init_mutex);
        m_statistic.s_plugin_init_count++;
        m_statistic.s_plugin_init_ms += get_time_of_ms() - t_plugin_init;
    }
#endif
    while(g_system_running){
        // 获取数据
        QueuePack pack{};
        RetStatus ret = get_input_unit(pack);
        if (ret != RetStatus::RET_STATUS_SUCCESS){
            d_rknn_infer_error("get_input_unit failed")
            continue;
        }
#ifdef PERFORMANCE_STATISTIC
        {
            std::lock_guard<std::mutex> proc_queue_lock(m_statistic.s_queue_mutex);
            m_statistic.s_queue_count++;
            m_statistic.s_queue_ms += get_time_of_ms() - pack.s_pack_record_ms;
        }
#endif
        auto output_unit = new OutputUnit();
        output_unit->n_outputs = pack.input_unit->n_inputs;
        output_unit->outputs = new rknn_output[output_unit->n_outputs];
        memset(output_unit->outputs, 0, output_unit->n_outputs * sizeof(rknn_output));
        for(int i = 0; i < output_unit->n_outputs; i++){
            output_unit->outputs[i].want_float = true;
        }

        // 推理
#ifdef PERFORMANCE_STATISTIC
        time_unit t_model_infer = get_time_of_ms();
#endif
        ret = m_rknn_model[idx].model_infer_sync(
                pack.input_unit->n_inputs,
                pack.input_unit->inputs,
                output_unit->n_outputs,
                output_unit->outputs);
        if (ret != RetStatus::RET_STATUS_SUCCESS){
            d_rknn_infer_error("model_infer_sync failed")
            continue;
        }
#ifdef PERFORMANCE_STATISTIC
        {
            std::lock_guard<std::mutex> proc_queue_lock(m_statistic.s_model_infer_mutex);
            m_statistic.s_model_infer_count++;
            m_statistic.s_model_infer_ms += get_time_of_ms() - t_model_infer;
        }
#endif

        // 输出结果
#ifdef PERFORMANCE_STATISTIC
        time_unit t_plugin_output = get_time_of_ms();
#endif
        if (0 != td_data.plugin->rknn_output(&td_data, output_unit)) {
            d_rknn_infer_error("rknn_output failed")
            continue;
        }
#ifdef PERFORMANCE_STATISTIC
        {
            std::lock_guard<std::mutex> proc_queue_lock(m_statistic.s_plugin_output_mutex);
            m_statistic.s_plugin_output_count++;
            m_statistic.s_plugin_output_ms += get_time_of_ms() - t_plugin_output;
        }
#endif
        // 释放资源
#ifdef PERFORMANCE_STATISTIC
        time_unit t_model_infer_release = get_time_of_ms();
#endif
        ret = m_rknn_model[idx].model_infer_release(output_unit->n_outputs, output_unit->outputs);
        if (ret != RetStatus::RET_STATUS_SUCCESS){
            d_rknn_infer_error("model_infer_release failed")
            continue;
        }
#ifdef PERFORMANCE_STATISTIC
        {
            std::lock_guard<std::mutex> proc_queue_lock(m_statistic.s_model_release_mutex);
            m_statistic.s_model_release_count++;
            m_statistic.s_model_release_ms += get_time_of_ms() - t_model_infer_release;
        }
#endif
        // 释放输入资源
#ifdef PERFORMANCE_STATISTIC
        time_unit t_plugin_input_release = get_time_of_ms();
#endif
        if(0 != td_data.plugin->rknn_input_release(&td_data, pack.input_unit)){
            d_rknn_infer_error("rknn_input_release failed")
            continue;
        }
#ifdef PERFORMANCE_STATISTIC
        {
            std::lock_guard<std::mutex> proc_queue_lock(m_statistic.s_plugin_input_release_mutex);
            m_statistic.s_plugin_input_release_count++;
            m_statistic.s_plugin_input_release_ms += get_time_of_ms() - t_plugin_input_release;
        }
#endif
        // 释放输出
        delete output_unit->outputs;
        delete output_unit;
        // 释放输入
        delete pack.input_unit;
    }

    // 插件反初始化
#ifdef PERFORMANCE_STATISTIC
    time_unit t_plugin_uninit = get_time_of_ms();
#endif
    if (0 != td_data.plugin->uninit(&td_data)) {
        d_rknn_infer_error("rknn_infer uninit failed")
        return;
    }
#ifdef PERFORMANCE_STATISTIC
    {
        std::lock_guard<std::mutex> proc_queue_lock(m_statistic.s_plugin_uninit_mutex);
        m_statistic.s_plugin_uninit_count++;
        m_statistic.s_plugin_uninit_ms += get_time_of_ms() - t_plugin_uninit;
    }
#endif
}
#ifdef PERFORMANCE_STATISTIC
void RknnInfer::print_statistic() const {
    d_time_info("queue_count: %d, queue_ms: %d, queue_avg_ms: %d",
                m_statistic.s_queue_count,
                m_statistic.s_queue_ms,
                m_statistic.s_queue_ms / m_statistic.s_queue_count)

    d_time_info("model_init_count: %d, model_init_ms: %d, model_init_avg_ms: %d",
                m_statistic.s_model_init_count,
                m_statistic.s_model_init_ms,
                m_statistic.s_model_init_ms / m_statistic.s_model_init_count)
    d_time_info("model_infer_count: %d, model_infer_ms: %d, model_infer_avg_ms: %d",
                m_statistic.s_model_infer_count,
                m_statistic.s_model_infer_ms,
                m_statistic.s_model_infer_ms / m_statistic.s_model_infer_count)
    d_time_info("model_release_count: %d, model_release_ms: %d, model_release_avg_ms: %d",
                m_statistic.s_model_release_count,
                m_statistic.s_model_release_ms,
                m_statistic.s_model_release_ms / m_statistic.s_model_release_count)

    d_time_info("plugin_init_count: %d, plugin_init_ms: %d, plugin_init_avg_ms: %d",
                m_statistic.s_plugin_init_count,
                m_statistic.s_plugin_init_ms,
                m_statistic.s_plugin_init_ms / m_statistic.s_plugin_init_count)
    d_time_info("plugin_uninit_count: %d, plugin_uninit_ms: %d, plugin_uninit_avg_ms: %d",
                m_statistic.s_plugin_uninit_count,
                m_statistic.s_plugin_uninit_ms,
                m_statistic.s_plugin_uninit_ms / m_statistic.s_plugin_uninit_count)
    d_time_info("plugin_output_count: %d, plugin_output_ms: %d, plugin_output_avg_ms: %d",
                m_statistic.s_plugin_output_count,
                m_statistic.s_plugin_output_ms,
                m_statistic.s_plugin_output_ms / m_statistic.s_plugin_output_count)
    d_time_info("plugin_input_count: %d, plugin_input_ms: %d, plugin_input_avg_ms: %d",
                m_statistic.s_plugin_input_count,
                m_statistic.s_plugin_input_ms,
                m_statistic.s_plugin_input_ms / m_statistic.s_plugin_input_count)
    d_time_info("plugin_input_release_count: %d, plugin_input_release_ms: %d, plugin_input_release_avg_ms: %d",
                m_statistic.s_plugin_input_release_count,
                m_statistic.s_plugin_input_release_ms,
                m_statistic.s_plugin_input_release_ms / m_statistic.s_plugin_input_release_count)
}
#endif
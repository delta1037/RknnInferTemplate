/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: 对外开放插件接口
 */
#ifndef RKNN_INFER_RKNN_INFER_API_H
#define RKNN_INFER_RKNN_INFER_API_H

#include "rknn_api.h"
#include "rknn_matmul_api.h"

// 插件在加载和关闭时，自动调用的构造函数和析构函数
#define plugin_init	__attribute__((constructor))
#define plugin_exit	__attribute__((destructor))

// 这里对线程类型做区分，因为本推理模板的线程调度是多对多的
enum ThreadType{
    // 输入类型线程，做数据采集和预处理
    THREAD_TYPE_INPUT,
    // 输出类型线程，做推理的输出
    THREAD_TYPE_OUTPUT,
};

// 输入单元，包含输入数据和输入数据数量
struct InputUnit{
    // 输入数据
    rknn_input *inputs;
    // 输入数据数量
    uint32_t n_inputs;
};

// 输出单元，包含输出数据和输出数据数量
struct OutputUnit{
    // 输出数据
    rknn_output *outputs;
    // 输出数据数量
    uint32_t n_outputs;
};

// 线程数据，保存一个线程用到的所有数据
struct PluginStruct;
struct ThreadData {
    // 线程数据(线程ID和线程类型)
    uint32_t thread_id;
    ThreadType thread_type;

    // 每个线程的插件私有数据
    void *plugin_private_data;

    // 共享接口
    PluginStruct *plugin;
};

// 插件程序给调度程序的配置
struct PluginConfigGet{
    // 输入线程个数
    uint32_t input_thread_nums;
    // 输出线程个数
    uint32_t output_thread_nums;

    // 任务队列个数限制(0代表无限制)，降低任务处理延时
    uint32_t task_queue_limit;

    // 是否需要输出 float 类型的输出结果
    bool output_want_float;

    // 默认配置
    PluginConfigGet(){
        input_thread_nums = 1;

        output_thread_nums = 1;

        task_queue_limit = 100;

        output_want_float = true;
    }
};

// 调度程序给插件程序的配置
struct PluginConfigSet{
    // 模型版本
    rknn_sdk_version sdk_version;
    // 输入输出 tensor 个数
    rknn_input_output_num io_num;
    // 输入 tensor 特征
    rknn_tensor_attr input_attr;
    // 输出 tensor 特征
    rknn_tensor_attr output_attr;
};

// 插件接口格式定义
struct PluginStruct {
    // 插件名称
    const char *plugin_name;
    // 插件版本
    int plugin_version;

    // 从插件中获取调度配置
    int (*get_config)(PluginConfigGet *plugin_config);

    // 给插件设置运行配置
    int (*set_config)(PluginConfigSet *plugin_config);

    // 插件初始化
    int (*init)(struct ThreadData *);
    // 插件反初始化
    int (*uninit)(struct ThreadData *);

    // 收集输入数据和释放输入资源
    int (*rknn_input)(struct ThreadData *, struct InputUnit *);
    int (*rknn_input_release)(struct ThreadData *, struct InputUnit *);

    int (*rknn_output)(struct ThreadData *, struct OutputUnit *);
};

// 插件向主程序注册和反注册接口
extern void plugin_register(struct PluginStruct *);
extern void plugin_unregister(struct PluginStruct *);
#endif //RKNN_INFER_RKNN_INFER_API_H

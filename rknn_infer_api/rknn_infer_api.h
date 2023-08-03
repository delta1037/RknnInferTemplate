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

struct InputUnit{
    // 输入数据
    rknn_input *inputs;
    // 输入数据数量
    uint32_t n_inputs;
};

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

    // 插件私有数据
    void *plugin_private_data;

    // 共享接口
    PluginStruct *plugin;
};

// 插件接口格式定义
struct PluginStruct {
    // 插件名称
    const char *plugin_name;
    // 插件版本
    int plugin_version;

    // 插件初始化
    int (*init)(struct ThreadData *);
    // 插件反初始化
    int (*uninit)(struct ThreadData *);

    int (*rknn_infer_config)(uint32_t *input_nums, uint32_t *output_nums);

    // 收集输入数据和释放输入资源
    int (*rknn_input)(struct ThreadData *, struct InputUnit *);
    int (*rknn_input_release)(struct ThreadData *, struct InputUnit *);

    int (*rknn_output)(struct ThreadData *, struct OutputUnit *);
};

// 插件向主程序注册和反注册接口
extern void plugin_register(struct PluginStruct *);
extern void plugin_unregister(struct PluginStruct *);
#endif //RKNN_INFER_RKNN_INFER_API_H

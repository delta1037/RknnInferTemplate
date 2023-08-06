/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: RKNN 推理插件模板
 */

/* 包含定义插件必须的头文件 */
#include <string>
#include <cstring>

#include "rknn_infer_api.h"
#include "plugin_common.h"
#include "utils_log.h"

// 插件全局配置信息，由调度程序给插件传来的信息
PluginConfigSet g_plugin_config_set;

// 插件私有变量定义
struct PluginPrivateData {
    // 可以给每个线程定制输入源
};

static int get_config(PluginConfigGet *plugin_config){
    // 输入线程个数
    plugin_config->input_thread_nums = 2;
    // 输出线程个数
    plugin_config->output_thread_nums = 2;
    // 是否需要输出float类型的输出结果
    plugin_config->output_want_float = true;
    return 0;
}

static int set_config(PluginConfigSet *plugin_config){
    // 注意拷贝构造函数
    memcpy(&g_plugin_config_set, plugin_config, sizeof(PluginConfigSet));
    d_rknn_plugin_info("plugin config set success")
    return 0;
}

static int rknn_plugin_init(struct ThreadData *td) {
    // 设置输入线程的输入源
    return 0;
}

static int rknn_plugin_uninit(struct ThreadData *td) {
    // 释放输入线程的输入源
    return 0;
}

static int rknn_plugin_input(struct ThreadData *td, struct InputUnit *input_unit) {
    // 根据数据源采集数据，使用 动态 的内存做封装
    return 0;
}

static int rknn_plugin_input_release(struct ThreadData *td, struct InputUnit *input_unit) {
    // 释放 rknn_plugin_input 中申请的内存
     return 0;
}

static int rknn_plugin_output(struct ThreadData *td, struct OutputUnit *output_unit) {
    // 处理输出数据
    d_rknn_plugin_info("plugin print output data, thread_id: %d", td->thread_id)
    return 0;
}

// 注册所有本插件的相关函数到插件结构体中
// 注意：插件名称和结构体定义名称必须和插件动态库名称一致
static struct PluginStruct rknn_plugin_name = {
        .plugin_name 		= "rknn_plugin_name",
        .plugin_version 	= 1,
        .get_config         = get_config,
        .set_config         = set_config,
        .init				= rknn_plugin_init,
        .uninit 			= rknn_plugin_uninit,
        .rknn_input 		= rknn_plugin_input,
        .rknn_input_release = rknn_plugin_input_release,
        .rknn_output		= rknn_plugin_output,
};

// 插件动态库在加载时会自动调用该函数
static void plugin_init plugin_auto_register(){
    d_rknn_plugin_info("auto register plugin %p, name: %s", &rknn_plugin_name, rknn_plugin_name.plugin_name)
    plugin_register(&rknn_plugin_name);
}

// 插件动态库在关闭时会自动调用该函数
static void plugin_exit plugin_auto_unregister(){
    d_rknn_plugin_info("auto unregister plugin %p, name: %s", &rknn_plugin_name, rknn_plugin_name.plugin_name)
    plugin_unregister(&rknn_plugin_name);
}

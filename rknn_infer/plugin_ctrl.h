/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: 插件控制
 */
#ifndef RKNN_INFER_PLUGIN_CTRL_H
#define RKNN_INFER_PLUGIN_CTRL_H
#include <string>
#include <dlfcn.h>
#include "rknn_infer_api.h"

// 获取动态库接口
struct PluginStruct *get_plugin(const std::string &plugin_name);

#endif // RKNN_INFER_PLUGIN_CTRL_H
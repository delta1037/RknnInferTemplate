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
static struct PluginStruct *get_plugin(char *plugin_name);

#endif // RKNN_INFER_PLUGIN_CTRL_H
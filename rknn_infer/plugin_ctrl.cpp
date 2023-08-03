/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: 插件控制
 */
#include <map>
#include <thread>
#include <mutex>
#include <cstring>
#include "plugin_ctrl.h"
#include "utils_log.h"

static std::mutex m_plugin_map_lock;
static std::map<std::string, struct PluginStruct *> g_plugin_map;

PluginStruct* map_plugin_find(const std::string &plugin_name){
    // 从 map 中查找
    std::lock_guard<std::mutex> map_lock(m_plugin_map_lock);
    auto iter = g_plugin_map.find(plugin_name);
    if (iter != g_plugin_map.end()){
        return iter->second;
    }
    return nullptr;
}

void map_plugin_insert(const std::string &plugin_name, PluginStruct* plugin){
    // 从 map 中查找
    std::lock_guard<std::mutex> map_lock(m_plugin_map_lock);
    auto iter = g_plugin_map.find(plugin_name);
    if (iter == g_plugin_map.end()){
        g_plugin_map.insert(std::make_pair(plugin_name, plugin));
    }
}

void map_plugin_remove(const std::string &plugin_name){
    // 从 map 中查找
    std::lock_guard<std::mutex> map_lock(m_plugin_map_lock);
    g_plugin_map.erase(plugin_name);
}

PluginStruct* load_plugin(const std::string &plugin_dll_path){
    struct PluginStruct *plugin;
    // 从 map 中查找
    plugin = map_plugin_find(plugin_dll_path);
    if (plugin){
        return plugin;
    }

    void *dll_handle = dlopen(plugin_dll_path.c_str(), RTLD_LAZY);
    if (!dll_handle) {
        return nullptr;
    }
    // 动态库打开后就可能已经注册过了（自动注册接口），再判断一次
    plugin = map_plugin_find(plugin_dll_path);
    if (plugin){
        return plugin;
    }

    // 如果没有注册过，就查找结构体定义位置
    plugin = static_cast<PluginStruct *>(dlsym(dll_handle, plugin_dll_path.c_str()));

    // 将查找到的结构体插入到 map 中
    if (plugin != nullptr) {
        map_plugin_insert(plugin_dll_path, plugin);
    }
    return plugin;
}

void plugin_register(struct PluginStruct *plugin){
    map_plugin_insert(plugin->plugin_name, plugin);
}

void plugin_unregister(struct PluginStruct *plugin){
    map_plugin_remove(plugin->plugin_name);
}

static struct PluginStruct *get_plugin(const std::string &plugin_dll_path){
    PluginStruct *it_find = map_plugin_find(plugin_dll_path);
    if (it_find == nullptr){
        it_find = load_plugin(plugin_dll_path);
    }
    // 判断接口是否是可用的
    if(it_find == nullptr){
        d_rknn_plugin_error("plugin is nullptr! plugin_name=%s", plugin_dll_path.c_str())
        return nullptr;
    }
    if (it_find->plugin_name == nullptr || strlen(it_find->plugin_name) == 0){
        d_rknn_plugin_error("plugin_name is nullptr! plugin_name=%s", plugin_dll_path.c_str())
        return nullptr;
    }
    if(it_find->init == nullptr){
        d_rknn_plugin_error("plugin init is nullptr! plugin_name=%s", plugin_dll_path.c_str())
        return nullptr;
    }
    if (it_find->uninit == nullptr){
        d_rknn_plugin_error("plugin uninit is nullptr! plugin_name=%s", plugin_dll_path.c_str())
        return nullptr;
    }
    if (it_find->rknn_infer_config == nullptr){
        d_rknn_plugin_error("plugin rknn_infer_config is nullptr! plugin_name=%s", plugin_dll_path.c_str())
        return nullptr;
    }
    if (it_find->rknn_input == nullptr){
        d_rknn_plugin_error("plugin rknn_input is nullptr! plugin_name=%s", plugin_dll_path.c_str())
        return nullptr;
    }
    if (it_find->rknn_output == nullptr){
        d_rknn_plugin_error("plugin rknn_infer is nullptr! plugin_name=%s", plugin_dll_path.c_str())
        return nullptr;
    }
    return it_find;
}
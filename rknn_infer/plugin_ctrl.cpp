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

PluginStruct* load_plugin(const std::string &plugin_name){
    struct PluginStruct *plugin;
    // 从 map 中查找
    plugin = map_plugin_find(plugin_name);
    if (plugin){
        return plugin;
    }

    char path_buffer[1024];
#ifdef __linux__
    // Linux 获取绝对路径
    char *ret = getcwd(path_buffer, 1024);
    if(ret == nullptr){
        d_rknn_plugin_error("getcwd error!!!");
        snprintf(path_buffer, 1024, "%s", "./");
    }
#endif
    std::string lib_path = std::string(path_buffer) + "/lib" + plugin_name + ".so";
    d_rknn_plugin_info("dlopen plugin %s, path:%s", plugin_name.c_str(), lib_path.c_str())
    void *dll_handle = dlopen(lib_path.c_str(), RTLD_LAZY);
    if (!dll_handle) {
        d_rknn_plugin_info("dlopen plugin %s, error: %s", plugin_name.c_str(), dlerror())
        return nullptr;
    }
    // 动态库打开后就可能已经注册过了（自动注册接口），再判断一次
    plugin = map_plugin_find(plugin_name);
    if (plugin){
        return plugin;
    }

    // 如果没有注册过，就查找结构体定义位置
    d_rknn_plugin_info("dlsym plugin %s, symbol:%s", plugin_name.c_str(), plugin_name.c_str())
    plugin = static_cast<PluginStruct *>(dlsym(dll_handle, plugin_name.c_str()));

    // 将查找到的结构体插入到 map 中
    if (plugin) {
        map_plugin_insert(plugin_name, plugin);
    }
    return plugin;
}

void plugin_register(struct PluginStruct *plugin){
    if(!plugin){
        d_rknn_plugin_error("plugin register error, plugin is null")
        return;
    }
    d_rknn_plugin_info("plugin register, plugin_name:%s, plugin_version", plugin->plugin_name, plugin->plugin_version)
    map_plugin_insert(plugin->plugin_name, plugin);
}

void plugin_unregister(struct PluginStruct *plugin){
    if(!plugin){
        d_rknn_plugin_error("plugin unregister error, plugin is null")
        return;
    }
    d_rknn_plugin_info("plugin unregister, plugin_name:%s, plugin_version", plugin->plugin_name, plugin->plugin_version)
    map_plugin_remove(plugin->plugin_name);
}

struct PluginStruct *get_plugin(const std::string &plugin_name){
    PluginStruct *it_find = map_plugin_find(plugin_name);
    if (it_find == nullptr){
        it_find = load_plugin(plugin_name);
    }
    // 判断接口是否是可用的
    if(it_find == nullptr){
        d_rknn_plugin_error("plugin is nullptr! plugin_name=%s", plugin_name.c_str())
        return nullptr;
    }
    if (it_find->plugin_name == nullptr || strlen(it_find->plugin_name) == 0){
        d_rknn_plugin_error("plugin_name is nullptr! plugin_name=%s", plugin_name.c_str())
        return nullptr;
    }

    if (it_find->get_config == nullptr){
        d_rknn_plugin_error("plugin get_config is nullptr! plugin_name=%s", plugin_name.c_str())
        return nullptr;
    }
    if (it_find->set_config == nullptr){
        d_rknn_plugin_error("plugin set_config is nullptr! plugin_name=%s", plugin_name.c_str())
        return nullptr;
    }

    if(it_find->init == nullptr){
        d_rknn_plugin_error("plugin init is nullptr! plugin_name=%s", plugin_name.c_str())
        return nullptr;
    }
    if (it_find->uninit == nullptr){
        d_rknn_plugin_error("plugin uninit is nullptr! plugin_name=%s", plugin_name.c_str())
        return nullptr;
    }

    if (it_find->rknn_input == nullptr){
        d_rknn_plugin_error("plugin rknn_input is nullptr! plugin_name=%s", plugin_name.c_str())
        return nullptr;
    }
    if (it_find->rknn_input_release == nullptr){
        d_rknn_plugin_error("plugin rknn_input_release is nullptr! plugin_name=%s", plugin_name.c_str())
        return nullptr;
    }

    if (it_find->rknn_output == nullptr){
        d_rknn_plugin_error("plugin rknn_infer is nullptr! plugin_name=%s", plugin_name.c_str())
        return nullptr;
    }
    d_rknn_plugin_info("plugin is find! plugin_name=%s, version:%d", plugin_name.c_str(), it_find->plugin_version)
    return it_find;
}
/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: 推理模板主文件
 */
#include <string>
#include "rknn_infer.h"
#include "utils_log.h"

bool g_system_running;

#ifdef __linux__
// 处理linux信号
#include <csignal>

void quit_handler(int sig_num){
    if(sig_num == SIGQUIT){
        g_system_running = false;
        d_rknn_infer_info("system received signal SIGQUIT")
    }else if(sig_num == SIGINT){
        g_system_running = false;
        d_rknn_infer_info("system received signal SIGINT")
    }else if (sig_num == SIGSTOP){
        g_system_running = false;
        d_rknn_infer_info("system received signal SIGSTOP")
    }
}
#endif

int main(int argc, char *argv[]) {
#ifdef __linux__
    // 注册信号处理函数
    signal(SIGQUIT, quit_handler);
#endif
    // 读取配置
    std::string model_path = "./model/RK3566_RK3568/mobilenet_v1.rknn";
    std::string plugin_path = "rknn_mobilenet";
    for(int idx = 0; idx < argc; idx++){
        std::string args = argv[idx];
        if (args == "-m" || args == "--model"){
            model_path = argv[++idx];
        }
        if (args == "-p" || args == "--plugin"){
            plugin_path = argv[++idx];
        }
    }
    if (model_path.empty()){
        d_rknn_infer_error("model path is empty!")
        return -1;
    }
    d_rknn_infer_info("model path: %s", model_path.c_str())
    if (plugin_path.empty()){
        d_rknn_infer_error("plugin path is empty!")
        return -1;
    }
    d_rknn_infer_info("plugin path: %s", plugin_path.c_str())

    // 启动推理
    g_system_running = true;
    RknnInfer rknn_infer(model_path, plugin_path);
    if (!rknn_infer.check_init()){
        d_rknn_infer_error("rknn infer init fail!")
        return -1;
    }
    rknn_infer.stop();
    d_rknn_infer_info("rknn infer stop!")

    // 输出时间统计
#ifdef PERFORMANCE_STATISTIC
    d_rknn_infer_info("performance statistic:")
    rknn_infer.print_statistic();
#endif
    return 0;
}
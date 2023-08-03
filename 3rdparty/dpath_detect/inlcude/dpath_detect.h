/**
*  @authors: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.03.02
 * @brief: 监测文件变动
*/
#include <map>
#include <list>
#include <string>
#include <thread>
#include <utility>
#include <functional>
#include <sys/stat.h>
#include <unistd.h>

enum PathChgType{
    PATH_CHG_MODIFY = 0
};

#define PATH_CHG_CALLBACK std::function<int(std::string &path, PathChgType status)>

class PathChg {
    struct PathStatus{
        // 记录修改时间
        time_t mtime;
    };

public:
    explicit PathChg(const std::list<std::string> &paths = std::list<std::string>(), uint32_t freq_s = 1){
        m_paths_list = paths;
        callback_func = nullptr;
        // 线程初始状态
        m_thread_running_status = false;

        // 监测频率控制
        if(freq_s > 0 && freq_s < 3600){
            m_detect_freq_s = freq_s;
        }else{
            m_detect_freq_s = 1;
        }
    }

    void add_path(const std::string &path){
        m_paths_list.push_back(path);
    }

    void start(PATH_CHG_CALLBACK callback){
        // 记录回调函数
        callback_func = std::move(callback);

        // 记录文件初始状态
        init_path_status_map();

        // 启动线程开启监测
        m_thread_running_status = true;
        m_path_chg_thread_ctrl = std::thread([this]{path_chg_thread();});
    }

    void stop(){
        m_thread_running_status = false;
        m_path_chg_thread_ctrl.join();

        callback_func = nullptr;
    }
private:
    static time_t get_path_mtime(const std::string &path){
        struct stat path_stat{};
        if(0 != stat(path.c_str(), &path_stat)){
            printf("Open path %s, ", path.c_str());
            perror("Error: ");
        }
        return path_stat.st_mtime;
    }

    void init_path_status_map(){
        for(const auto &item : m_paths_list){
            m_path_status_map[item] = {get_path_mtime(item)};
        }
    }

    void path_chg_thread() {
        while(m_thread_running_status){
            for(auto &item : m_path_status_map){
                if(get_path_mtime(item.first) != item.second.mtime){
                    // 更新时间
                    item.second.mtime = get_path_mtime(item.first);

                    // 通知
                    std::string path = item.first;
                    callback_func(path, PATH_CHG_MODIFY);
                }
            }
            sleep(m_detect_freq_s);
        }
    }

private:
    // 数据存储
    std::list<std::string> m_paths_list;
    std::map<std::string, PathStatus> m_path_status_map;

    // 线程管理
    bool m_thread_running_status;
    std::thread m_path_chg_thread_ctrl;

    // 回调函数记录
    PATH_CHG_CALLBACK callback_func;

    // 监测频率控制
    uint32_t m_detect_freq_s;
};
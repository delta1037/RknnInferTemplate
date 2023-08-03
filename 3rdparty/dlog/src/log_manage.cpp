#include <fstream>
#include <cstring>
#ifdef __linux__
#include <unistd.h>
#else
#include <Windows.h>
#include <tchar.h>
#endif
#include "log_manage.h"

using namespace std;

void Logger::log_message(short level, std::string &message) {
    if(!is_greater_than_level(level)){
        return;
    }

    switch (log_type){
        case OUTPUT_FILE:
            log_msg_file(level, message);
            break;
        case OUTPUT_SCREEN:
            log_msg_screen(level, message);
            break;
        case OUTPUT_PLAIN:
            log_msg_plain(level, message);
            break;
        case OUTPUT_NONE:
            break;
    }
}

void Logger::log_msg_file(short level, std::string &message) {
    std::fstream output_file_fd;
    output_file_fd.open(log_filename, std::ios_base::app);

    time_t now = time(nullptr);
    char *ct = ctime(&now);
    ct[strcspn(ct, "\n")] = '\0';
    output_file_fd << ct << " " << get_level_str(level) << " " << message << std::endl;
    output_file_fd.close();
}

void Logger::log_msg_plain(short level, std::string &message) {
    std::fstream output_file_fd;
    output_file_fd.open(log_filename, std::ios_base::app);
    output_file_fd << message << std::endl;
    output_file_fd.close();
}

void Logger::log_msg_screen(short level, std::string &message) {
    time_t now = time(nullptr);
    char *ct = ctime(&now);
    ct[strcspn(ct, "\n")] = '\0';
    std::cout << ct << " " << get_level_str(level) << " " << message << std::endl;
}

bool Logger::is_greater_than_level(short level) {
    return this->log_level <= level;
}

std::string Logger::get_level_str(short level) {
    if(level == LOG_ERROR){
        return "[ERROR]";
    }else if(level == LOG_WARN){
        return "[WARN] ";
    }else if(level == LOG_INFO){
        return "[INFO] ";
    }else if(level == LOG_DEBUG){
        return "[DEBUG]";
    }else if(level == LOG_FATAL){
        return "[FATAL]";
    }
    return "[NONE] ";
}

Logger::Logger(const string &logger_name, LOG_LEVEL log_level, LOG_TYPE log_type, string &log_filename) {
    this->log_level = log_level;
    this->log_type = log_type;
    if ((log_type == OUTPUT_FILE || log_type == OUTPUT_PLAIN) && log_filename.empty()){
        this->log_filename = logger_name + "_default.log";
    }else{
        this->log_filename = log_filename;
    }
}


LoggerCtl* LoggerCtl::_instance = nullptr;
LoggerCtl *LoggerCtl::instance() {
    if(_instance == nullptr){
        _instance = new LoggerCtl();
    }
    return _instance;
}

void *LoggerCtl::register_logger(const std::string &module_name) {
    auto it = logger_map.find(module_name);
    // 如果已经注册过直接返回注册过的
    if(it != logger_map.end()){
        return it->second;
    }

    // 根据module_name获取配置并注册
    LOG_TYPE log_type = OUTPUT_SCREEN;
    LOG_LEVEL log_level = LOG_INFO;
    string filename = "default.log";
    get_logger_config(module_name, log_type, log_level, filename);

    //printf("name :%s, type:%d level:%d file:%s\n", module_name.c_str(), log_type, log_level, filename.c_str());
    // 生成对象并注册到列表里
    auto *logger = new Logger(module_name, log_level, log_type, filename);
    logger_map.insert(make_pair(module_name, logger));
    return logger;
}

LoggerCtl::LoggerCtl() {
    // 临时路径缓存
    char path_buffer[DLOG_TEMP_BUFFER_SIZE];
#ifdef __linux__
    // Linux 获取绝对路径
    char *ret = getcwd(path_buffer, DLOG_TEMP_BUFFER_SIZE);
    if(ret == nullptr){
        printf("getcwd error!!!");
        snprintf(path_buffer, 1024, "%s", "./");
    }
#else
    // Window 获取绝对路径
    ::GetModuleFileName(nullptr, path_buffer, DLOG_TEMP_BUFFER_SIZE);
    (_tcsrchr(path_buffer, '\\'))[1] = 0;
#endif
    m_dlog_module_path = path_buffer;
    m_dlog_module_path += "/";
    m_dlog_config_path = m_dlog_module_path + LOGGER_CONFIG;
}

LoggerCtl::~LoggerCtl() {
    // 清理内存占用
    for(auto it : logger_map){
        delete it.second;
        it.second = nullptr;
    }
    logger_map.clear();
}

void LoggerCtl::get_logger_config(const std::string &name, LOG_TYPE &log_type, LOG_LEVEL &log_level, std::string &filename) {
    FILE *fp = fopen(m_dlog_config_path.c_str(), "r");
    if (fp == nullptr) {
        // 文件不存在
        printf("config file %s not exist\n", m_dlog_config_path.c_str());
        return;
    }

    char read_buffer[DLOG_TEMP_BUFFER_SIZE];
    while(fgets(read_buffer, DLOG_TEMP_BUFFER_SIZE, fp) != nullptr){
        // 清理空格
        int blank_count = 0;
        unsigned int read_len = strlen(read_buffer);
        for(int i = 0; i < read_len; i++){
            if(read_buffer[i] == ' '){
                blank_count++;
            }
            read_buffer[i - blank_count] = read_buffer[i];
        }
        read_buffer[read_len - blank_count] = '\0';
        // 跳过注释
        if(read_buffer[0] == '#'){
            continue;
        }

        // 解析 key 和 value
        char c_key[256], c_value[256];
        sscanf(read_buffer, "%s=%s", c_key, c_value);
        string key = c_key, value = c_value;

        // 跳过非当前模块的配置
        if(key.find("." + name + ".") == string::npos){
            continue;
        }

        if(key.find(".log_level") != string::npos){
            if(value == "DEBUG") {
                log_level = LOG_DEBUG;
            } else if(value == "WARN") {
                log_level = LOG_WARN;
            } else if(value == "INFO") {
                log_level = LOG_INFO;
            } else if(value == "ERROR") {
                log_level = LOG_ERROR;
            } else if(value == "FATAL") {
                log_level = LOG_FATAL;
            } else {
                // 默认的日志等级
                log_level = LOG_INFO;
            }
        } else if(key.find(".log_type") != string::npos){
            if(value == "SCREEN") {
                log_type = OUTPUT_SCREEN;
            } else if(value == "FILE") {
                log_type = OUTPUT_FILE;
            } else if(value == "PLAIN") {
                log_type = OUTPUT_PLAIN;
            } else if(value == "NONE") {
                log_type = OUTPUT_NONE;
            } else {
                // 默认的输出类型
                log_type = OUTPUT_SCREEN;
            }
        } else if(key.find(".log_file") != string::npos){
            // 获取输出为文件类型时的位置
            filename += m_dlog_module_path;
            filename += value;
        }
    }
    fclose(fp);
}

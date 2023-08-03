#ifndef LOG_MANAGE_H
#define LOG_MANAGE_H

#include <string>
#include <fstream>
#include <cstring>
#include <iostream>
#include <ctime>
#include <list>
#include <map>

#include "dlog.h"

#define DLOG_TEMP_BUFFER_SIZE 4096

/* log方式定义 */
enum LOG_TYPE {
    OUTPUT_FILE = 0,
    OUTPUT_SCREEN,
    OUTPUT_PLAIN,
    OUTPUT_NONE
};

class Logger {
public:
    Logger(const std::string &logger_name, LOG_LEVEL log_level, LOG_TYPE log_type, std::string &log_filename);
    void log_message(short level, std::string &message);

private:
    static std::string get_level_str(short level);
    bool is_greater_than_level(short level);

    // log to file
    void log_msg_file(short level, std::string &message);

    // log to file
    void log_msg_plain(short level, std::string &message);

    // log to screen
    static void log_msg_screen(short level, std::string &message);

private:
    LOG_LEVEL log_level;
    LOG_TYPE log_type;
    std::string log_filename;
};

#define LoggerMap std::map<std::string, Logger *>
class LoggerCtl {
public:
    static LoggerCtl* instance();
    LoggerCtl();
    ~LoggerCtl();

public:
    // 获取配置并初始化一个log实例
    // 注册模块并初始化实例
    void *register_logger(const std::string &module_name);

private:
    static LoggerCtl* _instance;

private:
    std::string m_dlog_module_path;
    std::string m_dlog_config_path;
    LoggerMap logger_map;

    void get_logger_config(const std::string &name, LOG_TYPE &log_type, LOG_LEVEL &log_level, std::string &filename);
};


#endif //LOG_MANAGE_H

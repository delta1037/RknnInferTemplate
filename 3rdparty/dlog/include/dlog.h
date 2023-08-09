#ifndef LOG_LOG_H
#define LOG_LOG_H

#include <string>

#define LOGGER_CONFIG "log.properties"

// 注册日志模块，返回实例
#define LOG_MODULE_INIT(module) log_module_init(#module)

// 对外提供的一些必要的定义
#define LOG_MAX_BUFFER 409600

#define CHECK_VAL(x,handle) if((x)){        \
                           handle;          \
                         }

#define CHECK(x,m,handle) if((x) == (m)){   \
                           handle;          \
                         }

/* 日志等级定义 */
enum LOG_LEVEL{
    LOG_DEBUG = 0,
    LOG_INFO,
    LOG_WARN,
    LOG_ERROR,
    LOG_FATAL
};

/* 初始化logger */
extern void *log_module_init(const char *module_name);

/* log内容接口 */
extern void log(void *logger, LOG_LEVEL logLevel, const char *format = "", ... );

#endif //LOG_LOG_H

#include <cstdarg>

#include "dlog.h"
#include "log_manage.h"

void *log_module_init(const char *module_name) {
    if(module_name == nullptr){
        printf("module name is NULL");
        return nullptr;
    }
    return LoggerCtl::instance()->register_logger(module_name);
}

std::string get_format_str(const char *format, va_list args) {
    char buffer[LOG_MAX_BUFFER]{};
    vsnprintf(buffer, LOG_MAX_BUFFER, format, args);
    return buffer;
}

/* log内容接口 */
void log(void *logger, LOG_LEVEL logLevel, const char *format, ... ){
    if (logger == nullptr){
        printf("logger is NULL");
        return;
    }
    if(format == nullptr){
        printf("format is NULL");
        return;
    }
    va_list vaList;
    va_start(vaList, format);
    std::string message = get_format_str(format, vaList);
    va_end(vaList);
    auto *logManage = (Logger *)logger;
    logManage->log_message(logLevel, message);
}
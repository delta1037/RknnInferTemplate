/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: 日志配置
 */
#ifndef RKNN_INFER_UTILS_LOG_H
#define RKNN_INFER_UTILS_LOG_H

#include "dlog.h"

#ifndef __linux__
#include "windows.h"
#else
#include "unistd.h"
#endif

#ifndef __linux__
static uint64_t get_thread_id(){
    return GetCurrentThreadId();
}
static uint64_t get_process_id(){
    return GetCurrentProcessId();
}
#else
#if __GLIBC__ == 2 && __GLIBC_MINOR__ < 30
#include <sys/syscall.h>
#define gettid() syscall(SYS_gettid)
#endif
static uint64_t get_thread_id(){
    return gettid();
}
static uint64_t get_process_id(){
    return getpid();
}
#endif

#define DLOG_FORMAT_PREFIX "<%d, %s, %s, %d> "
#ifndef __linux__
#define __FILENAME__ ( __builtin_strrchr(__FILE__, '\\') ? __builtin_strrchr(__FILE__, '\\') + 1 : __FILE__ )
#else
#define __FILENAME__ ( __builtin_strrchr(__FILE__, '/') ? __builtin_strrchr(__FILE__, '/') + 1 : __FILE__ )
#endif

#define d_rknn_infer_error(format, ...) log(LOG_MODULE_INIT(d_rknn_infer), LOG_ERROR, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_rknn_infer_info(format, ...)  log(LOG_MODULE_INIT(d_rknn_infer), LOG_INFO, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_rknn_infer_warn(format, ...) log(LOG_MODULE_INIT(d_rknn_infer), LOG_WARN, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_rknn_infer_debug(format, ...) log(LOG_MODULE_INIT(d_rknn_infer), LOG_DEBUG, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);

#define d_rknn_model_error(format, ...) log(LOG_MODULE_INIT(d_rknn_model), LOG_ERROR, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_rknn_model_info(format, ...)  log(LOG_MODULE_INIT(d_rknn_model), LOG_INFO, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_rknn_model_warn(format, ...) log(LOG_MODULE_INIT(d_rknn_model), LOG_WARN, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_rknn_model_debug(format, ...) log(LOG_MODULE_INIT(d_rknn_model), LOG_DEBUG, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);

#define d_rknn_plugin_error(format, ...) log(LOG_MODULE_INIT(d_rknn_plugin), LOG_ERROR, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_rknn_plugin_info(format, ...)  log(LOG_MODULE_INIT(d_rknn_plugin), LOG_INFO, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_rknn_plugin_warn(format, ...) log(LOG_MODULE_INIT(d_rknn_plugin), LOG_WARN, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_rknn_plugin_debug(format, ...) log(LOG_MODULE_INIT(d_rknn_plugin), LOG_DEBUG, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);

#define d_time_warn(format, ...)  log(LOG_MODULE_INIT(d_time), LOG_WARN, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_time_info(format, ...)  log(LOG_MODULE_INIT(d_time), LOG_INFO, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_time_debug(format, ...) log(LOG_MODULE_INIT(d_time), LOG_DEBUG, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);

#define d_mpp_module_error(format, ...) log(LOG_MODULE_INIT(d_mpp_module), LOG_ERROR, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_mpp_module_info(format, ...)  log(LOG_MODULE_INIT(d_mpp_module), LOG_INFO, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_mpp_module_warn(format, ...) log(LOG_MODULE_INIT(d_mpp_module), LOG_WARN, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_mpp_module_debug(format, ...) log(LOG_MODULE_INIT(d_mpp_module), LOG_DEBUG, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);

#define d_unit_test_error(format, ...) log(LOG_MODULE_INIT(d_unit_test), LOG_ERROR, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_unit_test_info(format, ...)  log(LOG_MODULE_INIT(d_unit_test), LOG_INFO, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_unit_test_warn(format, ...) log(LOG_MODULE_INIT(d_unit_test), LOG_WARN, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);
#define d_unit_test_debug(format, ...) log(LOG_MODULE_INIT(d_unit_test), LOG_DEBUG, DLOG_FORMAT_PREFIX#format, get_thread_id(), __FILENAME__, __FUNCTION__ , __LINE__, ##__VA_ARGS__);

#endif //RKNN_INFER_UTILS_LOG_H

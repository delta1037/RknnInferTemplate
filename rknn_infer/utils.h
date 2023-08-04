/**
 * @author: bo.liu
 * @mail: geniusrabbit@qq.com
 * @date: 2023.08.03
 * @brief: 对外开放插件接口
 */
#ifndef PLUGIN_RKNN_IMAGE_UTILS_H
#define PLUGIN_RKNN_IMAGE_UTILS_H
#include <ctime>
#ifdef __linux__
#include <sys/socket.h>
#include <arpa/inet.h>
#include<sys/time.h>
#else
#include <WS2tcpip.h>
#endif

typedef uint64_t time_unit;

enum RetStatus {
    // 成功
    RET_STATUS_SUCCESS,
    // 失败
    RET_STATUS_FAILED,
    // 未知
    RET_STATUS_UNKNOWN,
};

/* 获取NS时间 -9 */
static time_unit getTimeOfNs() {
    struct timespec tv{};
    clock_gettime(CLOCK_REALTIME, &tv);
    return tv.tv_sec*1000000000 + tv.tv_nsec;
}

#if defined(_WIN32)
#include <chrono>
#include <utility>
static int gettimeofday(struct timeval* tp, struct timezone* tzp) {
    namespace sc = std::chrono;
    sc::system_clock::duration d = sc::system_clock::now().time_since_epoch();
    sc::seconds s = sc::duration_cast<sc::seconds>(d);
    tp->tv_sec = long(s.count());
    tp->tv_usec = long(sc::duration_cast<sc::microseconds>(d - s).count());
    return 0;
}
#endif // _WIN32

/* 获取MS时间 -3 */
static time_unit get_time_of_ms(){
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    return tv.tv_sec * (time_unit)1000 + tv.tv_usec / 1000;
}

/* 获取S时间 */
static time_unit get_time_of_s(){
    struct timeval tv{};
    gettimeofday(&tv, nullptr);
    return tv.tv_sec;
}

/* 精确睡眠US时间 */
static void sleepUS(time_unit usec){
#if defined(_WIN32)
    Sleep(usec/1000);
#else
    struct timeval tv{};
    tv.tv_sec = long(usec / 1000000UL);
    tv.tv_usec = long(usec % 1000000UL);
    errno = 0;
    select(0, nullptr, nullptr, nullptr, &tv);
#endif
}

#endif //PLUGIN_RKNN_IMAGE_UTILS_H

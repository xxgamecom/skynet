#include "time_helper.h"

#include <ctime>

namespace skynet {

//
#define NANO_SEC    1000000000
#define MICRO_SEC   1000000

// 获取当前系统时间 (滴答数: 1滴答 = 10ms)
// centisecond: 1/100 second
// @param sec 当前秒数
// @param cs 剩余滴答数
void time_helper::systime(uint32_t* sec, uint32_t* cs)
{
    struct timespec ti;
    ::clock_gettime(CLOCK_REALTIME, &ti);       // CLOCK_REALTIME: 系统实时时间, 随系统实时时间改变而改变, 即从UTC1970-1-1 0:0:0开始计时
    *sec = (uint32_t)ti.tv_sec;                 // 秒数部分
    *cs = (uint32_t)(ti.tv_nsec / 10000000);    // 百分之一秒部分, 转纳秒数为百分之一秒 = 纳秒 / 10000000
}

uint64_t time_helper::get_time_tick()
{
    struct timespec ti;
    ::clock_gettime(CLOCK_MONOTONIC, &ti);
    return (uint64_t)ti.tv_sec * 100 + ti.tv_nsec / 10000000;
}

// return nanoseconds
uint64_t time_helper::get_time_ns()
{
    struct timespec ti;
    ::clock_gettime(CLOCK_MONOTONIC, &ti);
    return (uint64_t)ti.tv_sec * NANO_SEC + ti.tv_nsec;
}

// for profile

uint64_t time_helper::thread_time()
{
    struct timespec ti;
    ::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ti);
    return (uint64_t)ti.tv_sec * MICRO_SEC + (uint64_t)ti.tv_nsec / (NANO_SEC / MICRO_SEC);
}

}

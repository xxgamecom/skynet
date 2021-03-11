#include "time_helper.h"

#include <ctime>

#if defined(__APPLE__)
#include <AvailabilityMacros.h>
#include <sys/time.h>
#include <mach/task.h>
#include <mach/mach.h>
#endif


namespace skynet {

// 获取当前系统时间 (滴答数: 1滴答 = 10ms)
// centisecond: 1/100 second
// @param sec 当前秒数
// @param cs 剩余滴答数
void time_helper::systime(uint32_t* sec, uint32_t* cs)
{
#if !defined(__APPLE__) || defined(AVAILABLE_MAC_OS_X_VERSION_10_12_AND_LATER)
    struct timespec ti;
    ::clock_gettime(CLOCK_REALTIME, &ti);       // CLOCK_REALTIME: 系统实时时间, 随系统实时时间改变而改变, 即从UTC1970-1-1 0:0:0开始计时
    *sec = (uint32_t)ti.tv_sec;                 // 秒数部分
    *cs = (uint32_t)(ti.tv_nsec / 10000000);    // 百分之一秒部分, 转纳秒数为百分之一秒 = 纳秒 / 10000000
#else
    struct timeval tv;
    ::gettimeofday(&tv, NULL);
    *sec = tv.tv_sec;                           // 秒数部分
    *cs = tv.tv_usec / 10000;                   // 剩余滴答数
#endif
}

// 获取系统启动以来的滴答数(滴答数: 1滴答 = 10ms)
uint64_t time_helper::gettime()
{
    uint64_t t;
#if !defined(__APPLE__) || defined(AVAILABLE_MAC_OS_X_VERSION_10_12_AND_LATER)
    struct timespec ti;
    ::clock_gettime(CLOCK_MONOTONIC, &ti);      // CLOCK_MONOTONIC: 开机到现在的时间, 不受系统时间影响
    // 系统启动以来的滴答数 (百分之一秒)
    t = (uint64_t)ti.tv_sec * 100;              // 秒数转百分之一秒数
    t += ti.tv_nsec / 10000000;                 // 纳秒数转百分之一秒数 = 纳秒 / 10000000
#else
    struct timeval tv;
    ::gettimeofday(&tv, NULL);
    t = (uint64_t)tv.tv_sec * 100;              // 系统启动以来的滴答数 (1滴答 = 10ms)
    t += tv.tv_usec / 10000;                    //
#endif
    return t;
}

// for profile

#define NANO_SEC    1000000000
#define MICRO_SEC   1000000

uint64_t time_helper::thread_time()
{
#if !defined(__APPLE__) || defined(AVAILABLE_MAC_OS_X_VERSION_10_12_AND_LATER)
    struct timespec ti;
    ::clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ti);

    return (uint64_t)ti.tv_sec * MICRO_SEC + (uint64_t)ti.tv_nsec / (NANO_SEC / MICRO_SEC);
#else
    struct task_thread_times_info ti;
    mach_msg_type_number_t ti_count = TASK_THREAD_TIMES_INFO_COUNT;
    if (::task_info(mach_task_self(), TASK_THREAD_TIMES_INFO, (task_info_t)&ti, &ti_count) != KERN_SUCCESS)
    {
        return 0;
    }

    return (uint64_t)(ti.user_time.seconds) + (uint64_t)ti.user_time.microseconds;
#endif
}

}

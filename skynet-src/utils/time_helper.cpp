#include "time_helper.h"

#include <ctime>

namespace skynet {

//
#define NANO_SEC    1000000000
#define MICRO_SEC   1000000


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

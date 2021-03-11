#pragma once

#include <cstdint>

namespace skynet {

// time utils
class time_helper final
{
public:
    // 获取当前系统时间 (滴答数: 1滴答 = 10ms)
    // centisecond: 1/100 second
    // @param sec 当前秒数
    // @param cs 剩余滴答数
    static void systime(uint32_t* sec, uint32_t* cs);

    // 获取系统启动以来的滴答数(滴答数: 1滴答 = 10ms)
    static uint64_t gettime();

    // for profile, in micro second
    static uint64_t thread_time();

};

}


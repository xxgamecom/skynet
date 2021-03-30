#pragma once

#include <sstream>
#include <ctime>
#include <chrono>
#include <iomanip>

namespace skynet { namespace network {

// time helper class
class time_helper
{
public:
    typedef std::chrono::system_clock system_clock;                     // 系统时间(真实时间, 可以改变)
    typedef std::chrono::steady_clock steady_clock;                     // 稳定时钟
    typedef std::chrono::high_resolution_clock high_resolution_clock;   // 类似steady_clock

public:
    time_helper() = delete;
    ~time_helper() = delete;

public:
    // 当前时间(win下使用choron, linux使用更快的CLOCK_REALTIME_COARSE)
    static system_clock::time_point system_now();
    static steady_clock::time_point steady_now();
    static high_resolution_clock::time_point high_now();

public:
    // time_t -> time_point
    static system_clock::time_point from_time_t(std::time_t time);
    // time_point -> time_t
    static std::time_t to_time_t(const system_clock::time_point& tp);
};

} }

#include "time_helper.inl"


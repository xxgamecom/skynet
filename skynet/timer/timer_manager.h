#pragma once

#include <cstdint>

namespace skynet {

// forward declare
struct timer;

/**
 * skynet node timer manager
 *
 * 1) timer precision: 10ms. it means 1 tick = 10ms.
 * 2)
 */
class timer_manager
{
    // singleton
private:
    static timer_manager* instance_;
public:
    static timer_manager* instance();

    // timer_manager
private:
    timer*                      TI_ = nullptr;                  // todo: test move to cpp, and static

public:
    //
    void init();

public:
    // 帧函数, 定时器定时刷新(0.0025秒/帧)
    void update_time();

    // timeout function
    int timeout(uint32_t handle, int time, int session);

    // the number of ticks since the skynet node started. (ticks, 1 tick = 10ms)
    uint64_t now();
    // the number of seconds since the skynet node started. (seconds)
    uint32_t start_seconds();
};

}


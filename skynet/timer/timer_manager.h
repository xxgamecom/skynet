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
    timer*                      TI = nullptr;                   //

public:
    // 定时器初始化
    void init();

    // 帧函数, 定时器定时刷新(0.0025秒/帧)
    void update_time();

    // timeout function
    int timeout(uint32_t handle, int time, int session);

    // 返回当前进程启动后经过的时间 (0.01 秒)
    uint64_t now();
    // 返回当前进程的启动 UTC 时间（秒）
    uint32_t start_time();
};

}



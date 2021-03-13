#pragma once

#include "timer.h"

namespace skynet {

// 定时器管理
class timer_manager
{
    //
    // singleton
    //
private:
    static timer_manager* instance_;
public:
    static timer_manager* instance();

    //
    // timer_manager
    //

private:
    timer*                      TI = nullptr;                   //

public:
    // 定时器初始化
    void init();
    // 定时执行操作
    int timeout(uint32_t handle, int time, int session);

    // 帧函数, 定时器定时刷新(0.0025秒/帧)
    void update_time();

    // 返回当前进程启动后经过的时间 (0.01 秒)
    uint64_t now();
    // 返回当前进程的启动 UTC 时间（秒）
    uint32_t start_time();

private:
    //
    timer* create_timer();
};

}

#include "timer_manager.inl"


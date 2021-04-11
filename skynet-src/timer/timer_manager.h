#pragma once

#include <cstdint>
#include <cstring>

namespace skynet {

// forward declare
struct timer;

/**
 * skynet node timer manager
 *
 * 1) timer precision: 10ms. it means 1 tick = 10ms.
 * 2)
 *
 * TODO: MOVE this module to net, use io_service schedule.
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
    timer* TI_ = nullptr;

public:
    //
    void init();

public:
    // 定时器帧函数 (update interval: 2.5ms)
    void update_time();

    /**
     * create timer function
     *
     * @param svc_handle
     * @param time <=0, execute immediately. note: timer node are not created.
     *              >0, execute regularly. will create a timer node.
     * @param session_id
     * @return
     */
    int timeout(uint32_t svc_handle, int time, int session_id);

    // the number of ticks since the skynet node started. (ticks, 1 tick = 10ms)
    uint64_t now_ticks();
    // the number of seconds since the skynet node started. (seconds)
    uint32_t start_seconds();
};

}


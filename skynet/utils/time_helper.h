#pragma once

#include <cstdint>

namespace skynet {

// time utils
class time_helper final
{
public:
    // get elapsed tick since system reboot. (1 tick = 10 millseconds)
    static uint64_t get_time_tick();
    // get elapsed time since system reboot. (nanoseconds)
    static uint64_t get_time_ns();

    // for profile, in micro second
    static uint64_t thread_time();

};

}


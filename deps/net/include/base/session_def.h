#pragma once

#include "net_def.h"

#include <cstdint>

namespace skynet::net {

// session type
enum session_type
{
    TCP,
    UDP,
};

// idle type
enum session_idle_type
{
    IDLE_TYPE_READ = 1,             // read idle
    IDLE_TYPE_WRITE = 2,            // write idle
    IDLE_TYPE_BOTH = 3,             // read & write idle
};

}


#pragma once

#include <cstdint>

namespace skynet::net {

// session id
typedef uint32_t session_id_t;
#define INVALID_SESSION_ID (session_id_t)(-1)

// session type
enum session_type
{
    TCP,
    UDP,
};

// idle type
enum idle_type
{
    IDLE_TYPE_READ = 1,             // read idle
    IDLE_TYPE_WRITE = 2,            // write idle
    IDLE_TYPE_BOTH = 3,             // read & write idle
};

}


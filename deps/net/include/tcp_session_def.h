#pragma once

#include <cstdint>

namespace skynet { namespace net {

//---------------------------------------------------
// typedefs
//---------------------------------------------------

// session id
typedef uint32_t session_id_t;
#define INVALID_SESSION_ID (session_id_t)(-1)

//---------------------------------------------------
// idle type
//---------------------------------------------------

enum idle_type
{
    IDLE_TYPE_READ = 1,             // read idle
    IDLE_TYPE_WRITE = 2,            // write idle
    IDLE_TYPE_BOTH = 3,             // read & write idle
};

} }


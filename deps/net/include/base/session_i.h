#pragma once

#include "session_def.h"

namespace skynet::net {

// session base class
class basic_session
{
public:
    virtual ~basic_session() = default;

public:
    // socket id
    virtual void socket_id(uint32_t id) = 0;
    virtual uint32_t socket_id() = 0;

    /**
     * idle check
     * @param check_type @see enum session_idle_type
     * @param check_seconds the time determined as idle (seconds)
     */
    virtual void check_idle(session_idle_type check_type, int32_t check_seconds) = 0;

    // r/w statistics
    virtual int64_t read_bytes() = 0;
    virtual int64_t write_bytes() = 0;

    // r/w delta statistics (note: will reset after called)
    virtual int64_t delta_read_bytes() = 0;
    virtual int64_t delta_write_bytes() = 0;
};

}

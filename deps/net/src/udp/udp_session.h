#pragma once

#include "udp/udp_session_i.h"

namespace skynet::net::impl {

class udp_session_impl : public udp_session,
                         public std::enable_shared_from_this<udp_session_impl>,
                         public asio::noncopyable
{
public:
    udp_session_impl() = default;
    ~udp_session_impl() override = default;

    // basic_session impl
public:
    // session id
    void session_id(session_id_t id) override;
    session_id_t session_id() override;

    /**
     * idle check
     * @param check_type @see enum idle_type
     * @param check_seconds the time determined as idle (seconds)
     */
    void check_idle(idle_type check_type, int32_t check_seconds) override;

    // r/w statistics
    int64_t read_bytes() override;
    int64_t write_bytes() override;

    // r/w delta statistics (note: will reset after called)
    int64_t delta_read_bytes() override;
    int64_t delta_write_bytes() override;
};

}




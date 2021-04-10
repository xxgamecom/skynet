#pragma once

#include "asio.hpp"
#include "service/udp_server_i.h"

#include <memory>

namespace skynet::net::impl {

class udp_server_impl : public asio::noncopyable,
                        public udp_server,
                        public std::enable_shared_from_this<udp_server_impl>
{
public:
    udp_server_impl() = default;
    virtual ~udp_server_impl() override = default;

    // upd_server impl
public:
};

}


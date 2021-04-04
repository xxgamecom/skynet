#pragma once

#include "asio.hpp"
#include "udp/udp_client_i.h"

#include <memory>

namespace skynet::net::impl {

class udp_client_impl : public asio::noncopyable,
                        public udp_client,
                        public std::enable_shared_from_this<udp_client_impl>
{
public:
    udp_client_impl() = default;
    ~udp_client_impl() override = default;

    // udp_client impl
public:

};

}


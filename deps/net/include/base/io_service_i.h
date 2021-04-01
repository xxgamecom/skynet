#pragma once

#include "asio.hpp"

namespace skynet::net {

// io service wrapper (one asio::io_service per cpu)
class io_service
{
public:
    virtual ~io_service() = default;

public:
    virtual void run() = 0;
    virtual void stop() = 0;

    virtual asio::io_service& get_raw_ios() = 0;
};

}

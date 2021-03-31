#pragma once

#include "asio.hpp"

#include "../base/noncopyable.h"

#include <thread>
#include <functional>

namespace skynet { namespace net {

// io service wrapper (one asio::io_service per cpu)
class io_service final : private noncopyable,
                         public std::enable_shared_from_this<io_service>
{
private:
    asio::io_service ios_;                              // asio::io_service
    asio::io_service::work ios_work_;                   // ensure asio::io_service running
    std::shared_ptr<std::thread> ios_thread_ptr_;       // ios thread

public:
    io_service();
    ~io_service() = default;

public:
    void run();
    void stop();

    asio::io_service& get_raw_ios();
};

}}

#include "io_service.inl"

#pragma once

#include "base/io_service_i.h"

#include <thread>
#include <memory>
#include <functional>

namespace skynet::net::impl {

// io service wrapper (one asio::io_service per cpu)
class io_service_impl : public asio::noncopyable,
                        public io_service,
                        public std::enable_shared_from_this<io_service>
{
private:
    asio::io_service ios_;                              // asio::io_service
    asio::io_service::work ios_work_;                   // ensure asio::io_service running
    std::shared_ptr<std::thread> ios_thread_ptr_;       // ios thread

public:
    io_service_impl();
    ~io_service_impl() override = default;

public:
    void run() override;
    void stop() override;

    asio::io_service& get_raw_ios() override;
};

}

#include "io_service.inl"

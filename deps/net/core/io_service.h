#pragma once

#include "asio.hpp"

#include <thread>
#include <functional>

namespace skynet { namespace network {

// io service wrapper (one asio::io_service per cpu)
class io_service final : public std::enable_shared_from_this<io_service>
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

    // noncopyable
private:
    io_service(const io_service&) = delete;
    io_service& operator=(const io_service&) = delete;
};

} }

#include "io_service.inl"

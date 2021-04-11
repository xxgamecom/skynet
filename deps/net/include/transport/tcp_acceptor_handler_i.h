#pragma once

#include "tcp_acceptor_def.h"

#include <memory>
#include <cstdint>

namespace skynet::net {

class io_buffer;
class tcp_acceptor;
class tcp_session;

// 被动连接处理器
class tcp_acceptor_handler
{
public:
    virtual ~tcp_acceptor_handler() = default;

public:
    // 接收连接成功
    virtual void handle_accept_success(std::shared_ptr<tcp_acceptor> acceptor_ptr,
                                       std::shared_ptr<tcp_session> session_ptr) = 0;
    // 接收连接失败
    virtual void handle_accept_failed(std::shared_ptr<tcp_acceptor> acceptor_ptr,
                                      std::shared_ptr<tcp_session> session_ptr,
                                      int32_t err_code, std::string err_msg) = 0;
};

}


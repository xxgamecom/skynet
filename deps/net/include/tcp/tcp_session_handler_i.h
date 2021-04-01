#pragma once

#include "tcp_session_def.h"

#include <memory>
#include <cstdint>

namespace skynet::net {

class io_buffer;
class tcp_session;

// tcp会话处理器
class tcp_session_handler
{
public:
    virtual ~tcp_session_handler() = default;

public:
    // tcp会话读完成
    virtual void handle_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp会话写完成
    virtual void handle_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp会话闲置
    virtual void handle_session_idle(std::shared_ptr<tcp_session> session_ptr, idle_type type) = 0;
    // tcp会话关闭
    virtual void handle_sessoin_close(std::shared_ptr<tcp_session> session_ptr) = 0;
};

}


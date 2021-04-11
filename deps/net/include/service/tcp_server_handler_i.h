#pragma once

#include "base/session_def.h"

#include <string>
#include <cstdint>
#include <memory>

namespace skynet::net {

class tcp_session;

/**
 * tcp server event handler (callback)
 */
class tcp_server_handler
{
public:
    virtual ~tcp_server_handler() = default;

public:
    // 接收连接成功
    virtual void handle_tcp_server_accept(std::shared_ptr<tcp_session> session_ptr) = 0;

    // tcp会话读完成
    virtual void handle_tcp_server_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp会话写完成
    virtual void handle_tcp_server_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp会话闲置
    virtual void handle_tcp_server_session_idle(std::shared_ptr<tcp_session> session_ptr, session_idle_type type) = 0;
    // tcp会话关闭
    virtual void handle_tcp_server_sessoin_close(std::shared_ptr<tcp_session> session_ptr) = 0;
};

}

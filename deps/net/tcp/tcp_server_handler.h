#pragma once

#include <cstdint>
#include <memory>

namespace skynet { namespace net {

class io_buffer;
class tcp_session;

// 外部tcp服务端事件处理器接口
class tcp_server_handler
{
public:
    virtual ~tcp_server_handler() = default;

public:
    // 接收连接成功
    virtual void handle_accept(std::shared_ptr<tcp_session> session_ptr) = 0;

    // tcp会话读完成
    virtual void handle_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp会话写完成
    virtual void handle_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp会话闲置
    virtual void handle_session_idle(std::shared_ptr<tcp_session> session_ptr, idle_type type) = 0;
    // tcp会话关闭
    virtual void handle_sessoin_close(std::shared_ptr<tcp_session> session_ptr) = 0;
};

} }

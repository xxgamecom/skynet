#pragma once

#include <string>
#include <cstdint>
#include <memory>

namespace skynet { namespace net {

class io_buffer;
class tcp_session;

// 外部tcp客户端事件处理器接口
class tcp_client_handler
{
public:
    virtual ~tcp_client_handler() = default;

public:
    // 主动连接成功
    virtual void handle_connect_success(std::shared_ptr<tcp_session> session_ptr) = 0;
    // 主动连接失败
    virtual void handle_connect_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg) = 0;
    // 超时处理
    virtual void handle_connect_timeout(std::shared_ptr<tcp_session> session_ptr) = 0;

    // tcp会话读完成
    virtual void handle_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp会话写完成
    virtual void handle_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) = 0;
    // tcp会话关闭
    virtual void handle_sessoin_close(std::shared_ptr<tcp_session> session_ptr) = 0;
};

} }


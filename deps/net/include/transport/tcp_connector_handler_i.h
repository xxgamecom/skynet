#pragma once

#include <cstdint>
#include <string>
#include <memory>

namespace skynet::net {

class io_buffer;
class tcp_session;

// 主动连接处理器
class tcp_connector_handler
{
public:
    virtual ~tcp_connector_handler() = default;

public:
    // 地址解析成功
    virtual void handle_resolve_success(std::shared_ptr<tcp_session> session_ptr, std::string addr, uint16_t port) = 0;
    // 地址解析失败
    virtual void handle_resolve_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg) = 0;

    // 主动连接成功
    virtual void handle_connect_success(std::shared_ptr<tcp_session> session_ptr) = 0;
    // 主动连接失败
    virtual void handle_connect_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg) = 0;
    // 超时处理
    virtual void handle_connect_timeout(std::shared_ptr<tcp_session> session_ptr) = 0;
};

}


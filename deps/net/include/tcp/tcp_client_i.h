#pragma once

#include <string>
#include <cstdint>
#include <memory>

namespace skynet { namespace net {

// forward declare
class tcp_client_handler;

// tcp client
class tcp_client
{
public:
    virtual ~tcp_client() = default;

public:
    // 设置客户端服务外部处理器
    virtual void set_event_handler(std::shared_ptr<tcp_client_handler> event_handler_ptr) = 0;

    // 打开客户端服务(做一些初始化工作)
    virtual bool open() = 0;

    // 发起连接(提供URI字符串形式)
    virtual bool connect(const std::string remote_uri,
                         int32_t timeout_seconds = 0,
                         const std::string local_ip = "",
                         const uint16_t local_port = 0) = 0;
    // 发起连接(单独提供地址和端口形式)
    virtual bool connect(const std::string remote_addr,
                         const uint16_t remote_port,
                         int32_t timeout_seconds = 0,
                         const std::string local_ip = "",
                         const uint16_t local_port = 0) = 0;

    // 关闭客户端服务
    virtual void close() = 0;

//    // 获取会话配置
//    virtual tcp_client_session_config& get_session_config() = 0;

    // 发送数据
    virtual bool send(const char* data_ptr, int32_t data_len) = 0;
};

} }


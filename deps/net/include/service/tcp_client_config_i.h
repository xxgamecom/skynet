#pragma once

#include <cstdint>

namespace skynet::net {

// tcp_client的session配置项
class tcp_client_session_config
{
public:
    virtual ~tcp_client_session_config() = default;

    // 会话配置
public:
    // socket接收缓存大小
    virtual void socket_recv_buf_size(int32_t size) = 0;
    virtual int32_t socket_recv_buf_size() = 0;

    // socket发送缓存大小
    virtual void socket_send_buf_size(int32_t size) = 0;
    virtual int32_t socket_send_buf_size() = 0;

    // 是否开启socket的keepalive选项
    virtual void socket_keepalive(bool is_enable) = 0;
    virtual bool socket_keepalive() = 0;

    // 是否开启socket的nagle算法
    virtual void socket_nodelay(bool is_enable) = 0;
    virtual bool socket_nodelay() = 0;

    // 设置socket的linger选项
    virtual void socket_linger(int32_t timeout) = 0;
    virtual int32_t socket_linger() = 0;

    // 会话读消息缓存大小
    virtual void read_buf_size(int32_t size) = 0;
    virtual int32_t read_buf_size() = 0;

    // 会话写消息缓存大小
    virtual void write_buf_size(int32_t size) = 0;
    virtual int32_t write_buf_size() = 0;

    // 会话写消息队列大小
    virtual void write_queue_size(int32_t size) = 0;
    virtual int32_t write_queue_size() = 0;
};

}

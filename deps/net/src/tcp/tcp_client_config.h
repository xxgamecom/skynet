#pragma once

#include "base/socket_option_def.h"
#include "tcp/tcp_client_config_i.h"

#include <cstdint>

namespace skynet { namespace net { namespace impl {

// tcp_client的session配置项
class tcp_client_session_config_impl : public tcp_client_session_config
{
    // socket option
private:
    int32_t socket_opt_recv_buf_size_ = 16 * 1024;      // socket选项接收缓存大小, 默认16K
    int32_t socket_opt_send_buf_size_ = 16 * 1024;      // socket选项发送缓存大小, 默认16K
    bool socket_opt_keepalive_ = true;                  // socket选项keepalive
    bool socket_opt_nodelay_ = true;                    // socket选项nodelay
    int32_t socket_opt_linger_ = 0;                     // socket选项linger

    // session
private:
    int32_t msg_read_buf_size_ = 8192;                  // 会话读消息缓存大小
                                                        // 用于单次投递异步读数据, 非底层socket缓存, 默认8K

    int32_t msg_write_buf_size_ = 4096;                 // 会话写消息缓存大小(用于单次投递异步写数据, 非底层socket缓存, 默认4K)
    int32_t msg_write_queue_size_ = 4;                  // 会话写消息缓存队列大小, 默认4
                                                        // 可以单次写超过4K数据, 内部会根据队列情况进行切片排队

public:
    tcp_client_session_config_impl() = default;
    ~tcp_client_session_config_impl() override = default;

    // 会话配置
public:
    // socket接收缓存大小
    void socket_recv_buf_size(int32_t size) override;
    int32_t socket_recv_buf_size() override;

    // socket发送缓存大小
    void socket_send_buf_size(int32_t size) override;
    int32_t socket_send_buf_size() override;

    // 是否开启socket的keepalive选项
    void socket_keepalive(bool is_enable) override;
    bool socket_keepalive() override;

    // 是否开启socket的nagle算法
    void socket_nodelay(bool is_enable) override;
    bool socket_nodelay() override;

    // 设置socket的linger选项
    void socket_linger(int32_t timeout) override;
    int32_t socket_linger() override;

    // 会话读消息缓存大小
    void msg_read_buf_size(int32_t size) override;
    int32_t msg_read_buf_size() override;

    // 会话写消息缓存大小
    void msg_write_buf_size(int32_t size) override;
    int32_t msg_write_buf_size() override;

    // 会话写消息队列大小
    void msg_write_queue_size(int32_t size) override;
    int32_t msg_write_queue_size() override;
};

} } }

#include "tcp_client_config.inl"


#pragma once

#include "service/tcp_server_config_i.h"

namespace skynet::net::impl {

/**
 * tcp_server acceptor config
 */
class tcp_server_acceptor_config_impl : public tcp_server_acceptor_config
{
    // socket option
private:
    int32_t socket_opt_recv_buf_size_ = 16 * 1024;      // socket option - recv buffer size
    int32_t socket_opt_send_buf_size_ = 16 * 1024;      // socket option - send buffer size
    bool socket_opt_keepalive_ = true;                  // socket option - keepalive
    bool socket_opt_nodelay_ = true;                    // socket option - nodelay
    int32_t socket_opt_linger_ = 0;                     // socket option - linger

    // acceptor
private:
    int32_t sync_accept_num_ = 128;                     // 启动时投递异步accept次数, 默认128个

public:
    tcp_server_acceptor_config_impl() = default;
    ~tcp_server_acceptor_config_impl() override = default;

    // tcp_server_acceptor_config impl
public:
    void reset() override;

    // socket opt
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

    // socket的linger选项
    void socket_linger(int32_t timeout) override;
    int32_t socket_linger() override;

    // acceptor
public:
    // 同时投递多少个accept异步操作
    void sync_accept_num(int32_t num) override;
    int32_t sync_accept_num() override;
};

}

#include "tcp_server_acceptor_config.inl"

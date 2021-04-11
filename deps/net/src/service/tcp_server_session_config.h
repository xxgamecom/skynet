#pragma once

#include "service/tcp_server_config_i.h"

namespace skynet::net::impl {


/**
 * tcp_server session config
 *
 * 服务端会话内存估算: 只估算收发所需内存(1个读消息缓存, 1个写消息队列), 其他忽略
 *                     msg_read_buf_size_ + msg_write_buf_size_ * msg_write_queue_size_
 *                     默认 8192 +  4096 * 4 = 24576 = 24K, 因此, 10000并发需要234MB以上内存
 */
class tcp_server_session_config_impl : public tcp_server_session_config
{
    // socket option
private:
    int32_t socket_opt_recv_buf_size_ = 16 * 1024;      // socket option - recv buffer size, default: 16K
    int32_t socket_opt_send_buf_size_ = 16 * 1024;      // socket option - send buffer size, default: 16K
    bool socket_opt_keepalive_ = true;                  // socket option - keepalive
    bool socket_opt_nodelay_ = true;                    // socket option - nodelay
    int32_t socket_opt_linger_ = 0;                     // socket option - linger

    // session
private:
    int32_t session_thread_num_ = 0;                        // ios池大小, 默认使用CPU Core进行计算
    int32_t session_pool_size_ = 64 * 1024;                 // session pool size, default: 64K

    int32_t msg_read_buf_size_ = 8192;                      // session recv buffer size, default: 8K
    // 用于单次投递异步读数据, 非底层socket缓存

    int32_t msg_write_buf_size_ = 4096;                     // 会话写消息缓存大小(用于单次投递异步写数据, 非底层socket缓存, 默认4K)
    int32_t msg_write_queue_size_ = 4;                      // session send queue size, default: 4
    // 可以单次写超过4K数据, 内部会根据队列情况进行切片排队

    session_idle_type idle_check_type_ = IDLE_TYPE_BOTH;    // 判定会话闲置的类型
    int32_t idle_check_seconds_ = 60;                       // 判断会话闲置的时间(单位: 秒, 默认60秒判定会话为超时)

public:
    tcp_server_session_config_impl() = default;
    ~tcp_server_session_config_impl() override = default;

    // tcp_server_session_config impl
public:
    void reset() override;

    // socket opt
public:
    // socket recv buffer size
    void socket_recv_buf_size(int32_t size) override;
    int32_t socket_recv_buf_size() override;

    // socket send buffer size
    void socket_send_buf_size(int32_t size) override;
    int32_t socket_send_buf_size() override;

    // socket keepalive option
    void socket_keepalive(bool is_enable) override;
    bool socket_keepalive() override;

    // socket nagle algorithm option
    void socket_nodelay(bool is_enable) override;
    bool socket_nodelay() override;

    // socket linger option
    void socket_linger(int32_t timeout) override;
    int32_t socket_linger() override;

    // session config
public:
    // the number of session thread (ios池大小, 默认使用CPU Core进行计算)
    void session_thread_num(int32_t num) override;
    int32_t session_thread_num() override;

    // 会话池会话对象数量
    void session_pool_size(int32_t size) override;
    int32_t session_pool_size() override;

    // 会话读消息缓存大小(单次投递异步读数据, 非底层socket缓存)
    void read_buf_size(int32_t size) override;
    int32_t read_buf_size() override;

    // 会话写消息缓存大小(用于单次投递异步写数据, 非底层socket缓存)
    void write_buf_size(int32_t size) override;
    int32_t write_buf_size() override;

    // 会话写消息缓存队列大小(可以单次写超过4K数据, 内部会根据队列情况进行切片排队)
    void write_queue_size(int32_t size) override;
    int32_t write_queue_size() override;

    // 判定会话闲置的类型
    void idle_check_type(session_idle_type type) override;
    session_idle_type idle_check_type() override;

    // 判断会话闲置的时间(单位: 秒, 默认60秒判定会话为超时)
    void idle_check_seconds(int32_t seconds) override;
    int32_t idle_check_seconds() override;
};

}

#include "tcp_server_session_config.inl"

#pragma once

#include "tcp_session_def.h"

#include <cstdint>

namespace skynet::net {

/**
 * tcp_server acceptor config
 */
class tcp_server_acceptor_config
{
public:
    virtual ~tcp_server_acceptor_config() = default;

public:
    virtual void reset() = 0;

    // socket opt
public:
    // set/get socket recv buffer size
    virtual void socket_recv_buf_size(int32_t size) = 0;
    virtual int32_t socket_recv_buf_size() = 0;

    // set/get socket send buffer size
    virtual void socket_send_buf_size(int32_t size) = 0;
    virtual int32_t socket_send_buf_size() = 0;

    // socket keepalive option
    virtual void socket_keepalive(bool is_enable) = 0;
    virtual bool socket_keepalive() = 0;

    // socket nagle algorithm option
    virtual void socket_nodelay(bool is_enable) = 0;
    virtual bool socket_nodelay() = 0;

    // socket linger option
    virtual void socket_linger(int32_t timeout) = 0;
    virtual int32_t socket_linger() = 0;

    // acceptor
public:
    // 同时投递多少个async accept operations
    virtual void sync_accept_num(int32_t num) = 0;
    virtual int32_t sync_accept_num() = 0;
};


/**
 * tcp_server session config
 *
 * session memory estimate: 只估算收发所需内存(1个读消息缓存, 1个写消息队列), 其他忽略
 *                     msg_read_buf_size_ + msg_write_buf_size_ * msg_write_queue_size_
 *                     默认 8192 +  4096 * 4 = 24576 = 24K, 因此, 10000并发需要234MB以上内存
 */
class tcp_server_session_config
{
public:
    virtual ~tcp_server_session_config() = default;

public:
    virtual void reset() = 0;

    // socket opt
public:
    // get/set socket recv buffer size option
    virtual void socket_recv_buf_size(int32_t size) = 0;
    virtual int32_t socket_recv_buf_size() = 0;

    // get/set socket send buffer size option
    virtual void socket_send_buf_size(int32_t size) = 0;
    virtual int32_t socket_send_buf_size() = 0;

    // socket keepalive option
    virtual void socket_keepalive(bool is_enable) = 0;
    virtual bool socket_keepalive() = 0;

    // socket nagle algorithm option
    virtual void socket_nodelay(bool is_enable) = 0;
    virtual bool socket_nodelay() = 0;

    // socket linger option
    virtual void socket_linger(int32_t timeout) = 0;
    virtual int32_t socket_linger() = 0;

    // session config
public:
    // session所使用的线程数(ios池大小, 默认使用CPU Core进行计算)
    virtual void session_thread_num(int32_t num) = 0;
    virtual int32_t session_thread_num() = 0;

    // 会话池会话对象数量
    virtual void session_pool_size(int32_t size) = 0;
    virtual int32_t session_pool_size() = 0;

    // 会话读消息缓存大小(单次投递异步读数据, 非底层socket缓存)
    virtual void msg_read_buf_size(int32_t size) = 0;
    virtual int32_t msg_read_buf_size() = 0;

    // 会话写消息缓存大小(用于单次投递异步写数据, 非底层socket缓存)
    virtual void msg_write_buf_size(int32_t size) = 0;
    virtual int32_t msg_write_buf_size() = 0;

    // 会话写消息缓存队列大小(可以单次写超过4K数据, 内部会根据队列情况进行切片排队)
    virtual void msg_write_queue_size(int32_t size) = 0;
    virtual int32_t msg_write_queue_size() = 0;

    // 判定会话闲置的类型
    virtual void idle_check_type(idle_type type) = 0;
    virtual idle_type idle_check_type() = 0;

    // 判断会话闲置的时间(单位: 秒, 默认60秒判定会话为超时)
    virtual void idle_check_seconds(int32_t seconds) = 0;
    virtual int32_t idle_check_seconds() = 0;
};

}

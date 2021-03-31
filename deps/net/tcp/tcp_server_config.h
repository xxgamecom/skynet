#pragma once

#include "tcp_session_def.h"

#include <cstdint>
#include <thread>

namespace skynet { namespace net {

//--------------------------------------------------------------------------
// tcp_server_acceptor_config
//--------------------------------------------------------------------------

// tcp_server的acceptor配置项
class tcp_server_acceptor_config
{
    // socket option
private:
    int32_t socket_opt_recv_buf_size_ = 16 * 1024;      // socket选项接收缓存大小
    int32_t socket_opt_send_buf_size_ = 16 * 1024;      // socket选项发送缓存大小
    bool socket_opt_keepalive_ = true;                  // socket选项keepalive
    bool socket_opt_nodelay_ = true;                    // socket选项nodelay
    int32_t socket_opt_linger_ = 0;                     // socket选项linger

    // acceptor
private:
    int32_t sync_accept_num_ = 128;                     // 启动时投递异步accept次数, 默认128个

public:
    tcp_server_acceptor_config() = default;
    ~tcp_server_acceptor_config() = default;

public:
    // 重置
    void reset();

    // socket opt
public:
    // socket接收缓存大小
    void socket_recv_buf_size(int32_t size);
    int32_t socket_recv_buf_size();

    // socket发送缓存大小
    void socket_send_buf_size(int32_t size);
    int32_t socket_send_buf_size();

    // 是否开启socket的keepalive选项
    void socket_keepalive(bool is_enable);
    bool socket_keepalive();

    // 是否开启socket的nagle算法
    void socket_nodelay(bool is_enable);
    bool socket_nodelay();

    // socket的linger选项
    void socket_linger(int32_t timeout);
    int32_t socket_linger();

    // acceptor
public:
    // 同时投递多少个accept异步操作
    void sync_accept_num(int32_t num);
    int32_t sync_accept_num();
};

//--------------------------------------------------------------------------
// tcp_server_session_config
//--------------------------------------------------------------------------

// tcp_server的session配置项
// 服务端会话内存估算: 只估算收发所需内存(1个读消息缓存, 1个写消息队列), 其他忽略
//                     msg_read_buf_size_ + msg_write_buf_size_ * msg_write_queue_size_
//                     默认 8192 +  4096 * 4 = 24576 = 24K, 因此, 10000并发需要234MB以上内存
class tcp_server_session_config
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
    int32_t session_thread_num_ = 0;                    // ios池大小, 默认使用CPU Core进行计算
    int32_t session_pool_size_ = 64 * 1024;             // 会话池会话对象数量, 默认64K

    int32_t msg_read_buf_size_ = 8192;                  // 会话读消息缓存大小
                                                        // 用于单次投递异步读数据, 非底层socket缓存, 默认8K

    int32_t msg_write_buf_size_ = 4096;                 // 会话写消息缓存大小(用于单次投递异步写数据, 非底层socket缓存, 默认4K)
    int32_t msg_write_queue_size_ = 4;                  // 会话写消息缓存队列大小, 默认4
                                                        // 可以单次写超过4K数据, 内部会根据队列情况进行切片排队

    idle_type idle_check_type_ = IDLE_TYPE_BOTH;        // 判定会话闲置的类型
    int32_t idle_check_seconds_ = 60;                   // 判断会话闲置的时间(单位: 秒, 默认60秒判定会话为超时)

public:
    tcp_server_session_config() = default;
    ~tcp_server_session_config() = default;

public:
    // 重置
    void reset();

    // socket opt
public:
    // socket接收缓存大小
    void socket_recv_buf_size(int32_t size);
    int32_t socket_recv_buf_size();

    // socket发送缓存大小
    void socket_send_buf_size(int32_t size);
    int32_t socket_send_buf_size();

    // 是否开启socket的keepalive选项
    void socket_keepalive(bool is_enable);
    bool socket_keepalive();

    // 是否开启socket的nagle算法
    void socket_nodelay(bool is_enable);
    bool socket_nodelay();

    // socket的linger选项
    void socket_linger(int32_t timeout);
    int32_t socket_linger();

    // session配置
public:
    // session所使用的线程数(ios池大小, 默认使用CPU Core进行计算)
    void session_thread_num(int32_t num);
    int32_t session_thread_num();

    // 会话池会话对象数量
    void session_pool_size(int32_t size);
    int32_t session_pool_size();

    // 会话池会话对象最大数量
    void session_pool_max_size(int32_t size);
    int32_t session_pool_max_size();

    // 会话读消息缓存大小(单次投递异步读数据, 非底层socket缓存)
    void msg_read_buf_size(int32_t size);
    int32_t msg_read_buf_size();
    
    // 会话写消息缓存大小(用于单次投递异步写数据, 非底层socket缓存)
    void msg_write_buf_size(int32_t size);
    int32_t msg_write_buf_size();

    // 会话写消息缓存队列大小(可以单次写超过4K数据, 内部会根据队列情况进行切片排队)
    void msg_write_queue_size(int32_t size);
    int32_t msg_write_queue_size();

    // 判定会话闲置的类型
    void idle_check_type(idle_type type);
    idle_type idle_check_type();

    // 判断会话闲置的时间(单位: 秒, 默认60秒判定会话为超时)
    void idle_check_seconds(int32_t seconds);
    int32_t idle_check_seconds();
};

} }

#include "tcp_server_config.inl"


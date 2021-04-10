namespace skynet::net::impl {


// 重置
inline void tcp_server_acceptor_config_impl::reset()
{
    socket_opt_recv_buf_size_ = 16 * 1024;
    socket_opt_send_buf_size_ = 16 * 1024;
    socket_opt_keepalive_ = true;
    socket_opt_nodelay_ = true;
    socket_opt_linger_ = 0;
}

// socket接收缓存大小
inline void tcp_server_acceptor_config_impl::socket_recv_buf_size(int32_t size)
{
    socket_opt_recv_buf_size_ = size;
}

inline int32_t tcp_server_acceptor_config_impl::socket_recv_buf_size()
{
    return socket_opt_recv_buf_size_;
}

// socket发送缓存大小
inline void tcp_server_acceptor_config_impl::socket_send_buf_size(int32_t size)
{
    socket_opt_send_buf_size_ = size;
}

inline int32_t tcp_server_acceptor_config_impl::socket_send_buf_size()
{
    return socket_opt_send_buf_size_;
}

// 是否开启socket的keepalive选项
inline void tcp_server_acceptor_config_impl::socket_keepalive(bool is_enable)
{
    socket_opt_keepalive_ = is_enable;
}

inline bool tcp_server_acceptor_config_impl::socket_keepalive()
{
    return socket_opt_keepalive_;
}

// 是否开启socket的nagle算法
inline void tcp_server_acceptor_config_impl::socket_nodelay(bool is_enable)
{
    socket_opt_nodelay_ = is_enable;
}

inline bool tcp_server_acceptor_config_impl::socket_nodelay()
{
    return socket_opt_nodelay_;
}

// 设置socket的linger选项
inline void tcp_server_acceptor_config_impl::socket_linger(int32_t timeout)
{
    socket_opt_linger_ = timeout;
}

inline int32_t tcp_server_acceptor_config_impl::socket_linger()
{
    return socket_opt_linger_;
}

// 同时投递多少个accept异步操作
inline void tcp_server_acceptor_config_impl::sync_accept_num(int32_t num)
{
    sync_accept_num_ = num;
}

inline int32_t tcp_server_acceptor_config_impl::sync_accept_num()
{
    return sync_accept_num_;
}

}

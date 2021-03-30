namespace skynet { namespace network {

inline void tcp_client_session_config::socket_recv_buf_size(int32_t size)
{
    socket_opt_recv_buf_size_ = size;
}

inline int32_t tcp_client_session_config::socket_recv_buf_size()
{
    return socket_opt_recv_buf_size_;
}

inline void tcp_client_session_config::socket_send_buf_size(int32_t size)
{
    socket_opt_send_buf_size_ = size;
}

inline int32_t tcp_client_session_config::socket_send_buf_size()
{
    return socket_opt_send_buf_size_;
}

inline void tcp_client_session_config::socket_keepalive(bool is_enable)
{
    socket_opt_keepalive_ = is_enable;
}

inline bool tcp_client_session_config::socket_keepalive()
{
    return socket_opt_keepalive_;
}

inline void tcp_client_session_config::socket_nodelay(bool is_enable)
{
    socket_opt_nodelay_ = is_enable;
}

inline bool tcp_client_session_config::socket_nodelay()
{
    return socket_opt_nodelay_;
}

inline void tcp_client_session_config::socket_linger(int32_t timeout)
{
    socket_opt_linger_ = timeout;
}

inline int32_t tcp_client_session_config::socket_linger()
{
    return socket_opt_linger_;
}

inline void tcp_client_session_config::msg_read_buf_size(int32_t size)
{
    msg_read_buf_size_ = size;
}

inline int32_t tcp_client_session_config::msg_read_buf_size()
{
    return msg_read_buf_size_;
}

inline void tcp_client_session_config::msg_write_buf_size(int32_t size)
{
    msg_write_buf_size_ = size;
}

inline int32_t tcp_client_session_config::msg_write_buf_size()
{
    return msg_write_buf_size_;
}

inline void tcp_client_session_config::msg_write_queue_size(int32_t size)
{
    msg_write_queue_size_ = size;
}

inline int32_t tcp_client_session_config::msg_write_queue_size()
{
    return msg_write_queue_size_;
}

} }


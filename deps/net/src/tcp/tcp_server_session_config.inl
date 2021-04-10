namespace skynet::net::impl {

//--------------------------------------------------------------------------
// tcp_server_acceptor_config_impl
//--------------------------------------------------------------------------


//--------------------------------------------------------------------------
// tcp_server_session_config_impl
//--------------------------------------------------------------------------

// 重置
inline void tcp_server_session_config_impl::reset()
{
    socket_opt_recv_buf_size_ = 16 * 1024;
    socket_opt_send_buf_size_ = 16 * 1024;
    socket_opt_keepalive_ = true;
    socket_opt_nodelay_ = true;
    socket_opt_linger_ = 0;



    session_thread_num_ = 0;
    session_pool_size_ = 64 * 1024;

    msg_read_buf_size_ = 8192;


    msg_write_buf_size_ = 4096;
    msg_write_queue_size_ = 4;


    idle_check_type_ = IDLE_TYPE_BOTH;
    idle_check_seconds_ = 60;
}

// socket接收缓存大小
inline void tcp_server_session_config_impl::socket_recv_buf_size(int32_t size)
{
    socket_opt_recv_buf_size_ = size;
}

inline int32_t tcp_server_session_config_impl::socket_recv_buf_size()
{
    return socket_opt_recv_buf_size_;
}

// socket发送缓存大小
inline void tcp_server_session_config_impl::socket_send_buf_size(int32_t size)
{
    socket_opt_send_buf_size_ = size;
}

inline int32_t tcp_server_session_config_impl::socket_send_buf_size()
{
    return socket_opt_send_buf_size_;
}

// 是否开启socket的keepalive选项
inline void tcp_server_session_config_impl::socket_keepalive(bool is_enable)
{
    socket_opt_keepalive_ = is_enable;
}

inline bool tcp_server_session_config_impl::socket_keepalive()
{
    return socket_opt_keepalive_;
}

// 是否开启socket的nagle算法
inline void tcp_server_session_config_impl::socket_nodelay(bool is_enable)
{
    socket_opt_nodelay_ = is_enable;
}

inline bool tcp_server_session_config_impl::socket_nodelay()
{
    return socket_opt_nodelay_;
}

// 设置socket的linger选项
inline void tcp_server_session_config_impl::socket_linger(int32_t timeout)
{
    socket_opt_linger_ = timeout;
}

inline int32_t tcp_server_session_config_impl::socket_linger()
{
    return socket_opt_linger_;
}

// session所使用的线程数(ios池大小, 默认使用CPU Core进行计算)
inline void tcp_server_session_config_impl::session_thread_num(int32_t num)
{
    session_thread_num_ = num;
}

inline int32_t tcp_server_session_config_impl::session_thread_num()
{
    return session_thread_num_;
}

// 会话池会话对象数量
inline void tcp_server_session_config_impl::session_pool_size(int32_t size)
{
    session_pool_size_ = size;
}

inline int32_t tcp_server_session_config_impl::session_pool_size()
{
    return session_pool_size_;
}

// 会话读消息缓存大小(单次投递异步读数据, 非底层socket缓存)
inline void tcp_server_session_config_impl::read_buf_size(int32_t size)
{
    msg_read_buf_size_ = size;
}

inline int32_t tcp_server_session_config_impl::read_buf_size()
{
    return msg_read_buf_size_;
}

// 会话写消息缓存大小(用于单次投递异步写数据, 非底层socket缓存)
inline void tcp_server_session_config_impl::write_buf_size(int32_t size)
{
    msg_write_buf_size_ = size;
}

inline int32_t tcp_server_session_config_impl::write_buf_size()
{
    return msg_write_buf_size_;
}

// 会话写消息缓存队列大小(可以单次写超过4K数据, 内部会根据队列情况进行切片排队)
inline void tcp_server_session_config_impl::write_queue_size(int32_t size)
{
    msg_write_queue_size_ = size;
}

inline int32_t tcp_server_session_config_impl::write_queue_size()
{
    return msg_write_queue_size_;
}

// 判定会话闲置的类型
inline void tcp_server_session_config_impl::idle_check_type(idle_type type)
{
    idle_check_type_ = type;
}

inline idle_type tcp_server_session_config_impl::idle_check_type()
{
    return idle_check_type_;
}

// 判断会话闲置的时间(单位: 秒, 默认60秒判定会话为超时)
inline void tcp_server_session_config_impl::idle_check_seconds(int32_t seconds)
{
    idle_check_seconds_ = seconds;
}

inline int32_t tcp_server_session_config_impl::idle_check_seconds()
{
    return idle_check_seconds_;
}

}

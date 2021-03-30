namespace skynet { namespace network {

inline void tcp_session::set_event_handler(std::shared_ptr<tcp_session_handler> event_handler_ptr)
{
    event_handler_ptr_ = event_handler_ptr;
}

inline void tcp_session::session_id(session_id_t id)
{
    session_id_ = id;
}

inline session_id_t tcp_session::session_id()
{
    return session_id_;
}

inline void tcp_session::start_read()
{
    assert(state_ == SESSION_STATE_OPEN);

    // 更新读写时间
    last_read_time_ = last_write_time_ = std::chrono::steady_clock::steady_clock::now();

    async_read_once();
}

inline bool tcp_session::is_open()
{
    return (state_ == SESSION_STATE_OPEN &&
            socket_ptr_ != nullptr &&
            socket_ptr_->is_open());
}

inline std::shared_ptr<asio::ip::tcp::socket> tcp_session::get_socket()
{
    return socket_ptr_;
}

inline int64_t tcp_session::read_bytes()
{
    return read_bytes_;
}

inline int64_t tcp_session::write_bytes()
{
    return write_bytes_;
}

inline int64_t tcp_session::delta_read_bytes()
{
    int64_t delta_bytes = delta_read_bytes_;
    delta_read_bytes_ = 0;
    return delta_bytes;
}

inline int64_t tcp_session::delta_write_bytes()
{
    int64_t delta_bytes = delta_write_bytes_;
    delta_write_bytes_ = 0;
    return delta_bytes;
}

inline asio::ip::tcp::endpoint tcp_session::remote_endpoint()
{
    if (socket_ptr_ == nullptr || socket_ptr_->is_open() == false) return asio::ip::tcp::endpoint();

    asio::error_code ec;
    return socket_ptr_->remote_endpoint(ec);
}

inline asio::ip::tcp::endpoint tcp_session::local_endpoint()
{
    if (socket_ptr_ == nullptr || socket_ptr_->is_open() == false) return asio::ip::tcp::endpoint();

    asio::error_code ec;
    return socket_ptr_->local_endpoint(ec);
}

inline void tcp_session::async_read_once()
{
    // read some data
    msg_read_buf_ptr_->clear();
    socket_ptr_->async_read_some(asio::buffer(msg_read_buf_ptr_->data(), msg_read_buf_ptr_->capacity()),
                                 std::bind(&tcp_session::handle_async_read, shared_from_this(),
                                           std::placeholders::_1, std::placeholders::_2));
}

inline void tcp_session::async_write_once()
{
    // 从队列头部取一个数据缓存进行异步写投递
    std::shared_ptr<io_buffer> buf_ptr = msg_write_queue_.front();
    if (buf_ptr != nullptr)
    {
        asio::async_write(*socket_ptr_,
                          asio::buffer(buf_ptr->data(), buf_ptr->data_size()),
                          std::bind(&tcp_session::handle_async_write, shared_from_this(),
                                    std::placeholders::_1, std::placeholders::_2));
    }
}

} }


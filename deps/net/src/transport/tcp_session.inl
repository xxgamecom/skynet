namespace skynet::net::impl {

inline void tcp_session_impl::set_event_handler(std::shared_ptr<tcp_session_handler> event_handler_ptr)
{
    event_handler_ptr_ = event_handler_ptr;
}

inline void tcp_session_impl::socket_id(uint32_t id)
{
    socket_id_ = id;
}

inline uint32_t tcp_session_impl::socket_id()
{
    return socket_id_;
}

inline void tcp_session_impl::start_read()
{
    assert(state_ == SESSION_STATE_OPEN);

    last_read_time_ = last_write_time_ = std::chrono::steady_clock::steady_clock::now();

    async_read_once();
}

inline bool tcp_session_impl::is_open()
{
    return (state_ == SESSION_STATE_OPEN &&
            socket_ptr_ != nullptr &&
            socket_ptr_->is_open());
}

inline std::shared_ptr<asio::ip::tcp::socket> tcp_session_impl::get_socket()
{
    return socket_ptr_;
}

inline int64_t tcp_session_impl::read_bytes()
{
    return read_bytes_;
}

inline int64_t tcp_session_impl::write_bytes()
{
    return write_bytes_;
}

inline int64_t tcp_session_impl::delta_read_bytes()
{
    int64_t delta_bytes = delta_read_bytes_;
    delta_read_bytes_ = 0;
    return delta_bytes;
}

inline int64_t tcp_session_impl::delta_write_bytes()
{
    int64_t delta_bytes = delta_write_bytes_;
    delta_write_bytes_ = 0;
    return delta_bytes;
}

inline asio::ip::tcp::endpoint tcp_session_impl::remote_endpoint()
{
    if (socket_ptr_ == nullptr || socket_ptr_->is_open() == false) return asio::ip::tcp::endpoint();

    asio::error_code ec;
    return socket_ptr_->remote_endpoint(ec);
}

inline asio::ip::tcp::endpoint tcp_session_impl::local_endpoint()
{
    if (socket_ptr_ == nullptr || socket_ptr_->is_open() == false) return asio::ip::tcp::endpoint();

    asio::error_code ec;
    return socket_ptr_->local_endpoint(ec);
}

inline void tcp_session_impl::async_read_once()
{
    // read some data
    msg_read_buf_ptr_->clear();

    //
    auto self(shared_from_this());
    socket_ptr_->async_read_some(asio::buffer(msg_read_buf_ptr_->data(), msg_read_buf_ptr_->capacity()),
                                 [this, self](const asio::error_code& ec, size_t bytes_transferred) {
                                     handle_async_read(ec, bytes_transferred);
                                 });
}

inline void tcp_session_impl::async_write_once()
{
    // 从队列头部取一个数据缓存进行异步写投递
    std::shared_ptr<io_buffer> buf_ptr = msg_write_queue_.front();
    if (buf_ptr != nullptr)
    {
        auto self(shared_from_this());
        asio::async_write(*socket_ptr_, asio::buffer(buf_ptr->data(), buf_ptr->data_size()),
                          [this, self](const asio::error_code& ec, size_t bytes_transferred) {
                              handle_async_write(ec, bytes_transferred);
                          });
    }
}

}


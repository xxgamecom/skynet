namespace skynet::net::impl {

inline tcp_server_acceptor_config& tcp_server_impl::get_acceptor_config()
{
    return acceptor_config_;
}

inline tcp_server_session_config& tcp_server_impl::get_session_config()
{
    return session_config_;
}

inline std::shared_ptr<io_statistics> tcp_server_impl::get_io_statistics()
{
    return io_statistics_ptr_;
}

inline bool tcp_server_impl::do_accept(std::shared_ptr<tcp_acceptor> acceptor_ptr)
{
    std::shared_ptr<tcp_session> session_ptr = session_manager_ptr_->create_session();
    if (session_ptr == nullptr)
        return false;

    // 设置会话处理句柄
    session_ptr->set_event_handler(shared_from_this());

    // 设置会话
    if (session_ptr->open(session_ios_pool_ptr_->select_one()) == false)
    {
        session_ptr->close();
        session_manager_ptr_->release_session(session_ptr);
        return false;
    }

    // 接收连接
    acceptor_ptr->accept_once(session_ptr);

    return true;
}

inline std::string tcp_server_impl::make_key(const asio::ip::tcp::endpoint& ep)
{
    return make_key(ep.address().to_string(), ep.port());
}

inline std::string tcp_server_impl::make_key(const std::string& ip, uint16_t port)
{
    return ip + ":" + std::to_string(port);
}

}


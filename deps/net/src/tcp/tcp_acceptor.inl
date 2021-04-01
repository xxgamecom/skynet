namespace skynet::net::impl {

inline void tcp_acceptor_impl::set_event_handler(std::shared_ptr<tcp_acceptor_handler> event_handler_ptr)
{
    event_handler_ptr_ = event_handler_ptr;
}

inline asio::ip::tcp::endpoint tcp_acceptor_impl::local_endpoint() const
{
    if (acceptor_ptr_ == nullptr || acceptor_ptr_->is_open() == false)
        return asio::ip::tcp::endpoint();

    asio::error_code ec;
    return acceptor_ptr_->local_endpoint(ec);
}

}

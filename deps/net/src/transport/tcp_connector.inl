namespace skynet::net::impl {

inline void tcp_connector_impl::set_event_handler(std::shared_ptr<tcp_connector_handler> event_handler_ptr)
{
    event_handler_ptr_ = event_handler_ptr;
}

inline bool tcp_connector_impl::start_connect_timer(std::shared_ptr<tcp_session> session_ptr, int32_t timeout_seconds)
{
    if (timeout_seconds > 0)
    {
        connect_timer_ptr_ = std::make_shared<asio::steady_timer>(ios_ptr_->get_raw_ios());
        if (connect_timer_ptr_ == nullptr)
            return false;

        asio::error_code ec;
        connect_timer_ptr_->expires_from_now(std::chrono::seconds(timeout_seconds), ec);
        if (ec)
        {
            connect_timer_ptr_.reset();
            return false;
        }

        auto self(shared_from_this());
        connect_timer_ptr_->async_wait([this, self, session_ptr](const asio::error_code& ec) {
            handle_connect_timeout(session_ptr, ec);
        });
    }

    return true;
}

inline void tcp_connector_impl::stop_connect_timer()
{
    // 删除定时器
    if (connect_timer_ptr_ != nullptr)
    {
        asio::error_code ec;
        connect_timer_ptr_->cancel(ec);
        connect_timer_ptr_.reset();
    }
}

}

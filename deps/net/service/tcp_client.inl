namespace skynet { namespace network {

inline tcp_client_session_config& tcp_client::get_session_config()
{
    return session_config_;
}

inline bool tcp_client::send(const char* data_ptr, int32_t data_len)
{
    assert(session_ptr_ != nullptr && session_ptr_->is_open());
    if (session_ptr_ == nullptr || session_ptr_->is_open() == false)
        return false;

    if (session_ptr_->write(data_ptr, data_len) == false)
        return false;

    return true;
}

} }

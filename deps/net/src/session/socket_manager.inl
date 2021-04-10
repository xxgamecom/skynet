namespace skynet::net::impl {

inline size_t socket_manager_impl::get_session_count()
{
    std::lock_guard<std::mutex> guard(sessions_mutex_);

    return session_used_map_.size();
}

inline uint32_t socket_manager_impl::new_socket_id()
{
    ++id_generator_;
    if (id_generator_ == INVALID_SESSION_ID)
        ++id_generator_;

    return id_generator_;
}

}


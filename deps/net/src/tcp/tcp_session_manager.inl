namespace skynet::net::impl {

// 初始化
inline bool tcp_session_manager::init(int32_t session_pool_size,
                                      int32_t msg_read_buf_size,
                                      int32_t msg_write_buf_size,
                                      int32_t msg_write_queue_size)
{
    // 创建会话对象池
    session_pool_ptr_ = std::make_shared<object_pool<tcp_session_impl>>(session_pool_size,
                                                                        msg_read_buf_size,
                                                                        msg_write_buf_size,
                                                                        msg_write_queue_size);
    if (session_pool_ptr_ == nullptr)
        return false;

    return true;
}

// 清理
inline void tcp_session_manager::fini()
{
    std::lock_guard<std::mutex> guard(sessions_mutex_);

    session_used_map_.clear();
}

// 创建/是否会话实例
inline std::shared_ptr<tcp_session> tcp_session_manager::create_session()
{
    std::lock_guard<std::mutex> guard(sessions_mutex_);

    std::shared_ptr<tcp_session_impl> session_ptr = session_pool_ptr_->alloc();
    if (session_ptr == nullptr)
        return nullptr;

    // 添加到会话表
    session_id_t id = generate_session_id();
    session_ptr->session_id(id);
    session_used_map_.insert(std::make_pair(id, session_ptr));

    return session_ptr;
}

inline void tcp_session_manager::release_session(std::shared_ptr<tcp_session> session_ptr)
{
    std::lock_guard<std::mutex> guard(sessions_mutex_);

    if (session_ptr != nullptr)
    {
        session_used_map_.erase(session_ptr->session_id());
        session_pool_ptr_->free(std::dynamic_pointer_cast<tcp_session_impl>(session_ptr));
    }
}

// 获取会话数量
inline size_t tcp_session_manager::get_session_count()
{
    std::lock_guard<std::mutex> guard(sessions_mutex_);

    return session_used_map_.size();
}

// 生成session_id
inline session_id_t tcp_session_manager::generate_session_id()
{
    ++id_generator_;
    if (id_generator_ == INVALID_SESSION_ID) ++id_generator_;

    return id_generator_;
}

}


namespace skynet::net::impl {

// 队列是否就绪
inline bool tcp_session_write_queue::is_inited()
{
    return is_inited_;
}

// 写队列是否已满
inline bool tcp_session_write_queue::is_full()
{
    std::lock_guard<std::mutex> guard(write_queue_mutex_);

    return (write_queue_.size() >= write_queue_size_);
}

// 写队列是否已空
inline bool tcp_session_write_queue::is_empty()
{
    std::lock_guard<std::mutex> guard(write_queue_mutex_);

    return write_queue_.empty();
}

// 弹出头部数据(发送完后回收, 一次push可能会触发多个pop, 因为内部对大数据切片了)
inline void tcp_session_write_queue::pop_front()
{
    // 数据保护
    std::lock_guard<std::mutex> guard(write_queue_mutex_);

    // 有数据才能front操作, 否则deque会出错
    if (!write_queue_.empty())
    {
        // 弹出后回收缓存
        std::shared_ptr<io_buffer> buf_ptr = write_queue_.front();
        write_queue_.pop_front();
        if (buf_ptr != nullptr)
            write_msg_buf_pool_ptr_->free(buf_ptr);
    }
}

// 取头部数据(不弹出数据, 弹出数据需要调用pop)
inline std::shared_ptr<io_buffer> tcp_session_write_queue::front()
{
    // 数据保护
    std::lock_guard<std::mutex> guard(write_queue_mutex_);

    if (write_queue_.empty())
        return nullptr;
    else
        return write_queue_.front();
}

}


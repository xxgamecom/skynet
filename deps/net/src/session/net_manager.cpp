#include "net_manager.h"

namespace skynet::net::impl {

// 初始化
bool net_manager_impl::init(int32_t session_pool_size, int32_t read_buf_size, int32_t write_buf_size, int32_t write_queue_size)
{
    // create session pool
    tcp_session_pool_ptr_ = std::make_shared<object_pool<tcp_session_impl>>(session_pool_size, read_buf_size, write_buf_size, write_queue_size);
    if (tcp_session_pool_ptr_ == nullptr)
        return false;

    udp_session_pool_ptr_ = std::make_shared<object_pool<udp_session_impl>>(session_pool_size);
    if (udp_session_pool_ptr_ == nullptr)
        return false;

    return true;
}

void net_manager_impl::fini()
{
    std::lock_guard<std::mutex> guard(sessions_mutex_);

    session_used_map_.clear();
}

std::shared_ptr<basic_session> net_manager_impl::create_session(session_type type)
{
    std::lock_guard<std::mutex> guard(sessions_mutex_);

    std::shared_ptr<basic_session> session_ptr;

    if (type == session_type::TCP)
    {
        session_ptr = tcp_session_pool_ptr_->alloc();
    }
    else if (type == session_type::UDP)
    {
//        session_ptr = udp_session_pool_ptr_->alloc();
    }

    if (session_ptr == nullptr)
        return nullptr;

    // alloc session id
    uint32_t id = new_socket_id();
    session_ptr->socket_id(id);

    // save session info
    session_used_map_.insert(std::make_pair(id, session_ptr));

    return session_ptr;
}

void net_manager_impl::release_session(std::shared_ptr<basic_session> session_ptr)
{
    std::lock_guard<std::mutex> guard(sessions_mutex_);

    if (session_ptr != nullptr)
    {
        session_used_map_.erase(session_ptr->socket_id());

        //
        std::shared_ptr<tcp_session_impl> tmp1 = std::dynamic_pointer_cast<tcp_session_impl>(session_ptr);
        if (tmp1 != nullptr)
            tcp_session_pool_ptr_->free(tmp1);
//        std::shared_ptr<udp_session_impl> tmp2 = std::dynamic_pointer_cast<udp_session_impl>(session_ptr);
//        if (tmp2 != nullptr)
//            udp_session_pool_ptr_->free(tmp2);
    }
}

// 获取所有会话
size_t net_manager_impl::get_sessions(std::vector<std::weak_ptr<basic_session>>& sessions)
{
    std::lock_guard<std::mutex> guard(sessions_mutex_);

    std::transform(session_used_map_.begin(), session_used_map_.end(), std::back_inserter(sessions),
                   std::bind(&session_map::value_type::second, std::placeholders::_1));

    return session_used_map_.size();
}

}


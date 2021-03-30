#include "tcp_session_manager.h"

namespace skynet { namespace network {

// 获取所有会话
size_t tcp_session_manager::get_sessions(std::vector<std::weak_ptr<tcp_session>>& sessions)
{
    std::lock_guard<std::mutex> guard(sessions_mutex_);
    
    std::transform(session_used_map_.begin(), session_used_map_.end(), std::back_inserter(sessions),
                   std::bind(&session_map::value_type::second, std::placeholders::_1));

    return session_used_map_.size();
}

} }


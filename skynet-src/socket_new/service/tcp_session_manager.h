#pragma once

#include "../core/object_pool.h"
#include "../core/tcp_session.h"

#include <map>
#include <mutex>

namespace skynet { namespace network {

// tcp会话管理
class tcp_session_manager final
{
public:
    typedef std::map<session_id_t, std::weak_ptr<tcp_session>> session_map;

    // 会话信息
protected:
    session_id_t                                id_generator_ = INVALID_SESSION_ID;         // 会话ID生成器

    std::shared_ptr<object_pool<tcp_session>>   session_pool_ptr_;                          // 会话对象池
    session_map                                 session_used_map_;                          // 会话分配表
    std::mutex                                  sessions_mutex_;                            // 会话表保护

public:
    tcp_session_manager() = default;
    ~tcp_session_manager() = default;

    // 会话管理
public:
    // 初始化
    bool init(int32_t session_pool_size,
              int32_t msg_read_buf_size,
              int32_t msg_write_buf_size,
              int32_t msg_write_queue_size);
    // 清理
    void fini();

    // 创建/释放会话实例
    std::shared_ptr<tcp_session> create_session();
    void release_session(std::shared_ptr<tcp_session> session_ptr);

    // 获取所有会话
    size_t get_sessions(std::vector<std::weak_ptr<tcp_session>>& sessions);
    // 获取会话数量
    size_t get_session_count();

private:
    // 生成session_id
    session_id_t generate_session_id();

    // noncopyable
private:
    tcp_session_manager(const tcp_session_manager&) = delete;
    tcp_session_manager& operator=(const tcp_session_manager&) = delete;
};

} }

#include "tcp_session_manager.inl"


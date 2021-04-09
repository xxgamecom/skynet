#pragma once

#include "../base/object_pool.h"

#include "base/session_manager_i.h"
#include "../tcp/tcp_session.h"
#include "../udp/udp_session.h"

#include <map>
#include <mutex>

// forward declare
namespace skynet::net {
class tcp_session;
class udp_session;
}

namespace skynet::net::impl {

/**
 * session manager, used for manager tcp/udp session
 */
class session_manager_impl : public session_manager,
                             public asio::noncopyable
{
public:
    typedef std::map<session_id_t, std::weak_ptr<basic_session>> session_map;

protected:
    session_id_t id_generator_ = INVALID_SESSION_ID;                        // session id generator

    std::shared_ptr<object_pool<tcp_session_impl>> tcp_session_pool_ptr_;   // tcp session pool
    std::shared_ptr<object_pool<udp_session_impl>> udp_session_pool_ptr_;   // udp session pool
    session_map session_used_map_;                                          // session alloced map
    std::mutex sessions_mutex_;                                             // session pool protoected

public:
    session_manager_impl() = default;
    ~session_manager_impl() override = default;

    // session_manager impl
public:
    bool init(int32_t session_pool_size, int32_t read_buf_size, int32_t write_buf_size, int32_t write_queue_size) override;
    void fini() override;

    // create/release session, todo: refactor to template function
    std::shared_ptr<basic_session> create_session(session_type type) override;
    void release_session(std::shared_ptr<basic_session> session_ptr) override;

    // get all sessions
    size_t get_sessions(std::vector<std::weak_ptr<basic_session>>& sessions) override;
    // get the number of session (include tcp & udp session)
    size_t get_session_count() override;

    // create session id
    session_id_t create_session_id() override;
};

}

#include "session_manager.inl"


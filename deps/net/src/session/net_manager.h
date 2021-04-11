#pragma once

#include "base/net_manager_i.h"

#include "../base/object_pool.h"
#include "../transport/tcp_session.h"
#include "../transport/udp_session.h"

#include <map>
#include <mutex>
#include <atomic>

// forward declare
namespace skynet::net {
class tcp_session;
class udp_session;
}

namespace skynet::net::impl {

/**
 * socket manager, used for manager tcp/udp session
 *
 * socket_id is a logic id, include:
 * - tcp server id
 * - tcp client session id
 * - udp server id
 * - udp client session id
 */
class net_manager_impl : public net_manager,
                         public asio::noncopyable
{
public:
    typedef std::map<uint32_t, std::weak_ptr<basic_session>> session_map;

    // socket id
protected:
    std::atomic<uint32_t> id_generator_ = INVALID_SOCKET_ID;                // socket id generator

    // session
protected:
    std::shared_ptr<object_pool<tcp_session_impl>> tcp_session_pool_ptr_;   // tcp session pool
    std::shared_ptr<object_pool<udp_session_impl>> udp_session_pool_ptr_;   // udp session pool
    session_map session_used_map_;                                          // session alloced map
    std::mutex sessions_mutex_;                                             // session pool protoected

public:
    net_manager_impl() = default;
    ~net_manager_impl() override = default;

    // net_manager impl
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

    // create a new socket id
    uint32_t new_socket_id() override;
};

}

#include "net_manager.inl"


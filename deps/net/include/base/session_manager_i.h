#pragma once

#include "session_i.h"

namespace skynet::net {

class basic_session;

/**
 * session manager interface
 */
class session_manager
{
public:
    virtual ~session_manager() = default;

public:
    virtual bool init(int32_t session_pool_size, int32_t read_buf_size, int32_t write_buf_size, int32_t write_queue_size) = 0;
    virtual void fini() = 0;

    // create/release session
    virtual std::shared_ptr<basic_session> create_session(session_type type) = 0;
    virtual void release_session(std::shared_ptr<basic_session> session_ptr) = 0;

    // get all sessions
    virtual size_t get_sessions(std::vector<std::weak_ptr<basic_session>>& sessions) = 0;
    // get the number of session (include tcp & udp session)
    virtual size_t get_session_count() = 0;

    // create a new socket id
    virtual uint32_t new_socket_id() = 0;
};

}


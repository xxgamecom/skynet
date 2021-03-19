#pragma once

#include "hash_id.h"
#include "data_buffer.h"

namespace skynet {

// forward decleare
class service_context;

namespace service {

//
struct connection
{
    int                         socket_id = 0;                  // skynet socket id
    uint32_t                    agent_svc_handle = 0;           //
    uint32_t                    client_svc_handle = 0;          //
    char                        remote_name[32] { 0 };          //
    data_buffer                 buffer;                         //
};

// gate service mod own data
struct gate_mod
{
    service_context*            ctx = nullptr;
    int                         listen_id = -1;
    uint32_t                    watchdog_svc_handle = 0;
    uint32_t                    broker_svc_handle = 0;
    int                         client_msg_tag = 0;
    int                         header_size = 0;
    int                         max_connection = 0;
    struct hashid               hash;
    connection*                 conn = nullptr;
    // todo: save message pool ptr for release
    message_pool                mp;
};

} }


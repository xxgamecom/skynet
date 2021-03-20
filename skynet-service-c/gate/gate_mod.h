#pragma once

#include "hash_id.h"
#include "data_buffer.h"

#include <cstdint>
#include <memory>

// forward decleare
namespace skynet {
class service_context;
}

namespace skynet { namespace service {

//
struct connection
{
    int                         socket_id = -1;                 // skynet socket id
    uint32_t                    agent_svc_handle = 0;           //
    uint32_t                    client_svc_handle = 0;          //
    char                        remote_name[32] { 0 };          //
    data_buffer                 buffer;                         //
};

// gate service mod own data
struct gate_mod
{
    service_context*            svc_ctx = nullptr;              //
    int                         listen_id = -1;                 // listen socket id
    uint32_t                    watchdog_svc_handle = 0;        //
    uint32_t                    broker_svc_handle = 0;          //
    int                         msg_ptype = 0;                  // message protocol type
    int                         header_size = 0;                // package header size, (header == 'S') ? 2 : 4;
    hash_id                     hash;                           //

    // connection info
    int                         max_connection = 0;             // max connection
    std::shared_ptr<connection> connections;                    // connection array

    // todo: save message pool ptr for release
    message_pool                mp;
};

} }


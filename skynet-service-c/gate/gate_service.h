#pragma once

#include "skynet.h"
#include "hash_id.h"
#include "data_buffer.h"

namespace skynet { namespace service {

struct connection
{
    int                         socket_id = -1;                 // skynet socket id
    uint32_t                    agent_svc_handle = 0;           //
    uint32_t                    client_svc_handle = 0;          //
    char                        remote_name[32] { 0 };          //
    data_buffer                 buffer;                         //
};

//
class gate_service : public cservice
{
public:
    service_context*            svc_ctx_ = nullptr;             //
    int                         listen_id_ = -1;                // listen socket id
    uint32_t                    watchdog_svc_handle_ = 0;       //
    uint32_t                    broker_svc_handle_ = 0;         //
    int                         msg_ptype_ = 0;                 // message protocol type
    int                         header_size_ = 0;               // package header size, (header == 'S') ? 2 : 4;
    hash_id                     hash_;                          //

    // connection info
    int                         max_connection_ = 0;            // max connection
    std::shared_ptr<connection> connections_;                   // connection array

    // todo: save message pool ptr for release
    message_pool                mp_;

public:
    virtual ~gate_service() = default;

    // cservice impl
public:
    bool init(service_context* svc_ctx, const char* param) override;
    void fini() override;
    void signal(int signal) override;
    int callback(service_context* svc_ctx, int msg_ptype, int session_id, uint32_t src_svc_handle, const void* msg, size_t sz) override;

};

} }

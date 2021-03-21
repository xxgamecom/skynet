#pragma once

#include "skynet.h"

namespace skynet { namespace service {

//
class gate_service : public cservice
{
public:
    virtual ~gate_service() = default;

    // cservice impl
public:
    bool init(service_context* svc_ctx, const char* param) override;
    void fini() override;
    void signal(int signal) override;
    int callback(service_context* svc_ctx, void* ud, int msg_ptype, int session_id, uint32_t src_svc_handle, const void* msg, size_t sz) override;

};

} }

/**
 * C service mod interface
 *
 * C service mod specs:
 * 1) mod api:
 *   - create_service:  used to create c service instance.
 *   - release_service: used to release c service instance.
 *
 * 2) service api
 *   - init:     used to initialize c service instance, it's mainly used to set service message callback.
 *   - signal:   called when a signal is triggered.
 *   - callback: service messsage callback
 *
 * 3) c service mod search path set by config file's cservice_path variable.
 *
 * config file
 * cservice_path = root.."skynet/cservice/?.so;" .. root.."user/cservice/?.so"
 */

#pragma once

#include <cstdint>
#include <cctype>

// forward declare
namespace skynet {
class service_context;
}

// mod interface
#define CSERVICE_MOD_API     extern "C"

namespace skynet {

// c service callback prototype
typedef int (*cservice_callback)(service_context* svc_ctx, void* ud, int type, int session, uint32_t source , const void* msg, size_t sz);

// c service mod interface
class cservice
{
public:
    virtual ~cservice() = default;

    // interface
public:
    // initialize c service mod
    virtual bool init(service_context* svc_ctx, const char* param) = 0;
    // clean c service mod
    virtual void fini() = 0;
    // signal c service mod
    virtual void signal(int signal) = 0;
    // snlua service message callback
    virtual int callback(service_context* svc_ctx, void* ud, int msg_ptype, int session_id, uint32_t src_svc_handle, const void* msg, size_t sz) = 0;

};

}

//----------------------------------------------------
// c service mod interface
//----------------------------------------------------

typedef skynet::cservice* (*create_cservice_proc)();
typedef void (*release_cservice_proc)(skynet::cservice* svc_ptr);

#define CREATE_CSERVICE_PROC   ("create_service")
#define RELEASE_CSERVICE_PROC  ("release_service")


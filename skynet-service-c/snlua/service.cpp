//
// c service mod dll entry
//

#include "snlua_service.h"

// create c service mod: snlua_service
CSERVICE_MOD_API skynet::service::snlua_service* create_service()
{
    return new skynet::service::snlua_service;
}

// release c service mod: snlua_service
CSERVICE_MOD_API void release_service(skynet::service::cservice_mod* svc_ptr)
{
    auto snlua_svc_ptr = dynamic_cast<skynet::service::snlua_service*>(svc_ptr);
    if (snlua_svc_ptr != nullptr)
    {
        delete snlua_svc_ptr;
    }
}

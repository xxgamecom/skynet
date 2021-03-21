//
// dll entry
//

#include "gate_service.h"

// create c service
CSERVICE_MOD_API skynet::service::cservice_mod* create_service()
{
    return new skynet::service::gate_service;
}

// release c service
CSERVICE_MOD_API void release_service(skynet::service::cservice_mod* service_ptr)
{
    auto gate_service_ptr = dynamic_cast<skynet::service::gate_service*>(service_ptr);
    if (gate_service_ptr != nullptr)
    {
        delete gate_service_ptr;
    }
}


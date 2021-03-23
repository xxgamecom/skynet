//
// logger mod dll entry
//

#include "logger_service.h"

CSERVICE_MOD_API skynet::cservice* create_service()
{
    return new skynet::service::logger_service;
}

CSERVICE_MOD_API void release_service(skynet::cservice* svc_ptr)
{
    auto logger_svc_ptr = dynamic_cast<skynet::service::logger_service*>(svc_ptr);
    if (logger_svc_ptr != nullptr)
    {
        delete logger_svc_ptr;
    }
}


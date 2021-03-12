/**
 * only used to log service message
 */

#pragma once

#include <stdio.h>
#include <cstdint>

namespace skynet {

// forward declare
struct skynet_context;

// 
class service_log final
{
public:
    // open service log file
    static FILE* open_log_file(skynet_context* svc_ctx, uint32_t svc_handle);
    // close service log file
    static void close_log_file(skynet_context* svc_ctx, FILE* f, uint32_t svc_handle);

    // log service message
    static void log(FILE* f, uint32_t src_svc_handle, int type, int session, void* buffer, size_t sz);
};

}


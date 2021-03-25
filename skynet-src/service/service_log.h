#pragma once

#include <cstdio>
#include <cstdint>

namespace skynet {

// forward declare
class service_context;

/**
 * only used to log service message
 */
class service_log final
{
public:
    // open service log file
    static FILE* open_log_file(service_context* svc_ctx, uint32_t svc_handle);
    // close service log file
    static void close_log_file(service_context* svc_ctx, FILE* f, uint32_t svc_handle);

    // log service message
    static void log(FILE* f, uint32_t src_svc_handle, int msg_ptype, int session_id, void* buffer, size_t sz);
};

}


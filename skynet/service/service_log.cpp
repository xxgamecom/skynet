#include "service_log.h"

#include "../log/log.h"

#include "../mq/mq_msg.h"

#include "../node/node_env.h"
#include "../node/node_socket.h"

#include "../timer/timer_manager.h"

#include <string>

namespace skynet {

// log blob info
static void _log_blob(FILE* f, void* blob_ptr, size_t blob_sz)
{
    uint8_t* buf_ptr = (uint8_t*)blob_ptr;
    for (int i = 0; i != blob_sz; i++)
    {
        ::fprintf(f, "%02x", buf_ptr[i]);
    }
}

// log socket info
static void _log_socket(FILE* f, skynet_socket_message* msg, size_t sz)
{
    ::fprintf(f, "[socket] socket_event: %d, socket_id: %d, data_size: %d ", msg->socket_event, msg->socket_id, msg->ud);

    // no data
    if (msg->buffer == nullptr)
    {
        const char* buf_ptr = (const char*)(msg + 1);
        sz -= sizeof(skynet_socket_message);
        const char* eol = (const char*)::memchr(buf_ptr, '\0', sz);
        if (eol != nullptr)
        {
            sz = eol - buf_ptr;
        }
        ::fprintf(f, "[%*s]", (int)sz, (const char*)buf_ptr);
    }
    // has data
    else
    {
        sz = msg->ud;
        _log_blob(f, msg->buffer, sz);
    }
    ::fprintf(f, "\n");
    ::fflush(f);
}    

//
FILE* service_log::open_log_file(service_context* svc_ctx, uint32_t svc_handle)
{
    const char* log_path = node_env::instance()->get_env("log_path");
    if (log_path == nullptr)
        return nullptr;
    
    char tmp[::strlen(log_path) + 16];
    ::sprintf(tmp, "%s/%08x.log", log_path, svc_handle);
    FILE* f = ::fopen(tmp, "ab");
    if (f == nullptr)
    {
        skynet::log(svc_ctx, "Open log file %s fail", tmp);
        return nullptr;
    }

    // log to logger service
    skynet::log(svc_ctx, "Open log file %s", tmp);

    uint32_t start_time = timer_manager::instance()->start_time();
    uint64_t now = timer_manager::instance()->now();
    time_t ti = start_time + now / 100;
    ::fprintf(f, "open time: %u %s", (uint32_t)now, ::ctime(&ti));
    ::fflush(f);
    
    return f;
}

void service_log::close_log_file(service_context* svc_ctx, FILE* f, uint32_t svc_handle)
{
    skynet::log(svc_ctx, "Close log file :%08x", svc_handle);
    
    ::fprintf(f, "close time: %u\n", (uint32_t)timer_manager::instance()->now());
    ::fclose(f);
}

void service_log::log(FILE* f, uint32_t src_svc_handle, int type, int session, void* buffer, size_t sz)
{
    if (type == message_type::PTYPE_SOCKET)
    {
        _log_socket(f, (skynet_socket_message*)buffer, sz);
    }
    else
    {
        uint32_t ti = (uint32_t)timer_manager::instance()->now();
        ::fprintf(f, ":%08x %d %d %u ", src_svc_handle, type, session, ti);
        _log_blob(f, buffer, sz);
        ::fprintf(f, "\n");
        ::fflush(f);
    }
}

}


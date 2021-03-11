#include "skynet_log.h"

#include "../skynet.h"  // api

#include "../server/env.h"
#include "../server/skynet_socket.h"

#include "../log/skynet_error.h"
#include "../timer/timer_manager.h"

#include <string>

namespace skynet {

// 
static void _log_blob(FILE* f, void* buffer, size_t sz)
{
    uint8_t* buf = (uint8_t*)buffer;
    for (int i = 0; i != sz; i++)
    {
        ::fprintf(f, "%02x", buf[i]);
    }
}

static void _log_socket(FILE* f, skynet_socket_message* message, size_t sz)
{
    ::fprintf(f, "[socket] %d %d %d ", message->type, message->id, message->ud);

    if (message->buffer == nullptr)
    {
        const char* buffer = (const char*)(message + 1);
        sz -= sizeof(*message);
        const char* eol = (const char*)::memchr(buffer, '\0', sz);
        if (eol != nullptr)
        {
            sz = eol - buffer;
        }
        ::fprintf(f, "[%*s]", (int)sz, (const char *)buffer);
    }
    else
    {
        sz = message->ud;
        _log_blob(f, message->buffer, sz);
    }
    ::fprintf(f, "\n");
    ::fflush(f);
}    

//
FILE* skynet_log_open(skynet_context* ctx, uint32_t handle)
{
    const char* log_path = env::instance()->getenv("logpath");
    if (log_path == nullptr)
        return nullptr;
    
    size_t sz = ::strlen(log_path);
    char tmp[sz + 16];
    ::sprintf(tmp, "%s/%08x.log", log_path, handle);
    FILE* f = ::fopen(tmp, "ab");
    if (f != nullptr)
    {
        uint32_t starttime = timer_manager::instance()->starttime();
        uint64_t currenttime = timer_manager::instance()->now();
        time_t ti = starttime + currenttime / 100;
        skynet_error(ctx, "Open log file %s", tmp);
        ::fprintf(f, "open time: %u %s", (uint32_t)currenttime, ctime(&ti));
        ::fflush(f);
    }
    else
    {
        skynet_error(ctx, "Open log file %s fail", tmp);
    }
    
    return f;
}

void skynet_log_close(skynet_context* ctx, FILE* f, uint32_t handle)
{
    skynet_error(ctx, "Close log file :%08x", handle);
    
    ::fprintf(f, "close time: %u\n", (uint32_t)timer_manager::instance()->now());
    ::fclose(f);
}

void skynet_log_output(FILE* f, uint32_t source, int type, int session, void* buffer, size_t sz)
{
    if (type == PTYPE_SOCKET)
    {
        _log_socket(f, (skynet_socket_message*)buffer, sz);
    }
    else
    {
        uint32_t ti = (uint32_t)timer_manager::instance()->now();
        ::fprintf(f, ":%08x %d %d %u ", source, type, session, ti);
        _log_blob(f, buffer, sz);
        ::fprintf(f,"\n");
        ::fflush(f);
    }
}

}


/**
 * c service mod: logger
 *
 * 在skynet启动时，会启动一个"logger"服务，默认是logger类型服务，当然也可以配置成snlua类型。
 * 
 * // skynet_start.c
 * service_context* svc_ctx = service_manager::instance()->create_service(config->logservice, config->logger)
 * 
 * // skynet_main.c
 * config.logger = optstring("logger", nullptr);
 * config.logservice = optstring("logservice", "logger");
 * 
 * skynet输出日志通常是调用 skynet::log 这个api(lua层用skynet.log最后也是调用 skynet::log)。
 */

#include "skynet.h"
#include "logger_mod.h"

#include <cstdlib>
#include <cstdint>
#include <string>
#include <ctime>

namespace skynet { namespace service {

#define SIZETIMEFMT    250

static int _time_string(uint32_t time_secs, char tmp[SIZETIMEFMT])
{
    uint64_t now = timer_manager::instance()->now();
    time_t ti = now / 100 + time_secs;

    struct tm info;
    ::localtime_r(&ti, &info);
    ::strftime(tmp, SIZETIMEFMT, "%D %T", &info);
    return now % 100;
}

// logger service message callback
static int logger_cb(service_context* svc_ctx, void* ud, int msg_ptype, int session, uint32_t src_svc_handle, const void* msg, size_t sz)
{
    auto mod_ptr = (logger_mod*)ud;
    switch (msg_ptype)
    {
    case message_protocol_type::PTYPE_SYSTEM:
        if (!mod_ptr->log_filename.empty())
        {
            mod_ptr->log_handle = ::freopen(mod_ptr->log_filename.c_str(), "a", mod_ptr->log_handle);
        }
        break;
    case message_protocol_type::PTYPE_TEXT:
        if (!mod_ptr->log_filename.empty())
        {
            char tmp[SIZETIMEFMT];
            int ticks = _time_string(mod_ptr->start_seconds, tmp);
            ::fprintf(mod_ptr->log_handle, "%s.%02d ", tmp, ticks);
        }
        ::fprintf(mod_ptr->log_handle, "[:%08x] ", src_svc_handle);
        ::fwrite(msg, sz, 1, mod_ptr->log_handle);
        ::fprintf(mod_ptr->log_handle, "\n");
        ::fflush(mod_ptr->log_handle);
        break;
    default:
        break;
    }

    return 0;
}

//--------------------------------------
// logger service interface
//--------------------------------------

logger_mod* logger_create()
{
    return new logger_mod;
}

void logger_release(logger_mod* mod_ptr)
{
    if (mod_ptr->close)
    {
        ::fclose(mod_ptr->log_handle);
    }

    delete mod_ptr;
}

int logger_init(logger_mod* mod_ptr, service_context* svc_ctx, const char* param)
{
    //
    const char* r = service_command::handle_command(svc_ctx, "START_TIME");
    mod_ptr->start_seconds = ::strtoul(r, NULL, 10);

    // log to file
    if (param != nullptr)
    {
        // open log file
        mod_ptr->log_handle = ::fopen(param, "a");
        if (mod_ptr->log_handle == nullptr)
        {
            return 1;
        }

        mod_ptr->log_filename = param;
        mod_ptr->close = 1;
    }
    // log to stdout
    else
    {
        mod_ptr->log_handle = stdout;
    }

    if (mod_ptr->log_handle != 0)
    {
        svc_ctx->set_callback(mod_ptr, logger_cb);
        return 0;
    }

    return 1;
}

} }

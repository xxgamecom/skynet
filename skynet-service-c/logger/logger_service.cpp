#include "logger_service.h"

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

bool logger_service::init(skynet::service_context* svc_ctx, const char* param)
{
    //
    const char* r = service_command::handle_command(svc_ctx, "START_TIME");
    start_seconds_ = ::strtoul(r, NULL, 10);

    // log to file
    if (param != nullptr)
    {
        // open log file
        log_handle_ = ::fopen(param, "a");
        if (log_handle_ == nullptr)
        {
            return false;
        }

        log_filename_ = param;
        is_log_to_file_ = true;
    }
    // log to stdout
    else
    {
        log_handle_ = stdout;
        is_log_to_file_ = false;
    }

    if (log_handle_ != 0)
    {
//        svc_ctx->set_callback(mod_ptr, logger_cb);
        return true;
    }

    return false;
}

void logger_service::fini()
{
    if (is_log_to_file_ && log_handle_ != nullptr)
    {
        ::fclose(log_handle_);
        log_handle_ = nullptr;
        is_log_to_file_ = false;
    }
}

void logger_service::signal(int signal)
{

}
int logger_service::callback(skynet::service_context* svc_ctx, void* ud, int msg_ptype, int session_id, uint32_t src_svc_handle, const void* msg, size_t sz)
{
    switch (msg_ptype)
    {
    case message_protocol_type::PTYPE_SYSTEM:
        if (!log_filename_.empty())
        {
            log_handle_ = ::freopen(log_filename_.c_str(), "a", log_handle_);
        }
        break;
    case message_protocol_type::PTYPE_TEXT:
        if (!log_filename_.empty())
        {
            char tmp[SIZETIMEFMT];
            int ticks = _time_string(start_seconds_, tmp);
            ::fprintf(log_handle_, "%s.%02d ", tmp, ticks);
        }
        ::fprintf(log_handle_, "[:%08x] ", src_svc_handle);
        ::fwrite(msg, sz, 1, log_handle_);
        ::fprintf(log_handle_, "\n");
        ::fflush(log_handle_);
        break;
    }

    return 0;
}

} }

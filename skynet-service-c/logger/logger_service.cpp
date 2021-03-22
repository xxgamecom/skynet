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

bool logger_service::init(service_context* svc_ctx, const char* param)
{
    //
    const char* result = service_command::exec(svc_ctx, "START_TIME");
    try
    {
        start_seconds_ = std::stoul(result);
    }
    catch (...)
    {
        return false;
    }

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
        svc_ctx->set_callback(logger_cb, this);
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

int logger_service::logger_cb(service_context* svc_ctx, void* ud, int msg_ptype, int session_id, uint32_t src_svc_handle, const void* msg, size_t sz)
{
    auto svc_ptr = (logger_service*)ud;

    switch (msg_ptype)
    {
    case message_protocol_type::MSG_PTYPE_SYSTEM:
        if (!svc_ptr->log_filename_.empty())
        {
            svc_ptr->log_handle_ = ::freopen(svc_ptr->log_filename_.c_str(), "a", svc_ptr->log_handle_);
        }
        break;
    case message_protocol_type::MSG_PTYPE_TEXT:
        if (!svc_ptr->log_filename_.empty())
        {
            char tmp[SIZETIMEFMT];
            int ticks = _time_string(svc_ptr->start_seconds_, tmp);
            ::fprintf(svc_ptr->log_handle_, "%s.%02d ", tmp, ticks);
        }
        ::fprintf(svc_ptr->log_handle_, "[:%08x] ", src_svc_handle);
        ::fwrite(msg, sz, 1, svc_ptr->log_handle_);
        ::fprintf(svc_ptr->log_handle_, "\n");
        ::fflush(svc_ptr->log_handle_);
        break;
    }

    return 0;
}

} }

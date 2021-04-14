#include "log.h"

#include "../mq/mq_msg.h"
#include "../mq/mq_private.h"

#include "../service/service_context.h"
#include "../service/service_manager.h"

#include <cstring>

namespace skynet {

// log message max size, DISCARD THE PART BEYOND THE LENGTH (JUST PROTECT).
#define MAX_LOG_MSG_SIZE 64*1024

// log level
enum log_level
{
    LOG_LEVEL_TRACE = 0,
    LOG_LEVEL_DEBUG = 1,
    LOG_LEVEL_INFO = 2,
    LOG_LEVEL_WARN = 3,
    LOG_LEVEL_ERROR = 4,
};

// todo: process max_log_msg_size
void _log(service_context* svc_ctx, int log_level, const char* msg)
{
    // find logger service 'logger'
    static uint32_t log_svc_handle = 0;
    if (log_svc_handle == 0)
        log_svc_handle = service_manager::instance()->find_by_name("logger");
    if (log_svc_handle == 0)
        return;

    int msg_len = ::strlen(msg);
    if (msg_len <= 0)
        return;

    int log_level_len = sizeof(log_level);
    int actual_len = log_level_len + msg_len;

    // reach max limit
    if (actual_len > MAX_LOG_MSG_SIZE)
        actual_len = MAX_LOG_MSG_SIZE;

    char* data_ptr = new char[actual_len + 1] { 0 };
    // copy log level
    ::memcpy(data_ptr, &log_level, sizeof(log_level));
    // copy log message
    ::memcpy(data_ptr + log_level_len, msg, msg_len + 1);

    // push message to log service
    service_message smsg;
    smsg.src_svc_handle = svc_ctx != nullptr ? svc_ctx->svc_handle_ : 0;
    smsg.session_id = 0;
    smsg.data_ptr = data_ptr;
    smsg.data_size = actual_len | ((size_t)SERVICE_MSG_TYPE_TEXT << MESSAGE_TYPE_SHIFT);
    service_manager::instance()->push_service_message(log_svc_handle, &smsg);
}

void log_trace(service_context* svc_ctx, const char* msg)
{
    _log(svc_ctx, LOG_LEVEL_TRACE, msg);
}

void log_debug(service_context* svc_ctx, const char* msg)
{
    _log(svc_ctx, LOG_LEVEL_DEBUG, msg);
}

void log_info(service_context* svc_ctx, const char* msg)
{
    _log(svc_ctx, LOG_LEVEL_INFO, msg);
}

void log_warn(service_context* svc_ctx, const char* msg)
{
    _log(svc_ctx, LOG_LEVEL_WARN, msg);
}

void log_error(service_context* svc_ctx, const char* msg)
{
    _log(svc_ctx, LOG_LEVEL_ERROR, msg);
}

void log_trace(service_context* svc_ctx, const std::string& msg)
{
    _log(svc_ctx, LOG_LEVEL_TRACE, msg.c_str());
}

void log_debug(service_context* svc_ctx, const std::string& msg)
{
    _log(svc_ctx, LOG_LEVEL_DEBUG, msg.c_str());
}

void log_info(service_context* svc_ctx, const std::string& msg)
{
    _log(svc_ctx, LOG_LEVEL_INFO, msg.c_str());
}

void log_warn(service_context* svc_ctx, const std::string& msg)
{
    _log(svc_ctx, LOG_LEVEL_WARN, msg.c_str());
}

void log_error(service_context* svc_ctx, const std::string& msg)
{
    _log(svc_ctx, LOG_LEVEL_ERROR, msg.c_str());
}

}


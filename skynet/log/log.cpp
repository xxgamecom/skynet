#include "log.h"

#include "../skynet.h"      // api

#include "../mq/mq.h"
#include "../mq/mq_msg.h"
#include "../context/handle_manager.h"
#include "../context/service_context.h"

#include <cstdarg>

namespace skynet {

// 日志缓存长度
#define LOG_BUF_SIZE            256


/**
 * skynet log api implement
 * - find logger service and push log message to the logger service
 */
void log(service_context* svc_ctx, const char* msg, ...)
{
    static uint32_t log_svc_handle = 0;

    // find logger service 'logger'
    if (log_svc_handle == 0)
        log_svc_handle = handle_manager::instance()->find_by_name("logger");
    if (log_svc_handle == 0)
        return;

    // 
    char tmp[LOG_BUF_SIZE] = { 0 };

    va_list ap;
    va_start(ap, msg);
    int len = ::vsnprintf(tmp, LOG_BUF_SIZE, msg, ap);
    va_end(ap);

    // error
    if (len < 0)
    {
        ::perror("vsnprintf error :");
        return;
    }

    char* data_ptr = nullptr;

    // log message length < 256
    if (len >= 0 && len < LOG_BUF_SIZE)
    {
        data_ptr = new char[len + 1] { 0 };
        ::memcpy(data_ptr, tmp, len + 1);
    }
    // log message length >= 256
    else
    {
        int max_size = LOG_BUF_SIZE;
        for (;;)
        {
            // alloc double size
            max_size *= 2;
            data_ptr = new char[max_size];

            // 
            va_start(ap, msg);
            len = ::vsnprintf(data_ptr, max_size, msg, ap);
            va_end(ap);

            // alloc log message buffer success
            if (len < max_size)
                break;
            
            // not enought, try alloc again
            delete[] data_ptr;
        }
    }

    if (len < 0)
    {
        delete[] data_ptr;
        ::perror("vsnprintf error :");
        return;
    }    

    // push message to log service
    skynet_message smsg;
    smsg.src_svc_handle = svc_ctx != nullptr ? svc_ctx->svc_handle_ : 0;
    smsg.session = 0;
    smsg.data = data_ptr;
    smsg.sz = len | ((size_t)PTYPE_TEXT << MESSAGE_TYPE_SHIFT);
    skynet_context_push(log_svc_handle, &smsg);
}

}


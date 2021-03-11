/**
 * skynet内建的skynet_error C API实现
 * 
 * 决定skynet_error将信息输出到什么日志文件中。
 * 如果配置文件中将 logger = nil，将输出到标准输出，你可以定义一个路径和文件，这样就会输出到文件中。 
 */

#include "skynet_error.h"

#include "../skynet.h"      // api

#include "../mq/mq.h"
#include "../server/server.h"

#include "../context/handle_manager.h"
#include "../context/service_context.h"

#include <cstdarg>

namespace skynet {

#define LOG_MESSAGE_SIZE 256

// 错误输出
// skynet输出日志通常是调用skynet_error这个api(lua层用skynet.error最后也是调用skynet_error)。
// 查找名称为“logger”对应的ctx的handle id，然后向该id发送消息包skynet_context_push，消息包的类型为PTYPE_TEXT，没有设置PTYPE_ALLOCSESSION标记表示不需要接收方返回。
void skynet_error(skynet_context* ctx, const char* msg, ...)
{
    static uint32_t log_svc_handle = 0;

    // check log c service handle
    if (log_svc_handle == 0)
        log_svc_handle = handle_manager::instance()->find_by_name("logger");
    
    // no log c service, just skip the msg
    if (log_svc_handle == 0)
        return;

    char tmp[LOG_MESSAGE_SIZE];
    char* data = nullptr;

    va_list ap;
    va_start(ap, msg);
    int len = ::vsnprintf(tmp, LOG_MESSAGE_SIZE, msg, ap);
    va_end(ap);

    if (len >=0 && len < LOG_MESSAGE_SIZE)
    {
    //     data = skynet_strdup(tmp);
    }
    else
    {
        int max_size = LOG_MESSAGE_SIZE;
        for (;;)
        {
            max_size *= 2;
            data = new char[max_size];
            va_start(ap, msg);
            len = ::vsnprintf(data, max_size, msg, ap);
            va_end(ap);
            if (len < max_size)
            {
                break;
            }
            delete[] data;
        }
    }
    if (len < 0)
    {
        delete[] data;
        ::perror("vsnprintf error :");
        return;
    }

    skynet_message smsg;
    if (ctx == nullptr)
    {
        smsg.source = 0;
    }
    else
    {
        smsg.source = ctx->handle_;
    }
    smsg.session = 0;
    smsg.data = data;
    smsg.sz = len | ((size_t)PTYPE_TEXT << MESSAGE_TYPE_SHIFT);
    skynet_context_push(log_svc_handle, &smsg);
}

}


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

#include <cstdlib>
#include <cstdint>
#include <string>
#include <ctime>

namespace skynet { namespace service {

// logger service mod data
struct logger
{
    FILE*                       file_handle = nullptr;          // log file handle
    char*                       filename = nullptr;             // log file name
    uint32_t                    start_seconds = 0;              // the number of seconds since skynet node started.
    int                         close = 0;                      // 是否输出到文件标识, 0代表文件句柄是标准输出, 1代表输出到文件
};


#define SIZETIMEFMT    250

static int timestring(logger* inst, char tmp[SIZETIMEFMT])
{
    uint64_t now = timer_manager::instance()->now();

    time_t ti = now / 100 + inst->start_seconds;
    struct tm info;
    ::localtime_r(&ti, &info);
    ::strftime(tmp, SIZETIMEFMT, "%D %T", &info);
    return now % 100;
}

// 当工作线程分发这条消息包时，最终会调用logger服务的消息回调函数logger_cb
// 不同服务类型的消息回调函数接口参数是一样的。
// skynet输出日志通常是调用skynet::log这个api(lua层用skynet.log最后也是调用skynet::log)。
// 由于skynet::log发送的消息包的type是PTYPE_TEXT，会把消息包源地址以及消息包数据一起写到文件句柄里。
// @param context ctx
// @param ud ctx userdata
// @param type 消息类型
// @param session
// @param source
// @param msg
// @param sz
static int logger_cb(service_context* context, void* ud, int type, int session, uint32_t source, const void* msg, size_t sz)
{
    logger* inst = (logger*)ud;
    switch (type)
    {
    case message_protocol_type::PTYPE_SYSTEM:
        if (inst->filename != nullptr)
        {
            inst->file_handle = ::freopen(inst->filename, "a", inst->file_handle);
        }
        break;
    case message_protocol_type::PTYPE_TEXT:
        if (inst->filename != nullptr)
        {
            char tmp[SIZETIMEFMT];
            int csec = timestring((logger*)ud, tmp);
            ::fprintf(inst->file_handle, "%s.%02d ", tmp, csec);
        }
        ::fprintf(inst->file_handle, "[:%08x] ", source);
        ::fwrite(msg, sz, 1, inst->file_handle);
        ::fprintf(inst->file_handle, "\n");
        ::fflush(inst->file_handle);
        break;
    }

    return 0;
}

logger* logger_create()
{
    return new logger;
}

void logger_release(logger* inst)
{
    if (inst->close)
    {
        ::fclose(inst->file_handle);
    }

    delete[] inst->filename;
    delete inst;
}

int logger_init(logger* inst, service_context* svc_ctx, const char* param)
{
    //
    const char* r = service_command::handle_command(svc_ctx, "START_TIME", nullptr);
    inst->start_seconds = ::strtoul(r, NULL, 10);

    // log file name
    if (param != nullptr)
    {
        inst->file_handle = ::fopen(param, "a");
        if (inst->file_handle == nullptr)
        {
            return 1;
        }
        inst->filename = new char[::strlen(param) + 1];
        ::strcpy(inst->filename, param);
        inst->close = 1;
    }
    //
    else
    {
        inst->file_handle = stdout;
    }

    if (inst->file_handle != 0)
    {
        svc_ctx->set_callback(inst, logger_cb);
        return 0;
    }

    return 1;
}

} }

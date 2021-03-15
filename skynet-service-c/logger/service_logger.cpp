/**
 * C服务模块: logger
 * 
 * logger服务功能简单，在skynet启动时，会启动一个"logger"服务，默认是logger类型服务，当然也可以配置成snlua类型。
 * 
 * //skynet_start.c
 * struct service_context *ctx = service_manager::instance()->create_service(config->logservice, config->logger)
 * 
 * //skynet_main.c
 * config.logger = optstring("logger", nullptr);
 * config.logservice = optstring("logservice", "logger");
 * 
 * skynet输出日志通常是调用skynet_error这个api(lua层用skynet.error最后也是调用skynet_error)。
 */

// #include "skynet.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

// logger 服务数据块
struct logger
{
    FILE*                       handle;                     // 文件句柄
    char*                       filename;                   //
    int                         close;                      // 是否输出到文件标识, 0代表文件句柄是标准输出, 1代表输出到文件
};

logger* logger_create()
{
    logger* inst = new logger;
    inst->handle = nullptr;
    inst->close = 0;
    inst->filename = nullptr;

    return inst;
}

void logger_release(logger* inst)
{
    if (inst->close)
    {
        ::fclose(inst->handle);
    }
    // skynet_free(inst->filename);
    // skynet_free(inst);
}

// // 当工作线程分发这条消息包时，最终会调用logger服务的消息回调函数logger_cb
// // 不同服务类型的消息回调函数接口参数是一样的。
// // skynet输出日志通常是调用skynet_error这个api(lua层用skynet.error最后也是调用skynet_error)。
// // 由于skynet_error发送的消息包的type是PTYPE_TEXT，会把消息包源地址以及消息包数据一起写到文件句柄里。
// // @param context ctx
// // @param ud ctx userdata
// // @param type 消息类型
// // @param session
// // @param source
// // @param msg
// // @param sz
// static int logger_cb(service_context* context, void* ud, int type, int session, uint32_t source, const void* msg, size_t sz)
// {
//     // struct logger * inst = ud;
//     // switch (type)
//     // {
//     // case message_type::PTYPE_SYSTEM:
//     //     if (inst->filename)
//     //     {
//     //         inst->handle = freopen(inst->filename, "a", inst->handle);
//     //     }
//     //     break;
//     // case message_type::PTYPE_TEXT:
//     //     fprintf(inst->handle, "[:%08x] ",source);
//     //     fwrite(msg, sz, 1, inst->handle);
//     //     fprintf(inst->handle, "\n");
//     //     fflush(inst->handle);
//     //     break;
//     // }

//     return 0;
// }

// int logger_init(struct logger* inst, service_context* ctx, const char* parm)
// {
//     // if (parm)
//     // {
//     //     inst->handle = fopen(parm,"w");
//     //     if (inst->handle == nullptr)
//     //     {
//     //         return 1;
//     //     }
//     //     inst->filename = skynet_malloc(strlen(parm)+1);
//     //     strcpy(inst->filename, parm);
//     //     inst->close = 1;
//     // }
//     // else
//     // {
//     //     inst->handle = stdout;
//     // }
//     // if (inst->handle)
//     // {
//     //     skynet_callback(ctx, inst, logger_cb);
//     //     return 0;
//     // }
    
//     return 1;
// }

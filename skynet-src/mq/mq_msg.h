#pragma once

#include <cstdint>
#include <cstdlib>

namespace skynet {

//----------------------------------------------
// service_message
//----------------------------------------------

/**
 * skynet service message type, used for interaction between skynet services
 */
enum service_message_type
{
    SERVICE_MSG_TYPE_TEXT           = 0,                        // internal use, text message, 内部服务最为常用的文本消息类型
    SERVICE_MSG_TYPE_RESPONSE       = 1,                        // 响应消息
    SERVICE_MSG_TYPE_MULTICAST      = 2,                        // 组播消息
    SERVICE_MSG_TYPE_CLIENT         = 3,                        // 用来处理客户端的请求消息, 是 gate 给外部连接定义出来的消息类型
    SERVICE_MSG_TYPE_SYSTEM         = 4,                        // 系统消息
    SERVICE_MSG_TYPE_HARBOR         = 5,                        // 跨节点消息
    SERVICE_MSG_TYPE_SOCKET         = 6,                        // 套接字消息, 不一定是客户端的消息, 也可以能是服务之间的消息
    SERVICE_MSG_TYPE_ERROR          = 7,                        // 错误消息, 一般服务退出的时候会发送error消息给关联的服务
    SERVICE_MSG_TYPE_RESERVED_QUEUE = 8,                        //
    SERVICE_MSG_TYPE_RESERVED_DEBUG = 9,                        //
    SERVICE_MSG_TYPE_RESERVED_LUA   = 10,                       // lua类型消息, 最常用
    SERVICE_MSG_TYPE_RESERVED_SNAX  = 11,                       // snax服务消息
};

// message tag
#define MESSAGE_TAG_DONT_COPY       0x10000                     // don't copy message
#define MESSAGE_TAG_ALLOC_SESSION   0x20000                     // set in msg_ptype when sending a package
                                                                // send api method will ignore session arguemnts and allocate a new session id.

// skynet service message, used for interaction between services
struct service_message
{
    uint32_t                        src_svc_handle = 0;         // source service handle
    int                             session_id = 0;             // message session id
    void*                           data = nullptr;             // message data
    size_t                          sz = 0;                     // message data size, high 8 bits: message type
};

//
#define MESSAGE_TYPE_MASK           (SIZE_MAX >> 8)             // service_message.sz high 8 bits: message type
#define MESSAGE_TYPE_SHIFT          ((sizeof(size_t) - 1) * 8)  // type is encoding in service_message.sz high 8bit

}


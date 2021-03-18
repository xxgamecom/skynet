#pragma once

#include <cstdint>
#include <cstdlib>

namespace skynet {

//----------------------------------------------
// skynet_message
//----------------------------------------------

// skynet message
struct skynet_message
{
    uint32_t                        src_svc_handle = 0;         // source service handle
    int                             session = 0;                // message session id
    void*                           data = nullptr;             // message data
    size_t                          sz = 0;                     // message data size, high 8 bits: message type
};

// type
// 服务间的交互，只有很少的服务只需要处理别人发送过来的请求，而不需要向外提出请求。所以我们至少需要区分请求包和回应包。
// 这两种包显然是有不同的处理方式，但它们需要从同一个 callback 函数入口进入。这就需要用一个额外的参数区分。

// 消息类型定义
// 
enum message_protocol_type
{
    PTYPE_TEXT                      = 0,                        // internal use, text message, 内部服务最为常用的文本消息类型
    PTYPE_RESPONSE                  = 1,                        // 响应消息
    PTYPE_MULTICAST                 = 2,                        // 组播消息
    PTYPE_CLIENT                    = 3,                        // 用来处理客户端的请求消息, 是 gate 给外部连接定义出来的消息类型
    PTYPE_SYSTEM                    = 4,                        // 系统消息
    PTYPE_HARBOR                    = 5,                        // 跨节点消息
    PTYPE_SOCKET                    = 6,                        // 套接字消息, 不一定是客户端的消息, 也可以能是服务之间的消息
    PTYPE_ERROR                     = 7,                        // 错误消息, 一般服务退出的时候会发送error消息给关联的服务
    PTYPE_RESERVED_QUEUE            = 8,                        //
    PTYPE_RESERVED_DEBUG            = 9,                        //
    PTYPE_RESERVED_LUA              = 10,                       // lua类型消息, 最常用
    PTYPE_RESERVED_SNAX             = 11,                       // snax服务消息
};

// message tag
#define MESSAGE_TAG_DONT_COPY       0x10000                     // 让框架不复制 msg/sz 指代的数据包
#define MESSAGE_TAG_ALLOC_SESSION   0x20000                     // 使用 send 发送一个包的时候，你可以在 type 里设上, send api 就会忽略掉传入的 session 参数，而会分配出一个当前服务从来没有使用过的 session 号，发送出去

// 
#define MESSAGE_TYPE_MASK           (SIZE_MAX >> 8)             // skynet_message.sz high 8 bits: message type
#define MESSAGE_TYPE_SHIFT          ((sizeof(size_t) - 1) * 8)  // type is encoding in skynet_message.sz high 8bit

}

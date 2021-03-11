/**
 * skynet api
 */

#pragma once

#include <cstddef>
#include <cstdint>

namespace skynet {


// type
// 服务间的交互，只有很少的服务只需要处理别人发送过来的请求，而不需要向外提出请求。所以我们至少需要区分请求包和回应包。
// 这两种包显然是有不同的处理方式，但它们需要从同一个 callback 函数入口进入。这就需要用一个额外的参数区分。

// 消息类型定义
#define PTYPE_TEXT              0                       // 内部服务最为常用的文本消息类型
#define PTYPE_RESPONSE          1                       // 响应消息
#define PTYPE_MULTICAST         2                       // 组播消息
#define PTYPE_CLIENT            3                       // 用来处理客户端的请求消息, 是 gate 给外部连接定义出来的消息类型
#define PTYPE_SYSTEM            4                       // 系统消息
#define PTYPE_HARBOR            5                       // 跨节点消息
#define PTYPE_SOCKET            6                       // 套接字消息, 不一定是客户端的消息, 也可以能是服务之间的消息
// read lualib/skynet.lua examples/simplemonitor.lua
#define PTYPE_ERROR             7                       // 错误消息, 一般服务退出的时候会发送error消息给关联的服务
// read lualib/skynet.lua lualib/mqueue.lua lualib/snax.lua
#define PTYPE_RESERVED_QUEUE    8                       //
#define PTYPE_RESERVED_DEBUG    9                       //
#define PTYPE_RESERVED_LUA      10                      // lua类型消息, 最常用
#define PTYPE_RESERVED_SNAX     11                      // snax服务消息

#define PTYPE_TAG_DONTCOPY      0x10000                 // 让框架不复制 msg/sz 指代的数据包 (否则 skynet 会用 malloc 分配一块内存, 把数据复制进去)
#define PTYPE_TAG_ALLOCSESSION  0x20000                 // 使用 skynet_send 发送一个包的时候，你可以在 type 里设上, send api 就会忽略掉传入的 session 参数，而会分配出一个当前服务从来没有使用过的 session 号，发送出去

// forward delcare, skynet service context
struct skynet_context;

//
uint32_t skynet_queryname(skynet_context* context, const char* name);

//
// @param source 地址, 原则上不需要填写source地址, 因为默认就是它自己. 0是系统默认保留的handle, 可以指代自己.
// @param destination 地址
// @param type
// @param session skynet核心只解决单向消息包的发送问题, 每个服务仅有一个callback函数, 所以需要一个标识来区分消息包, 这就是session的作用
//                可以在 type 里设上 alloc session 的 tag (PTYPE_TAG_ALLOCSESSION), send api 就会忽略掉传入的 session 参数，而会分配出一个当前服务从来没有使用过的 session 号，发送出去。
//                同时约定，接收方在处理完这个消息后，把这个 session 原样发送回来。这样，编写服务的人只需要在 callback 函数里记录下所有待返回的 session 表，就可以在收到每个消息后，正确的调用对应的处理函数。
int skynet_send(skynet_context* context, uint32_t source, uint32_t destination, int type, int session, void* msg, size_t sz);
//
int skynet_sendname(skynet_context* context, uint32_t source, const char* destination, int type, int session, void* msg, size_t sz);

// 
void skynet_debug_memory(const char *info);	// for debug use, output current service memory to stderr


}


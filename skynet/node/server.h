#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <memory>

namespace skynet {

struct skynet_message;
class service_context;
class skynet_monitor;
class message_queue;


// 投递服务消息
int skynet_context_push(uint32_t svc_handle, skynet_message* message);
// 发送消息
void skynet_context_send(service_context* svc_ctx, void* msg, size_t sz, uint32_t src_svc_handle, int type, int session);

// 派发消息, 工作线程的核心逻辑 // return next queue
message_queue* skynet_context_message_dispatch(skynet_monitor& sm, message_queue*, int weight);

// for log output before exit
void skynet_context_dispatchall(service_context* svc_ctx);

// for monitor
void skynet_context_endless(uint32_t svc_handle);


// query by service name
uint32_t skynet_query_by_name(service_context* svc_ctx, const char* name);

//
// @param src_svc_handle 0: reserve service handle, self
// @param dst_svc_handle
// @param type
// @param session 每个服务仅有一个callback函数, 所以需要一个标识来区分消息包, 这就是session的作用
//                可以在 type 里设上 alloc session 的 tag (message_type::TAG_ALLOC_SESSION), send api 就会忽略掉传入的 session 参数，而会分配出一个当前服务从来没有使用过的 session 号，发送出去。
//                同时约定，接收方在处理完这个消息后，把这个 session 原样发送回来。这样，编写服务的人只需要在 callback 函数里记录下所有待返回的 session 表，就可以在收到每个消息后，正确的调用对应的处理函数。
int skynet_send(service_context* svc_ctx, uint32_t src_svc_handle, uint32_t dst_svc_handle, int type, int session, void* msg, size_t sz);

/**
 * send by service name or service address (format: ":%08x")
 *
 * @param svc_ctx
 * @param src_svc_handle 0: reserve service handle, self
 * @param dst_name_or_addr service name or service address (format: ":%08x")
 * @param type
 * @param session
 * @param msg
 * @param sz
 */
int skynet_send_by_name(service_context* svc_ctx, uint32_t src_svc_handle, const char* dst_name_or_addr, int type, int session, void* msg, size_t sz);


}


#pragma once

#include <stdint.h>
#include <stdlib.h>

namespace skynet {

struct skynet_context;
struct skynet_message;
class skynet_monitor;
class message_queue;


// 投递服务消息
int skynet_context_push(uint32_t handle, skynet_message *message);
// 发送消息
void skynet_context_send(skynet_context* ctx, void* msg, size_t sz, uint32_t source, int type, int session);

// 派发消息, 工作线程的核心逻辑 // return next queue
message_queue* skynet_context_message_dispatch(skynet_monitor*, message_queue*, int weight);

// for skynet_error output before exit
void skynet_context_dispatchall(skynet_context* ctx);

// for monitor
void skynet_context_endless(uint32_t handle);




}


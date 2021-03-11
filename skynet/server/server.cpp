/**
 * skynet是以服务为主体进行运作的，服务称作为skynet_context(简称ctx)，是一个c结构，是skynet里最重要的结构，整个skynet的运作都是围绕ctx进行的。
 * 
 * skynet_server提供的api主要分两大类：
 * 1. 对ctx的一系列操作，比如创建，删除ctx等
 * 2. 如何发送消息和处理自身的消息
 * 
 * 为了统一对ctx操作的接口，采用指令的格式，定义了一系列指令(cmd_xxx):
 * 1. cmd_launch创建一个新服务，
 * 2. cmd_exit服务自身退出，
 * 3. cmd_kill杀掉一个服务等，
 * 上层统一调用skynet_command接口即可执行这些操作。
 * 
 * 对ctx操作，通常会先调用skynet_context_grab将引用计数+1，操作完调用skynet_context_release将引用计数-1，以保证操作ctx过程中，不会被其他线程释放掉。
 */

#include "server.h"
#include "skynet_instruction.h"
#include "env.h"
#include "skynet_monitor.h"

#include "../log/skynet_log.h"
#include "../log/skynet_error.h"

#include "../mq/mq.h"
#include "../mod/module_manager.h"

#include "../context/handle_manager.h"
#include "../context/service_context.h"

#include "../utils/time_helper.h"

namespace skynet {

// #ifdef CALLING_CHECK

// #define CHECKCALLING_DECL struct spinlock calling;
// #define CHECKCALLING_INIT(ctx) spinlock_init(&ctx->calling);
// #define CHECKCALLING_DESTROY(ctx) spinlock_destroy(&ctx->calling);
// #define CHECKCALLING_BEGIN(ctx) if (!(spinlock_trylock(&ctx->calling))) { assert(0); }
// #define CHECKCALLING_END(ctx) spinlock_unlock(&ctx->calling);

// #else

// #define CHECKCALLING_DECL
// #define CHECKCALLING_INIT(ctx)
// #define CHECKCALLING_DESTROY(ctx)
// #define CHECKCALLING_BEGIN(ctx)
// #define CHECKCALLING_END(ctx)

// #endif

//
struct drop_t
{
    uint32_t handle;
};

static void drop_message(skynet_message* msg, void* ud)
{
    drop_t* d = (drop_t*)ud;
//     skynet_free(msg->data);
    uint32_t source = d->handle;
    assert(source != 0);
    // report error to the message source
    // skynet_send(NULL, source, msg->source, PTYPE_ERROR, 0, NULL, 0);
}

// 投递服务消息
int skynet_context_push(uint32_t handle, skynet_message* message)
{
    // 增加服务引用计数
    skynet_context* ctx = handle_manager::instance()->grab(handle);
    if (ctx == nullptr)
        return -1;

    // 消息入队
    ctx->queue_->push(message);
    // 减少服务引用计数
    // skynet_context_release(ctx);

    return 0;
}

// 设置服务阻塞标记
void skynet_context_endless(uint32_t handle)
{
    skynet_context* ctx = handle_manager::instance()->grab(handle);
    if (ctx == nullptr)
        return;

    // 阻塞标记
    ctx->endless_ = true;
    // skynet_context_release(ctx);
}

// 处理一条ctx消息，即调用ctx的消息回调函数
static void dispatch_message(skynet_context* ctx, skynet_message* msg)
{
    // assert(ctx->init);
//     CHECKCALLING_BEGIN(ctx)

    int type = msg->sz >> MESSAGE_TYPE_SHIFT;
    size_t sz = msg->sz & MESSAGE_TYPE_MASK;
    if (ctx->log_fd_ != nullptr)
    {
        skynet_log_output(ctx->log_fd_, msg->source, type, msg->session, msg->data, sz);
    }
    ++ctx->message_count_;
    int reserve_msg;
    if (ctx->profile_)
    {
        ctx->cpu_start_ = time_helper::thread_time();
        reserve_msg = ctx->cb_(ctx, ctx->cb_ud_, type, msg->session, msg->source, msg->data, sz);        // 消息回调函数
        uint64_t cost_time = time_helper::thread_time() - ctx->cpu_start_;
        ctx->cpu_cost_ += cost_time;
    }
    else
    {
        reserve_msg = ctx->cb_(ctx, ctx->cb_ud_, type, msg->session, msg->source, msg->data, sz);        // 消息回调函数
    }
    if (reserve_msg == 0)
    {
//         skynet_free(msg->data);
    }

//     CHECKCALLING_END(ctx)
}

// 派发所有消息
void skynet_context_dispatchall(struct skynet_context * ctx)
{
    // for skynet_error
    skynet_message msg;
    message_queue* q = ctx->queue_;
    while (!q->pop(&msg))
    {
        dispatch_message(ctx, &msg);
    }
}

// 派发服务消息, 工作线程的核心逻辑
// 派发完成后，它会进入睡眠状态，等待另外两个线程来唤醒
// skynet可以启动多个工作线程，调用skynet_context_message_dispatch这个api不断的分发消息
// @param sm skynet_monitor监测消息是否堵住
// @param q 需要分发的次级消息队列
// @param weight 权重, -1只处理一条消息，0处理完q中所有消息，>0处理长度右移weight位(1/(2*weight))条消息
//               在启动工作线程时，会为每个线程设置一个weight，即线程0-3是-1，4-7是0，8-15是1，16-23是2，24-31是3。
//               -1表示每帧只处理mq中一个消息包，
//                0表示每帧处理mq中所有消息包，
//                1表示每帧处理mq长度的1/2条(>>1右移一位)消息，
//                2表示每帧处理mq长度的1/4(右移2位)，
//                3表示每帧处理mq长度的1/8(右移3位)。
// @return 返回下一个要分发的mq, 供下一帧调用
message_queue* skynet_context_message_dispatch(skynet_monitor& sm, message_queue* q, int weight)
{
    // peek next q from glboal_mq
    if (q == nullptr)
    {
        q = global_mq::instance()->pop();
        if (q == nullptr)
            return nullptr;
    }

    // 当前 message_queue 所属服务的 context 的handle id
    uint32_t svc_handle = q->svc_handle_;

    skynet_context* ctx = handle_manager::instance()->grab(svc_handle);
    // service ctx not exists
    if (ctx == nullptr)
    {
        struct drop_t d = { svc_handle };
        q->release(drop_message, &d);
        return global_mq::instance()->pop();
    }

    int n = 1;
    skynet_message msg;

    for (int i = 0; i < n; i++)
    {
        // 从message_queue 中 pop一个msg出来
        if (q->pop(&msg))
        {
            // 如果ctx的次级消息队列为空，返回
            // skynet_context_release(ctx);
            return global_mq::instance()->pop();
        }
        else if (i == 0 && weight >= 0)
        {
            // weight: -1只处理一条消息，0处理完q中所有消息，>0处理长度右移weight位(1/(2*weight))条消息
            n = q->length();
            n >>= weight;
        }
        int overload = q->overload();
        if (overload)
        {
            skynet_error(ctx, "May overload, message queue length = %d", overload);
        }

        sm.trigger(msg.source , svc_handle);

        if (ctx->cb_ == nullptr)
        {
//             skynet_free(msg.data);
        }
        else
        {
            dispatch_message(ctx, &msg); // 处理消息
        }

        sm.trigger(0, 0);
    }

    assert(q == ctx->queue_);
    message_queue* nq = global_mq::instance()->pop();
    if (nq != nullptr)
    {
        // If global mq is not empty , push q back, and return next queue (nq)
        // Else (global mq is empty or block, don't push q back, and return q again (for next dispatch)
        global_mq::instance()->push(q);
        q = nq;
    }

//     skynet_context_release(ctx);

    return q;
}

// static void copy_name(char name[GLOBALNAME_LENGTH], const char * addr)
// {
//     int i;
//     for (i=0;i<GLOBALNAME_LENGTH && addr[i];i++)
//     {
//         name[i] = addr[i];
//     }
//     for (;i<GLOBALNAME_LENGTH;i++)
//     {
//         name[i] = '\0';
//     }
// }

// 
uint32_t skynet_queryname(skynet_context* ctx, const char* name)
{
    switch(name[0])
    {
    case ':':
        return strtoul(name+1, NULL, 16);
    case '.':
        return handle_manager::instance()->find_by_name(name + 1);
    }

    // skynet_error(ctx, "Don't support query global name %s",name);
    return 0;
}

static void _filter_args(skynet_context* ctx, int type, int* session, void** data, size_t* sz)
{
//     int needcopy = !(type & PTYPE_TAG_DONTCOPY);
//     int allocsession = type & PTYPE_TAG_ALLOCSESSION;
//     type &= 0xff;

//     if (allocsession)
//     {
//         assert(*session == 0);
//         *session = skynet_context_newsession(ctx);
//     }

//     if (needcopy && *data)
//     {
//         char * msg = skynet_malloc(*sz+1);
//         memcpy(msg, *data, *sz);
//         msg[*sz] = '\0';
//         *data = msg;
//     }

    *sz |= (size_t)type << MESSAGE_TYPE_SHIFT;
}

// 发送消息
// ctx之间通过消息进行通信，调用skynet_send向对方发送消息(skynet_sendname最终也会调用skynet_send)。
// @param ctx            源服务的ctx，可以为NULL，drop_message时这个参数为NULL
// @param source         源服务地址，通常设置为0即可，api里会设置成ctx->handle，当context为NULL时，需指定source
// @param destination     目的服务地址
// @param type             消息类型， skynet定义了多种消息，PTYPE_TEXT，PTYPE_CLIENT，PTYPE_RESPONSE等（详情见skynet.h）
// @param session         如果在type里设上allocsession的tag(PTYPE_TAG_ALLOCSESSION)，api会忽略掉传入的session参数，重新生成一个新的唯一的
// @param data             消息包数据
// @param sz             消息包长度
// @return int session, 源服务保存这个session，同时约定，目的服务处理完这个消息后，把这个session原样发送回来(skynet_message结构里带有一个session字段)，
//         源服务就知道是哪个请求的返回，从而正确调用对应的回调函数。
int skynet_send(skynet_context* ctx, uint32_t source, uint32_t destination , int type, int session, void* data, size_t sz)
{
//     if ((sz & MESSAGE_TYPE_MASK) != sz)
//     {
//         skynet_error(ctx, "The message to %x is too large", destination);
//         if (type & PTYPE_TAG_DONTCOPY)
//         {
//             skynet_free(data);
//         }
//         return -2;
//     }

//     _filter_args(ctx, type, &session, (void **)&data, &sz); // 预处理消息数据块

//     if (source == 0)
//     {
//         source = ctx->handle;
//     }

//     if (destination == 0)
//     {
//         if (data)
//         {
//             skynet_error(ctx, "Destination address can't be 0");
//             skynet_free(data);
//             return -1;
//         }

//         return session;
//     }
//     // 不是同一skynet节点里服务，交给harbor
//     if (skynet_harbor_message_isremote(destination))
//     {
//         struct remote_message* rmsg = skynet_malloc(sizeof(*rmsg));
//         rmsg->destination.handle = destination;
//         rmsg->message = data;
//         rmsg->sz = sz & MESSAGE_TYPE_MASK;
//         rmsg->type = sz >> MESSAGE_TYPE_SHIFT;
//         skynet_harbor_send(rmsg, source, session);    // 分布式消息发送
//     }
//     else
//     {
//         struct skynet_message smsg;
//         smsg.source = source;
//         smsg.session = session;
//         smsg.data = data;
//         smsg.sz = sz;
//         // push给目的ctx
//         if (skynet_context_push(destination, &smsg))
//         {
//             skynet_free(data);
//             return -1;
//         }
//     }

    return session;
}

int skynet_sendname(skynet_context* ctx, uint32_t source, const char * addr , int type, int session, void * data, size_t sz)
{
    if (source == 0)
        source = ctx->handle_;

    uint32_t des = 0;
    if (addr[0] == ':')
    {
        des = strtoul(addr+1, NULL, 16);
    } 
    else if (addr[0] == '.')
    {
//         des = handle_manager::instance()->find_by_name(addr + 1);
//         if (des == 0)
//         {
//             if (type & PTYPE_TAG_DONTCOPY)
//             {
//                 skynet_free(data);
//             }
//             return -1;
//         }
    } 
    else
    {
        if ((sz & MESSAGE_TYPE_MASK) != sz)
        {
            skynet_error(ctx, "The message to %s is too large", addr);
//             if (type & PTYPE_TAG_DONTCOPY)
//             {
//                 skynet_free(data);
//             }
            return -2;
        }
        _filter_args(ctx, type, &session, (void**)&data, &sz);

//         remote_message* rmsg = skynet_malloc(sizeof(*rmsg));
//         copy_name(rmsg->destination.name, addr);
//         rmsg->destination.handle = 0;
//         rmsg->message = data;
//         rmsg->sz = sz & MESSAGE_TYPE_MASK;
//         rmsg->type = sz >> MESSAGE_TYPE_SHIFT;

//         skynet_harbor_send(rmsg, source, session);
//         return session;
    }

    return skynet_send(ctx, source, des, type, session, data, sz);
}

// 发送消息
void skynet_context_send(skynet_context* ctx, void* msg, size_t sz, uint32_t source, int type, int session)
{
    skynet_message smsg;
    smsg.source = source;
    smsg.session = session;
    smsg.data = msg;
    smsg.sz = sz | (size_t)type << MESSAGE_TYPE_SHIFT;

    ctx->queue_->push(&smsg);
}


}


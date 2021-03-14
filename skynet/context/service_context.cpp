#include "service_context.h"
#include "handle_manager.h"

#include "../node/node.h"

#include "../log/log.h"

#include "../mq/mq.h"
#include "../mq/mq_msg.h"

#include "../mod/cservice_mod_manager.h"

namespace skynet {

// 
static void delete_context(service_context* svc_ctx)
{
    if (svc_ctx->log_fd_ != nullptr)
    {
        ::fclose(svc_ctx->log_fd_);
    }

    svc_ctx->mod_->instance_release(svc_ctx->instance_);
    svc_ctx->queue_->mark_release();

    delete svc_ctx;
    node::instance()->dec_svc_ctx();
}

// // 创建服务的一个session id
int service_context::new_session()
{
    // 获取session id, session id必须为正数
    // session always be a positive number
    int session = ++session_id_;
    if (session <= 0)
    {
        session_id_ = 1;
        return 1;
    }

    return session;
}

// 
void service_context::reserve()
{
    grab();
    // don't count the context reserved, because skynet abort (the worker threads terminate) only when the total context is 0 .
    // the reserved context will be release at last.
    node::instance()->dec_svc_ctx();
}


// 启动一个新服务ctx：name为服务模块的名字，parm为参数，由模块自己解释含义
// @param svc_name 服务模块名
// @param param 服务参数
service_context* skynet_context_new(const char* svc_name, const char* param)
{
    // query c service
    cservice_mod* mod = cservice_mod_manager::instance()->query(svc_name);
    if (mod == nullptr)
        return nullptr;

    // create service mod own data block (如: struct snlua, struct logger,  struct gate)
    void* inst = mod->instance_create();
    if (inst == nullptr)
        return nullptr;
    
    // create service context
    service_context* ctx = new service_context;

//     CHECKCALLING_INIT(ctx)

    ctx->mod_ = mod;
    ctx->instance_ = inst;
    ctx->ref_ = 2;        // 初始化完成会调用skynet_context_release将引用计数-1，ref变成1而不会被释放掉
    ctx->cb_ = nullptr;
    ctx->cb_ud_ = nullptr;
    ctx->session_id_ = 0;
    ctx->log_fd_ = nullptr;

    ctx->is_init_ = false;
    ctx->is_blocked_ = false;

    ctx->cpu_cost_ = 0;
    ctx->cpu_start_ = 0;
    ctx->message_count_ = 0;
    ctx->profile_ = node::instance()->is_profile();

    // Should set to 0 first to avoid skynet_handle_retireall get an uninitialized handle
    ctx->svc_handle_ = 0;
    ctx->svc_handle_ = handle_manager::instance()->registe(ctx);        // 从skynet_handle获得唯一的标识id
    // 初始化次级消息队列
    message_queue* queue = ctx->queue_ = message_queue::create(ctx->svc_handle_);
    // init function maybe use ctx->handle, so it must init at last
    // 增加服务数量
    node::instance()->inc_svc_ctx();


    // 调用服务模块的初始化方法
    CHECKCALLING_BEGIN(ctx)
    int r = mod->instance_init(inst, ctx, param);  // 初始化ctx独有的数据块
    CHECKCALLING_END(ctx)

    // 服务模块初始化成功
    if (r == 0)
    {
        service_context* ret = skynet_context_release(ctx);

        //
        if (ret != nullptr)
            ctx->is_init_ = true;

        // 将服务的消息队列加到全局消息队列中, 这样才能收到消息回调
        global_mq::instance()->push(queue);
        if (ret != nullptr)
        {
            log(ret, "LAUNCH %s %s", svc_name, param ? param : "");
        }
        return ret;
    } 
    // 服务模块初始化失败
    else
    {
        log(ctx, "FAILED launch %s", svc_name);
        uint32_t handle = ctx->svc_handle_;
        skynet_context_release(ctx);
        handle_manager::instance()->retire(handle);
        // drop_t d = { handle };
        // queue->release(drop_message, &d);
        return nullptr;
    }
}

// 创建服务ctx
service_context* skynet_context_release(service_context* svc_ctx)
{
    if (--svc_ctx->ref_ == 0)
    {
        delete_context(svc_ctx);
        return nullptr;
    }

    return svc_ctx;
}

// 投递服务消息
int skynet_context_push(uint32_t svc_handle, skynet_message* message)
{
    // 增加服务引用计数
    service_context* svc_ctx = handle_manager::instance()->grab(svc_handle);
    if (svc_ctx == nullptr)
        return -1;

    // 消息入队
    svc_ctx->queue_->push(message);
    // 减少服务引用计数
    // skynet_context_release(svc_ctx);

    return 0;
}

// 发送消息
void skynet_context_send(service_context* svc_ctx, void* msg, size_t sz, uint32_t src_svc_handle, int type, int session)
{
    skynet_message smsg;
    smsg.src_svc_handle = src_svc_handle;
    smsg.session = session;
    smsg.data = msg;
    smsg.sz = sz | (size_t)type << MESSAGE_TYPE_SHIFT;

    svc_ctx->queue_->push(&smsg);
}

void skynet_context_blocked(uint32_t svc_handle)
{
    service_context* svc_ctx = handle_manager::instance()->grab(svc_handle);
    if (svc_ctx == nullptr)
        return;

    // mark blocked
    svc_ctx->is_blocked_ = true;
    // skynet_context_release(svc_ctx);
}

}

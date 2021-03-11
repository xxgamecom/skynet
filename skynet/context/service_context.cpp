#include "service_context.h"
#include "handle_manager.h"

#include "../log/skynet_error.h"
#include "../mq/mq.h"
#include "../server/skynet_node.h"
#include "../mod/cservice_mod_manager.h"

namespace skynet {

// 
static void delete_context(skynet_context* svc_ctx)
{
    if (svc_ctx->log_fd_ != nullptr)
    {
        ::fclose(svc_ctx->log_fd_);
    }

    svc_ctx->mod_->instance_release(svc_ctx->instance_);
    svc_ctx->queue_->mark_release();

//     CHECKCALLING_DESTROY(svc_ctx)

    delete svc_ctx;
    skynet_node::instance()->dec_svc_ctx();
}

// // 创建服务的一个session id
int skynet_context::newsession()
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

// 增加服务引用计数
void skynet_context::grab()
{
    ++ref_;
}

// 
void skynet_context::reserve()
{
    grab();
    // don't count the context reserved, because skynet abort (the worker threads terminate) only when the total context is 0 .
    // the reserved context will be release at last.
    skynet_node::instance()->dec_svc_ctx();
}

uint32_t skynet_context::handle()
{
    return handle_;
}

// 设置回调函数
void skynet_context::callback(void* ud, skynet_cb cb)
{
    cb_ = cb;
    cb_ud_ = ud;
}


// 启动一个新服务ctx：name为服务模块的名字，parm为参数，由模块自己解释含义
// @param name 服务模块名
// @param param 服务参数
skynet_context* skynet_context_new(const char* name, const char* param)
{
    // query c service
    cservice_mod* mod = cservice_mod_manager::instance()->query(name);
    if (mod == nullptr)
        return nullptr;

    // ctx独有的数据块(如struct snlua, struct logger,  struct gate等)，最终会调用c服务里的xxx_create
    void* inst = mod->instance_create();
    if (inst == nullptr)
        return nullptr;
    
    // 创建并初始化 服务context
    skynet_context* ctx = new skynet_context;

//     CHECKCALLING_INIT(ctx)

    ctx->mod_ = mod;
    ctx->instance_ = inst;
    ctx->ref_ = 2;        // 初始化完成会调用skynet_context_release将引用计数-1，ref变成1而不会被释放掉
    ctx->cb_ = nullptr;
    ctx->cb_ud_ = nullptr;
    ctx->session_id_ = 0;
    ctx->log_fd_ = nullptr;

//     ctx->init = false;
    ctx->endless_ = false;

    ctx->cpu_cost_ = 0;
    ctx->cpu_start_ = 0;
    ctx->message_count_ = 0;
//     ctx->profile_ = G_NODE.profile;

    // Should set to 0 first to avoid skynet_handle_retireall get an uninitialized handle
    ctx->handle_ = 0;
    ctx->handle_ = handle_manager::instance()->registe(ctx);        // 从skynet_handle获得唯一的标识id
    // 初始化次级消息队列
    message_queue* queue = ctx->queue_ = message_queue::create(ctx->handle_);
    // init function maybe use ctx->handle, so it must init at last
    // 增加服务数量
    skynet_node::instance()->inc_svc_ctx();


//     // 调用服务模块的初始化方法
//     CHECKCALLING_BEGIN(ctx)
    int r = mod->instance_init(inst, ctx, param);  // 初始化ctx独有的数据块
//     CHECKCALLING_END(ctx)
    // 服务模块初始化成功
    if (r == 0)
    {
        skynet_context* ret = skynet_context_release(ctx);
        if (ret != nullptr)
        {
//             ctx->init = true;
        }
        // 将服务的消息队列加到全局消息队列中, 这样才能收到消息回调
        global_mq::instance()->push(queue);
        if (ret != nullptr)
        {
            skynet_error(ret, "LAUNCH %s %s", name, param ? param : "");
        }
        return ret;
    } 
    // 服务模块初始化失败
    else
    {
//         skynet_error(ctx, "FAILED launch %s", name);
//         uint32_t handle = ctx->handle;
//         skynet_context_release(ctx);
//         skynet_handle_retire(handle);
//         struct drop_t d = { handle };
//         skynet_mq_release(queue, drop_message, &d);
        return nullptr;
    }
}

// 创建服务ctx
skynet_context* skynet_context_release(skynet_context* svc_ctx)
{
//     if (ATOM_DEC(&svc_ctx->ref) == 0)
//     {
//         delete_context(svc_ctx);
//         return nullptr;
//     }

    return svc_ctx;
}


}

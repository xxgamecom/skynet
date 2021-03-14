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

#pragma once

#include "node_config.h"

#include <atomic>

namespace skynet {

// forward declare
class node_config;
class service_context;
class mq_private;
class service_monitor;

// server node
class node final
{
private:
    static node* instance_;
public:
    static node* instance();

    // node info
private:
    node_config                 node_config_;               // skynet node config

    std::atomic<int>            total_ { 0 };               // servcie context count in this skynet node
    bool                        is_init_ = false;           // 是否已初始化, 1表示已经初始化
    uint32_t                    monitor_exit_ = 0;          // monitor exit service handle
    bool                        profile_ = false;           // enable statistics profiler, default: disable

private:
    node() = default;
public:
    ~node() = default;

public:
    // initialize skynet node 
    bool init(const std::string config_filename);
    // clean skynet node 
    void fini();

    // start skynet node
    void start();

public:
    // get service ctx count
    int total_svc_ctx();
    // inc service ctx count
    void inc_svc_ctx();
    // dec service ctx count
    void dec_svc_ctx();

    //
    uint32_t get_monitor_exit();
    void set_monitor_exit(uint32_t monitor_exit);

    // 
    void enable_profiler(int enable);
    bool is_profile();

public:
    // for log output before exit
    void dispatch_all(service_context* svc_ctx);
    // 派发消息, 工作线程的核心逻辑 // return next queue
    mq_private* message_dispatch(service_monitor& svc_monitor, mq_private*, int weight);

private:
    void _bootstrap(service_context* log_svc_ctx, const char* cmdline);
};


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

#include "node.inl"

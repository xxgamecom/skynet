/**
 * skynet提供的api主要分两大类:
 * 1. 对ctx的一系列操作，比如创建，删除ctx等
 * 2. 如何发送消息和处理自身的消息
 *
 * node service command (cmd_xxx):
 * 1. cmd_launch 创建一个新服务，
 * 2. cmd_exit 服务自身退出，
 * 3. cmd_kill 杀掉一个服务等，
 * 上层统一调用 service_command::exec 接口即可执行这些操作。
 *
 * 对ctx操作，通常会先调用 service_context::grab 将引用计数+1，
 * 操作完调用 service_manager::instance()->release_service() 将引用计数-1，
 * 以保证操作ctx过程中，不会被其他线程释放掉。
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
struct skynet_message;

// server node
class node final
{
private:
    static node* instance_;
public:
    static node* instance();

    // node info
private:
    node_config                 config_;                    // skynet node config

    uint32_t                    monitor_exit_ = 0;          // monitor exit service handle
    bool                        profile_ = false;           // enable statistics profiler, default: disable

public:
    //
    bool init(const std::string config_filename);
    void fini();

    //
    void start();

    //
    uint32_t get_monitor_exit();
    void set_monitor_exit(uint32_t monitor_exit);

    // 
    void enable_profiler(int enable);
    bool is_profile();

    // process service message, called by work thread, return next queue
    mq_private* dispatch_message(service_monitor& svc_monitor, mq_private*, int weight);

private:
    //
    void _bootstrap(service_context* log_svc_ctx, const char* cmdline);
    // for log output before exit
    void _dispatch_all(service_context* svc_ctx);

    // handle service message (call service message callback)
    void _do_dispatch_message(service_context* svc_ctx, skynet_message* msg);
};

}

#include "node.inl"

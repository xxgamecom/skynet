/**
 * 监视服务死循环
 * 
 * 工作原理:
 * 1. 工作线程每次处理一个服务的一个消息时，都会在一个和服务相关的全局变量处自增1。
 * 2. monitor线程每隔一小段时间（5秒左右）检测一下所有的工作线程，看有没有长期没有自增的，若有就认为其正在处理的消息可能陷入死循环了。
 * 3. 发现这种异常情况后，skynet 仅仅输出一行 log，它无法从外部中断消息处理过程，而死循环的服务，将永久占据一个核心，让系统整体性能下降。
 * 4. 采用 skynet 的 kill 指令是无法杀掉死循环的服务的。
 */

#include "skynet_monitor.h"
#include "server.h"

#include "../log/log.h"

namespace skynet {

void skynet_monitor::trigger(uint32_t src_svc_handle, uint32_t dst_svc_handle)
{
    src_svc_handle_ = src_svc_handle;
    dst_svc_handle_ = dst_svc_handle;
    ++version_;
}

void skynet_monitor::check()
{
    // no message or 处理消息比较耗时
    if (version_ == check_version_)
    {
        // 说明可能死循环或比较耗时
        if (dst_svc_handle_ != 0)
        {
            skynet_context_endless(dst_svc_handle_);
            log(nullptr, "A message from [ :%08x ] to [ :%08x ] maybe in an endless loop (version = %d)", src_svc_handle_, dst_svc_handle_, version_.load());
        }
    }
    else
    {
        check_version_ = version_;
    }
}

}


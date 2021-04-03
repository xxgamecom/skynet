/**
 * monitor service dead lock
 *
 * 1. 工作线程每次处理一个服务的一个消息时，都会在一个和服务相关的全局变量处自增1。
 * 2. monitor thread will check all worker thread every 5 seconds，看有没有长期没有自增的，若有就认为其正在处理的消息可能陷入死循环了。
 * 3. when service dead lock detected, skynet just output a log message, 它无法从外部中断消息处理过程，而死循环的服务，将永久占据一个核心，让系统整体性能下降。
 * 4. use skynet kill command to terminate the dead lock service.
 */

#include "service_monitor.h"
#include "service_context.h"
#include "service_manager.h"

#include "../log/log.h"

namespace skynet {

void service_monitor::process_begin(uint32_t src_svc_handle, uint32_t dst_svc_handle)
{
    src_svc_handle_ = src_svc_handle;
    dst_svc_handle_ = dst_svc_handle;
    ++last_version_;
}

void service_monitor::process_end()
{
    src_svc_handle_ = 0;
    dst_svc_handle_ = 0;
    ++last_version_;
}

void service_monitor::check()
{
    // no message or 处理消息比较耗时
    if (check_version_ == last_version_)
    {
        // 可能死循环或比较耗时
        if (dst_svc_handle_ != 0)
        {
            // process blocked service
            service_manager::instance()->process_blocked_service(dst_svc_handle_);

            // just output a log message
            log_warn(nullptr, fmt::format("A message from [:{:08X}] to [:{:08X}] maybe in an dead loop (last_version = {})", src_svc_handle_, dst_svc_handle_, last_version_.load()));
        }
    }
    else
    {
        check_version_ = last_version_;
    }
}

}


#include "skynet_command.h"
#include "node.h"
#include "node_env.h"

#include "../log/service_log.h"
#include "../log/log.h"

#include "../mq/mq_private.h"
#include "../mq/mq_msg.h"

#include "../timer/timer_manager.h"
#include "../mod/cservice_mod_manager.h"

#include "../context/handle_manager.h"
#include "../context/service_context.h"

#include "../utils/time_helper.h"

#include <cstdio>
#include <cstring>

namespace skynet {

// ":00000000" 格式的服务句柄字符串 -> uint32_t服务句柄
static uint32_t _to_svc_handle(service_context* svc_ctx, const char* param)
{
    uint32_t svc_handle = 0;

    // :00000000 格式服务句柄字符串
    if (param[0] == ':')
    {
        svc_handle = ::strtoul(param + 1, nullptr, 16);
    }
    // local service name
    else if (param[0] == '.')
    {
        svc_handle = handle_manager::instance()->find_by_name(param + 1);
    }
    //
    else
    {
        log(svc_ctx, "Can't convert %s to svc_handle", param);
    }

    return svc_handle;
}

// id -> hex string
static void _id_to_hex(uint32_t id, char* str)
{
    static char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
    str[0] = ':';
    for (int i = 0; i < 8; i++)
    {
        str[i + 1] = hex[(id >> ((7 - i) * 4)) & 0xf];
    }
    str[9] = '\0';
}

// 
static void _handle_exit(service_context* svc_ctx, uint32_t svc_handle)
{
    if (svc_handle == 0)
    {
        svc_handle = svc_ctx->svc_handle_;
        log(svc_ctx, "KILL self");
    }
    else
    {
        log(svc_ctx, "KILL :%0x", svc_handle);
    }

    if (node::instance()->get_monitor_exit() != 0)
    {
         skynet_send(svc_ctx, svc_handle, node::instance()->get_monitor_exit(), message_type::PTYPE_CLIENT, 0, nullptr, 0);
    }

    handle_manager::instance()->retire(svc_handle);
}


// skynet cmd: timeout
// @param param 
static const char* cmd_timeout(service_context* svc_ctx, const char* param)
{
    // get time & session id
    char* session_ptr = nullptr;
    int ti = ::strtol(param, &session_ptr, 10);

    int session = svc_ctx->new_session();
    timer_manager::instance()->timeout(svc_ctx->svc_handle_, ti, session);
    
    ::sprintf(svc_ctx->result_, "%d", session);
    return svc_ctx->result_;
}

// skynet指令: reg, 给自身起一个名字（支持多个）
// cmd_name给指定ctx起一个名字，即将ctx->handle绑定一个名称(handle_manager::instance()->set_handle_by_name)
static const char* cmd_reg(service_context* svc_ctx, const char* param)
{
    if (param == nullptr || param[0] == '\0')
    {
        ::sprintf(svc_ctx->result_, ":%x", svc_ctx->svc_handle_);
        return svc_ctx->result_;
    }
    else if (param[0] == '.')
    {
        return handle_manager::instance()->set_handle_by_name(param + 1, svc_ctx->svc_handle_);
    }
    else
    {
        log(svc_ctx, "Can't register global name %s in C", param);
        return nullptr;
    }
}

// skynet cmd: query, 通过名字查找对应的handle
// 发送消息前先要找到对应的ctx，才能给ctx发送消息
static const char* cmd_query(service_context* svc_ctx, const char* param)
{
    // local service
    if (param[0] == '.')
    {
        uint32_t svc_handle = handle_manager::instance()->find_by_name(param + 1);
        if (svc_handle != 0)
        {
            ::sprintf(svc_ctx->result_, ":%x", svc_handle);
            return svc_ctx->result_;
        }
    }

    return nullptr;
}

// skynet cmd: name
static const char* cmd_name(service_context* svc_ctx, const char* param)
{
    int size = ::strlen(param);
    char name[size + 1];
    char svc_handle[size + 1];
    ::sscanf(param, "%s %s", name, svc_handle);
    if (svc_handle[0] != ':')
        return nullptr;

    uint32_t handle_id = ::strtoul(svc_handle + 1, nullptr, 16);
    if (handle_id == 0)
        return nullptr;

    if (name[0] == '.')
    {
        return handle_manager::instance()->set_handle_by_name(name + 1, handle_id);
    }
    else
    {
        log(svc_ctx, "Can't set global name %s in C", name);
    }

    return nullptr;
}

// skynet cmd: exit, 服务主动退出
// cmd_kill杀掉某个服务（被动），都会调用到handle_exit，
// 然后调用到skynet_handle_retire，回收ctx->handle，供之后创建新的ctx使用，并将引用计数-1，
// 如果ctx没有在其他地方引用，ctx->ref此时是0，所以可以删掉，delete_context主要是做一些清理回收工作。
// 如果其他地方有引用，在下一次skynet_context_release时删掉
static const char* cmd_exit(service_context* svc_ctx, const char* param)
{
    _handle_exit(svc_ctx, 0);
    return nullptr;
}

// skynet cmd: kill
static const char* cmd_kill(service_context* svc_ctx, const char* param)
{
    uint32_t svc_handle = _to_svc_handle(svc_ctx, param);
    if (svc_handle != 0)
    {
        _handle_exit(svc_ctx, svc_handle);
    }

    return nullptr;
}

// skynet cmd: launch
// 启动一个新服务，最终会通过skynet_context_new创建一个ctx，初始化ctx中各个数据。
static const char* cmd_launch(service_context* context, const char* param)
{
    size_t sz = ::strlen(param);
    char tmp[sz + 1];
    ::strcpy(tmp, param);
    char* args = tmp;
    char* mod_name = ::strsep(&args, " \t\r\n");
    args = ::strsep(&args, "\r\n");

    service_context* svc_ctx = skynet_context_new(mod_name, args);
    if (svc_ctx == nullptr)
    {
        return nullptr;
    }
    else
    {
        _id_to_hex(svc_ctx->svc_handle_, context->result_);
        return context->result_;
    }
}

// skynet cmd: getenv, 获取skynet环境变量，是key-value结构，所有ctx共享的
static const char* cmd_get_env(service_context* svc_ctx, const char* param)
{
    return node_env::instance()->get_env(param);
}

// skynet cmd: setenv, 设置skynet环境变量，是key-value结构，所有ctx共享的
static const char* cmd_set_env(service_context* svc_ctx, const char* param)
{
    size_t sz = ::strlen(param);
    char key[sz + 1];
    int i;
    for (i = 0; param[i] != ' ' && param[i]; i++)
    {
        key[i] = param[i];
    }
    if (param[i] == '\0')
        return nullptr;

    key[i] = '\0';
    param += i+1;

    node_env::instance()->set_env(key, param);

    return nullptr;
}

// skynet cmd: start_time
static const char* cmd_start_time(service_context* svc_ctx, const char* param)
{
    uint32_t sec = timer_manager::instance()->start_time();
    ::sprintf(svc_ctx->result_, "%u", sec);
    return svc_ctx->result_;
}

// skynet cmd: abort
static const char* cmd_abort(service_context* svc_ctx, const char* param)
{
    handle_manager::instance()->retire_all();
    return nullptr;
}

// skynet cmd: monitor
// @param param ":00000000" 格式的服务句柄字符串
static const char* cmd_monitor(service_context* svc_ctx, const char* param)
{
    uint32_t svc_handle = 0;
    if (param == nullptr || param[0] == '\0')
    {
        // monitor thread has exit
        if (node::instance()->get_monitor_exit() != 0)
        {
            // return current monitor serivce
            ::sprintf(svc_ctx->result_, ":%x", node::instance()->get_monitor_exit());
            return svc_ctx->result_;
        }

        return nullptr;
    }
    else
    {
        svc_handle = _to_svc_handle(svc_ctx, param);
    }

    node::instance()->set_monitor_exit(svc_handle);

    return nullptr;
}

// skynet cmd: stat, 查看ctx的内部状态信息，比如查看当前的消息队列长度，查看累计消耗CPU时间，查看消息是否阻塞等
static const char* cmd_stat(service_context* svc_ctx, const char* param)
{
    // message queue length
    if (::strcmp(param, "mqlen") == 0)
    {
        int len = svc_ctx->queue_->length();
        sprintf(svc_ctx->result_, "%d", len);
    }
    // maybe dead loop or blocked
    else if (::strcmp(param, "is_blocked") == 0)
    {
        if (svc_ctx->is_blocked_)
        {
            ::strcpy(svc_ctx->result_, "1");
            svc_ctx->is_blocked_ = false;
        }
        else
        {
            ::strcpy(svc_ctx->result_, "0");
        }
    }
    // cpu usage
    else if (::strcmp(param, "cpu") == 0)
    {
        double t = (double)svc_ctx->cpu_cost_ / 1000000.0;    // microsecond
        ::sprintf(svc_ctx->result_, "%lf", t);
    }
    // 
    else if (::strcmp(param, "time") == 0)
    {
        if (svc_ctx->profile_)
        {
            uint64_t ti = time_helper::thread_time() - svc_ctx->cpu_start_;
            double t = (double)ti / 1000000.0;    // microsecond
            ::sprintf(svc_ctx->result_, "%lf", t);
        }
        else
        {
            ::strcpy(svc_ctx->result_, "0");
        }
    } 
    //
    else if (::strcmp(param, "message") == 0)
    {
        ::sprintf(svc_ctx->result_, "%d", svc_ctx->message_count_);
    }
    // 
    else
    {
        svc_ctx->result_[0] = '\0';
    }

    return svc_ctx->result_;
}

// skynet cmd: set logger on
static const char* cmd_log_on(service_context* context, const char* param)
{
    uint32_t svc_handle = _to_svc_handle(context, param);
    if (svc_handle == 0)
        return nullptr;

    service_context* svc_ctx = handle_manager::instance()->grab(svc_handle);
    if (svc_ctx == nullptr)
        return nullptr;

    FILE* last_f = svc_ctx->log_fd_;
    if (last_f == nullptr)
    {
        // open service log file
        FILE* f = service_log::open_log_file(context, svc_handle);
        if (f != nullptr)
        {
            if (!svc_ctx->log_fd_.compare_exchange_strong(last_f, f))
            {
                // log file opens in other thread, close this
                ::fclose(f);
            }
        }
    }

    // skynet_context_release(svc_ctx);

    return nullptr;
}

// skynet cmd: set logger off
static const char* cmd_log_off(service_context* context, const char* param)
{
    uint32_t svc_handle = _to_svc_handle(context, param);
    if (svc_handle == 0)
        return nullptr;

    service_context* svc_ctx = handle_manager::instance()->grab(svc_handle);
    if (svc_ctx == nullptr)
        return nullptr;

    FILE* last_f = svc_ctx->log_fd_;
    if (last_f != nullptr)
    {
        // log file may close in other thread
        if (svc_ctx->log_fd_.compare_exchange_strong(last_f, nullptr))
        {
            service_log::close_log_file(context, last_f, svc_handle);
        }
    }

    // skynet_context_release(svc_ctx);

    return nullptr;
}

// skynet cmd: signal, 在skynet控制台，可以给指定的ctx发信号以完成相应的命令
static const char* cmd_signal(service_context* context, const char* param)
{
    uint32_t svc_handle = _to_svc_handle(context, param);
    if (svc_handle == 0)
        return nullptr;

    service_context* svc_ctx = handle_manager::instance()->grab(svc_handle);
    if (svc_ctx == nullptr)
        return nullptr;

    int sig = 0;
    param = ::strchr(param, ' ');
    if (param != nullptr)
    {
        sig = ::strtol(param, NULL, 0);
    }

    // NOTICE: the signal function should be thread safe
    svc_ctx->mod_->instance_signal(svc_ctx->instance_, sig);

    // skynet_context_release(svc_ctx);

    return nullptr;
}


// skynet instruction function data structure
struct cmd_func
{
    const char* name;
    const char* (*func)(service_context* svc_ctx, const char* param);
};

// skynet instructions
static cmd_func CMD_FUNCS[] = {
    { "TIMEOUT", cmd_timeout },
    { "REG", cmd_reg },
    { "QUERY", cmd_query },
    { "NAME", cmd_name },
    { "EXIT", cmd_exit },
    { "KILL", cmd_kill },
    { "LAUNCH", cmd_launch },
    { "GETENV", cmd_get_env },
    { "SETENV", cmd_set_env },
    { "START_TIME", cmd_start_time },
    { "ABORT", cmd_abort },
    { "MONITOR", cmd_monitor },
    { "STAT", cmd_stat },
    { "LOG_ON", cmd_log_on },
    { "LOG_OFF", cmd_log_off },
    { "SIGNAL", cmd_signal },

    { nullptr, nullptr }
};

const char* skynet_command::handle_command(service_context* svc_ctx, const char* cmd , const char* param)
{
    for (auto& inst : CMD_FUNCS)
    {
        // end
        if (inst.name == nullptr)
            break;

        // 
        if (::strcmp(cmd, inst.name) == 0)
            return inst.func(svc_ctx, param);
    }

    return nullptr;
}

}

#include "skynet_instruction.h"
#include "skynet_node.h"
#include "env.h"

#include "../log/skynet_log.h"
#include "../log/skynet_error.h"

#include "../mq/mq.h"
#include "../timer/timer_manager.h"
#include "../mod/cservice_mod_manager.h"

#include "../context/handle_manager.h"
#include "../context/service_context.h"

#include "../utils/time_helper.h"

#include <cstdio>
#include <cstring>

namespace skynet {

// ":00000000" 格式的服务句柄字符串 -> uint32_t服务句柄
static uint32_t _to_svc_handle(skynet_context* svc_ctx, const char* param)
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
        skynet_error(svc_ctx, "Can't convert %s to svc_handle", param);
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
static void _handle_exit(skynet_context* svc_ctx, uint32_t svc_handle)
{
    if (svc_handle == 0)
    {
        svc_handle = svc_ctx->handle_;
        skynet_error(svc_ctx, "KILL self");
    }
    else
    {
        skynet_error(svc_ctx, "KILL :%0x", svc_handle);
    }

    if (skynet_node::instance()->get_monitor_exit() != 0)
    {
        // skynet_send(svc_ctx, svc_handle, skynet_node::instance()->get_monitor_exit(), PTYPE_CLIENT, 0, nullptr, 0);
    }

    handle_manager::instance()->retire(svc_handle);
}


// skynet cmd: timeout
// @param param 
static const char* instruction_timeout(skynet_context* svc_ctx, const char* param)
{
    // get time & session id
    char* session_ptr = nullptr;
    int ti = ::strtol(param, &session_ptr, 10);

    int session = svc_ctx->newsession();
    timer_manager::instance()->timeout(svc_ctx->handle_, ti, session);
    
    ::sprintf(svc_ctx->result_, "%d", session);
    return svc_ctx->result_;
}

// skynet指令: reg, 给自身起一个名字（支持多个）
// instruction_name给指定ctx起一个名字，即将ctx->handle绑定一个名称(handle_manager::instance()->set_handle_by_name)
static const char* instruction_reg(skynet_context* svc_ctx, const char* param)
{
    if (param == nullptr || param[0] == '\0')
    {
        ::sprintf(svc_ctx->result_, ":%x", svc_ctx->handle_);
        return svc_ctx->result_;
    }
    else if (param[0] == '.')
    {
        return handle_manager::instance()->set_handle_by_name(param + 1, svc_ctx->handle_);
    }
    else
    {
        skynet_error(svc_ctx, "Can't register global name %s in C", param);
        return nullptr;
    }
}

// skynet cmd: query, 通过名字查找对应的handle
// 发送消息前先要找到对应的ctx，才能给ctx发送消息
static const char* instruction_query(skynet_context* svc_ctx, const char* param)
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
static const char* instruction_name(skynet_context* svc_ctx, const char* param)
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
        skynet_error(svc_ctx, "Can't set global name %s in C", name);
    }

    return nullptr;
}

// skynet cmd: exit, 服务主动退出
// instruction_kill杀掉某个服务（被动），都会调用到handle_exit，
// 然后调用到skynet_handle_retire，回收ctx->handle，供之后创建新的ctx使用，并将引用计数-1，
// 如果ctx没有在其他地方引用，ctx->ref此时是0，所以可以删掉，delete_context主要是做一些清理回收工作。
// 如果其他地方有引用，在下一次skynet_context_release时删掉
static const char* instruction_exit(skynet_context* svc_ctx, const char* param)
{
    _handle_exit(svc_ctx, 0);
    return nullptr;
}

// skynet cmd: kill
static const char* instruction_kill(skynet_context* svc_ctx, const char* param)
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
static const char* instruction_launch(skynet_context* context, const char* param)
{
    size_t sz = ::strlen(param);
    char tmp[sz + 1];
    ::strcpy(tmp, param);
    char* args = tmp;
    char* mod = strsep(&args, " \t\r\n");
    args = ::strsep(&args, "\r\n");

    // skynet_context* svc_ctx = skynet_context_new(mod, args);
    // if (svc_ctx == nullptr)
    // {
    //    return nullptr;
    // }
    // else
    {
        // _id_to_hex(svc_ctx->handle_, context->result_);
        return context->result_;
    }
}

// skynet cmd: getenv, 获取skynet环境变量，是key-value结构，所有ctx共享的
static const char* instruction_getenv(skynet_context* svc_ctx, const char* param)
{
    return env::instance()->getenv(param);
}

// skynet cmd: setenv, 设置skynet环境变量，是key-value结构，所有ctx共享的
static const char* instruction_setenv(skynet_context* svc_ctx, const char* param)
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

    env::instance()->setenv(key, param);

    return nullptr;
}

// skynet cmd: starttime
static const char* instruction_starttime(skynet_context* svc_ctx, const char* param)
{
    uint32_t sec = timer_manager::instance()->starttime();
    ::sprintf(svc_ctx->result_, "%u", sec);
    return svc_ctx->result_;
}

// skynet cmd: abort
static const char* instruction_abort(skynet_context* svc_ctx, const char* param)
{
    handle_manager::instance()->retireall();
    return nullptr;
}

// skynet cmd: monitor
// @param param ":00000000" 格式的服务句柄字符串
static const char* instruction_monitor(skynet_context* svc_ctx, const char* param)
{
    uint32_t svc_handle = 0;
    if (param == nullptr || param[0] == '\0')
    {
        // monitor thread has exit
        if (skynet_node::instance()->get_monitor_exit() != 0)
        {
            // return current monitor serivce
            ::sprintf(svc_ctx->result_, ":%x", skynet_node::instance()->get_monitor_exit());
            return svc_ctx->result_;
        }

        return nullptr;
    }
    else
    {
        svc_handle = _to_svc_handle(svc_ctx, param);
    }

    skynet_node::instance()->set_monitor_exit(svc_handle);

    return nullptr;
}

// skynet cmd: stat, 查看ctx的内部状态信息，比如查看当前的消息队列长度，查看累计消耗CPU时间，查看消息是否阻塞等
static const char* instruction_stat(skynet_context* svc_ctx, const char* param)
{
    // message queue length
    if (::strcmp(param, "mqlen") == 0)
    {
        int len = svc_ctx->queue_->length();
        sprintf(svc_ctx->result_, "%d", len);
    }
    // 服务堵塞(可能服务进入死循环)
    else if (::strcmp(param, "endless") == 0)
    {
        if (svc_ctx->endless_)
        {
            ::strcpy(svc_ctx->result_, "1");
            svc_ctx->endless_ = false;
        }
        else
        {
            ::strcpy(svc_ctx->result_, "0");
        }
    }
    // cpu使用情况
    else if (::strcmp(param, "cpu") == 0)
    {
        double t = (double)svc_ctx->cpu_cost_ / 1000000.0;    // microsec
        ::sprintf(svc_ctx->result_, "%lf", t);
    }
    // 
    else if (::strcmp(param, "time") == 0)
    {
        if (svc_ctx->profile_)
        {
            uint64_t ti = time_helper::thread_time() - svc_ctx->cpu_start_;
            double t = (double)ti / 1000000.0;    // microsec
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
static const char* instruction_logon(skynet_context* context, const char* param)
{
    uint32_t svc_handle = _to_svc_handle(context, param);
    if (svc_handle == 0)
        return nullptr;

    skynet_context* svc_ctx = handle_manager::instance()->grab(svc_handle);
    if (svc_ctx == nullptr)
        return nullptr;

    FILE* last_f = svc_ctx->log_fd_;
    if (last_f == nullptr)
    {
        // open service log file
        FILE* f = skynet_log_open(context, svc_handle);
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
static const char* instruction_logoff(skynet_context* context, const char* param)
{
    uint32_t svc_handle = _to_svc_handle(context, param);
    if (svc_handle == 0)
        return nullptr;

    skynet_context* svc_ctx = handle_manager::instance()->grab(svc_handle);
    if (svc_ctx == nullptr)
        return nullptr;

    FILE* last_f = svc_ctx->log_fd_;
    if (last_f != nullptr)
    {
        // log file may close in other thread
        if (svc_ctx->log_fd_.compare_exchange_strong(last_f, nullptr))
        {
            skynet_log_close(context, last_f, svc_handle);
        }
    }

    // skynet_context_release(svc_ctx);

    return nullptr;
}

// skynet cmd: signal, 在skynet控制台，可以给指定的ctx发信号以完成相应的命令
static const char* instruction_signal(skynet_context* context, const char* param)
{
    uint32_t svc_handle = _to_svc_handle(context, param);
    if (svc_handle == 0)
        return nullptr;

    skynet_context* svc_ctx = handle_manager::instance()->grab(svc_handle);
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
struct instruction_func
{
    const char* name;
    const char* (*func)(skynet_context* svc_ctx, const char* param);
};

// skynet instructions
static instruction_func INSTRUCTION_FUNCS[] = {
    { "TIMEOUT", instruction_timeout },
    { "REG", instruction_reg },
    { "QUERY", instruction_query },
    { "NAME", instruction_name },
    { "EXIT", instruction_exit },
    { "KILL", instruction_kill },
    { "LAUNCH", instruction_launch },
    { "GETENV", instruction_getenv },
    { "SETENV", instruction_setenv },
    { "STARTTIME", instruction_starttime },
    { "ABORT", instruction_abort },
    { "MONITOR", instruction_monitor },
    { "STAT", instruction_stat },
    { "LOGON", instruction_logon },
    { "LOGOFF", instruction_logoff },
    { "SIGNAL", instruction_signal },

    { nullptr, nullptr }
};

const char* skynet_instruction::handle_instruction(skynet_context* svc_ctx, const char* instruction , const char* param)
{
    for (auto& inst : INSTRUCTION_FUNCS)
    {
        // end
        if (inst.name == nullptr)
            break;

        // 
        if (::strcmp(instruction, inst.name) == 0)
            return inst.func(svc_ctx, param);
    }

    return nullptr;
}

}


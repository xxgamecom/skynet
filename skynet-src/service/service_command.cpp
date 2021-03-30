#include "service_command.h"
#include "service_context.h"
#include "service_manager.h"
#include "service_log.h"

#include "../mod/mod_manager.h"

#include "../node/node.h"
#include "../node/node_env.h"

#include "../log/log.h"

#include "../mq/mq_msg.h"
#include "../mq/mq_private.h"

#include "../timer/timer_manager.h"

#include "../utils/time_helper.h"

#include <cstdio>
#include <unordered_map>

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
        svc_handle = service_manager::instance()->find_by_name(param + 1);
    }
    //
    else
    {
        log_error(svc_ctx, fmt::format("Can't convert {} to svc_handle", param));
    }

    return svc_handle;
}

// id -> hex string
static void _id_to_hex(uint32_t id, char* str)
{
    char hex[16] = { '0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F' };
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
        log_info(svc_ctx, "KILL self");
    }
    else
    {
        log_info(svc_ctx, fmt::format("KILL :{:0X}", svc_handle));
    }

    if (node::instance()->get_monitor_exit() != 0)
    {
        service_manager::instance()->send(svc_ctx, svc_handle, node::instance()->get_monitor_exit(),
            SERVICE_MSG_TYPE_CLIENT, 0, nullptr, 0);
    }

    service_manager::instance()->unregister_service(svc_handle);
}


// skynet cmd: timeout
// @param param timeout ticks
const char* cmd_timeout(service_context* svc_ctx, const char* param)
{
    // get time & session id
    char* session_ptr = nullptr;
    int ticks = ::strtol(param, &session_ptr, 10);

    int session_id = svc_ctx->new_session();
    timer_manager::instance()->instance()->timeout(svc_ctx->svc_handle_, ticks, session_id);
    
    ::sprintf(svc_ctx->cmd_result_, "%d", session_id);
    return svc_ctx->cmd_result_;
}

// skynet cmd: reg, register service name（支持多个）
// cmd_name给指定ctx起一个名字，即将ctx->handle绑定一个名称(service_manager::instance()->set_handle_by_name)
const char* cmd_register(service_context* svc_ctx, const char* param)
{
    if (param == nullptr || param[0] == '\0')
    {
        ::sprintf(svc_ctx->cmd_result_, ":%x", svc_ctx->svc_handle_);
        return svc_ctx->cmd_result_;
    }
    else if (param[0] == '.')
    {
        return service_manager::instance()->set_handle_by_name(param + 1, svc_ctx->svc_handle_);
    }
    else
    {
        log_error(svc_ctx, fmt::format("Can't register global name {} in C", param));
        return nullptr;
    }
}

// skynet cmd: query, 通过名字查找对应的handle
// 发送消息前先要找到对应的ctx，才能给ctx发送消息
const char* cmd_query(service_context* svc_ctx, const char* param)
{
    // local service
    if (param[0] == '.')
    {
        uint32_t svc_handle = service_manager::instance()->find_by_name(param + 1);
        if (svc_handle != 0)
        {
            ::sprintf(svc_ctx->cmd_result_, ":%x", svc_handle);
            return svc_ctx->cmd_result_;
        }
    }

    return nullptr;
}

// skynet cmd: name
const char* cmd_name(service_context* svc_ctx, const char* param)
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
        return service_manager::instance()->set_handle_by_name(name + 1, handle_id);
    }
    else
    {
        log_error(svc_ctx, fmt::format("Can't set global name {} in C", (char*)name));
    }

    return nullptr;
}

// skynet cmd: exit, 服务主动退出
// cmd_kill杀掉某个服务（被动），都会调用到handle_exit，
// 然后调用到skynet_handle_retire，回收ctx->handle，供之后创建新的ctx使用，并将引用计数-1，
// 如果ctx没有在其他地方引用，ctx->ref此时是0，所以可以删掉，delete_context主要是做一些清理回收工作。
// 如果其他地方有引用，在下一次service_context_release时删掉
const char* cmd_exit(service_context* svc_ctx, const char* param)
{
    _handle_exit(svc_ctx, 0);
    return nullptr;
}

// skynet cmd: kill
const char* cmd_kill(service_context* svc_ctx, const char* param)
{
    uint32_t svc_handle = _to_svc_handle(svc_ctx, param);
    if (svc_handle != 0)
    {
        _handle_exit(svc_ctx, svc_handle);
    }

    return nullptr;
}

// skynet cmd: launch
// 启动一个新服务，最终会通过service_context_new创建一个ctx，初始化ctx中各个数据。
const char* cmd_launch(service_context* context, const char* param)
{
    size_t sz = ::strlen(param);
    char tmp[sz + 1];
    ::strcpy(tmp, param);
    char* args = tmp;
    char* mod_name = ::strsep(&args, " \t\r\n");
    args = ::strsep(&args, "\r\n");

    service_context* svc_ctx = service_manager::instance()->create_service(mod_name, args);
    if (svc_ctx == nullptr)
        return nullptr;

    _id_to_hex(svc_ctx->svc_handle_, context->cmd_result_);
    return context->cmd_result_;
}

// skynet cmd: getenv, 获取skynet环境变量，是key-value结构，所有ctx共享的
const char* cmd_get_env(service_context* svc_ctx, const char* param)
{
    return node_env::instance()->get_env(param);
}

// skynet cmd: setenv, 设置skynet环境变量，是key-value结构，所有ctx共享的
const char* cmd_set_env(service_context* svc_ctx, const char* param)
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
// get skynet node start time (seconds)
const char* cmd_start_time(service_context* svc_ctx, const char* param)
{
    uint32_t start_seconds = timer_manager::instance()->instance()->start_seconds();
    ::sprintf(svc_ctx->cmd_result_, "%u", start_seconds);
    return svc_ctx->cmd_result_;
}

// skynet cmd: abort
// abort all service
const char* cmd_abort(service_context* svc_ctx, const char* param)
{
    service_manager::instance()->unregister_service_all();
    return nullptr;
}

// skynet cmd: monitor
// @param param service handle string format: ":00000000"
const char* cmd_monitor(service_context* svc_ctx, const char* param)
{
    uint32_t svc_handle = 0;
    if (param == nullptr || param[0] == '\0')
    {
        // monitor thread has exit
        if (node::instance()->get_monitor_exit() != 0)
        {
            // return current monitor serivce
            ::sprintf(svc_ctx->cmd_result_, ":%x", node::instance()->get_monitor_exit());
            return svc_ctx->cmd_result_;
        }

        return nullptr;
    }

    svc_handle = _to_svc_handle(svc_ctx, param);
    node::instance()->set_monitor_exit(svc_handle);

    return nullptr;
}

// skynet cmd: stat
// query service statistics info, such as mq length, cpu usage, service blocked, message count etc.
const char* cmd_stat(service_context* svc_ctx, const char* param)
{
    // message queue length
    if (::strcmp(param, "mqlen") == 0)
    {
        int len = svc_ctx->queue_->length();
        sprintf(svc_ctx->cmd_result_, "%d", len);
    }
    // maybe dead loop or blocked
    else if (::strcmp(param, "is_blocked") == 0)
    {
        if (svc_ctx->is_blocked_)
        {
            ::strcpy(svc_ctx->cmd_result_, "1");
            svc_ctx->is_blocked_ = false;
        }
        else
        {
            ::strcpy(svc_ctx->cmd_result_, "0");
        }
    }
    // cpu usage
    else if (::strcmp(param, "cpu") == 0)
    {
        double t = (double)svc_ctx->cpu_cost_ / 1000000.0;    // microsecond
        ::sprintf(svc_ctx->cmd_result_, "%lf", t);
    }
    // 
    else if (::strcmp(param, "time") == 0)
    {
        if (svc_ctx->profile_)
        {
            uint64_t ti = time_helper::thread_time() - svc_ctx->cpu_start_;
            double t = (double)ti / 1000000.0;    // microsecond
            ::sprintf(svc_ctx->cmd_result_, "%lf", t);
        }
        else
        {
            ::strcpy(svc_ctx->cmd_result_, "0");
        }
    } 
    // message count
    else if (::strcmp(param, "message") == 0)
    {
        ::sprintf(svc_ctx->cmd_result_, "%d", svc_ctx->message_count_);
    }
    // 
    else
    {
        svc_ctx->cmd_result_[0] = '\0';
    }

    return svc_ctx->cmd_result_;
}

// skynet cmd: log_on
// set service file log on
const char* cmd_service_log_on(service_context* context, const char* param)
{
    uint32_t svc_handle = _to_svc_handle(context, param);
    if (svc_handle == 0)
        return nullptr;

    service_context* svc_ctx = service_manager::instance()->grab(svc_handle);
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

    service_manager::instance()->release_service(svc_ctx);

    return nullptr;
}

// skynet cmd: log_off
// set service file log off
const char* cmd_service_log_off(service_context* context, const char* param)
{
    uint32_t svc_handle = _to_svc_handle(context, param);
    if (svc_handle == 0)
        return nullptr;

    service_context* svc_ctx = service_manager::instance()->grab(svc_handle);
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

    service_manager::instance()->release_service(svc_ctx);

    return nullptr;
}

// skynet cmd: signal
// in skynet console, send signal to the service
const char* cmd_signal(service_context* context, const char* param)
{
    uint32_t svc_handle = _to_svc_handle(context, param);
    if (svc_handle == 0)
        return nullptr;

    service_context* svc_ctx = service_manager::instance()->grab(svc_handle);
    if (svc_ctx == nullptr)
        return nullptr;

    int sig = 0;
    param = ::strchr(param, ' ');
    if (param != nullptr)
    {
        sig = ::strtol(param, NULL, 0);
    }

    // NOTICE: the signal function should be thread safe
    svc_ctx->svc_ptr_->signal(sig);

    service_manager::instance()->release_service(svc_ctx);

    return nullptr;
}

//
typedef const char* (*cmd_proc)(service_context* context, const char* param);

//
static std::unordered_map<std::string, cmd_proc> cmd_map {
    { "TIMEOUT", cmd_timeout },
    { "REGISTER", cmd_register },
    { "QUERY", cmd_query },
    { "NAME", cmd_name },
    { "EXIT", cmd_exit },
    { "KILL", cmd_kill },
    { "LAUNCH", cmd_launch },
    { "GET_ENV", cmd_get_env },
    { "SET_ENV", cmd_set_env },
    { "START_TIME", cmd_start_time },
    { "ABORT", cmd_abort },
    { "MONITOR", cmd_monitor },
    { "STAT", cmd_stat },
    { "LOG_ON", cmd_service_log_on },
    { "LOG_OFF", cmd_service_log_off },
    { "SIGNAL", cmd_signal },
};

const char* service_command::exec(service_context* svc_ctx, const char* cmd , const char* cmd_param/* = nullptr*/)
{
    auto itr_find = cmd_map.find(cmd);
    if (itr_find != cmd_map.end())
        return itr_find->second(svc_ctx, cmd_param);

    return nullptr;
}

}


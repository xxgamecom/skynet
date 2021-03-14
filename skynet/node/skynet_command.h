/**
 * skynet command specs:
 * 1) 基础服务的统一入口, 接收一个字符串参数, 返回一个字符串结果
 * 2) 可以看成是一种文本协议
 * 3) 在 handle_command 保证在调用过程中，不会切出当前的服务线程，导致状态改变的不可预知性。
 */

#pragma once

namespace skynet {

// forward declare
class service_context;

// 
class skynet_command final
{
public:
    // handle skynet instruction
    static const char* handle_command(service_context* svc_ctx, const char* cmd, const char* param);
};

}
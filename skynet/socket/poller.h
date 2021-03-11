#pragma once

#include "server_def.h"

#include <cstdbool>

namespace skynet { namespace socket {

// 每次返回多少事件
#define MAX_EVENT                   64

// forward declare
class socket;

// event poller
class poller final
{
public:
    struct event
    {
        socket*                     socket_ptr = nullptr;                   // 事件关联的socket对象

        bool                        is_read = false;                        // 是否可读
        bool                        is_write = false;                       // 是否可写
        bool                        is_error = false;                       // 是否错误
        bool                        is_eof = false;                         // 是否结束
    };

private:
    int                             poll_fd_ = INVALID_FD;                  //

public:
    poller() = default;
    ~poller();

public:
    //  初始化
    bool init();
    // 清理
    void fini();

    // 错误检查
    bool is_invalid();

    /**
     * 在轮序句柄fd中添加一个指定sock文件描述符，用来检测该socket
     * 
     * @param sock_fd 待处理的文件描述符, 一般为socket()返回结果
     * @param ud 自己使用的指针地址特殊处理
     * @return 0 成功, -1 失败
     */
    bool add(int sock_fd, void* ud);
    
    /**
     * 在轮询句柄fd中删除注册过的sock描述符
     * 
     * @param sock 待处理的文件描述符, socket()创建的句柄
     */
    void del(int sock_fd);
    
    /**
     * 在轮序句柄fd中修改sock注册类型
     * 
     * @param sock_fd  : 待处理的句柄
     * @param ud    : 用户自定义数据地址
     * @param enable: true表示开启写, false表示还是监听读
     */
    void write(int sock_fd, void* ud, bool enable);

    /**
     * 获取需要处理的事件
     * 
     * @param event_ptr 一段struct event内存的首地址
     * @param max e内存能够使用的最大值
     * @return 返回等待到的变动数, 相对于e
     */
    int wait(event* event_ptr, int max = MAX_EVENT);
};

} }

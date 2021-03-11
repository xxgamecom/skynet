#pragma once

namespace skynet { namespace socket {

// socket辅助工具
class socket_helper final
{
public:
    // return -1 means failed
    // or return AF_INET or AF_INET6
    static int bind(const char* host, int port, int protocol, int* family);
    // 
    static int listen(const char* host, int port, int backlog);
    // 
    static bool keepalive(int fd);
    // 
    static bool reuse_address(int fd);
    // 为套接字描述符设置为非阻塞的
    static void nonblocking(int fd);
};

} }


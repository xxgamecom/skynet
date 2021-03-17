#pragma once

namespace skynet {

// socket辅助工具
class socket_helper final
{
public:
    // @return -1 failed
    //         AF_INET
    //         AF_INET6
    static int bind(const char* host, int port, int protocol, int* family);
    // 
    static int listen(const char* host, int port, int backlog);

    // set socket option: keepalive
    static bool keepalive(int fd);
    // set socket option: reuse address
    static bool reuse_address(int fd);
    // set socket option: nonblocking
    static void nonblocking(int fd);
};

}


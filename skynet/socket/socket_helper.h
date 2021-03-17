#pragma once

#include <sys/socket.h>
#include <fcntl.h>

namespace skynet {

// socket辅助工具
class socket_helper final
{
public:
    // shutdown read/write
    static bool shutdown(int fd);
    static bool shutdown_read(int fd);
    static bool shutdown_write(int fd);

    // set socket option: keepalive
    static bool keepalive(int fd);
    // set socket option: reuse address
    static bool reuse_address(int fd);
    // set socket option: nonblocking
    static bool nonblocking(int fd);
};

}

#include "socket_helper.inl"

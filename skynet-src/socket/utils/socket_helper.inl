namespace skynet {

// shutdown read/write
inline bool socket_helper::shutdown(int fd)
{
    return ::shutdown(fd, SHUT_RDWR) != -1;
}

inline bool socket_helper::shutdown_read(int fd)
{
    return ::shutdown(fd, SHUT_RD) != -1;
}

inline bool socket_helper::shutdown_write(int fd)
{
    return ::shutdown(fd, SHUT_WR) != -1;
}

inline bool socket_helper::keepalive(int fd)
{
    int keepalive = 1;
    return ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepalive , sizeof(keepalive)) != -1;
}

inline bool socket_helper::reuse_address(int fd)
{
    int reuse = 1;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&reuse, sizeof(reuse)) != -1;
}

//
inline bool socket_helper::nonblocking(int fd)
{
    int flag = ::fcntl(fd, F_GETFL, 0);
    if (flag == -1) return false;
    return ::fcntl(fd, F_SETFL, flag | O_NONBLOCK) != -1;
}

}

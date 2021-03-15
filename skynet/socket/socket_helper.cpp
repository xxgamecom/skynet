#include "socket_helper.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <memory>

namespace skynet { namespace socket {

// return -1 means failed
// or return AF_INET or AF_INET6
int socket_helper::bind(const char* host, int port, int protocol, int* family)
{
    struct addrinfo ai_hints;
    memset(&ai_hints, 0, sizeof(ai_hints));
    ai_hints.ai_family = AF_UNSPEC;
    // tcp
    if (protocol == IPPROTO_TCP)
    {
        ai_hints.ai_socktype = SOCK_STREAM;
    }
    // udp
    else
    {
        assert(protocol == IPPROTO_UDP);
        ai_hints.ai_socktype = SOCK_DGRAM;
    }
    ai_hints.ai_protocol = protocol;

    // host: INADDR_ANY
    if (host == nullptr || host[0] == 0)
        host = "0.0.0.0";
    // port
    char portstr[16] = { 0 };
    sprintf(portstr, "%d", port);

    struct addrinfo* ai_list = nullptr;
    int status = ::getaddrinfo(host, portstr, &ai_hints, &ai_list);
    if (status != 0)
    {
        return -1;
    }

    *family = ai_list->ai_family;
    int fd = ::socket(*family, ai_list->ai_socktype, 0);
    if (fd < 0)
    {
        ::freeaddrinfo(ai_list);
        return -1;
    }

    // reuse address
    if (!reuse_address(fd))
    {
        ::close(fd);
        ::freeaddrinfo(ai_list);
        return -1;
    }

    // socket binding
    status = ::bind(fd, (struct sockaddr *)ai_list->ai_addr, ai_list->ai_addrlen);
    if (status != 0)
    {
        ::close(fd);
        ::freeaddrinfo(ai_list);
        return -1;        
    }

    // 
    ::freeaddrinfo(ai_list);
    return fd;
}

int socket_helper::listen(const char* host, int port, int backlog)
{
    // bind
    int family = 0;
    int listen_fd = socket_helper::bind(host, port, IPPROTO_TCP, &family);
    if (listen_fd < 0)
    {
        return -1;
    }

    // listen
    if (::listen(listen_fd, backlog) == -1)
    {
        ::close(listen_fd);
        return -1;
    }
    
    return listen_fd;
}

bool socket_helper::keepalive(int fd)
{
    int keepalive = 1;
    return ::setsockopt(fd, SOL_SOCKET, SO_KEEPALIVE, (void*)&keepalive , sizeof(keepalive)) != -1;
}


bool socket_helper::reuse_address(int fd)
{
    int reuse = 1;
    return ::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&reuse, sizeof(reuse)) != -1;
}

//
void socket_helper::nonblocking(int fd)
{
    int flag = ::fcntl(fd, F_GETFL, 0);
    if (flag == -1)
        return;

    ::fcntl(fd, F_SETFL, flag | O_NONBLOCK);
}

} }

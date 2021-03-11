#include "pipe.h"

#include <iostream>
#include <cassert>

#include <sys/types.h>
#include <unistd.h>

namespace skynet { namespace socket {

bool pipe::init()
{
    int fd[2] = { 0 };
    if (::pipe(fd) == -1)
    {
        return false;
    }
    
    recv_fd_ = fd[0];
    send_fd_ = fd[1];

    FD_ZERO(&rfds_);

    //
    assert(recv_fd_ < FD_SETSIZE);

    return true;
}

void pipe::fini()
{
    if (recv_fd_ != INVALID_FD)
    {
        ::close(recv_fd_);
        recv_fd_ = INVALID_FD;
    }
    if (send_fd_ != INVALID_FD)
    {
        ::close(send_fd_);
        send_fd_ = INVALID_FD;
    }
}

// 获取读数据fd
int pipe::recv_fd()
{
    return recv_fd_;
}

// 获取写数据fd
int pipe::send_fd()
{
    return send_fd_;
}

// 是否有数据可读
bool pipe::is_readable()
{
    FD_SET(recv_fd_, &rfds_);
    timeval tv = {0, 0};
    return ::select(recv_fd_ + 1, &rfds_, NULL, NULL, &tv) == 1;
}

// 读取数
int pipe::read(char* buf_ptr, int sz)
{
    for (;;)
    {
        int n = ::read(recv_fd_, buf_ptr, sz);
        if (n < 0)
        {
            // interupte
            if (errno == EINTR)
                continue;

            return -1;
        }
        
        // must atomic read from a pipe
        assert(n == sz);
        return n;
    }
}

int pipe::write(const char* data_ptr, int data_sz)
{
    return ::write(send_fd_, data_ptr, data_sz);
}

} }

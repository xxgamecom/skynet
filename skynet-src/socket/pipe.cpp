#include "pipe.h"

#include <iostream>
#include <cassert>

#include <sys/types.h>
#include <unistd.h>

namespace skynet {

bool pipe::init()
{
    int fd[2] = { 0 };
    if (::pipe(fd) == -1)
        return false;

    read_fd_ = fd[0];
    write_fd_ = fd[1];

    //
    FD_ZERO(&read_fds_);

    //
    assert(read_fd_ < FD_SETSIZE);

    return true;
}

void pipe::fini()
{
    if (read_fd_ != INVALID_FD)
    {
        ::close(read_fd_);
        read_fd_ = INVALID_FD;
    }
    if (write_fd_ != INVALID_FD)
    {
        ::close(write_fd_);
        write_fd_ = INVALID_FD;
    }
}

// 是否有数据可读
bool pipe::is_readable()
{
    FD_SET(read_fd_, &read_fds_);
    timeval tv = { 0, 0 };
    return ::select(read_fd_ + 1, &read_fds_, NULL, NULL, &tv) == 1;
}

// 读取数
int pipe::read(char* buf_ptr, int sz)
{
    for (;;)
    {
        int n = ::read(read_fd_, buf_ptr, sz);
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
    return ::write(write_fd_, data_ptr, data_sz);
}

}

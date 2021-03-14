#pragma once

#include "socket_server_def.h"

namespace skynet { namespace socket {

class pipe final
{
private:
    fd_set                              rfds_;                              // 用于select的fd集

    int                                 read_fd_ = INVALID_FD;              // 读数据pipe句柄
    int                                 write_fd_ = INVALID_FD;             // 写数据pipe句柄

public:
    ~pipe() = default;

public:
    // initialize
    bool init();
    // clean
    void fini();

public:
    //
    bool is_readable();

    // get read fd
    int read_fd();
    // get write fd
    int write_fd();

    // recv data (blocked)
    int read(char* buf_ptr, int sz);
    // write data (blocked)
    int write(const char* data_ptr, int data_sz);
};

} }

#include "pipe.inl"


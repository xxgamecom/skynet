#pragma once

#include "../socket_server_def.h"

namespace skynet {

class pipe final
{
private:
    fd_set read_fds_;                           // fd set for readable check

    int read_fd_ = INVALID_FD;                  // pipe handle for read data
    int write_fd_ = INVALID_FD;                 // pipe handle for write data

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

}

#include "pipe.inl"


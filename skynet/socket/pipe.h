#pragma once

#include "server_def.h"

namespace skynet { namespace socket {

class pipe final
{
private:
    fd_set                              rfds_;                              // 用于select的fd集

    int                                 recv_fd_ = INVALID_FD;              // 读数据pipe句柄
    int                                 send_fd_ = INVALID_FD;              // 写数据pipe句柄

public:
    ~pipe() = default;

public:
    // 初始化
    bool init();
    // 清理
    void fini();

    // 获取读数据fd
    int recv_fd();
    // 获取写数据fd
    int send_fd();

    // 是否有数据可读
    bool is_readable();

    // 读取数 (blocked)
    int read(char* buf_ptr, int sz);
    // 写数据 (blocked)
    int write(const char* data_ptr, int data_sz);
};

} }

#pragma once

#include <cstdint>

namespace skynet::net {

/**
 * io statistics
 */
class io_statistics
{
public:
    virtual ~io_statistics() = default;

public:
    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual void reset() = 0;

    // 吞吐量
public:
    // 更新吞吐量计数器
    virtual void update_throughput() = 0;

    // 读写字节数
    virtual int64_t read_bytes() = 0;
    virtual int64_t write_bytes() = 0;

    // 读写字节数吞吐量(每秒字节数)
    virtual double read_bytes_throughput() = 0;
    virtual double write_bytes_throughput() = 0;

    // 最大读写字节数吞吐量(每秒读字节数)
    virtual double largest_read_bytes_throughput() = 0;
    virtual double largest_write_bytes_throughput() = 0;
};

}

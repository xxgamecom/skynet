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

    // throughput
public:
    // update throughput count
    virtual void update_throughput() = 0;

    // r/w bytes
    virtual int64_t read_bytes() = 0;
    virtual int64_t write_bytes() = 0;

    // r/w bytes throughput (bytes/seconds)
    virtual double read_bytes_throughput() = 0;
    virtual double write_bytes_throughput() = 0;

    // max r/w bytes throughput (bytes/seconds)
    virtual double largest_read_bytes_throughput() = 0;
    virtual double largest_write_bytes_throughput() = 0;
};

}

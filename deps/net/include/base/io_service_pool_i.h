#pragma once

#include <cstdint>
#include <memory>

namespace skynet { namespace net {

class io_service;

/**
 * io service pool
 */
class io_service_pool
{
public:
    virtual ~io_service_pool() = default;

public:
    virtual void run() = 0;
    virtual void stop() = 0;

public:
    // choose one ios
    virtual std::shared_ptr<io_service>& select_one() = 0;
    //
    virtual uint32_t pool_size() const = 0;
};

} }

#pragma once

#include "base/io_service_pool_i.h"

#include <cstdint>
#include <memory>
#include <vector>
#include <mutex>

// forward delcare
namespace skynet::net {
class io_service;
}

namespace skynet::net::impl {

// io service pool
class io_service_pool_impl : public io_service_pool
{
private:
    uint32_t pool_size_ = 0;                                    // io_service array size
    std::vector<std::shared_ptr<io_service>> io_services_;      // io_service array
    std::mutex io_services_mutex_;                              // io_service array mutex

    uint32_t select_index_ = 0;                                 // io_service select index

public:
    explicit io_service_pool_impl(uint32_t pool_size);
    ~io_service_pool_impl() override = default;

public:
    void run() override;
    void stop() override;

public:
    // choose one ios
    std::shared_ptr<io_service>& select_one() override;
    //
    uint32_t pool_size() const override;
};

}

#include "io_service_pool.inl"


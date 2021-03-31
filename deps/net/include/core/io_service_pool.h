#pragma once

#include <cstdint>
#include <memory>
#include <vector>
#include <mutex>

namespace skynet { namespace net {

class io_service;

// io service pool
class io_service_pool final
{
private:
    uint32_t pool_size_ = 0;                                    // io_service array size
    std::vector<std::shared_ptr<io_service>> io_services_;      // io_service array
    std::mutex io_services_mutex_;                              // io_service array mutex

    uint32_t select_index_ = 0;                                 // io_service select index

public:
    explicit io_service_pool(const uint32_t pool_size);
    ~io_service_pool() = default;

public:
    void run();
    void stop();

public:
    // choose one ios
    std::shared_ptr<io_service>& select_one();
    //
    uint32_t pool_size() const;
};

} }

#include "io_service_pool.inl"


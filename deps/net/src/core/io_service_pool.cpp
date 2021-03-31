#include "io_service_pool.h"
#include "io_service.h"

#include <cassert>

namespace skynet { namespace net {

io_service_pool::io_service_pool(const uint32_t pool_size)
:
pool_size_(pool_size)
{
    assert(pool_size > 0);
    std::shared_ptr<io_service> ios_ptr;
    for (uint32_t i=0; i<pool_size_; ++i)
    {
        ios_ptr = std::make_shared<io_service>();
        if (ios_ptr != nullptr) io_services_.push_back(ios_ptr);
    }
}

void io_service_pool::run()
{
    for (auto& itr : io_services_)
    {
        itr->run();
    }
}

void io_service_pool::stop()
{
    for (auto& itr : io_services_)
    {
        itr->stop();
    }
    io_services_.clear();

    select_index_ = 0;
}

} }


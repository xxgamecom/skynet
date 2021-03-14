#pragma once

#include <cstdint>
#include <shared_mutex>

namespace skynet {

// forward declare
class service_context;

/**
 * service context manager
 * 1) store all service context
 * 2) generate service handle
 *
 * service handle specs:
 * 1) 0 is reserved
 * 2) count start of 1
 */
class service_context_manager
{
private:
    static service_context_manager* instance_;
public:
    static service_context_manager* instance();

private:
    // constants
    enum
    {
        DEFAULT_SLOT_SIZE       = 4,                                    // default service context array size
        MAX_SLOT_SIZE           = 0x40000000,                           // max service context array size
    };

    // service handle naming (corresponding relationship between handle and name)
    struct handle_name
    {
        std::string             svc_name = "";                          // service name
        uint32_t                svc_handle = 0;                         // service handle
    };

private:
    std::shared_mutex           rw_mutex_;                              // read write lock (need C++17)

    // service context data
    uint32_t                    alloc_svc_handle_seed_ = 1;             // service handle seed, used to alloc service handle (count start of 1, 0 is reserved)
    int                         svc_ctx_slot_size_ = DEFAULT_SLOT_SIZE; // service context array size (2^n, initialize is 4)
    service_context**           svc_ctx_slot_ = nullptr;                // service context array

    // service naming (register a name for service handle, name can be more than one)
    int                         name_cap_ = 2;                          // service alias name list capacity (2^n)
    int                         name_count_ = 0;                        // number of service alias name in service alias name list.
    handle_name*                name_ = nullptr;                        // service alias name list (sort by svc_name, because find_by_name() use binary search)

public:
    // initialize service context manager
    void init();

public:
    // register service context, return service handle
    uint32_t register_svc_ctx(service_context* svc_ctx);
    // unregister service context by service handle
    int unregister(uint32_t svc_handle);
    // unregister all service context
    void unregister_all();

    // 利于ID获取服务上下文指针
    service_context* grab(uint32_t svc_handle);

    // find service handle by service name (binary search)
    uint32_t find_by_name(const char* svc_name);
    // set service handle alias (sort by svc_name, because find_by_name() use binary search)
    const char* set_handle_by_name(const char* svc_name, uint32_t svc_handle);

private:
    //
    const char* _insert_name(const char* svc_name, uint32_t svc_handle);
    // 把name插入到name数组中，再关联handle
    void _insert_name_before(char* svc_name, uint32_t svc_handle, int before);
};

// query by service name or service address string, return service handle
uint32_t skynet_query_by_name_or_addr(service_context* svc_ctx, const char* name_or_addr);

}


#pragma once

#include <cstdint>
#include <shared_mutex>
#include <atomic>

namespace skynet {

// forward declare
struct skynet_message;
class service_context;

/**
 * skynet node service manager
 * 1) store all service context
 * 2) generate service handle
 *
 * service handle specs:
 * 1) 0 is reserved
 * 2) count start of 1
 */
class service_manager
{
private:
    static service_manager* instance_;
public:
    static service_manager* instance();

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

    std::atomic<int>            svc_count_ { 0 };                       // service context count in this skynet node

public:
    // initialize service context manager
    bool init();
    void fini();

public:
    // create service context
    // @param svc_name service name
    // @param param service mod data
    service_context* create_service(const char* svc_name, const char* param);
    service_context* release_service(service_context* svc_ctx);

    // register service context, return service handle
    uint32_t register_service(service_context* svc_ctx);
    int unregister_service(uint32_t svc_handle);
    void unregister_service_all();

    // service has blocked, process
    void process_blocked_service(uint32_t svc_handle);

    // grab a service by service handle
    service_context* grab(uint32_t svc_handle);

    // find service handle by service name (binary search)
    uint32_t find_by_name(const char* svc_name);
    // set service handle alias (sort by svc_name, because find_by_name() use binary search)
    const char* set_handle_by_name(const char* svc_name, uint32_t svc_handle);

    // query by service name or service address string, return service handle
    uint32_t query_by_name_or_addr(service_context* svc_ctx, const char* name_or_addr);

    // push service message
    int push_service_message(uint32_t svc_handle, skynet_message* message);

public:
    //
    void svc_inc();
    void svc_dec();
    int svc_count();

private:
    //
    const char* _insert_name(const char* svc_name, uint32_t svc_handle);
    // 把name插入到name数组中，再关联handle
    const char* _insert_name_before(const char* svc_name, uint32_t svc_handle, int before);
};

}

#include "service_manager.inl"


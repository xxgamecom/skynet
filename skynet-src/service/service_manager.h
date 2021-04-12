#pragma once

#include <cstdint>
#include <shared_mutex>
#include <atomic>

namespace skynet {

// forward declare
struct service_message;
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
        DEFAULT_SLOT_SIZE = 4,                              // default service context array size
        MAX_SLOT_SIZE = 0x40000000,                         // max service context array size
    };

    // service handle naming (corresponding relationship between handle and name)
    struct handle_name
    {
        std::string svc_name = "";                          // service name
        uint32_t svc_handle = 0;                            // service handle
    };

private:
    std::shared_mutex rw_mutex_;                            // read write lock (need C++17)

    // service context data
    uint32_t alloc_svc_handle_seed_ = 1;                    // service handle seed, used to alloc service handle (count start of 1, 0 is reserved)
    int svc_ctx_slot_size_ = DEFAULT_SLOT_SIZE;             // service context array size (2^n, initialize is 4)
    service_context** svc_ctx_slot_ = nullptr;              // service context array

    // service naming (register a name for service handle, name can be more than one)
    int name_cap_ = 2;                                      // service alias name list capacity (2^n)
    int name_count_ = 0;                                    // number of service alias name in service alias name list.
    handle_name* name_ = nullptr;                           // service alias name list (sort by svc_name, because find_by_name() use binary search)

    std::atomic<int> svc_count_ { 0 };                   // service context count in this skynet node

public:
    // initialize service context manager
    bool init();
    void fini();

public:
    service_context* create_service(const char* svc_name, const char* svc_args);
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
    uint32_t query_by_name(service_context* svc_ctx, const char* name_or_addr);

    //
    int svc_count();

public:
    // push service message
    int push_service_message(uint32_t svc_handle, service_message* message);

    //
    // @param src_svc_handle 0: reserve service handle, self
    // @param dst_svc_handle
    // @param svc_msg_type
    // @param session 每个服务仅有一个callback函数, 所以需要一个标识来区分消息包, 这就是session的作用
    //                可以在 svc_msg_type 里设上 alloc session 的 tag (MESSAGE_TAG_ALLOC_SESSION), send api 就会忽略掉传入的 session 参数，而会分配出一个当前服务从来没有使用过的 session 号，发送出去。
    //                同时约定，接收方在处理完这个消息后，把这个 session 原样发送回来。这样，编写服务的人只需要在 callback 函数里记录下所有待返回的 session 表，就可以在收到每个消息后，正确的调用对应的处理函数。
    int send(service_context* svc_ctx, uint32_t src_svc_handle, uint32_t dst_svc_handle, int svc_msg_type, int session_id, void* msg, size_t msg_sz);

    /**
     * send by service name or service address (format: ":%08x")
     *
     * @param svc_ctx
     * @param src_svc_handle 0: reserve service handle, self
     * @param dst_name_or_addr service name or service address (format: ":%08x")
     * @param svc_msg_type
     * @param session
     * @param msg
     * @param msg_sz
     */
    int send_by_name(service_context* svc_ctx, uint32_t src_svc_handle, const char* dst_name_or_addr, int svc_msg_type, int session, void* msg, size_t msg_sz);

private:
    //
    const char* _insert_name(const char* svc_name, uint32_t svc_handle);
    // 把name插入到name数组中，再关联handle
    const char* _insert_name_before(const char* svc_name, uint32_t svc_handle, int before);
};

}

#include "service_manager.inl"


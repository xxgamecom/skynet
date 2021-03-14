#include "service_context_manager.h"
#include "service_context.h"

#include "../log/log.h"

#include <mutex>

namespace skynet {

// high 8 bits: remote service id
#define HANDLE_MASK                 0xFFFFFF        // handle mask
#define HANDLE_REMOTE_SHIFT         24              // remote service id offset

service_context_manager* service_context_manager::instance_ = nullptr;

service_context_manager* service_context_manager::instance()
{
    static std::once_flag oc;
    std::call_once(oc, [&](){
        instance_ = new service_context_manager;
    });

    return instance_;
}

// 
void service_context_manager::init()
{
    // 给slot分配slot_size个ctx内存
    svc_ctx_slot_size_ = DEFAULT_SLOT_SIZE;
    svc_ctx_slot_ = new service_context* [svc_ctx_slot_size_] { nullptr };

    alloc_svc_handle_seed_ = 1;
    name_cap_ = 2;
    name_count_ = 0;

    //
    name_ = new handle_name[name_cap_];

    // Don't need to free H
}

uint32_t service_context_manager::register_svc_ctx(service_context* svc_ctx)
{
    // write lock
    std::unique_lock<std::shared_mutex> wlock(rw_mutex_);

    for (;;)
    {
        uint32_t svc_handle = alloc_svc_handle_seed_;
        for (int i = 0; i < svc_ctx_slot_size_; i++, svc_handle++)
        {
            if (svc_handle > HANDLE_MASK)
            {
                // 0 is reserved
                svc_handle = 1;
            }
            int hash = svc_handle & (svc_ctx_slot_size_ - 1);
            if (svc_ctx_slot_[hash] == nullptr)
            {
                svc_ctx_slot_[hash] = svc_ctx;
                alloc_svc_handle_seed_ = svc_handle + 1;

                return svc_handle;
            }
        }
        assert((svc_ctx_slot_size_ * 2 - 1) <= HANDLE_MASK);
        service_context** new_slot = new service_context* [2 * svc_ctx_slot_size_];
        ::memset(new_slot, 0, svc_ctx_slot_size_ * 2 * sizeof(service_context*));
        for (int i = 0; i < svc_ctx_slot_size_; i++)
        {
            int hash = svc_ctx_slot_[i]->svc_handle_ & (svc_ctx_slot_size_ * 2 - 1);
            assert(new_slot[hash] == nullptr);
            new_slot[hash] = svc_ctx_slot_[i];
        }

        delete[] svc_ctx_slot_;
        svc_ctx_slot_ = new_slot;
        svc_ctx_slot_size_ *= 2;
    }
}

// 注销一个服务
int service_context_manager::retire(uint32_t svc_handle)
{
    int ret = 0;

    // write lock
    std::unique_lock<std::shared_mutex> wlock(rw_mutex_);

    uint32_t hash = svc_handle & (svc_ctx_slot_size_ - 1);
    service_context* svc_ctx = svc_ctx_slot_[hash];

    if (svc_ctx != nullptr && svc_ctx->svc_handle_ == svc_handle)
    {
        svc_ctx_slot_[hash] = nullptr;
        ret = 1;
        int j = 0, n = name_count_;
        for (int i = 0; i < n; ++i)
        {
            if (name_[i].svc_handle == svc_handle)
            {
                // delete[] name_[i].name;
                continue;
            }
            else if (i!=j)
            {
                name_[j] = name_[i];
            }
            ++j;
        }
        name_count_ = j;
    }
    else
    {
        svc_ctx = nullptr;
    }

    wlock.unlock();

     if (svc_ctx != nullptr)
     {
    //     // release service context may call skynet_handle_*, so wunlock first.
    //     service_context_release(svc_ctx);
     }

    return ret;
}

// 注销全部服务
void service_context_manager::retire_all()
{
    for (;;)
    {
        int n = 0;
        for (int i = 0; i < svc_ctx_slot_size_; i++)
        {
            uint32_t svc_handle = 0;

            // read scope lock
            {
                std::shared_lock<std::shared_mutex> rlock(rw_mutex_);

                service_context* svc_ctx = svc_ctx_slot_[i];
                if (svc_ctx != nullptr)
                {
                    svc_handle = svc_ctx->svc_handle_;
                }
            }

            if (svc_handle != 0)
            {
                if (retire(svc_handle))
                {
                    ++n;
                }
            }
        }

        //
        if (n == 0)
        {
            return;
        }
    }
}

// 取得一个服务 (增加服务引用计数)
service_context* service_context_manager::grab(uint32_t svc_handle)
{
    service_context* result = nullptr;

    // read lock
    std::shared_lock<std::shared_mutex> rlock(rw_mutex_);

    uint32_t hash = svc_handle & (svc_ctx_slot_size_ - 1);
    service_context* svc_ctx = svc_ctx_slot_[hash];
    if (svc_ctx != nullptr && svc_ctx->svc_handle_ == svc_handle)
    {
        result = svc_ctx;
        result->grab();
    }

    return result;
}

// 通过name找handle
// S->name是按handle_name->name升序排序的，通过二分查找快速地查找name对应的handle
uint32_t service_context_manager::find_by_name(const char* svc_name)
{
    // read lock
    std::shared_lock<std::shared_mutex> rlock(rw_mutex_);

    uint32_t svc_handle = 0;

    int begin = 0;
    int end = name_count_ - 1;
    while (begin <= end)
    {
        int mid = (begin + end)/2;
        handle_name* n = &name_[mid];
        
        int c = n->svc_name.compare(svc_name);
        if (c == 0)
        {
            svc_handle = n->svc_handle;
            break;
        }
        if (c < 0)
        {
            begin = mid + 1;
        }
        else
        {
            end = mid - 1;
        }
    }

    return svc_handle;
}

// 给服务handle注册命名, 保证注册完s->name的有序
const char* service_context_manager::set_handle_by_name(const char* svc_name, uint32_t svc_handle)
{
    // write lock
    std::unique_lock<std::shared_mutex> wlock(rw_mutex_);

    return _insert_name(svc_name, svc_handle);
}

// 
const char* service_context_manager::_insert_name(const char* svc_name, uint32_t svc_handle)
{
    int begin = 0;
    int end = name_count_ - 1;
    while (begin <= end)
    {
        int mid = (begin + end) / 2;
        handle_name* n = &name_[mid];

        int c = n->svc_name.compare(svc_name);
        // exists
        if (c == 0)
            return nullptr;

        if (c < 0)
        {
            begin = mid + 1;
        }
        else
        {
            end = mid - 1;
        }
    }

    // char* result = skynet_strdup(svc_name);
    // _insert_name_before(result, svc_handle, begin);

    // return result;

    return nullptr;
}


// 把name插入到name数组中，再关联handle
void service_context_manager::_insert_name_before(char* svc_name, uint32_t svc_handle, int before)
{
    if (name_count_ >= name_cap_)
    {
        name_cap_ *= 2;
        assert(name_cap_ <= MAX_SLOT_SIZE);

        handle_name* n = new handle_name[name_cap_];
        for (int i = 0; i < before; i++)
        {
            n[i] = name_[i];
        }
        for (int i = before; i < name_count_; i++)
        {
            n[i+1] = name_[i];
        }
        delete[] name_;
        name_ = n;
    }
    else
    {
        for (int i = name_count_; i > before; i--)
        {
            name_[i] = name_[i-1];
        }
    }
    name_[before].svc_name = svc_name;
    name_[before].svc_handle = svc_handle;
    ++name_count_;
}

uint32_t skynet_query_by_name_or_addr(service_context* svc_ctx, const char* name_or_addr)
{
    // service address
    if (name_or_addr[0] == ':')
    {
        return ::strtoul(name_or_addr + 1, NULL, 16);
    }
    // local service
    else if (name_or_addr[0] == '.')
    {
        return service_context_manager::instance()->find_by_name(name_or_addr + 1);
    }
    // global service
    else
    {
        // not support query global service, just log a message
        log(svc_ctx, "Don't support query global name %s", name_or_addr);
        return 0;
    }
}


}

#include "handle_manager.h"
#include "service_context.h"

#include "../log/log.h"

#include <mutex>

namespace skynet {

// high 8 bits: remote service id
#define HANDLE_MASK                 0xFFFFFF        // handle mask
#define HANDLE_REMOTE_SHIFT         24              // remote service id offset

#define DEFAULT_SLOT_SIZE           4
#define MAX_SLOT_SIZE               0x40000000

handle_manager* handle_manager::instance_ = nullptr;

handle_manager* handle_manager::instance()
{
    static std::once_flag oc;
    std::call_once(oc, [&](){
        instance_ = new handle_manager;
    });

    return instance_;
}

// 
void handle_manager::init()
{
    // 给slot分配slot_size个ctx内存
    slot_size_ = DEFAULT_SLOT_SIZE;
    slot_ = new service_context* [slot_size_] { nullptr };

    handle_index_ = 1;
    name_cap_ = 2;
    name_count_ = 0;

    //
    name_ = new handle_name[name_cap_];

    // Don't need to free H
}

uint32_t handle_manager::registe(service_context* ctx)
{
    // write lock
    std::unique_lock<std::shared_mutex> wlock(rw_mutex_);

    for (;;)
    {
        uint32_t handle = handle_index_;
        for (int i = 0; i < slot_size_; i++, handle++)
        {
            if (handle > HANDLE_MASK)
            {
                // 0 is reserved
                handle = 1;
            }
            int hash = handle & (slot_size_ - 1);
            if (slot_[hash] == nullptr)
            {
                slot_[hash] = ctx;
                handle_index_ = handle + 1;

                return handle;
            }
        }
        assert((slot_size_ * 2 - 1) <= HANDLE_MASK);
        service_context** new_slot = new service_context* [2 * slot_size_];
        ::memset(new_slot, 0, slot_size_ * 2 * sizeof(service_context*));
        for (int i = 0; i < slot_size_; i++)
        {
            int hash = slot_[i]->svc_handle_ & (slot_size_ * 2 - 1);
            assert(new_slot[hash] == nullptr);
            new_slot[hash] = slot_[i];
        }

        delete[] slot_;
        slot_ = new_slot;
        slot_size_ *= 2;
    }
}

// 注销一个服务
int handle_manager::retire(uint32_t svc_handle)
{
    int ret = 0;

    // write lock
    std::unique_lock<std::shared_mutex> wlock(rw_mutex_);

    uint32_t hash = svc_handle & (slot_size_ - 1);
    service_context* ctx = slot_[hash];

    if (ctx != nullptr && ctx->svc_handle_ == svc_handle)
    {
        slot_[hash] = nullptr;
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
        ctx = nullptr;
    }

    wlock.unlock();

     if (ctx != nullptr)
     {
    //     // release ctx may call skynet_handle_* , so wunlock first.
    //     skynet_context_release(ctx);
     }

    return ret;
}

// 注销全部服务
void handle_manager::retire_all()
{
    for (;;)
    {
        int n = 0;
        for (int i = 0; i < slot_size_; i++)
        {
            uint32_t svc_handle = 0;

            // read scope lock
            {
                std::shared_lock<std::shared_mutex> rlock(rw_mutex_);

                service_context* ctx = slot_[i];
                if (ctx != nullptr)
                {
                    svc_handle = ctx->svc_handle_;
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
service_context* handle_manager::grab(uint32_t svc_handle)
{
    service_context* result = nullptr;

    // read lock
    std::shared_lock<std::shared_mutex> rlock(rw_mutex_);

    uint32_t hash = svc_handle & (slot_size_-1);
    service_context* ctx = slot_[hash];
    if (ctx != nullptr && ctx->svc_handle_ == svc_handle)
    {
        result = ctx;
        result->grab();
    }

    return result;
}

// 通过name找handle
// S->name是按handle_name->name升序排序的，通过二分查找快速地查找name对应的handle
uint32_t handle_manager::find_by_name(const char* svc_name)
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
const char* handle_manager::set_handle_by_name(const char* svc_name, uint32_t svc_handle)
{
    // write lock
    std::unique_lock<std::shared_mutex> wlock(rw_mutex_);

    return _insert_name(svc_name, svc_handle);
}

// 
const char* handle_manager::_insert_name(const char* svc_name, uint32_t svc_handle)
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
void handle_manager::_insert_name_before(char* svc_name, uint32_t svc_handle, int before)
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
        return handle_manager::instance()->find_by_name(name_or_addr + 1);
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

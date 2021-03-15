#include "service_manager.h"
#include "service_context.h"

#include "../node/node.h"

#include "../log/log.h"

#include "../mq/mq_msg.h"
#include "../mq/mq_private.h"
#include "../mq/mq_global.h"

#include "../mod/cservice_mod_manager.h"

#include <mutex>

namespace skynet {

// high 8 bits: remote service id
#define HANDLE_MASK                 0x00FFFFFF      // handle mask, high 8bits is harbor id
#define HANDLE_REMOTE_SHIFT         24              // remote service id offset (harbor id)

service_manager* service_manager::instance_ = nullptr;

service_manager* service_manager::instance()
{
    static std::once_flag oc;
    std::call_once(oc, [&](){
        instance_ = new service_manager;
    });

    return instance_;
}

//
bool service_manager::init()
{
    // 给slot分配slot_size个ctx内存
    svc_ctx_slot_size_ = DEFAULT_SLOT_SIZE;
    svc_ctx_slot_ = new service_context* [svc_ctx_slot_size_] { nullptr };

    alloc_svc_handle_seed_ = 1;
    name_cap_ = 2;
    name_count_ = 0;

    //
    name_ = new handle_name[name_cap_];

    svc_count_ = 0;

    return true;
}

service_context* service_manager::create_service(const char* svc_name, const char* param)
{
    // query c service
    cservice_mod* mod = cservice_mod_manager::instance()->query(svc_name);
    if (mod == nullptr)
        return nullptr;

    // create service mod own data block (如: struct snlua, struct logger,  struct gate)
    void* inst = mod->instance_create();
    if (inst == nullptr)
        return nullptr;

    // create service context
    service_context* svc_ctx = new service_context;

    svc_ctx->mod_ = mod;
    svc_ctx->instance_ = inst;
    svc_ctx->ref_ = 2;        // 初始化完成会调用 service_manager::instance()->release_service() 将引用计数-1，ref变成1而不会被释放掉
    svc_ctx->cb_ = nullptr;
    svc_ctx->cb_ud_ = nullptr;
    svc_ctx->session_id_ = 0;
    svc_ctx->log_fd_ = nullptr;

    svc_ctx->is_init_ = false;
    svc_ctx->is_blocked_ = false;

    svc_ctx->cpu_cost_ = 0;
    svc_ctx->cpu_start_ = 0;
    svc_ctx->message_count_ = 0;
    svc_ctx->profile_ = node::instance()->is_profile();

    // Should set to 0 first to avoid unregister_service_all() get an uninitialized handle
    // initialize function maybe use svc_ctx->svc_handle_, so it must init at last
    svc_ctx->svc_handle_ = 0;
    svc_ctx->svc_handle_ = register_service(svc_ctx); // register service context and get service handle

    // initialize service private queue
    mq_private* queue = mq_private::create(svc_ctx->svc_handle_);
    svc_ctx->queue_ = queue;

    // increase service count
    svc_inc();

    // init mod data
    int r = mod->instance_init(inst, svc_ctx, param);

    // service mod initialize success
    if (r == 0)
    {
        service_context* ret = release_service(svc_ctx);
        if (ret != nullptr)
            svc_ctx->is_init_ = true;

        // put service private queue into global mq. service can recv message now.
        mq_global::instance()->push(queue);
        if (ret != nullptr)
            log(ret, "LAUNCH %s %s", svc_name, param ? param : "");

        return ret;
    }
        // service mod initialize failed
    else
    {
        log(svc_ctx, "FAILED launch %s", svc_name);

        uint32_t svc_handle = svc_ctx->svc_handle_;
        release_service(svc_ctx);
        unregister_service(svc_handle);
        // drop_t d = { svc_handle };
        // queue->release(drop_message, &d);

        return nullptr;
    }
}

uint32_t service_manager::register_service(service_context* svc_ctx)
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

int service_manager::unregister_service(uint32_t svc_handle)
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
                continue;
            }
            else if (i != j)
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
         // release service context may call skynet_handle_*, so wunlock first.
         release_service(svc_ctx);
     }

    return ret;
}

// 注销全部服务
void service_manager::unregister_service_all()
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
                if (unregister_service(svc_handle))
                    ++n;
            }
        }

        //
        if (n == 0)
            return;
    }
}

void service_manager::process_blocked_service(uint32_t svc_handle)
{
    service_context* svc_ctx = service_manager::instance()->grab(svc_handle);
    if (svc_ctx != nullptr)
    {
        // mark blocked
        svc_ctx->is_blocked_ = true;
        // try release
        service_manager::instance()->release_service(svc_ctx);
    }
}

// 取得一个服务 (增加服务引用计数)
service_context* service_manager::grab(uint32_t svc_handle)
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
uint32_t service_manager::find_by_name(const char* svc_name)
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
const char* service_manager::set_handle_by_name(const char* svc_name, uint32_t svc_handle)
{
    // write lock
    std::unique_lock<std::shared_mutex> wlock(rw_mutex_);

    return _insert_name(svc_name, svc_handle);
}

uint32_t service_manager::query_by_name_or_addr(service_context* svc_ctx, const char* name_or_addr)
{
    // service address
    if (name_or_addr[0] == ':')
    {
        return ::strtoul(name_or_addr + 1, NULL, 16);
    }
    // local service
    else if (name_or_addr[0] == '.')
    {
        return service_manager::instance()->find_by_name(name_or_addr + 1);
    }
    // global service
    else
    {
        // not support query global service, just log a message
        log(svc_ctx, "Don't support query global name %s", name_or_addr);
        return 0;
    }
}

// 投递服务消息
int service_manager::push_service_message(uint32_t svc_handle, skynet_message* message)
{
    // 增加服务引用计数
    service_context* svc_ctx = grab(svc_handle);
    if (svc_ctx == nullptr)
        return -1;

    // 消息入队
    svc_ctx->queue_->push(message);
    // 减少服务引用计数
    release_service(svc_ctx);

    return 0;
}

// 
const char* service_manager::_insert_name(const char* svc_name, uint32_t svc_handle)
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

    return _insert_name_before(svc_name, svc_handle, begin);
}

// 把name插入到name数组中，再关联handle
const char* service_manager::_insert_name_before(const char* svc_name, uint32_t svc_handle, int before)
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

    return name_[before].svc_name.c_str();
}


// 创建服务ctx
service_context* service_manager::release_service(service_context* svc_ctx)
{
    // need delete service
    if (--svc_ctx->ref_ == 0)
    {
        if (svc_ctx->log_fd_ != nullptr)
        {
            ::fclose(svc_ctx->log_fd_);
        }

        svc_ctx->mod_->instance_release(svc_ctx->instance_);
        svc_ctx->queue_->mark_release();

        delete svc_ctx;
        service_manager::instance()->svc_dec();

        return nullptr;
    }

    return svc_ctx;
}

}

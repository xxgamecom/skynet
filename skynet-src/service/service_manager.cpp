#include "service_manager.h"
#include "service_context.h"

#include "../mod/mod_manager.h"

#include "../node/node.h"

#include "../log/log.h"

#include "../mq/mq_msg.h"
#include "../mq/mq_private.h"
#include "../mq/mq_global.h"

#include <cstring>
#include <mutex>

namespace skynet {

// TODO: delete harbor
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

service_context* service_manager::create_service(const char* svc_name, const char* svc_args)
{
    // query c service mod info
    auto mod_ptr = mod_manager::instance()->query(svc_name);
    if (mod_ptr == nullptr)
    {
        // not exists, try load
        mod_ptr = mod_manager::instance()->load(svc_name);
        if (mod_ptr == nullptr)
            return nullptr;
    }

    // create service mod own data block (如: struct snlua, struct logger,  struct gate)
    cservice* svc_ptr = mod_ptr->create_func_();
    if (svc_ptr == nullptr)
        return nullptr;

    // create service context
    auto svc_ctx = new service_context;

    svc_ctx->svc_mod_ptr_ = mod_ptr;
    svc_ctx->svc_ptr_ = svc_ptr;
    svc_ctx->ref_ = 2;        // 初始化完成会调用 service_manager::instance()->release_service() 将引用计数-1，ref变成1而不会被释放掉
    svc_ctx->msg_callback_ = nullptr;
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
    ++svc_count_;

    // initialize service mod success
    if (svc_ptr->init(svc_ctx, svc_args))
    {
        service_context* ret = release_service(svc_ctx);
        if (ret != nullptr)
            svc_ctx->is_init_ = true;

        // put service private queue into global mq. service can recv message now.
        mq_global::instance()->push(queue);
        if (ret != nullptr)
            log(ret, "LAUNCH %s %s", svc_name, svc_args != nullptr ? svc_args : "");

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
    service_context* svc_ctx = grab(svc_handle);
    if (svc_ctx != nullptr)
    {
        // mark blocked
        svc_ctx->is_blocked_ = true;
        // try release
        release_service(svc_ctx);
    }
}

// 取得一个服务 (增加服务引用计数)
service_context* service_manager::grab(uint32_t svc_handle)
{
    // read lock
    std::shared_lock<std::shared_mutex> rlock(rw_mutex_);

    uint32_t hash = svc_handle & (svc_ctx_slot_size_ - 1);
    service_context* svc_ctx = svc_ctx_slot_[hash];
    if (svc_ctx == nullptr || svc_ctx->svc_handle_ != svc_handle)
        return nullptr;

    //
    svc_ctx->grab();

    return svc_ctx;
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

uint32_t service_manager::query_by_name(service_context* svc_ctx, const char* name_or_addr)
{
    // service address
    if (name_or_addr[0] == ':')
    {
        return ::strtoul(name_or_addr + 1, NULL, 16);
    }
    // local service
    else if (name_or_addr[0] == '.')
    {
        return find_by_name(name_or_addr + 1);
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
int service_manager::push_service_message(uint32_t svc_handle, service_message* message)
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

        svc_ctx->svc_mod_ptr_->release_func_(svc_ctx->svc_ptr_);
        svc_ctx->queue_->mark_release();

        delete svc_ctx;
        --svc_count_;

        return nullptr;
    }

    return svc_ctx;
}

// 发送消息
// ctx之间通过消息进行通信，调用skynet_send向对方发送消息(skynet_sendname最终也会调用skynet_send)。
// @param svc_ctx            源服务的ctx，可以为NULL，drop_message时这个参数为NULL
// @param src_svc_handle         源服务地址，通常设置为0即可，api里会设置成ctx->handle，当context为NULL时，需指定source
// @param dst_svc_handle     目的服务地址
// @param msg_ptype             消息类型， skynet定义了多种消息，SERVICE_MSG_TYPE_TEXT，SERVICE_MSG_TYPE_CLIENT，SERVICE_MSG_TYPE_RESPONSE等（详情见skynet.h）
// @param session         如果在type里设上allocsession的tag(MESSAGE_TAG_ALLOC_SESSION)，api会忽略掉传入的session参数，重新生成一个新的唯一的
// @param msg             消息包数据
// @param msg_sz             消息包长度
// @return int session, 源服务保存这个session，同时约定，目的服务处理完这个消息后，把这个session原样发送回来(skynet_message结构里带有一个session字段)，
//         源服务就知道是哪个请求的返回，从而正确调用对应的回调函数。
int service_manager::send(service_context* svc_ctx, uint32_t src_svc_handle, uint32_t dst_svc_handle , int msg_ptype, int session_id, void* msg, size_t msg_sz)
{
    if ((msg_sz & MESSAGE_TYPE_MASK) != msg_sz)
    {
        log(svc_ctx, "The message to %x is too large", dst_svc_handle);
        if (msg_ptype & MESSAGE_TAG_DONT_COPY)
        {
            delete[] msg;
        }

        // too large
        return -2;
    }

    // 预处理消息数据块
    bool need_copy = (msg_ptype & MESSAGE_TAG_DONT_COPY) == 0;
    bool need_alloc_session = (msg_ptype & MESSAGE_TAG_ALLOC_SESSION) != 0;
    msg_ptype &= 0xff;

    if (need_alloc_session)
    {
        assert(session_id == 0);
        session_id = svc_ctx->new_session();
    }
    if (need_copy && msg != nullptr)
    {
        char* new_msg = new char[msg_sz + 1];
        ::memcpy(new_msg, msg, msg_sz);
        new_msg[msg_sz] = '\0';
        msg = new_msg;
    }
    msg_sz |= (size_t)msg_ptype << MESSAGE_TYPE_SHIFT;

    if (dst_svc_handle == 0)
    {
        if (msg != nullptr)
        {
            log(svc_ctx, "Destination service handle can't be 0");
            delete[] msg;
            return -1;
        }

        return session_id;
    }

    if (src_svc_handle == 0)
        src_svc_handle = svc_ctx->svc_handle_;

    // push message to dst service
    service_message smsg;
    smsg.src_svc_handle = src_svc_handle;
    smsg.session_id = session_id;
    smsg.data_ptr = msg;
    smsg.data_size = msg_sz;
    if (push_service_message(dst_svc_handle, &smsg))
    {
        delete[] msg;
        return -1;
    }

    return session_id;
}

int service_manager::send_by_name(service_context* svc_ctx, uint32_t src_svc_handle, const char* dst_name_or_addr, int msg_ptype, int session, void* msg, size_t sz)
{
    if (src_svc_handle == 0)
        src_svc_handle = svc_ctx->svc_handle_;

    uint32_t des = 0;
    // service address
    if (dst_name_or_addr[0] == ':')
    {
        des = ::strtoul(dst_name_or_addr + 1, nullptr, 16);
    }
    // local service
    else if (dst_name_or_addr[0] == '.')
    {
        des = find_by_name(dst_name_or_addr + 1);
        if (des == 0)
        {
            if (msg_ptype & MESSAGE_TAG_DONT_COPY)
            {
                delete[] msg;
            }
            return -1;
        }
    }

    return send(svc_ctx, src_svc_handle, des, msg_ptype, session, msg, sz);
}

}

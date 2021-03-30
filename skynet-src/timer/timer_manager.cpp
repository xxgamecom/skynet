/**
 * skynet定时器
 * 
 * 设计原理: 参考了linux内核动态定时器的机制, http://www.cnblogs.com/leaven/archive/2010/08/19/1803382.html
 * 假定一个定时器要经过 interval 个时钟滴答后才到期 (interval = expires - jiffies), 则linux采用了下列思想来实现其动态内核定时器机制:
 * 1. 对于那些 0 <= interval <= 255 的定时器, linux严格按照定时器向量的基本语义来组织这些定时器, 
 *    也即linux内核最关心那些在接下来的255个时钟节拍内到期的定时器, 因此将它们按照各自不同的expires值组织成256个定时器向量。
 * 2. 而对于 256 <= interval <= 0xffffffff 的定时器, 由于它们离到期还有段时间, 因此内核并不关心它们,
 *    而是将它们以一种扩展的定时器向量语义(或称为"松散的定时器向量语义")进行组织。 即各定时器的expires值可以互不相同的一个定时器队列。
 * 
 * skynet定时器实现:
 * 1. skynet的时间精度被设计为 0.01秒 (即: 百分之一秒, 10ms), 也就是说skynet内的1个时间滴答为0.01秒, 1秒为100滴答, 这对于游戏服务器来说已经足够了。
 * 2. skynet为每个定时器设置一个到期的滴答数, 与当前系统的滴答数(启动时间0, 然后1滴答1滴答的往前进)比较差值。
 * 3. 如果 interval 的值较小 (0 <= interval <= 2^8 -1), 表示定时器即将到来, 需要严格关注, 把它们保持在 2^8 个定时器链表里。
 * 4. 如果 interval 的值较大, 表示定时器越远, 可以不用关注, 划分为4个等级, 2^8 <= interval <= 2^(8+6)-1, 2^8 <= interval <= 2^(8+6+6)-1, ...,
 *    每个等级只需要2^6个定时器链表保存, 比如对于 2^8 <= interval <= 2^(8+6)-1 的定时器, 将 interval >> 8 相同的值idx保存在第一个等级位置为idx的链表里。
 *    这样做的优势是: 不用为每一个interval创建一个链表, 而只需要 2^8 + 4*(2^6)个链表, 大大节省了内存。
 * 5. 之后, 在不同的情况下, 分配不同等级的定时器, 等级越高, 表示越遥远, 需要重新分配的次数越少。
 * 
 * 每2.5毫秒就更新一下timer中的时间 (2.5ms调用一次skynet_updatetime), 具体参考: thread_timer 线程函数
 * 
 */

#include "timer_manager.h"
#include "timer.h"

#include "../memory/skynet_malloc.h"

#include "../mq/mq_msg.h"
#include "../mq/mq_private.h"

#include "../log/log.h"

#include "../service/service_context.h"
#include "../service/service_manager.h"

#include "../utils/time_helper.h"

#include <ctime>
#include <cassert>

namespace skynet {

//
typedef void (*timer_execute_func)(void* ud, void* arg);

// 每次更新都会对一个叫time的计数器做加1操作，所以这个计数器其实可以当作时间来看待

//
struct timer_event
{
    uint32_t            svc_handle;                 // source service handle, which service set the timer.
                                                    // it also the target of sending timeout messages.

    int                 session;                    // a self increasing id. todo: check this, this means context call session_id?
                                                    // when overflowing, restart with 1, so don't set a timer that takes a long time.
};

// create a timer
static timer* create_timer()
{
    auto r = new timer;

    for (int i = 0; i < TIME_NEAR; i++)
    {
        link_clear(&r->near[i]);
    }

    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < TIME_LEVEL; j++)
        {
            link_clear(&r->t[i][j]);
        }
    }

    r->current = 0;

    return r;
}

// add a timer
static void timer_add(timer* t, void* arg, size_t sz, int time)
{
     timer_node* node = (timer_node*)skynet_malloc(sizeof(timer_node) + sz);
     ::memcpy(node + 1, arg, sz);

     //
     std::lock_guard<std::mutex> lock(t->mutex);

     node->expire = time + t->time;
     add_node(t, node);
}

// 重新分配定时器所在区间
// 每帧除了触发定时器外，还需重新分配定时器所在区间, 因为T->near里保存即将触发的定时器, 所以每TIME_NEAR-1（2^8-1）个滴答数才有可能需要分配。否则，分配T->t中某个等级即可
// 当T->time的低8位不全为0时，不需要分配，所以每2^8个滴答数才有需要分配一次；
// 当T->time的第9-14位不全为0时，重新分配T[0]等级，每2^8个滴答数分配一次，idx从1开始，每次分配+1；
// 当T->time的第15-20位不全为0时，重新分配T[1]等级，每2^(8+6)个滴答数分配一次，idx从1开始，每次分配+1；
// 当T->time的第21-26位不全为0时，重新分配T[2]等级，每2^(8+6+6)个滴答数分配一次，idx从1开始，每次分配+1；
// 当T->time的第27-32位不全为0时，重新分配T[3]等级，每2^(8+6+6+6)个滴答数分配一次，idx从1开始，每次分配+1；
// 即等级越大的定时器越遥远，越不关注，需要重新分配的次数也就越少。
static void timer_shift(struct timer* T)
{
    int mask = TIME_NEAR;
    uint32_t ct = ++T->time;
    if (ct == 0)
    {
        move_list(T, 3, 0);
    }
    else
    {
        uint32_t time = ct >> TIME_NEAR_SHIFT;
        int i = 0;

        while ((ct & (mask-1)) == 0)
        {
            int idx = time & TIME_LEVEL_MASK;
            if (idx != 0)
            {
                move_list(T, i, idx);
                break;
            }
            mask <<= TIME_LEVEL_SHIFT;
            time >>= TIME_LEVEL_SHIFT;
            ++i;
        }
    }
}

// 派发定时器事件
static inline void dispatch_list(timer_node* current)
{
    do
    {
        timer_event* event = (timer_event*)(current + 1);
        service_message msg;
        msg.src_svc_handle = 0;
        msg.session_id = event->session;
        msg.data_ptr = nullptr;
        msg.data_size = (size_t)SERVICE_MSG_TYPE_RESPONSE << MESSAGE_TYPE_SHIFT;

        service_manager::instance()->push_service_message(event->svc_handle, &msg);

        timer_node* temp = current;
        current = current->next;
        skynet_free(temp);
    } while (current != nullptr);
}

// 执行定时器
static inline void timer_execute(timer* T)
{
    int idx = T->time & TIME_NEAR_MASK;

    while (T->near[idx].head.next != nullptr)
    {
        timer_node* current = link_clear(&T->near[idx]);

        T->mutex.unlock();

        // dispatch_list don't need lock T
        dispatch_list(current);

        T->mutex.lock();
    }
}

// 更新定时器
static void timer_update(timer* T)
{
    T->mutex.lock();

    // try to dispatch timeout 0 (rare condition)
    timer_execute(T);
    // shift time first, and then dispatch timer message
    timer_shift(T);
    //
    timer_execute(T);

    T->mutex.unlock();
}

// 获取当前系统时间 (tick: 1 tick = 10ms)
// centisecond: 1/100 second
// @param sec 当前秒数
// @param cs 剩余滴答数
static void systime(uint32_t* sec, uint32_t* cs)
{
    struct timespec ti;
    ::clock_gettime(CLOCK_REALTIME, &ti);   // CLOCK_REALTIME: 系统实时时间, 随系统实时时间改变而改变, 即从UTC1970-1-1 0:0:0开始计时
    *sec = (uint32_t)ti.tv_sec;                     // 秒数部分
    *cs = (uint32_t)(ti.tv_nsec / 10000000);        // 百分之一秒部分, 转纳秒数为百分之一秒 = 纳秒 / 10000000
}

timer_manager* timer_manager::instance_ = nullptr;

timer_manager* timer_manager::instance()
{
    static std::once_flag oc;
    std::call_once(oc, [&]() { instance_ = new timer_manager; });

    return instance_;
}

void timer_manager::init()
{
    TI_ = create_timer();

    uint32_t current = 0;
    systime(&TI_->start_seconds, &current);

    TI_->current = current;
    TI_->current_tick = time_helper::get_time_tick();
}

// 创建定时操作
// @param handle
// @param time <=0时, 说明是需要立即执行, 无需创建定时器节点
//              >0时, 说明需要定时执行, 创建一个定时器节点, 后续定时触发操作
// @param session
int timer_manager::timeout(uint32_t handle, int time, int session)
{
    // time<=0说明是立即发送消息, 无需定时处理
    if (time <= 0)
    {
        service_message msg;
        msg.src_svc_handle = 0;
        msg.session_id = session;
        msg.data_ptr = nullptr;
        msg.data_size = (size_t)SERVICE_MSG_TYPE_RESPONSE << MESSAGE_TYPE_SHIFT;

        if (service_manager::instance()->push_service_message(handle, &msg))
        {
            return -1;
        }
    }
    // 定时发送消息, 增加一个定时器, 后续定时触发
    else
    {
        timer_event event;
        event.svc_handle = handle;
        event.session = session;
        timer_add(TI_, &event, sizeof(event), time);
    }

    return session;
}

// 刷新进程时间, 在定时器线程中定期执行, 执行频率: 2.5ms
void timer_manager::update_time()
{
    // current ticks
    uint64_t ct = time_helper::get_time_tick();

    //
    if (ct < TI_->current_tick)
    {
        log_error(nullptr, fmt::format("time diff error: change from {} to {}", ct, TI_->current_tick));
        TI_->current_tick = ct;
    }
    //
    else if (ct != TI_->current_tick)
    {
        // 距离上次执行的时间差
        uint32_t diff = (uint32_t)(ct - TI_->current_tick);
        TI_->current_tick = ct;  // 记录当前执行时间点
        TI_->current += diff;    // 更新当前时间
        
        // 更新定时器
        for (int i = 0; i < diff; i++)
        {
            timer_update(TI_);
        }
    }
}

// 返回当前进程启动后经过的时间 (0.01 秒)
uint64_t timer_manager::now()
{
    return TI_->current;
}

// 返回当前进程的启动 UTC 时间（秒）
uint32_t timer_manager::start_seconds()
{
    return TI_->start_seconds;
}

}


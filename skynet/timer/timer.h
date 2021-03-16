#pragma once

#include <cstdint>
#include <mutex>

namespace skynet {

// 到期时间较小的定时器 (滴答数较小 0~256), 即即将触发的定时器, 保持在 2^8 个定时器链表里
#define TIME_NEAR_SHIFT         8
#define TIME_NEAR               (1 << TIME_NEAR_SHIFT)          // 2^8

// 到期时间较大的定时器 (滴答数较大 >256), 表示定时器较远, 可以不用关注, 划分为4个等级, 2^8~2^(8+6)-1, 2^(8+6)-1~2^(8+6+6)-1, ... 
// 每个等级只需要 2^6 个定时器链表保存
#define TIME_LEVEL_SHIFT        6                               //
#define TIME_LEVEL              (1 << TIME_LEVEL_SHIFT)         // 2^6
#define TIME_NEAR_MASK          (TIME_NEAR-1)                   // 2^8 - 1
#define TIME_LEVEL_MASK         (TIME_LEVEL-1)                  // 2^6 - 1

// 定时器节点
struct timer_node
{
    timer_node*                 next = nullptr;                 // next timer node ptr
    uint32_t                    expire = 0;                     // expire ticks (the number of ticks since skynet node startup)
};

// 定时器链表
struct link_list
{
    timer_node                  head;                           //
    timer_node*                 tail = nullptr;                 //
};

// 有四个级别的定时器数组，这些数组在timer_shift中被不断地重新调整优先级，
// 直到移动到near数组中。四个级别分别是0，1，2，3，级别越大，expire也就越大，也就是超时时间越大

// 定时器信息
struct timer
{
    link_list                   near[TIME_NEAR];                // 临近的定时器数组
    link_list                   t[4][TIME_LEVEL];               // 四个级别的定时器数组

    std::mutex                  mutex;
    
    uint32_t                    time = 0;                       // 启动到现在走过的滴答数，等同于current
    uint32_t                    start_time = 0;                 // 节点开始运行的时间点，timestamp，秒数
    uint64_t                    current = 0;                    // 节点已经运行的时间, 滴答数
    uint64_t                    current_point = 0;              // 当前时间, 滴答数
};


timer_node* link_clear(link_list* list);
void link(link_list* list, timer_node* node);
void move_list(timer* T, int level, int idx);
void add_node(timer* T, timer_node* node);

}



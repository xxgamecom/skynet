#pragma once

#include "../core/object_pool.h"
#include "../core/io_buffer.h"

#include <cstdint>
#include <deque>
#include <mutex>

namespace skynet { namespace network {

// tcp会话内部写消息队列, 线程不是完全安全(用于发送大数据时进行切片用)
// 这里的内部保护只保证1个线程写的情况是安全的, 底层永远只有一个线程读(ios的IO线程)
// 因此, 外部有多个写的情况下, 外部需要自己加锁保护来保证外部逻辑上是原子的
class tcp_session_write_queue
{
private:
    bool                                        is_inited_ = false;                 // 写消息队列是否就绪

    std::deque<std::shared_ptr<io_buffer>>      write_queue_;                       // 写消息队列
    size_t                                      write_queue_size_ = 0;              // 写消息队列元素数量
    std::mutex                                  write_queue_mutex_;                 // 写消息队列保护

    size_t                                      write_msg_buf_size_ = 0;            // 单个写消息缓存大小(存放到写消息队列中)
    std::shared_ptr<object_pool<io_buffer>>   write_msg_buf_pool_ptr_;            // 写消息缓存池

public:
    tcp_session_write_queue() = default;
    ~tcp_session_write_queue() = default;

public:
    // 初始化
    bool init(size_t write_msg_buf_size, size_t write_msg_queue_size);
    // 清理
    void clear();

public:
    // 尾部压入数据(发送大数据时, 内部会进行切片后入列)
    size_t push_back(const char* data_ptr, size_t data_len);
    // 弹出头部数据(发送完后回收, 一次push可能会触发多个pop, 因为内部对大数据切片了)
    void pop_front();
    // 取头部数据(不弹出数据, 弹出数据需要调用pop)
    std::shared_ptr<io_buffer> front();

public:
    // 队列是否就绪
    bool is_inited();
    // 写队列是否已满
    bool is_full();
    // 写队列是否已空
    bool is_empty();
};

} }

#include "tcp_session_write_queue.inl"


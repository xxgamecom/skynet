#include "tcp_session_write_queue.h"

namespace skynet { namespace net { namespace impl {

// 初始化
bool tcp_session_write_queue::init(size_t write_msg_buf_size, size_t write_msg_queue_size)
{
    // 只有在大小不同的情况才重新分配写消息缓存
    if (write_msg_buf_size_ != write_msg_buf_size ||
        write_queue_size_ != write_msg_queue_size)
    {
        write_msg_buf_size_ = write_msg_buf_size;
        write_queue_size_   = write_msg_queue_size;

        write_msg_buf_pool_ptr_ = std::make_shared<object_pool<io_buffer>>(write_queue_size_, write_msg_buf_size_);
        if (write_msg_buf_pool_ptr_ == nullptr)
            return false;
    }

    is_inited_ = true;

    return true;
}

// 清理
void tcp_session_write_queue::clear()
{
    std::lock_guard<std::mutex> guard(write_queue_mutex_);

    // 回收所有缓存, 并清空写消息队列
    for (auto& itr : write_queue_)
    {
        if (itr != nullptr)
        {
            write_msg_buf_pool_ptr_->free(itr);
        }
    }
    write_queue_.clear();
}

// 尾部压入数据(发送大数据时, 内部会进行切片后入列)
size_t tcp_session_write_queue::push_back(const char* data_ptr, size_t data_len)
{
    assert(write_queue_size_ > 0);
    assert(write_msg_buf_size_ > 0 && write_msg_buf_pool_ptr_ != nullptr);

    // 确保发送数据有效
    assert(data_ptr != nullptr && data_len > 0);
    if (data_ptr == nullptr || data_len <= 0)
        return 0;

    // 数据写入保护
    std::lock_guard<std::mutex> guard(write_queue_mutex_);

    // 写队列已满, 不能再写
    if (write_queue_.size() >= write_queue_size_) return 0;

    // 数据切片
    char* tmp_data_ptr = const_cast<char*>(data_ptr);
    size_t slice_count = data_len / write_msg_buf_size_;
    size_t left_size   = data_len % write_msg_buf_size_;
    size_t write_size = 0;
    bool is_all_write = true;

    // 切片入列
    std::shared_ptr<io_buffer> buf_ptr;
    for (size_t i=0; i<slice_count; ++i)
    {
        buf_ptr = write_msg_buf_pool_ptr_->alloc();
        if (buf_ptr == nullptr)
        {
            is_all_write = false;
            break;
        }

        // 写入缓存
        buf_ptr->data(tmp_data_ptr, write_msg_buf_size_);
        write_queue_.push_back(buf_ptr);

        tmp_data_ptr += write_msg_buf_size_;
        write_size   += write_msg_buf_size_;
    }

    // 前面的切片全写完了, 剩余数据入列
    if (is_all_write && left_size > 0)
    {
        buf_ptr = write_msg_buf_pool_ptr_->alloc();
        if (buf_ptr != nullptr)
        {
            buf_ptr->data(tmp_data_ptr, left_size);
            write_queue_.push_back(buf_ptr);
            write_size += left_size;
        }
    }

    return write_size;
}

} } }


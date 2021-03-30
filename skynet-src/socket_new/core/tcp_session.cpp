#include "tcp_session.h"

namespace skynet { namespace network {

tcp_session::tcp_session(int32_t msg_read_buf_size,
                         int32_t msg_write_buf_size,
                         int32_t msg_write_queue_size)
    :
    msg_read_buf_size_(msg_read_buf_size),
    msg_write_buf_size_(msg_write_buf_size),
    msg_write_queue_size_(msg_write_queue_size),
    read_bytes_(0),
    write_bytes_(0),
    delta_read_bytes_(0),
    delta_write_bytes_(0)
{
    assert(msg_read_buf_size_ > 0);
    assert(msg_write_buf_size_ > 0);
    assert(msg_write_queue_size_ > 0);
}

// 打开会话
bool tcp_session::open(std::shared_ptr<io_service> ios_ptr,
                       std::string local_ip/* = ""*/,
                       uint16_t local_port/* = 0*/)
{
    // 确保会话是关闭状态
    assert(state_ == SESSION_STATE_CLOSE);
    if (state_ != SESSION_STATE_CLOSE)
        return false;

    // 创建读消息缓存, 写消息队列
    if (msg_read_buf_ptr_ == nullptr)
    {
        msg_read_buf_ptr_ = std::make_shared<io_buffer>(msg_read_buf_size_);
        if (msg_read_buf_ptr_ == nullptr)
            return false;
    }
    if (msg_write_queue_.is_inited() == false)
    {
        if (msg_write_queue_.init(msg_write_buf_size_, msg_write_queue_size_) == false)
            return false;
    }

    // socket不重用, 使用外部传入的ios重建
    // 因为重用的socket其底层的ios会导致线程负载不均匀
    socket_ptr_ = std::make_shared<asio::ip::tcp::socket>(ios_ptr->get_raw_ios());
    if (socket_ptr_ == nullptr)
        return false;

    // 这里实际只有客户端才会进行绑定, 服务端是不会进行绑定
    // 服务端绑定会出错, 因为accept时不允许socket为打开状态
    if (local_ip != "" || local_port != 0)
    {
        asio::error_code ec;

        // open
        if (socket_ptr_->open(asio::ip::tcp::v4(), ec))
            return false;

        // bind
        asio::ip::tcp::endpoint local_endpoint(asio::ip::address::from_string(local_ip, ec), local_port);
        if (socket_ptr_->bind(local_endpoint, ec))
            return false;
    }

    // 重置IO统计量
    read_bytes_ = 0;
    write_bytes_ = 0;
    delta_read_bytes_ = 0;
    delta_write_bytes_ = 0;

    // 更新读写时间
    last_read_time_ = time_helper::steady_now();
    last_write_time_ = time_helper::steady_now();

    // 会话状态切换到 '打开'
    state_ = SESSION_STATE_OPEN;

    return true;
}

// 关闭会话(is_force: 是否立即关闭, 如果立即关闭不等待剩余数据发送完毕)
void tcp_session::close(bool is_force/* = true*/)
{
    // 不立即关闭
    if (is_force == false)
    {
        // 无数据则立即关闭
        if (msg_write_queue_.is_empty())
            is_force = true;
        else
            state_ = SESSION_STATE_CLOSING;
    }

    // 立即关闭
    if (is_force)
    {
        // 会话状态切到 '关闭中'
        state_ = SESSION_STATE_CLOSING;

        // 关闭socket
        if (socket_ptr_ != nullptr && socket_ptr_->is_open())
        {
            asio::error_code ec;
            socket_ptr_->shutdown(asio::ip::tcp::socket::shutdown_both, ec);
            socket_ptr_->close(ec);

            // 通知外部会话关闭
            if (event_handler_ptr_ != nullptr)
                event_handler_ptr_->handle_sessoin_close(shared_from_this());
        }

        remote_addr_ = "";
        remote_port_ = 0;
        session_id_ = INVALID_SESSION_ID;

        // 清理回写消息队列
        msg_write_queue_.clear();

        state_ = SESSION_STATE_CLOSE;
    }
}

// 写入数据, 返回写入的字节数(异步写入, 写入成功后通过handle_session_write回调)
// 这里的内部保护只保证1个线程写的情况是安全的, 具体查看tcp_session_write_queue的说明
size_t tcp_session::write(const char* data_ptr, size_t data_len)
{
    // 确保只有在'打开'状态能写
    if (state_ != SESSION_STATE_OPEN)
        return 0;

    bool is_empty_before_write = msg_write_queue_.is_empty();
    size_t write_bytes = msg_write_queue_.push_back(data_ptr, data_len);

    // 写之前队列空, 需要投递一次异步写IO操作
    if (is_empty_before_write)
        async_write_once();

    return write_bytes;
}

// 用于外部对会话进行闲置测试(type为要检测的闲置类型, check_seconds为判断闲置的时间(秒))
void tcp_session::check_idle(idle_type check_type, int32_t check_seconds)
{
    // 只有在'打开'状态才进行闲置检测
    if (state_ != SESSION_STATE_OPEN) return;

    // 获取当前时间和最近读写时间差值
    auto now = time_helper::steady_now();
    int64_t read_idle_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - last_read_time_).count();
    int64_t write_idle_seconds = std::chrono::duration_cast<std::chrono::seconds>(now - last_write_time_).count();

    bool is_idle = false;
    switch (check_type)
    {
    case IDLE_TYPE_READ:
        is_idle = (read_idle_seconds > check_seconds);
        break;
    case IDLE_TYPE_WRITE:
        is_idle = (write_idle_seconds > check_seconds);
        break;
    case IDLE_TYPE_BOTH:
        is_idle = (read_idle_seconds > check_seconds && write_idle_seconds > check_seconds);
        break;
    default:
        assert(false);
    }

    // 闲置时回调给外部处理
    if (is_idle)
    {
        if (event_handler_ptr_ != nullptr)
            event_handler_ptr_->handle_session_idle(shared_from_this(), check_type);
    }
}

// socket选项
bool tcp_session::set_sock_option(sock_options opt, int32_t value)
{
    // socket没有打开
    if (socket_ptr_ == nullptr || socket_ptr_->is_open() == false) return false;

    asio::error_code ec;
    switch (opt)
    {
    case SOCK_OPT_RECV_BUFFER:
        socket_ptr_->set_option(asio::ip::tcp::socket::receive_buffer_size(value), ec);
        break;
    case SOCK_OPT_SEND_BUFFER:
        socket_ptr_->set_option(asio::ip::tcp::socket::send_buffer_size(value), ec);
        break;
    case SOCK_OPT_KEEPALIVE:
        socket_ptr_->set_option(asio::ip::tcp::socket::keep_alive(value != 0), ec);
        break;
    case SOCK_OPT_NODELAY:
        socket_ptr_->set_option(asio::ip::tcp::no_delay(value != 0), ec);
        break;
    case SOCK_OPT_LINGER:
        socket_ptr_->set_option(asio::ip::tcp::socket::linger((value <= 0 ? false : true), (value <= 0 ? 0 : value)), ec);
        break;
    default:
        return false;
    }

    return (!ec ? true : false);
}

bool tcp_session::get_sock_option(sock_options opt, int32_t& value)
{
    // socket没有打开
    if (socket_ptr_ == nullptr || socket_ptr_->is_open() == false) return false;

    value = 0;
    asio::error_code ec;
    switch (opt)
    {
    case SOCK_OPT_RECV_BUFFER:
    {
        asio::ip::tcp::socket::receive_buffer_size opt;
        socket_ptr_->get_option(opt, ec);
        if (!ec) value = opt.value();
    }
        break;
    case SOCK_OPT_SEND_BUFFER:
    {
        asio::ip::tcp::socket::send_buffer_size opt;
        socket_ptr_->get_option(opt, ec);
        if (!ec) value = opt.value();
    }
        break;
    case SOCK_OPT_KEEPALIVE:
    {
        asio::ip::tcp::socket::keep_alive opt;
        socket_ptr_->get_option(opt, ec);
        if (!ec) value = opt.value();
    }
        break;
    case SOCK_OPT_NODELAY:
    {
        asio::ip::tcp::no_delay opt;
        socket_ptr_->get_option(opt, ec);
        if (!ec) value = opt.value();
    }
        break;
    case SOCK_OPT_LINGER:
    {
        asio::ip::tcp::socket::linger opt;
        socket_ptr_->get_option(opt, ec);
        if (!ec) value = opt.timeout();
    }
        break;
    default:
        return false;
    }

    return (!ec ? true : false);
}

// 处理完成的读操作
void tcp_session::handle_async_read(const asio::error_code& ec, size_t bytes_transferred)
{
    // 数据读取成功
    if (!ec)
    {
        // 更新读字节计数
        read_bytes_ += bytes_transferred;
        delta_read_bytes_ += bytes_transferred;

        // 更新读时间
        last_read_time_ = time_helper::steady_now();

        if (bytes_transferred > 0)
        {
            // 设置读缓存数据大小
            msg_read_buf_ptr_->data_size(bytes_transferred);

            // 数据外部处理
            if (event_handler_ptr_ != nullptr)
                event_handler_ptr_->handle_session_read(shared_from_this(),
                                                        msg_read_buf_ptr_->data(),
                                                        msg_read_buf_ptr_->data_size());

            // '打开' 状态都继续投递异步读
            if (state_ == SESSION_STATE_OPEN)
                async_read_once();
        }
            // 对端关闭
        else
        {
            // 关闭会话
            close();
        }
    }
        // 数据读取出错
    else
    {
        // 关闭会话
        close();
    }
}

// 处理完成的写操作
void tcp_session::handle_async_write(const asio::error_code& ec, size_t bytes_transferred)
{
    if (!ec)
    {
        // 更新写字节计数
        write_bytes_ += bytes_transferred;
        delta_write_bytes_ += bytes_transferred;

        // 更新写时间
        last_write_time_ = time_helper::steady_now();

        std::shared_ptr<io_buffer> buf_ptr = msg_write_queue_.front();

        // 写数据回调, 这里需要确认外部是否需要数据?
        if (event_handler_ptr_ != nullptr)
            event_handler_ptr_->handle_session_write(shared_from_this(),
                                                     buf_ptr == nullptr ? nullptr : buf_ptr->data(),
                                                     bytes_transferred);

        // 弹出已写数据
        msg_write_queue_.pop_front();

        // 队列还有数据, 继续发送
        if (msg_write_queue_.is_empty() == false)
            async_write_once();
    }
    else
    {
        // 弹出要写的数据
        msg_write_queue_.pop_front();
        // 关闭会话
        close();
    }
}

}}


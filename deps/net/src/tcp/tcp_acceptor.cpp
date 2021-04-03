#include "tcp_acceptor.h"
#include "tcp_session.h"

namespace skynet::net::impl {

tcp_acceptor_impl::tcp_acceptor_impl(std::shared_ptr<io_service> ios_ptr,
                                     std::shared_ptr<tcp_acceptor_handler> event_handler_ptr/* = nullptr*/)
:
ios_ptr_(ios_ptr),
event_handler_ptr_(event_handler_ptr)
{
}

// 打开acceptor
bool tcp_acceptor_impl::open(const std::string local_ip,
                             uint16_t local_port,
                             bool is_reuse_addr/* = true*/,
                             int32_t backlog/* = DEFAULT_BACKLOG*/)
{
    // 之前的acceptor没有关闭
    assert(acceptor_ptr_ == nullptr);
    if (acceptor_ptr_ != nullptr)
        return false;

    // acceptor端口不能为空
    assert(local_port != 0);
    if (local_port == 0)
        return false;

    // ios必须有效
    assert(ios_ptr_ != nullptr);
    if (ios_ptr_ == nullptr)
        return false;

    bool is_ok = false;
    do
    {
        // create acceptor
        acceptor_ptr_ = std::make_shared<asio::ip::tcp::acceptor>(ios_ptr_->get_raw_ios());
        if (acceptor_ptr_ == nullptr) break;

        asio::error_code ec;

        // open
        if (acceptor_ptr_->open(asio::ip::tcp::v4(), ec))
            break;

        // reuse address
        if (acceptor_ptr_->set_option(asio::ip::tcp::acceptor::reuse_address(is_reuse_addr), ec))
            break;

        // bind
        asio::ip::tcp::endpoint local_endpoint(asio::ip::address::from_string(local_ip, ec), local_port);
        if (acceptor_ptr_->bind(local_endpoint, ec))
            break;

        // listen
        if (acceptor_ptr_->listen(backlog, ec))
            break;

        is_ok = true;
    } while (0);

    if (is_ok == false)
    {
        close();
    }

    return is_ok;
}

// 关闭acceptor
void tcp_acceptor_impl::close()
{
    if (acceptor_ptr_ != nullptr)
    {
        asio::error_code ec;
        acceptor_ptr_->close(ec);
        acceptor_ptr_.reset();
    }
}

// 投递一次异步accept
void tcp_acceptor_impl::accept_once(std::shared_ptr<tcp_session> session_ptr)
{
    assert(session_ptr != nullptr && session_ptr->is_open() == false);
    assert(acceptor_ptr_ != nullptr && acceptor_ptr_->is_open());

    auto self(shared_from_this());
    acceptor_ptr_->async_accept(*(session_ptr->get_socket()), [this, self, session_ptr](const asio::error_code& ec) {
        handle_async_accept(session_ptr, ec);
    });

}

// socket选项
bool tcp_acceptor_impl::set_sock_option(sock_options opt, int32_t value)
{
    // socket没有打开
    if (acceptor_ptr_ == nullptr || acceptor_ptr_->is_open() == false) return false;

    asio::error_code ec;
    switch (opt)
    {
    case SOCK_OPT_RECV_BUFFER:
        acceptor_ptr_->set_option(asio::ip::tcp::acceptor::receive_buffer_size(value), ec);
        break;
    case SOCK_OPT_SEND_BUFFER:
        acceptor_ptr_->set_option(asio::ip::tcp::acceptor::send_buffer_size(value), ec);
        break;
    case SOCK_OPT_KEEPALIVE:
        acceptor_ptr_->set_option(asio::ip::tcp::acceptor::keep_alive(value != 0), ec);
        break;
    case SOCK_OPT_NODELAY:
        acceptor_ptr_->set_option(asio::ip::tcp::no_delay(value != 0), ec);
        break;
    case SOCK_OPT_LINGER:
        acceptor_ptr_->set_option(asio::ip::tcp::acceptor::linger((value <= 0 ? false : true), (value <= 0 ? 0 : value)), ec);
        break;
    default:
        return false;
    }

    return (!ec ? true : false);
}

bool tcp_acceptor_impl::get_sock_option(sock_options opt, int32_t& value)
{
    // socket没有打开
    if (acceptor_ptr_ == nullptr || acceptor_ptr_->is_open() == false) return false;

    value = 0;
    asio::error_code ec;
    switch (opt)
    {
    case SOCK_OPT_RECV_BUFFER:
    {
        asio::ip::tcp::acceptor::receive_buffer_size opt;
        acceptor_ptr_->get_option(opt, ec);
        if (!ec) value = opt.value();
    }
        break;
    case SOCK_OPT_SEND_BUFFER:
    {
        asio::ip::tcp::acceptor::send_buffer_size opt;
        acceptor_ptr_->get_option(opt, ec);
        if (!ec) value = opt.value();
    }
        break;
    case SOCK_OPT_KEEPALIVE:
    {
        asio::ip::tcp::acceptor::keep_alive opt;
        acceptor_ptr_->get_option(opt, ec);
        if (!ec) value = opt.value();
    }
        break;
    case SOCK_OPT_NODELAY:
    {
        asio::ip::tcp::no_delay opt;
        acceptor_ptr_->get_option(opt, ec);
        if (!ec) value = opt.value();
    }
        break;
    case SOCK_OPT_LINGER:
    {
        asio::ip::tcp::acceptor::linger opt;
        acceptor_ptr_->get_option(opt, ec);
        if (!ec) value = opt.timeout();
    }
        break;
    default:
        return false;
    }

    return (!ec ? true : false);
}

// 处理接收
void tcp_acceptor_impl::handle_async_accept(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec)
{
    if (!ec)
    {
        assert(session_ptr != nullptr && session_ptr->is_open());

        // 接受连接成功回调
        if (event_handler_ptr_ != nullptr)
            event_handler_ptr_->handle_accept_success(shared_from_this(), session_ptr);
    }
    else
    {
        // 接受连接失败回调
        if (event_handler_ptr_ != nullptr)
            event_handler_ptr_->handle_accept_failed(shared_from_this(), session_ptr, ec.value(), ec.message());
    }
}

}


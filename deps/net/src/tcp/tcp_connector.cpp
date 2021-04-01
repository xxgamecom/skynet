#include "tcp_connector.h"
#include "tcp_session.h"

namespace skynet::net::impl {

tcp_connector_impl::tcp_connector_impl(std::shared_ptr<io_service> ios_ptr)
:
ios_ptr_(ios_ptr),
resolver_(ios_ptr->get_raw_ios())
{
}

bool tcp_connector_impl::connect(std::shared_ptr<tcp_session> session_ptr,
                                 const std::string remote_addr,
                                 uint16_t remote_port,
                                 int32_t timeout_seconds/* = 0*/,
                                 const std::string local_ip/* = ""*/,
                                 uint16_t local_port/* = 0*/)
{
    // 检查会话, 确保处于关闭状态
    assert(session_ptr != nullptr && session_ptr->is_open() == false);
    if (session_ptr == nullptr || session_ptr->is_open())
        return false;

    // 检查地址
    assert(!remote_addr.empty() && remote_port != 0);
    if (remote_addr.empty() || remote_port == 0)
        return false;

    // 正在连接
    if (is_connecting_)
        return false;

    // 设置连接状态
    is_connecting_ = true;

    // 打开会话(创建会话socket, bind等)
    if (session_ptr->open(ios_ptr_, local_ip, local_port) == false)
    {
        session_ptr->close();
        return false;
    }

    // 启动连接超时处理
    if (start_connect_timer(session_ptr, timeout_seconds) == false)
    {
        session_ptr->close();
        return false;
    }

    // address resolve, only support use the first address to connect now.
    // todo: try to connect with other address when the connection is unsuccessful.
    asio::ip::tcp::resolver::query query(remote_addr, std::to_string(static_cast<uint32_t>(remote_port)));
    resolver_.async_resolve(query, std::bind(&tcp_connector_impl::handle_async_resolve, shared_from_this(),
                                             session_ptr, std::placeholders::_1, std::placeholders::_2));

    return true;
}

void tcp_connector_impl::handle_async_resolve(std::shared_ptr<tcp_session> session_ptr,
                                              const asio::error_code& ec,
                                              asio::ip::tcp::resolver::iterator endpoint_itr)
{
    // 解析成功
    if (!ec)
    {
        asio::ip::tcp::endpoint remote_ep = *endpoint_itr;

        // 解析成功回调
        if (event_handler_ptr_ != nullptr)
        {
            asio::error_code ec1;
            event_handler_ptr_->handle_resolve_success(session_ptr,
                                                       remote_ep.address().to_string(ec1),
                                                       remote_ep.port());
        }

        // 开始异步连接
        session_ptr->get_socket()->async_connect(remote_ep, std::bind(&tcp_connector_impl::handle_async_connect,
                                                                      shared_from_this(), session_ptr,
                                                                      std::placeholders::_1));
    }
    // 解析失败
    else
    {
        // 停止连接定时器
        stop_connect_timer();

        // 解析成功回调
        if (event_handler_ptr_ != nullptr)
            event_handler_ptr_->handle_resolve_failed(session_ptr, ec.value(), ec.message());

        // 设置连接状态
        is_connecting_ = false;
    }
}

void tcp_connector_impl::handle_async_connect(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec)
{
    // 停止连接定时器
    stop_connect_timer();

    // 连接成功
    if (!ec)
    {
        // 连接成功回调
        if (event_handler_ptr_ != nullptr)
            event_handler_ptr_->handle_connect_success(session_ptr);
    }
        // 连接失败
    else
    {
        // 连接失败回调
        if (event_handler_ptr_ != nullptr)
            event_handler_ptr_->handle_connect_failed(session_ptr, ec.value(), ec.message());
    }

    // 设置连接状态
    is_connecting_ = false;
}

void tcp_connector_impl::handle_timeout(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec)
{
    // 停止连接定时器
    stop_connect_timer();

    if (!ec)
    {
        // 连接超时回调
        if (event_handler_ptr_ != nullptr)
            event_handler_ptr_->handle_connect_timeout(session_ptr);
    }

    // 设置连接状态
    is_connecting_ = false;
}

}


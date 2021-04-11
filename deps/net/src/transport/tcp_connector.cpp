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
                                 std::string remote_addr,
                                 uint16_t remote_port,
                                 int32_t timeout_seconds/* = 0*/,
                                 std::string local_ip/* = ""*/,
                                 uint16_t local_port/* = 0*/)
{
    // check session, ensure closed
    assert(session_ptr != nullptr && !session_ptr->is_open());
    if (session_ptr == nullptr || session_ptr->is_open())
        return false;

    // check remote address
    assert(!remote_addr.empty() && remote_port != 0);
    if (remote_addr.empty() || remote_port == 0)
        return false;

    // check connecting
    if (is_connecting_)
        return false;

    //
    is_connecting_ = true;

    // open session (create session socket, bind...)
    if (!session_ptr->open(ios_ptr_, local_ip, local_port))
    {
        session_ptr->close();
        return false;
    }

    // start connect timer
    if (!start_connect_timer(session_ptr, timeout_seconds))
    {
        session_ptr->close();
        return false;
    }

    // address resolve, only support use the first address to connect now.
    // todo: try to connect with other address when the connection is unsuccessful.
    asio::ip::tcp::resolver::query query(remote_addr, std::to_string(static_cast<uint32_t>(remote_port)));

    // do host resolve
    auto self(shared_from_this());
    resolver_.async_resolve(query, [this, self, session_ptr] (const asio::error_code& ec, asio::ip::tcp::resolver::iterator endpoint_itr) {
        handle_async_resolve(session_ptr, ec, endpoint_itr);
    });

    return true;
}

void tcp_connector_impl::handle_async_resolve(std::shared_ptr<tcp_session> session_ptr,
                                              const asio::error_code& ec,
                                              asio::ip::tcp::resolver::iterator endpoint_itr)
{
    // resolve success
    if (!ec)
    {
        asio::ip::tcp::endpoint remote_ep = *endpoint_itr;

        // resolve success callback
        if (event_handler_ptr_ != nullptr)
        {
            asio::error_code ec1;
            event_handler_ptr_->handle_resolve_success(session_ptr,
                                                       remote_ep.address().to_string(ec1),
                                                       remote_ep.port());
        }

        // start async connect
        auto self(shared_from_this());
        session_ptr->get_socket()->async_connect(remote_ep, [this, self, session_ptr](const std::error_code& ec) {
            handle_async_connect(session_ptr, ec);
        });
    }
    // resolve failed
    else
    {
        stop_connect_timer();

        // resolve failed callback
        if (event_handler_ptr_ != nullptr)
            event_handler_ptr_->handle_resolve_failed(session_ptr, ec.value(), ec.message());

        // reset connect status
        is_connecting_ = false;
    }
}

void tcp_connector_impl::handle_async_connect(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec)
{
    stop_connect_timer();

    if (!ec)
    {
        // connect success callback
        if (event_handler_ptr_ != nullptr)
            event_handler_ptr_->handle_connect_success(session_ptr);
    }
    else
    {
        // connect failed callback
        if (event_handler_ptr_ != nullptr)
            event_handler_ptr_->handle_connect_failed(session_ptr, ec.value(), ec.message());
    }

    // reset connect status
    is_connecting_ = false;
}

void tcp_connector_impl::handle_connect_timeout(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec)
{
    // stop connect timer
    stop_connect_timer();

    // has error
    if (!ec)
    {
        // connect timeout callback
        if (event_handler_ptr_ != nullptr)
            event_handler_ptr_->handle_connect_timeout(session_ptr);
    }

    // reset connect status
    is_connecting_ = false;
}

}


#include "tcp_server.h"
#include "tcp_acceptor.h"

#include "../session/io_statistics.h"
#include "../session/session_manager.h"

#include "tcp/tcp_server_handler_i.h"

namespace skynet::net::impl {

tcp_server_impl::tcp_server_impl(std::shared_ptr<io_service> acceptor_ios_ptr,
                                 std::shared_ptr<session_manager> session_manager_ptr,
                                 std::shared_ptr<tcp_server_acceptor_config> acceptor_config_ptr,
                                 std::shared_ptr<tcp_server_session_config> session_config_ptr)
:
acceptor_config_ptr_(acceptor_config_ptr),
session_config_ptr_(session_config_ptr),
acceptor_ios_ptr_(acceptor_ios_ptr),
session_manager_ptr_(session_manager_ptr)
{
}

bool tcp_server_impl::init()
{
    is_inited = true;

    return true;
}

void tcp_server_impl::fini()
{
    acceptor_ios_ptr_.reset();
    is_inited = false;
}

void tcp_server_impl::set_event_handler(std::shared_ptr<tcp_server_handler> event_handler_ptr)
{
    event_handler_ptr_ = event_handler_ptr;
}

bool tcp_server_impl::open(std::string local_uri)
{
    uri_codec uri = uri_codec::from_string(local_uri);
    if (!uri.is_valid())
        return false;

    return open(uri.host().value(), uri.port().value());
}

bool tcp_server_impl::open(std::string local_ip, uint16_t local_port)
{
    return open({ std::make_pair(local_ip, local_port) });
}

bool tcp_server_impl::open(std::initializer_list<std::pair<std::string, uint16_t>> local_endpoints)
{
    // check init
    if (!is_inited)
        return false;

    // check local endpoints
    assert(local_endpoints.size() > 0);
    if (local_endpoints.size() == 0)
        return false;

    bool is_ok = false;
    do
    {
        // calc ios pool size (session_thread_num为默认, 按CPU Core设置)
        if (session_config_ptr_->session_thread_num() == 0)
        {
            // the number of cpu logic core, 超过1个时, 需要减掉1, acceptor占用一个ios
            int32_t core_num = std::thread::hardware_concurrency();
            core_num = core_num <= 1 ? 1 : core_num - 1;

            session_config_ptr_->session_thread_num(core_num);
        }

        // create session ios pool
        session_ios_pool_ptr_ = std::make_shared<io_service_pool_impl>(session_config_ptr_->session_thread_num());
        if (session_ios_pool_ptr_ == nullptr)
            break;

        std::shared_ptr<tcp_acceptor> acceptor_ptr;
        bool is_acceptor_ok = true;
        for (auto& itr : local_endpoints)
        {
            std::string key = make_key(itr.first, itr.second);

            // 确保还没有该地址的acceptor
            auto itr_find = acceptors_.find(key);
            if (itr_find != acceptors_.end())
                continue;

            // create acceptor
            acceptor_ptr = std::make_shared<tcp_acceptor_impl>(0, acceptor_ios_ptr_, shared_from_this());
            if (acceptor_ptr == nullptr)
            {
                is_acceptor_ok = false;
                break;
            }

            // open acceptor
            if (acceptor_ptr->open(itr.first, itr.second, true) == false)
            {
                is_acceptor_ok = false;
                break;
            }

            // set acceptor socket options
            acceptor_ptr->set_sock_option(SOCK_OPT_RECV_BUFFER, acceptor_config_ptr_->socket_recv_buf_size());
            acceptor_ptr->set_sock_option(SOCK_OPT_SEND_BUFFER, acceptor_config_ptr_->socket_send_buf_size());
            acceptor_ptr->set_sock_option(SOCK_OPT_KEEPALIVE, acceptor_config_ptr_->socket_keepalive() ? 1 : 0);
            acceptor_ptr->set_sock_option(SOCK_OPT_NODELAY, acceptor_config_ptr_->socket_nodelay() ? 1 : 0);
            acceptor_ptr->set_sock_option(SOCK_OPT_LINGER, acceptor_config_ptr_->socket_linger());

            // add acceptor map
            acceptors_[key] = acceptor_ptr;
        }
        if (is_acceptor_ok == false)
            break;

        // post async accept
        for (auto& itr : acceptors_)
        {
            for (int32_t i = 0; i < acceptor_config_ptr_->sync_accept_num(); ++i)
            {
                do_accept(itr.second);
            }
        }

        // start ios
        session_ios_pool_ptr_->run();
        acceptor_ios_ptr_->run();

        is_ok = true;
    } while (0);

    if (!is_ok)
    {
        close();
    }

    return is_ok;
}

void tcp_server_impl::close()
{
    // clear acceptor
    for (auto& itr :acceptors_)
    {
        itr.second->close();
    }

    // clear ios
    if (acceptor_ios_ptr_ != nullptr)
    {
        acceptor_ios_ptr_->stop();
    }
    if (session_ios_pool_ptr_ != nullptr)
    {
        session_ios_pool_ptr_->stop();
    }
}

//------------------------------------------------------------------------------
// tcp_acceptor_handler impl
//------------------------------------------------------------------------------

void tcp_server_impl::handle_accept_success(std::shared_ptr<tcp_acceptor> acceptor_ptr,
                                            std::shared_ptr<tcp_session> session_ptr)
{
    // set session socket option
    session_ptr->set_sock_option(SOCK_OPT_RECV_BUFFER, session_config_ptr_->socket_recv_buf_size());
    session_ptr->set_sock_option(SOCK_OPT_SEND_BUFFER, session_config_ptr_->socket_send_buf_size());
    session_ptr->set_sock_option(SOCK_OPT_KEEPALIVE, session_config_ptr_->socket_keepalive() ? 1 : 0);
    session_ptr->set_sock_option(SOCK_OPT_NODELAY, session_config_ptr_->socket_nodelay() ? 1 : 0);
    session_ptr->set_sock_option(SOCK_OPT_LINGER, session_config_ptr_->socket_linger());

    // accept callback
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_accept(session_ptr);

    // 开始读
    session_ptr->start_read();

    // post async accept
    do_accept(acceptor_ptr);
}

void tcp_server_impl::handle_accept_failed(std::shared_ptr<tcp_acceptor> acceptor_ptr,
                                           std::shared_ptr<tcp_session> session_ptr,
                                           int32_t err_code, std::string err_msg)
{
    session_ptr->close();
    session_manager_ptr_->release_session(session_ptr);
}

//------------------------------------------------------------------------------
// tcp_session_handler impl
//------------------------------------------------------------------------------

void tcp_server_impl::handle_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len)
{
    // read callback
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_session_read(session_ptr, data_ptr, data_len);
}

void tcp_server_impl::handle_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len)
{
    // write callback
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_session_write(session_ptr, data_ptr, data_len);
}

void tcp_server_impl::handle_session_idle(std::shared_ptr<tcp_session> session_ptr, idle_type type)
{
    // idle callback
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_session_idle(session_ptr, type);
}

void tcp_server_impl::handle_sessoin_close(std::shared_ptr<tcp_session> session_ptr)
{
    // close callback
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_sessoin_close(session_ptr);

    // recycle session
    session_manager_ptr_->release_session(session_ptr);
}

// accept client
bool tcp_server_impl::do_accept(std::shared_ptr<tcp_acceptor> acceptor_ptr)
{
    std::shared_ptr<tcp_session> session_ptr = std::dynamic_pointer_cast<tcp_session>(session_manager_ptr_->create_session(session_type::TCP));
    if (session_ptr == nullptr)
        return false;

    // 设置会话处理句柄
    session_ptr->set_event_handler(shared_from_this());

    // 设置会话
    if (!session_ptr->open(session_ios_pool_ptr_->select_one()))
    {
        session_ptr->close();
        session_manager_ptr_->release_session(session_ptr);
        return false;
    }

    // 接收连接
    acceptor_ptr->accept_once(session_ptr);

    return true;
}

std::string tcp_server_impl::make_key(const asio::ip::tcp::endpoint& ep)
{
    return make_key(ep.address().to_string(), ep.port());
}

std::string tcp_server_impl::make_key(const std::string& ip, uint16_t port)
{
    return ip + ":" + std::to_string(port);
}

}


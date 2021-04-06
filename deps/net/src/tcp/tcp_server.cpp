#include "tcp_server.h"
#include "tcp_acceptor.h"
#include "tcp_io_statistics.h"

#include "tcp/tcp_server_handler_i.h"

namespace skynet::net::impl {

void tcp_server_impl::set_event_handler(std::shared_ptr<tcp_server_handler> event_handler_ptr)
{
    event_handler_ptr_ = event_handler_ptr;
}

bool tcp_server_impl::open(const std::string local_uri, bool is_reuse_addr/* = true*/)
{
    uri_codec uri = uri_codec::from_string(local_uri);
    if (!uri.is_valid())
        return false;

    return open(uri.host().value(), uri.port().value(), is_reuse_addr);
}

bool tcp_server_impl::open(const std::string local_ip, uint16_t local_port, bool is_reuse_addr/* = true*/)
{
    return open({ std::make_pair(local_ip, local_port) }, is_reuse_addr);
}

bool tcp_server_impl::open(std::initializer_list<std::pair<std::string, uint16_t>> local_endpoints, bool is_reuse_addr/* = true*/)
{
    assert(local_endpoints.size() > 0);
    if (local_endpoints.size() == 0)
        return false;

    bool is_ok = false;
    do
    {
        // calc ios pool size (session_thread_num为默认, 按CPU Core设置)
        if (session_config_.session_thread_num() == 0)
        {
            // the number of cpu logic core
            int32_t core_num = std::thread::hardware_concurrency();

            // 超过1个时, 需要减掉1, acceptor占用一个ios
            if (core_num <= 1) core_num = 1;
            else core_num -= 1;

            session_config_.session_thread_num(core_num);
        }

        // create session ios pool
        session_ios_pool_ptr_ = std::make_shared<io_service_pool_impl>(session_config_.session_thread_num());
        if (session_ios_pool_ptr_ == nullptr)
            break;

        // create acceptor ios
        acceptor_ios_ptr_ = std::make_shared<io_service_impl>();
        if (acceptor_ios_ptr_ == nullptr)
            break;

        // create session manager
        session_manager_ptr_ = std::make_shared<tcp_session_manager>();
        if (session_manager_ptr_ == nullptr)
            break;
        if (!session_manager_ptr_->init(session_config_.session_pool_size(),
                                        session_config_.msg_read_buf_size(),
                                        session_config_.msg_write_buf_size(),
                                        session_config_.msg_write_queue_size()))
            break;

        // create session idle checker
        session_idle_checker_ptr_ = std::make_shared<tcp_session_idle_checker>(session_manager_ptr_, acceptor_ios_ptr_);
        if (session_idle_checker_ptr_ == nullptr)
            break;

        // create io statistics
        io_statistics_ptr_ = std::make_shared<tcp_io_statistics_impl>(session_manager_ptr_, acceptor_ios_ptr_);
        if (io_statistics_ptr_ == nullptr)
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
            acceptor_ptr = std::make_shared<tcp_acceptor_impl>(acceptor_ios_ptr_, shared_from_this());
            if (acceptor_ptr == nullptr)
            {
                is_acceptor_ok = false;
                break;
            }

            // open acceptor
            if (acceptor_ptr->open(itr.first, itr.second, is_reuse_addr) == false)
            {
                is_acceptor_ok = false;
                break;
            }

            // set acceptor socket options
            acceptor_ptr->set_sock_option(SOCK_OPT_RECV_BUFFER, acceptor_config_.socket_recv_buf_size());
            acceptor_ptr->set_sock_option(SOCK_OPT_SEND_BUFFER, acceptor_config_.socket_send_buf_size());
            acceptor_ptr->set_sock_option(SOCK_OPT_KEEPALIVE, acceptor_config_.socket_keepalive() ? 1 : 0);
            acceptor_ptr->set_sock_option(SOCK_OPT_NODELAY, acceptor_config_.socket_nodelay() ? 1 : 0);
            acceptor_ptr->set_sock_option(SOCK_OPT_LINGER, acceptor_config_.socket_linger());

            // add acceptor map
            acceptors_[key] = acceptor_ptr;
        }
        if (is_acceptor_ok == false)
            break;

        // start session idle check
        if (!session_idle_checker_ptr_->start(session_config_.idle_check_type(),
                                              session_config_.idle_check_seconds()))
        {
            break;
        }

        // start io statistics
        if (io_statistics_ptr_->start() == false)
            break;

        // post async accept
        for (auto& itr : acceptors_)
        {
            for (int32_t i = 0; i < acceptor_config_.sync_accept_num(); ++i)
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

    // stop io statistics
    if (io_statistics_ptr_ != nullptr)
    {
        io_statistics_ptr_->stop();
    }

    // clear session manager
    if (session_idle_checker_ptr_ != nullptr)
    {
        session_idle_checker_ptr_->stop();
    }

    // clear session
    if (session_manager_ptr_ != nullptr)
    {
        session_manager_ptr_->fini();
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
    session_ptr->set_sock_option(SOCK_OPT_RECV_BUFFER, session_config_.socket_recv_buf_size());
    session_ptr->set_sock_option(SOCK_OPT_SEND_BUFFER, session_config_.socket_send_buf_size());
    session_ptr->set_sock_option(SOCK_OPT_KEEPALIVE, session_config_.socket_keepalive() ? 1 : 0);
    session_ptr->set_sock_option(SOCK_OPT_NODELAY, session_config_.socket_nodelay() ? 1 : 0);
    session_ptr->set_sock_option(SOCK_OPT_LINGER, session_config_.socket_linger());

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

}


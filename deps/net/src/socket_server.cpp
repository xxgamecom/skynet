#include "socket_server.h"
#include "session/io_statistics.h"

#include "uri/uri_codec.h"
#include "tcp/tcp_acceptor.h"

namespace skynet::net::impl {

bool socket_server_impl::init()
{
    assert(!is_inited);

    do
    {
        // default acceptor config
        tcp_acceptor_config_ptr_ = std::make_shared<tcp_server_acceptor_config_impl>();
        if (tcp_acceptor_config_ptr_ == nullptr)
            break;

        // default session config
        session_config_ptr_ = std::make_shared<tcp_server_session_config_impl>();
        if (session_config_ptr_ == nullptr)
            break;

        // calc ios pool size (session_thread_num为默认, 按CPU Core设置)
        if (session_config_ptr_->session_thread_num() == 0)
        {
            // the number of cpu logic core, 超过1个时, 需要减掉1, acceptor占用一个ios
            int32_t core_num = std::thread::hardware_concurrency();
            core_num = core_num <= 1 ? 1 : core_num - 1;

            session_config_ptr_->session_thread_num(core_num);
        }

        // create acceptor io service
        acceptor_ios_ptr_ = std::make_shared<io_service_impl>();
        if (acceptor_ios_ptr_ == nullptr)
            break;

        // create session ios pool
        session_ios_pool_ptr_ = std::make_shared<io_service_pool_impl>(session_config_ptr_->session_thread_num());
        if (session_ios_pool_ptr_ == nullptr)
            break;

        // create session mananger
        session_manager_ptr_ = std::make_shared<session_manager_impl>();
        if (session_manager_ptr_ == nullptr)
            break;
        if (!session_manager_ptr_->init(session_config_ptr_->session_pool_size(),
                                        session_config_ptr_->read_buf_size(),
                                        session_config_ptr_->write_buf_size(),
                                        session_config_ptr_->write_queue_size()))
            break;

        // create io statistics
        io_statistics_ptr_ = std::make_shared<io_statistics_impl>(session_manager_ptr_, acceptor_ios_ptr_);
        if (io_statistics_ptr_ == nullptr)
            break;

        // create session idle checker
        session_idle_checker_ptr_ = std::make_shared<session_idle_checker>(session_manager_ptr_, acceptor_ios_ptr_);
        if (session_idle_checker_ptr_ == nullptr)
            break;

        // start session idle check
        if (!session_idle_checker_ptr_->start(session_config_ptr_->idle_check_type(),
                                              session_config_ptr_->idle_check_seconds()))
        {
            break;
        }

        // start io statistics
        if (io_statistics_ptr_->start() == false)
            break;

        // start io service
        session_ios_pool_ptr_->run();
        acceptor_ios_ptr_->run();

        is_inited = true;
    } while (0);

    if (!is_inited)
    {
        fini();
    }

    return is_inited;
}

void socket_server_impl::fini()
{
    // clear acceptor
    for (auto& itr :acceptor_map_)
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

    acceptor_ios_ptr_.reset();

    //
    is_inited = false;
}

int socket_server_impl::listen(std::string local_uri, uint32_t svc_handle)
{
    uri_codec uri = uri_codec::from_string(local_uri);
    if (!uri.is_valid())
        return INVALID_SESSION_ID;

    return listen(uri.host().value(), uri.port().value(), svc_handle);
}

int socket_server_impl::listen(std::string local_ip, uint16_t local_port, uint32_t svc_handle)
{
    // check init
    assert(is_inited);
    if (!is_inited)
        return INVALID_SESSION_ID;

    // key = `ip:port` string
    std::string key = make_key(local_ip, local_port);
    // ensure haven't an acceptor for that address
    auto itr_find = acceptor_map_.find(key);
    if (itr_find != acceptor_map_.end())
        return INVALID_SESSION_ID;

    // create acceptor
    std::shared_ptr<tcp_acceptor> acceptor_ptr = std::make_shared<tcp_acceptor_impl>(svc_handle, acceptor_ios_ptr_, shared_from_this());
    if (acceptor_ptr == nullptr)
        return INVALID_SESSION_ID;

    // open acceptor (bind & listen)
    if (!acceptor_ptr->open(local_ip, local_port, true))
        return INVALID_SESSION_ID;

    // set acceptor socket options
    acceptor_ptr->set_sock_option(SOCK_OPT_RECV_BUFFER, tcp_acceptor_config_ptr_->socket_recv_buf_size());
    acceptor_ptr->set_sock_option(SOCK_OPT_SEND_BUFFER, tcp_acceptor_config_ptr_->socket_send_buf_size());
    acceptor_ptr->set_sock_option(SOCK_OPT_KEEPALIVE, tcp_acceptor_config_ptr_->socket_keepalive() ? 1 : 0);
    acceptor_ptr->set_sock_option(SOCK_OPT_NODELAY, tcp_acceptor_config_ptr_->socket_nodelay() ? 1 : 0);
    acceptor_ptr->set_sock_option(SOCK_OPT_LINGER, tcp_acceptor_config_ptr_->socket_linger());

    // alloc socket id
    session_id_t session_id = session_manager_ptr_->create_session_id();

    // save acceptor
    acceptor_map_[key] = acceptor_ptr;
    socket_id_acceptor_map_[session_id] = acceptor_ptr;

    // post async accept
    for (int32_t i = 0; i < tcp_acceptor_config_ptr_->sync_accept_num(); ++i)
    {
        do_accept(acceptor_ptr);
    }

    return session_id;
}

std::shared_ptr<tcp_server_acceptor_config> socket_server_impl::acceptor_config()
{
    return tcp_acceptor_config_ptr_;
}

void socket_server_impl::acceptor_config(std::shared_ptr<tcp_server_acceptor_config> acceptor_config_ptr)
{
    tcp_acceptor_config_ptr_ = acceptor_config_ptr;
}

std::shared_ptr<tcp_server_session_config> socket_server_impl::session_config()
{
    return session_config_ptr_;
}

void socket_server_impl::session_config(std::shared_ptr<tcp_server_session_config> session_config_ptr)
{
    session_config_ptr_ = session_config_ptr;
}

void socket_server_impl::handle_accept_success(std::shared_ptr<tcp_acceptor> acceptor_ptr, std::shared_ptr<tcp_session> session_ptr)
{
    // set session socket option
    session_ptr->set_sock_option(SOCK_OPT_RECV_BUFFER, session_config_ptr_->socket_recv_buf_size());
    session_ptr->set_sock_option(SOCK_OPT_SEND_BUFFER, session_config_ptr_->socket_send_buf_size());
    session_ptr->set_sock_option(SOCK_OPT_KEEPALIVE, session_config_ptr_->socket_keepalive() ? 1 : 0);
    session_ptr->set_sock_option(SOCK_OPT_NODELAY, session_config_ptr_->socket_nodelay() ? 1 : 0);
    session_ptr->set_sock_option(SOCK_OPT_LINGER, session_config_ptr_->socket_linger());

    // accept callback
//    if (event_handler_ptr_ != nullptr)
//        event_handler_ptr_->handle_accept(session_ptr);

    // start session read
    session_ptr->start_read();

    // post async accept
    do_accept(acceptor_ptr);
}

void socket_server_impl::handle_accept_failed(std::shared_ptr<tcp_acceptor> acceptor_ptr, std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg)
{
    session_ptr->close();
    session_manager_ptr_->release_session(session_ptr);
}

void socket_server_impl::handle_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len)
{
//    // read callback
//    if (event_handler_ptr_ != nullptr)
//        event_handler_ptr_->handle_session_read(session_ptr, data_ptr, data_len);
}

void socket_server_impl::handle_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len)
{
//    // write callback
//    if (event_handler_ptr_ != nullptr)
//        event_handler_ptr_->handle_session_write(session_ptr, data_ptr, data_len);
}

void socket_server_impl::handle_session_idle(std::shared_ptr<tcp_session> session_ptr, idle_type type)
{
//    // idle callback
//    if (event_handler_ptr_ != nullptr)
//        event_handler_ptr_->handle_session_idle(session_ptr, type);
}

void socket_server_impl::handle_sessoin_close(std::shared_ptr<tcp_session> session_ptr)
{
//    // close callback
//    if (event_handler_ptr_ != nullptr)
//        event_handler_ptr_->handle_sessoin_close(session_ptr);
//
    // recycle session
    session_manager_ptr_->release_session(session_ptr);
}

bool socket_server_impl::do_accept(std::shared_ptr<tcp_acceptor> acceptor_ptr)
{
    std::shared_ptr<tcp_session> session_ptr = std::dynamic_pointer_cast<tcp_session>(session_manager_ptr_->create_session(session_type::TCP));
    if (session_ptr == nullptr)
        return false;

    // set session event handler
    session_ptr->set_event_handler(shared_from_this());

    // open session
    if (!session_ptr->open(session_ios_pool_ptr_->select_one()))
    {
        session_ptr->close();
        session_manager_ptr_->release_session(session_ptr);
        return false;
    }

    // accept client
    acceptor_ptr->accept_once(session_ptr);

    return true;
}

std::string socket_server_impl::make_key(const asio::ip::tcp::endpoint& ep)
{
    return make_key(ep.address().to_string(), ep.port());
}

std::string socket_server_impl::make_key(const std::string& ip, uint16_t port)
{
    return ip + ":" + std::to_string(port);
}


}

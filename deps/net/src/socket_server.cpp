#include "socket_server.h"
#include "session/io_statistics.h"

#include "uri/uri_codec.h"
#include "tcp/tcp_acceptor.h"
#include "tcp/tcp_client.h"

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
        tcp_session_config_ptr_ = std::make_shared<tcp_server_session_config_impl>();
        if (tcp_session_config_ptr_ == nullptr)
            break;

        // calc ios pool size (session_thread_num为默认, 按CPU Core设置)
        if (tcp_session_config_ptr_->session_thread_num() == 0)
        {
            // the number of cpu logic core, 超过1个时, 需要减掉1, acceptor占用一个ios
            int32_t core_num = std::thread::hardware_concurrency();
            core_num = core_num <= 1 ? 1 : core_num - 1;

            tcp_session_config_ptr_->session_thread_num(core_num);
        }

        // create acceptor io service
        tcp_acceptor_ios_ptr_ = std::make_shared<io_service_impl>();
        if (tcp_acceptor_ios_ptr_ == nullptr)
            break;

        // create session ios pool
        session_ios_pool_ptr_ = std::make_shared<io_service_pool_impl>(tcp_session_config_ptr_->session_thread_num());
        if (session_ios_pool_ptr_ == nullptr)
            break;

        // create session mananger
        session_manager_ptr_ = std::make_shared<socket_manager_impl>();
        if (session_manager_ptr_ == nullptr)
            break;
        if (!session_manager_ptr_->init(tcp_session_config_ptr_->session_pool_size(),
                                        tcp_session_config_ptr_->read_buf_size(),
                                        tcp_session_config_ptr_->write_buf_size(),
                                        tcp_session_config_ptr_->write_queue_size()))
            break;

        // create io statistics
        io_statistics_ptr_ = std::make_shared<io_statistics_impl>(session_manager_ptr_, tcp_acceptor_ios_ptr_);
        if (io_statistics_ptr_ == nullptr)
            break;

        // create session idle checker
        session_idle_checker_ptr_ = std::make_shared<session_idle_checker>(session_manager_ptr_, tcp_acceptor_ios_ptr_);
        if (session_idle_checker_ptr_ == nullptr)
            break;

        // start session idle check
        if (!session_idle_checker_ptr_->start(tcp_session_config_ptr_->idle_check_type(),
                                              tcp_session_config_ptr_->idle_check_seconds()))
        {
            break;
        }

        // start io statistics
        if (io_statistics_ptr_->start() == false)
            break;

        // start io service
        session_ios_pool_ptr_->run();
        tcp_acceptor_ios_ptr_->run();

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
    for (auto& itr :acceptor_id_tcp_acceptors_)
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
    if (tcp_acceptor_ios_ptr_ != nullptr)
    {
        tcp_acceptor_ios_ptr_->stop();
    }
    if (session_ios_pool_ptr_ != nullptr)
    {
        session_ios_pool_ptr_->stop();
    }

    tcp_acceptor_ios_ptr_.reset();

    //
    is_inited = false;
}

int socket_server_impl::open_tcp_server(std::string local_uri, uint32_t svc_handle)
{
    uri_codec uri = uri_codec::from_string(local_uri);
    if (!uri.is_valid())
        return INVALID_SESSION_ID;

    return open_tcp_server(uri.host().value(), uri.port().value(), svc_handle);
}

int socket_server_impl::open_tcp_server(std::string local_ip, uint16_t local_port, uint32_t svc_handle)
{
    // check init
    assert(is_inited);
    if (!is_inited)
        return INVALID_SESSION_ID;

    // acceptor_id = `ip:port` string
    std::string acceptor_id = make_key(local_ip, local_port);
    // ensure haven't an acceptor for that address
    auto itr_find = acceptor_id_tcp_acceptors_.find(acceptor_id);
    if (itr_find != acceptor_id_tcp_acceptors_.end())
        return INVALID_SESSION_ID;

    // alloc acceptor socket id
    uint32_t socket_id = session_manager_ptr_->new_socket_id();

    // create acceptor
    auto acceptor_ptr = std::make_shared<tcp_acceptor_impl>(acceptor_id, socket_id, svc_handle, tcp_acceptor_ios_ptr_, shared_from_this());
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

    // save acceptor info
    acceptor_id_tcp_acceptors_[acceptor_id] = acceptor_ptr;
    socket_id_tcp_acceptors_[socket_id] = acceptor_ptr;

    // post async accept
    for (int32_t i = 0; i < tcp_acceptor_config_ptr_->sync_accept_num(); ++i)
    {
        do_accept(acceptor_ptr);
    }

    return socket_id;
}

void socket_server_impl::close_tcp_server(uint32_t socket_id, uint32_t svc_handle)
{
    // close acceptor
    auto itr_find = socket_id_tcp_acceptors_.find(socket_id);
    if (itr_find != socket_id_tcp_acceptors_.end())
    {
        // close
        auto acceptor_ptr = itr_find->second;
        acceptor_ptr->close();

        // erase acceptor info
        socket_id_tcp_acceptors_.erase(socket_id);
        acceptor_id_tcp_acceptors_.erase(acceptor_ptr->acceptor_id());
    }

//    // clear ios
//    if (tcp_acceptor_ios_ptr_ != nullptr)
//    {
//        tcp_acceptor_ios_ptr_->stop();
//    }
//    if (session_ios_pool_ptr_ != nullptr)
//    {
//        session_ios_pool_ptr_->stop();
//    }
}

int socket_server_impl::open_tcp_client(std::string remote_uri, uint32_t svc_handle)
{
    uri_codec uri = uri_codec::from_string(remote_uri);
    if (!uri.is_valid())
        return INVALID_SESSION_ID;

    return open_tcp_client(uri.host().value(), uri.port().value(), svc_handle);
}

int socket_server_impl::open_tcp_client(std::string remote_host, uint16_t remote_port, uint32_t svc_handle)
{
    // check init
    assert(is_inited);
    if (!is_inited)
        return INVALID_SESSION_ID;

    // alloc client socket id
    uint32_t socket_id = session_manager_ptr_->new_socket_id();

    // create tcp client
    auto tcp_client_ptr = std::make_shared<tcp_client_impl>(socket_id);

    // save client info
    tcp_clients_[socket_id] = tcp_client_ptr;

    return socket_id;
}

void socket_server_impl::close_tcp_client(uint32_t socket_id, uint32_t svc_handle)
{
    auto itr_find = tcp_clients_.find(socket_id);
    if (itr_find == tcp_clients_.end())
        return;

    // close tcp client
    auto tcp_client_ptr = itr_find->second;
    tcp_client_ptr->close();

    // erase tcp client
    tcp_clients_.erase(socket_id);
}

// create udp server
int socket_server_impl::open_udp_server(std::string local_uri, uint32_t svc_handle)
{
    return INVALID_SESSION_ID;
}

void socket_server_impl::close_udp_server(uint32_t socket_id, uint32_t svc_handle)
{

}

// create udp client
int socket_server_impl::open_udp_client(std::string remote_uri, uint32_t svc_handle)
{
    uri_codec uri = uri_codec::from_string(remote_uri);
    if (!uri.is_valid())
        return INVALID_SESSION_ID;

    return open_udp_client(uri.host().value(), uri.port().value());
}

int socket_server_impl::open_udp_client(std::string remote_host, uint16_t remote_port, uint32_t svc_handle)
{
    return INVALID_SESSION_ID;
}

void socket_server_impl::close_udp_client(uint32_t socket_id, uint32_t svc_handle)
{

}

void socket_server_impl::stop(int32_t socket_id, uint32_t svc_handle)
{

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
    return tcp_session_config_ptr_;
}

void socket_server_impl::session_config(std::shared_ptr<tcp_server_session_config> session_config_ptr)
{
    tcp_session_config_ptr_ = session_config_ptr;
}

void socket_server_impl::handle_accept_success(std::shared_ptr<tcp_acceptor> acceptor_ptr, std::shared_ptr<tcp_session> session_ptr)
{
    // set session socket option
    session_ptr->set_sock_option(SOCK_OPT_RECV_BUFFER, tcp_session_config_ptr_->socket_recv_buf_size());
    session_ptr->set_sock_option(SOCK_OPT_SEND_BUFFER, tcp_session_config_ptr_->socket_send_buf_size());
    session_ptr->set_sock_option(SOCK_OPT_KEEPALIVE, tcp_session_config_ptr_->socket_keepalive() ? 1 : 0);
    session_ptr->set_sock_option(SOCK_OPT_NODELAY, tcp_session_config_ptr_->socket_nodelay() ? 1 : 0);
    session_ptr->set_sock_option(SOCK_OPT_LINGER, tcp_session_config_ptr_->socket_linger());

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

std::string socket_server_impl::make_key(const std::string& ip, uint16_t port)
{
    return ip + ":" + std::to_string(port);
}


}

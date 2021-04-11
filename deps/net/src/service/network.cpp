#include "network.h"
#include "service/network_handler_i.h"

#include "../session/io_statistics.h"

#include "../uri/uri_codec.h"
#include "../transport/tcp_acceptor.h"

#include "tcp_client.h"
#include "tcp_server.h"

namespace skynet::net::impl {

network_impl::~network_impl()
{
    fini();
}

bool network_impl::init()
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
        net_manager_ptr_ = std::make_shared<net_manager_impl>();
        if (net_manager_ptr_ == nullptr)
            break;
        if (!net_manager_ptr_->init(tcp_session_config_ptr_->session_pool_size(),
                                        tcp_session_config_ptr_->read_buf_size(),
                                        tcp_session_config_ptr_->write_buf_size(),
                                        tcp_session_config_ptr_->write_queue_size()))
            break;

        // create io statistics
        io_statistics_ptr_ = std::make_shared<io_statistics_impl>(net_manager_ptr_, tcp_acceptor_ios_ptr_);
        if (io_statistics_ptr_ == nullptr)
            break;

        // create session idle checker
        session_idle_checker_ptr_ = std::make_shared<session_idle_checker>(net_manager_ptr_, tcp_acceptor_ios_ptr_);
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

void network_impl::fini()
{
//    // clear acceptor
//    for (auto& itr :acceptor_id_tcp_acceptors_)
//    {
//        itr.second->close();
//    }

    // clear tcp client
    for (auto& itr : tcp_clients_)
    {
        itr.second->close();
    }
    tcp_clients_.clear();

    // clear tcp server
    for (auto& itr : tcp_servers_)
    {
        itr.second->close();
    }
    tcp_servers_.clear();

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
    if (net_manager_ptr_ != nullptr)
    {
        net_manager_ptr_->fini();
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

// set network event handler
void network_impl::set_event_handler(std::shared_ptr<network_tcp_server_handler> event_handler_ptr)
{
    tcp_server_handler_ptr_ = event_handler_ptr;
}

void network_impl::set_event_handler(std::shared_ptr<network_tcp_client_handler> event_handler_ptr)
{
    tcp_client_handler_ptr_ = event_handler_ptr;
}

void network_impl::set_event_handler(std::shared_ptr<network_udp_server_handler> event_handler_ptr)
{
    udp_server_handler_ptr_ = event_handler_ptr;
}

void network_impl::set_event_handler(std::shared_ptr<network_udp_client_handler> event_handler_ptr)
{
    udp_client_handler_ptr_ = event_handler_ptr;
}

int network_impl::open_tcp_server(std::string local_uri, uint32_t svc_handle)
{
    uri_codec uri = uri_codec::from_string(local_uri);
    if (!uri.is_valid())
        return INVALID_SOCKET_ID;

    return open_tcp_server(uri.host().value(), uri.port().value(), svc_handle);
}

int network_impl::open_tcp_server(std::string local_ip, uint16_t local_port, uint32_t svc_handle)
{
    // check init
    assert(is_inited);
    if (!is_inited)
        return INVALID_SOCKET_ID;

    // alloc server socket id
    uint32_t socket_id = net_manager_ptr_->new_socket_id();

    // create tcp server
    auto tcp_server_ptr = std::make_shared<tcp_server_impl>(svc_handle, socket_id, tcp_acceptor_ios_ptr_, session_ios_pool_ptr_,
                                                            net_manager_ptr_, tcp_acceptor_config_ptr_, tcp_session_config_ptr_);
    if (tcp_server_ptr == nullptr)
        return INVALID_SOCKET_ID;

    tcp_server_ptr->set_event_handler(shared_from_this());
    tcp_server_ptr->init();

//        tcp_server_ptr->session_config()->session_thread_num(thread_count - 1);
//        tcp_server_ptr->session_config()->socket_recv_buf_size(32 * 1024);
//        tcp_server_ptr->session_config()->socket_send_buf_size(32 * 1024);

    if (!tcp_server_ptr->open(local_ip, local_port))
        return INVALID_SOCKET_ID;

    // save tcp server info
    tcp_servers_[socket_id] = tcp_server_ptr;

    return socket_id;
}

void network_impl::close_tcp_server(uint32_t socket_id, uint32_t svc_handle)
{
    auto itr_find = tcp_servers_.find(socket_id);
    if (itr_find == tcp_servers_.end())
        return;

    // close tcp server
    auto tcp_server_ptr = itr_find->second;
    tcp_server_ptr->close();

    // erase tcp server
    tcp_servers_.erase(socket_id);
}

int network_impl::open_tcp_client(std::string remote_uri, uint32_t svc_handle)
{
    uri_codec uri = uri_codec::from_string(remote_uri);
    if (!uri.is_valid())
        return INVALID_SOCKET_ID;

    return open_tcp_client(uri.host().value(), uri.port().value(), svc_handle);
}

int network_impl::open_tcp_client(std::string remote_host, uint16_t remote_port, uint32_t svc_handle)
{
    // check init
    assert(is_inited);
    if (!is_inited)
        return INVALID_SOCKET_ID;

    // alloc client socket id
    uint32_t socket_id = net_manager_ptr_->new_socket_id();

    // create tcp client
    auto tcp_client_ptr = std::make_shared<tcp_client_impl>(socket_id, session_ios_pool_ptr_->select_one());
    tcp_client_ptr->set_event_handler(shared_from_this());

    // init tcp client
    if (!tcp_client_ptr->init())
        return INVALID_SOCKET_ID;

    // connect
    if (!tcp_client_ptr->connect(remote_host, remote_port))
        return INVALID_SOCKET_ID;

    // save client info
    tcp_clients_[socket_id] = tcp_client_ptr;

    return socket_id;
}

void network_impl::close_tcp_client(uint32_t socket_id, uint32_t svc_handle)
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
int network_impl::udp_socket(std::string local_uri, uint32_t svc_handle)
{
    uri_codec uri = uri_codec::from_string(local_uri);
    if (!uri.is_valid())
        return INVALID_SOCKET_ID;

    return udp_socket(uri.host().value(), uri.port().value(), svc_handle);
}

int network_impl::udp_socket(std::string local_ip, uint16_t local_port, uint32_t svc_handle)
{
    return INVALID_SOCKET_ID;
}

void network_impl::close_udp_server(uint32_t socket_id, uint32_t svc_handle)
{

}

// create udp client
int network_impl::open_udp_client(std::string remote_uri, uint32_t svc_handle)
{
    uri_codec uri = uri_codec::from_string(remote_uri);
    if (!uri.is_valid())
        return INVALID_SOCKET_ID;

    return open_udp_client(uri.host().value(), uri.port().value());
}

int network_impl::open_udp_client(std::string remote_host, uint16_t remote_port, uint32_t svc_handle)
{
    return INVALID_SOCKET_ID;
}

void network_impl::close_udp_client(uint32_t socket_id, uint32_t svc_handle)
{

}

void network_impl::stop(int32_t socket_id, uint32_t svc_handle)
{

}

// get io statistics
std::shared_ptr<io_statistics> network_impl::get_io_statistics()
{
    return io_statistics_ptr_;
}

std::shared_ptr<tcp_server_acceptor_config> network_impl::acceptor_config()
{
    return tcp_acceptor_config_ptr_;
}

void network_impl::acceptor_config(std::shared_ptr<tcp_server_acceptor_config> acceptor_config_ptr)
{
    tcp_acceptor_config_ptr_ = acceptor_config_ptr;
}

std::shared_ptr<tcp_server_session_config> network_impl::session_config()
{
    return tcp_session_config_ptr_;
}

void network_impl::session_config(std::shared_ptr<tcp_server_session_config> session_config_ptr)
{
    tcp_session_config_ptr_ = session_config_ptr;
}

void network_impl::handle_tcp_server_accept(std::shared_ptr<tcp_session> session_ptr)
{
    if (tcp_server_handler_ptr_ != nullptr)
        tcp_server_handler_ptr_->handle_accept(session_ptr);
}

void network_impl::handle_tcp_server_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len)
{
    if (tcp_server_handler_ptr_ != nullptr)
        tcp_server_handler_ptr_->handle_session_read(session_ptr, data_ptr, data_len);
}

void network_impl::handle_tcp_server_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len)
{
    if (tcp_server_handler_ptr_ != nullptr)
        tcp_server_handler_ptr_->handle_session_write(session_ptr, data_ptr, data_len);
}

void network_impl::handle_tcp_server_session_idle(std::shared_ptr<tcp_session> session_ptr, session_idle_type type)
{
    if (tcp_server_handler_ptr_ != nullptr)
        tcp_server_handler_ptr_->handle_session_idle(session_ptr, type);
}

void network_impl::handle_tcp_server_sessoin_close(std::shared_ptr<tcp_session> session_ptr)
{
    if (tcp_server_handler_ptr_ != nullptr)
        tcp_server_handler_ptr_->handle_sessoin_close(session_ptr);
}

void network_impl::handle_tcp_client_connect_success(std::shared_ptr<tcp_session> session_ptr)
{
    if (tcp_client_handler_ptr_ != nullptr)
        tcp_client_handler_ptr_->handle_tcp_client_connect_success(session_ptr);
}

void network_impl::handle_tcp_client_connect_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg)
{
    if (tcp_client_handler_ptr_ != nullptr)
        tcp_client_handler_ptr_->handle_tcp_client_connect_failed(session_ptr, err_code, err_msg);
}

void network_impl::handle_tcp_client_connect_timeout(std::shared_ptr<tcp_session> session_ptr)
{
    if (tcp_client_handler_ptr_ != nullptr)
        tcp_client_handler_ptr_->handle_tcp_client_connect_timeout(session_ptr);
}

void network_impl::handle_tcp_client_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len)
{
    if (tcp_client_handler_ptr_ != nullptr)
        tcp_client_handler_ptr_->handle_tcp_client_read(session_ptr, data_ptr, data_len);
}

void network_impl::handle_tcp_client_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len)
{
    if (tcp_client_handler_ptr_ != nullptr)
        tcp_client_handler_ptr_->handle_tcp_client_write(session_ptr, data_ptr, data_len);
}

void network_impl::handle_tcp_client_close(std::shared_ptr<tcp_session> session_ptr)
{
    if (tcp_client_handler_ptr_ != nullptr)
        tcp_client_handler_ptr_->handle_tcp_client_close(session_ptr);
}

}

#pragma once

#include "service/network_i.h"

#include "base/io_statistics_i.h"

// tcp server
#include "tcp_server_acceptor_config.h"
#include "tcp_server_session_config.h"
#include "transport/tcp_acceptor_handler_i.h"
#include "transport/tcp_session_handler_i.h"

// tcp server
#include "service/tcp_server_i.h"
#include "service/tcp_server_handler_i.h"

// tcp client
#include "service/tcp_client_i.h"
#include "service/tcp_client_handler_i.h"

//
#include "../base/io_service_pool.h"
#include "../session/net_manager.h"
#include "../session/session_idle_checker.h"

// forward delcare
namespace skynet::net {
class network_tcp_server_handler;
class network_tcp_client_handler;
}

namespace skynet::net::impl {

/**
 * socket server
 * include tcp/udp server, tcp/udp client
 */
class network_impl : public network,
                     public tcp_server_handler,
                     public tcp_client_handler,
                     public std::enable_shared_from_this<network_impl>,
                     public asio::noncopyable
{
private:
    bool is_inited = false;

    std::shared_ptr<network_tcp_server_handler> tcp_server_handler_ptr_;        // network tcp server event handler (callback)
    std::shared_ptr<network_tcp_client_handler> tcp_client_handler_ptr_;        // network tcp client event handler (callback)
    std::shared_ptr<network_udp_server_handler> udp_server_handler_ptr_;        // network udp server event handler (callback)
    std::shared_ptr<network_udp_client_handler> udp_client_handler_ptr_;        // network udp client event handler (callback)

    // tcp
private:
    // tcp server config
    std::shared_ptr<tcp_server_acceptor_config> tcp_acceptor_config_ptr_;       // tcp acceptor config
    std::shared_ptr<tcp_server_session_config> tcp_session_config_ptr_;         // tcp session config

    // tcp acceptor io service
    std::shared_ptr<io_service> tcp_acceptor_ios_ptr_;                          // tcp server acceptor io service (only for acceptor)

    // tcp server
    std::map<uint32_t, std::shared_ptr<tcp_server>> tcp_servers_;               // tcp server map (k: `socket_id`, v: tcp_server)
    // tcp client
    std::map<uint32_t, std::shared_ptr<tcp_client>> tcp_clients_;               // tcp client map (k: `socket_id`, v: tcp_client)

    // udp
private:
//    std::map<uint32_t, std::shared_ptr<udp_server>> udp_servers_;               // udp server map (k: `socket_id`, v: udp_server)
//    // udp client
//    std::map<uint32_t, std::shared_ptr<udp_client>> udp_clients_;               // udp client map (k: `socket_id`, v: udp_client)

    // session
private:
    std::shared_ptr<io_service_pool> session_ios_pool_ptr_;                     // session io service pool
    std::shared_ptr<io_statistics> io_statistics_ptr_;                          // server io statistics
    std::shared_ptr<net_manager> net_manager_ptr_;                              // network mananger
    std::shared_ptr<session_idle_checker> session_idle_checker_ptr_;            //

public:
    network_impl() = default;
    ~network_impl() override;

    // socket_server impl
public:
    bool init() override;
    void fini() override;

    // set network event handler
    void set_event_handler(std::shared_ptr<network_tcp_server_handler> event_handler_ptr) override;
    void set_event_handler(std::shared_ptr<network_tcp_client_handler> event_handler_ptr) override;
    void set_event_handler(std::shared_ptr<network_udp_server_handler> event_handler_ptr) override;
    void set_event_handler(std::shared_ptr<network_udp_client_handler> event_handler_ptr) override;

    // open/close tcp server
    int open_tcp_server(std::string local_uri, uint32_t svc_handle) override;
    int open_tcp_server(std::string local_ip, uint16_t local_port, uint32_t svc_handle) override;
    void close_tcp_server(uint32_t socket_id, uint32_t svc_handle) override;

    // open/close tcp client
    int open_tcp_client(std::string remote_uri, uint32_t svc_handle) override;
    int open_tcp_client(std::string remote_host, uint16_t remote_port, uint32_t svc_handle) override;
    void close_tcp_client(uint32_t socket_id, uint32_t svc_handle) override;

    // create udp server
    int open_udp_server(std::string local_uri, uint32_t svc_handle) override;
    int open_udp_server(std::string local_ip, uint16_t local_port, uint32_t svc_handle) override;
    void close_udp_server(uint32_t socket_id, uint32_t svc_handle) override;

    // create udp client
    int open_udp_client(std::string remote_uri, uint32_t svc_handle) override;
    int open_udp_client(std::string remote_host, uint16_t remote_port, uint32_t svc_handle) override;
    void close_udp_client(uint32_t socket_id, uint32_t svc_handle) override;

public:
    // start
    void start(int32_t socket_id, uint32_t svc_handle);
    // pause
    void pause(int32_t socket_id, uint32_t svc_handle);
    // stop
    void stop(int32_t socket_id, uint32_t svc_handle);

    // get io statistics
    std::shared_ptr<io_statistics> get_io_statistics();

    // options
public:
    // get/set config
    std::shared_ptr<tcp_server_acceptor_config> acceptor_config();
    void acceptor_config(std::shared_ptr<tcp_server_acceptor_config> acceptor_config_ptr);

    std::shared_ptr<tcp_server_session_config> session_config();
    void session_config(std::shared_ptr<tcp_server_session_config> session_config_ptr);

    // tcp_server_handler impl
protected:
    void handle_tcp_server_accept(std::shared_ptr<tcp_session> session_ptr) override;
    void handle_tcp_server_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    void handle_tcp_server_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    void handle_tcp_server_session_idle(std::shared_ptr<tcp_session> session_ptr, session_idle_type type) override;
    void handle_tcp_server_sessoin_close(std::shared_ptr<tcp_session> session_ptr) override;

    // tcp_client_handler impl
protected:
    void handle_tcp_client_connect_success(std::shared_ptr<tcp_session> session_ptr) override;
    void handle_tcp_client_connect_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg) override;
    void handle_tcp_client_connect_timeout(std::shared_ptr<tcp_session> session_ptr) override;

    void handle_tcp_client_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    void handle_tcp_client_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    void handle_tcp_client_close(std::shared_ptr<tcp_session> session_ptr) override;

    // udp_server_handler impl
protected:

    // udp_client_handler impl
protected:
};

}


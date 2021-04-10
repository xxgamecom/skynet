#pragma once

#include "socket_server_i.h"

#include "base/io_statistics_i.h"

// tcp server
#include "tcp/tcp_server_acceptor_config.h"
#include "tcp/tcp_server_session_config.h"
#include "tcp/tcp_acceptor_handler_i.h"
#include "tcp/tcp_session_handler_i.h"

// tcp client
#include "tcp/tcp_client_i.h"
#include "tcp/tcp_client_handler_i.h"

//
#include "base/io_service_pool.h"
#include "session/socket_manager.h"
#include "session/session_idle_checker.h"

namespace skynet::net::impl {

/**
 * socket server
 * include tcp/udp server, tcp/udp client
 */
class socket_server_impl : public socket_server,
                           public tcp_acceptor_handler,
                           public tcp_session_handler,
                           public std::enable_shared_from_this<socket_server_impl>,
                           public asio::noncopyable
{
private:
    bool is_inited = false;

    // tcp server
private:
    // tcp server config
    std::shared_ptr<tcp_server_acceptor_config> tcp_acceptor_config_ptr_;               // tcp acceptor config
    std::shared_ptr<tcp_server_session_config> tcp_session_config_ptr_;                 // tcp session config

    // tcp acceptor
    std::shared_ptr<io_service> tcp_acceptor_ios_ptr_;                                  // tcp server acceptor io service (only for acceptor)
    std::map<std::string, std::shared_ptr<tcp_acceptor>> acceptor_id_tcp_acceptors_;    // tcp acceptor map (k: `acceptor_id`, v: tcp_acceptor)
    std::map<uint32_t, std::shared_ptr<tcp_acceptor>> socket_id_tcp_acceptors_;         // tcp acceptor map (k: `socket_id`, v: tcp_accepotr)

    // tcp client
private:
    std::map<uint32_t, std::shared_ptr<tcp_client>> tcp_clients_;                       // tcp client map (k: `socket_id`, v: tcp_connector)

    // udp
private:
    // udp session config

    // session
private:
    std::shared_ptr<io_service_pool> session_ios_pool_ptr_;                             // session io service pool
    std::shared_ptr<io_statistics> io_statistics_ptr_;                                  // server io statistics
    std::shared_ptr<session_manager> session_manager_ptr_;                              // server session manager
    std::shared_ptr<session_idle_checker> session_idle_checker_ptr_;                    //

public:
    socket_server_impl() = default;
    ~socket_server_impl() override = default;

    // socket_server impl
public:
    bool init() override;
    void fini() override;

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

public:
    // get io statistics
    std::shared_ptr<io_statistics> get_io_statistics()
    {
        return io_statistics_ptr_;
    }

    // options
public:
    // get/set config
    std::shared_ptr<tcp_server_acceptor_config> acceptor_config();
    void acceptor_config(std::shared_ptr<tcp_server_acceptor_config> acceptor_config_ptr);

    std::shared_ptr<tcp_server_session_config> session_config();
    void session_config(std::shared_ptr<tcp_server_session_config> session_config_ptr);

    // tcp_acceptor_handler impl
protected:
    void handle_accept_success(std::shared_ptr<tcp_acceptor> acceptor_ptr, std::shared_ptr<tcp_session> session_ptr) override;
    void handle_accept_failed(std::shared_ptr<tcp_acceptor> acceptor_ptr, std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg) override;

    // tcp_session_handler impl
protected:
    void handle_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    void handle_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    void handle_session_idle(std::shared_ptr<tcp_session> session_ptr, idle_type type) override;
    void handle_sessoin_close(std::shared_ptr<tcp_session> session_ptr) override;

private:
    // accept client
    bool do_accept(std::shared_ptr<tcp_acceptor> acceptor_ptr);

private:
    std::string make_key(const std::string& ip, uint16_t port);
};

}


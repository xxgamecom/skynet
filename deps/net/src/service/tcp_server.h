#pragma once

#include "service/tcp_server_i.h"
#include "base/io_statistics_i.h"

#include "../uri/uri_codec.h"

#include "../transport/tcp_acceptor.h"
#include "../transport/tcp_session.h"

#include "tcp_server_acceptor_config.h"
#include "tcp_server_session_config.h"

#include "../base/io_service_pool.h"
#include "../session/socket_manager.h"
#include "../session/session_idle_checker.h"


// forward delcare
namespace skynet::net {
class tcp_server_handler;
}

namespace skynet::net::impl {

// tcp server
class tcp_server_impl : public tcp_server,
                        public tcp_acceptor_handler,
                        public tcp_session_handler,
                        public std::enable_shared_from_this<tcp_server_impl>,
                        public asio::noncopyable
{
    typedef std::map<std::string, std::shared_ptr<tcp_acceptor>> acceptor_map;
protected:
    bool is_inited = false;                                                 // is initialized
    uint32_t socket_id_ = INVALID_SESSION_ID;                               // server logic id
    uint32_t svc_handle_ = 0;                                               // skynet service id

    std::shared_ptr<tcp_server_handler> event_handler_ptr_;                 // event handler

    // server config
    std::shared_ptr<tcp_server_acceptor_config> acceptor_config_ptr_;       // acceptor config
    std::shared_ptr<tcp_server_session_config> session_config_ptr_;         // session config

    // acceptor
    std::shared_ptr<io_service> acceptor_ios_ptr_;                          // for accept only
    acceptor_map acceptors_;                                                // acceptor map (k: `ip:port`, v: tcp_acceptor)

    // session
    std::shared_ptr<io_service_pool> session_ios_pool_ptr_;
    std::shared_ptr<session_manager> session_manager_ptr_;

public:
    tcp_server_impl(uint32_t svc_handle, uint32_t socket_id,
                    std::shared_ptr<io_service> acceptor_ios_ptr,
                    std::shared_ptr<io_service_pool> session_ios_pool_ptr,
                    std::shared_ptr<session_manager> session_manager_ptr,
                    std::shared_ptr<tcp_server_acceptor_config> acceptor_config_ptr,
                    std::shared_ptr<tcp_server_session_config> session_config_ptr);
    ~tcp_server_impl() override = default;

public:
    bool init() override;
    void fini() override;

    void set_event_handler(std::shared_ptr<tcp_server_handler> event_handler_ptr) override;

public:
    // start/close
    bool open(std::string local_uri) override;
    bool open(std::string local_ip, uint16_t local_port) override;
    bool open(std::initializer_list<std::pair<std::string, uint16_t>> local_endpoints) override;
    void close() override;

    // tcp_acceptor_handler impl
protected:
    // 接收连接成功
    void handle_accept_success(std::shared_ptr<tcp_acceptor> acceptor_ptr,
                               std::shared_ptr<tcp_session> session_ptr) override;
    // 接收连接失败
    void handle_accept_failed(std::shared_ptr<tcp_acceptor> acceptor_ptr,
                              std::shared_ptr<tcp_session> session_ptr,
                              int32_t err_code, std::string err_msg) override;

    // tcp_session_handler impl
protected:
    // tcp session read complete
    void handle_tcp_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    // tcp session write complete
    void handle_tcp_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    // tcp session idle
    void handle_tcp_session_idle(std::shared_ptr<tcp_session> session_ptr, idle_type type) override;
    // tcp session closed
    void handle_tcp_sessoin_close(std::shared_ptr<tcp_session> session_ptr) override;

protected:
    // accept client
    bool do_accept(std::shared_ptr<tcp_acceptor> acceptor_ptr);

private:
    std::string make_key(const std::string& ip, uint16_t port);
};

}



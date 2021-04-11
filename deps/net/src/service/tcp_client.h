#pragma once

#include "service/tcp_client_i.h"
#include "service/tcp_client_handler_i.h"

#include "../uri/uri_codec.h"

#include "../base/io_service.h"

#include "../transport/tcp_connector.h"
#include "../transport/tcp_session.h"

#include "tcp_client_config.h"

#include <memory>

// forward declare
namespace skynet::net {
class tcp_client_handler;
}

namespace skynet::net::impl {

// tcp client implement
class tcp_client_impl : public tcp_client,
                        public tcp_connector_handler,
                        public tcp_session_handler,
                        public std::enable_shared_from_this<tcp_client_impl>,
                        public asio::noncopyable
{
protected:
    uint32_t socket_id_ = INVALID_SOCKET_ID;                // client socket id

    tcp_client_session_config_impl session_config_;         // tcp session config

protected:
    std::shared_ptr<io_service> ios_ptr_;                   // io service

    std::shared_ptr<tcp_connector> connector_ptr_;
    std::shared_ptr<tcp_session> session_ptr_;

    std::shared_ptr<tcp_client_handler> event_handler_ptr_;

public:
    tcp_client_impl(uint32_t socket_id, std::shared_ptr<io_service> ios_ptr);
    ~tcp_client_impl() override = default;

    // tcp_client impl
public:
    bool init() override;
    void fini() override;

    // set tcp client event handler
    void set_event_handler(std::shared_ptr<tcp_client_handler> event_handler_ptr) override;

    bool connect(std::string remote_uri, int32_t timeout_seconds = 0,
                 std::string local_ip = "", uint16_t local_port = 0) override;
    bool connect(std::string remote_addr, uint16_t remote_port, int32_t timeout_seconds = 0,
                 std::string local_ip = "", uint16_t local_port = 0) override;

    void close() override;

    uint32_t socket_id() override;

    tcp_client_session_config& session_config() override;

    bool send(const char* data_ptr, int32_t data_len) override;

    // tcp_connector_handler impl
protected:
    // 地址解析成功
    void handle_resolve_success(std::shared_ptr<tcp_session> session_ptr, std::string addr, uint16_t port) override;
    // 地址解析失败
    void handle_resolve_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg) override;

    // 主动连接成功
    void handle_connect_success(std::shared_ptr<tcp_session> session_ptr) override;
    // 主动连接失败
    void handle_connect_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg) override;
    // 超时处理
    void handle_connect_timeout(std::shared_ptr<tcp_session> session_ptr) override;

    // tcp_session_handler impl
protected:
    // tcp会话读完成
    void handle_tcp_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    // tcp会话写完成
    void handle_tcp_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    // tcp会话闲置
    void handle_tcp_session_idle(std::shared_ptr<tcp_session> session_ptr, session_idle_type type) override;
    // tcp会话关闭
    void handle_tcp_sessoin_close(std::shared_ptr<tcp_session> session_ptr) override;
};

}



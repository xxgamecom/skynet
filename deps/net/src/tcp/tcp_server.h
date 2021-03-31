#pragma once

#include "tcp/tcp_server_i.h"
#include "base/io_statistics_i.h"

#include "../uri/uri_codec.h"

#include "../core/io_service_pool.h"

#include "tcp_acceptor.h"
#include "tcp_server_config.h"
#include "tcp_session_manager.h"
#include "tcp_session_idle_checker.h"

namespace skynet { namespace net {

class tcp_server_handler;

namespace impl {

// tcp服务端
class tcp_server_impl : public asio::noncopyable,
                        public tcp_server,
                        public tcp_acceptor_handler,
                        public tcp_session_handler,
                        public std::enable_shared_from_this<tcp_server_impl>
{
protected:
    typedef std::map<std::string, std::shared_ptr<tcp_acceptor>> acceptor_map;

    // 服务配置
protected:
    tcp_server_acceptor_config_impl acceptor_config_;                       // 服务端的acceptor配置
    tcp_server_session_config_impl session_config_;                         // 服务端的session配置

    // 外部处理器
protected:
    std::shared_ptr<tcp_server_handler> event_handler_ptr_;                 // 外部事件处理器

    // acceptor相关
protected:
    std::shared_ptr<io_service> acceptor_ios_ptr_;                          // acceptor的ios(独立)
    acceptor_map acceptors_;                                                // acceptor表(key为对端ip+":"+port串)

    std::shared_ptr<io_service_pool> session_ios_pool_ptr_;                 // session的ios池
    std::shared_ptr<tcp_session_manager> session_manager_ptr_;              // 会话管理
    std::shared_ptr<tcp_session_idle_checker> session_idle_checker_ptr_;    // 会话闲置检测器(共用acceptor的ios)

    // IO统计
protected:
    std::shared_ptr<io_statistics> io_statistics_ptr_;                      // IO统计

public:
    tcp_server_impl() = default;
    virtual ~tcp_server_impl() = default;

public:
    // 设置外部服务处理器
    void set_event_handler(std::shared_ptr<tcp_server_handler> event_handler_ptr) override;

public:
    // start/close
    bool open(const std::string local_uri, bool is_reuse_addr = true) override;
    bool open(const std::string local_ip, const uint16_t local_port, bool is_reuse_addr = true) override;
    bool open(std::initializer_list<std::pair<std::string, uint16_t>> local_endpoints, bool is_reuse_addr = true) override;
    void close() override;

    // get config
    tcp_server_acceptor_config& get_acceptor_config() override;
    tcp_server_session_config& get_session_config() override;

    // get io statistics
    std::shared_ptr<io_statistics> get_io_statistics() override;

protected:
    // do accept
    bool do_accept(std::shared_ptr<tcp_acceptor> acceptor_ptr);

    // tcp_acceptor_handler impl
protected:
    // 接收连接成功
    virtual void handle_accept_success(std::shared_ptr<tcp_acceptor> acceptor_ptr,
                                       std::shared_ptr<tcp_session> session_ptr) override;
    // 接收连接失败
    virtual void handle_accept_failed(std::shared_ptr<tcp_acceptor> acceptor_ptr,
                                      std::shared_ptr<tcp_session> session_ptr,
                                      int32_t err_code, std::string err_msg) override;

    // tcp_session_handler impl
protected:
    // tc session read complete
    virtual void handle_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    // tcp session write complete
    virtual void handle_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    // tcp session idle
    virtual void handle_session_idle(std::shared_ptr<tcp_session> session_ptr, idle_type type) override;
    // tcp session closed
    virtual void handle_sessoin_close(std::shared_ptr<tcp_session> session_ptr) override;

private:
    std::string make_key(const asio::ip::tcp::endpoint& ep);
    std::string make_key(const std::string& ip, const uint16_t port);
};

} } }

#include "tcp_server.inl"


#pragma once

#include "../core/io_service.h"
#include "../core/tcp_connector.h"
#include "../core/tcp_session.h"

#include "tcp_client_config.h"

#include "../uri/uri_codec.h"

#include <memory>

namespace skynet { namespace network {

class tcp_client_handler;

// tcp客户端
class tcp_client : public tcp_connector_handler,
                   public tcp_session_handler,
                   public std::enable_shared_from_this<tcp_client>
{
protected:
    tcp_client_session_config session_config_;              // 会话配置

protected:
    std::shared_ptr<io_service> ios_ptr_;                   // ios

    std::shared_ptr<tcp_connector> connector_ptr_;          // 主动连接器
    std::shared_ptr<tcp_session> session_ptr_;              // tcp会话

    std::shared_ptr<tcp_client_handler> event_handler_ptr_; // 外部事件处理器

public:
    tcp_client() = default;
    virtual ~tcp_client() = default;

public:
    // 打开客户端服务(做一些初始化工作)
    bool open();

    // 发起连接(提供URI字符串形式)
    bool connect(const std::string remote_uri,
                 int32_t timeout_seconds = 0,
                 const std::string local_ip = "",
                 const uint16_t local_port = 0);
    // 发起连接(单独提供地址和端口形式)
    bool connect(const std::string remote_addr,
                 const uint16_t remote_port,
                 int32_t timeout_seconds = 0,
                 const std::string local_ip = "",
                 const uint16_t local_port = 0);

    // 关闭客户端服务
    void close();

    // 设置客户端服务外部处理器
    void set_event_handler(std::shared_ptr<tcp_client_handler> event_handler_ptr);

    // 获取会话配置
    tcp_client_session_config& get_session_config();

    // 发送数据
    bool send(const char* data_ptr, int32_t data_len);

    // tcp_connector_handler impl
protected:
    // 地址解析成功
    virtual void handle_resolve_success(std::shared_ptr<tcp_session> session_ptr, std::string addr, uint16_t port) override;
    // 地址解析失败
    virtual void handle_resolve_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg) override;

    // 主动连接成功
    virtual void handle_connect_success(std::shared_ptr<tcp_session> session_ptr) override;
    // 主动连接失败
    virtual void handle_connect_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg) override;
    // 超时处理
    virtual void handle_connect_timeout(std::shared_ptr<tcp_session> session_ptr) override;

    // tcp_session_handler impl
protected:
    // tcp会话读完成
    virtual void handle_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    // tcp会话写完成
    virtual void handle_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    // tcp会话闲置
    virtual void handle_session_idle(std::shared_ptr<tcp_session> session_ptr, idle_type type) override;
    // tcp会话关闭
    virtual void handle_sessoin_close(std::shared_ptr<tcp_session> session_ptr) override;

    // noncopyable
private:
    tcp_client(const tcp_client&) = delete;
    tcp_client& operator=(const tcp_client&) = delete;
};

} }

#include "tcp_client.inl"

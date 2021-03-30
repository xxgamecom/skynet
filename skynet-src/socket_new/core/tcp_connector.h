#pragma once

#include "../core/io_service.h"

#include "tcp_connector_handler.h"

#include <string>
#include <memory>
#include <cstdint>

namespace skynet { namespace network {

class tcp_session;

// tcp主动连接器, 连接成功后会初始化外部传入的tcp会话对象
class tcp_connector : public std::enable_shared_from_this<tcp_connector>
{
protected:
    std::shared_ptr<io_service> ios_ptr_;                       // ios(取自io_service_pool)

    asio::ip::tcp::resolver resolver_;                          // 地址解析器
    std::shared_ptr<asio::deadline_timer> connect_timer_ptr_;   // 连接定时器, 连接超时控制
    bool is_connecting_ = false;                                // 是否正在连接

    std::shared_ptr<tcp_connector_handler> event_handler_ptr_;  // 外部主动连接事件处理器

public:
    explicit tcp_connector(std::shared_ptr<io_service> ios_ptr);
    ~tcp_connector() = default;

public:
    // 设置连接事件处理器
    void set_event_handler(std::shared_ptr<tcp_connector_handler> event_handler_ptr);

    // 连接(超时包括地址解析和实际连接所需时间)
    bool connect(std::shared_ptr<tcp_session> session_ptr,
                 const std::string remote_addr,
                 const uint16_t remote_port,
                 int32_t timeout_seconds = 0,
                 const std::string local_ip = "",
                 const uint16_t local_port = 0);

    // 连接定时器
protected:
    // 启动连接定时器
    bool start_connect_timer(std::shared_ptr<tcp_session> session_ptr, int32_t timeout_seconds);
    // 关闭连接定时器
    void stop_connect_timer();

    // 处理函数
protected:
    // 处理异步地址解析
    void handle_async_resolve(std::shared_ptr<tcp_session> session_ptr,
                              asio::ip::tcp::resolver::iterator endpoint_itr,
                              const asio::error_code& ec);
    // 处理异步连接
    void handle_async_connect(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec);

    // 处理连接超时
    void handle_timeout(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec);

    // noncopyable
private:
    tcp_connector(const tcp_connector&) = delete;
    tcp_connector& operator=(const tcp_connector&) = delete;
};

}}

#include "tcp_connector.inl"


#pragma once

#include "../core/io_service.h"

#include "socket_option_def.h"
#include "tcp_acceptor_def.h"
#include "tcp_acceptor_handler.h"

namespace skynet { namespace network {

class tcp_session;

// tcp被动连接器(用于接收来自远端的连接)
class tcp_acceptor : public std::enable_shared_from_this<tcp_acceptor>
{
protected:
    std::shared_ptr<io_service> ios_ptr_;                       // acceptor使用独立的ios, 不和会话公用
    std::shared_ptr<asio::ip::tcp::acceptor> acceptor_ptr_;     // acceptor

    std::shared_ptr<tcp_acceptor_handler> event_handler_ptr_;   // 外部被动连接事件处理器

public:
    explicit tcp_acceptor(std::shared_ptr<io_service> ios_ptr,
                          std::shared_ptr<tcp_acceptor_handler> event_handler_ptr = nullptr);
    ~tcp_acceptor() = default;

public:
    // 设置接收事件处理器
    void set_event_handler(std::shared_ptr<tcp_acceptor_handler> event_handler_ptr);

public:
    // 打开acceptor
    bool open(const std::string local_ip, const uint16_t local_port, bool is_reuse_addr = true, int32_t backlog = DEFAULT_BACKLOG);
    // 关闭acceptor
    void close();

    // 投递一次异步accept
    void accept_once(std::shared_ptr<tcp_session> session_ptr);

    // 本地端点信息
    asio::ip::tcp::endpoint local_endpoint() const;

    // socket选项
public:
    bool set_sock_option(sock_options opt, int32_t value);
    bool get_sock_option(sock_options opt, int32_t& value);

    // 处理函数
protected:
    // 处理接收
    void handle_async_accept(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec);

    // noncopyable
private:
    tcp_acceptor(const tcp_acceptor&) = delete;
    tcp_acceptor& operator=(const tcp_acceptor&) = delete;
};

}}

#include "tcp_acceptor.inl"


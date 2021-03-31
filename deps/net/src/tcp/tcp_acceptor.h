#pragma once

#include "../core/io_service.h"

#include "base/socket_option_def.h"

#include "tcp/tcp_acceptor_def.h"
#include "tcp/tcp_acceptor_i.h"
#include "tcp/tcp_acceptor_handler_i.h"

namespace skynet { namespace net {

class tcp_session;

namespace impl {

// tcp被动连接器(用于接收来自远端的连接)
class tcp_acceptor_impl : public asio::noncopyable,
                          public tcp_acceptor,
                          public std::enable_shared_from_this<tcp_acceptor_impl>
{
protected:
    std::shared_ptr<io_service> ios_ptr_;                       // acceptor使用独立的ios, 不和会话公用
    std::shared_ptr<asio::ip::tcp::acceptor> acceptor_ptr_;     // acceptor

    std::shared_ptr<tcp_acceptor_handler> event_handler_ptr_;   // 外部被动连接事件处理器

public:
    explicit tcp_acceptor_impl(std::shared_ptr<io_service> ios_ptr,
                               std::shared_ptr<tcp_acceptor_handler> event_handler_ptr = nullptr);
    ~tcp_acceptor_impl() = default;

    // tcp_acceptor impl
public:
    // 设置接收事件处理器
    void set_event_handler(std::shared_ptr<tcp_acceptor_handler> event_handler_ptr) override;

    // 打开acceptor
    bool open(const std::string local_ip, const uint16_t local_port, bool is_reuse_addr = true, int32_t backlog = DEFAULT_BACKLOG) override;
    // 关闭acceptor
    void close() override;

public:
    // 投递一次异步accept
    void accept_once(std::shared_ptr<tcp_session> session_ptr) override;

    // 本地端点信息
    asio::ip::tcp::endpoint local_endpoint() const;

    // socket选项
public:
    bool set_sock_option(sock_options opt, int32_t value) override;
    bool get_sock_option(sock_options opt, int32_t& value) override;

    // 处理函数
protected:
    // 处理接收
    void handle_async_accept(std::shared_ptr<tcp_session> session_ptr, const asio::error_code& ec);
};

} } }

#include "tcp_acceptor.inl"


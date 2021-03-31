#include "tcp_client.h"
#include "tcp_client_handler.h"

namespace skynet { namespace net {

// 设置客户端服务外部处理器
void tcp_client::set_event_handler(std::shared_ptr<tcp_client_handler> event_handler_ptr)
{
    event_handler_ptr_ = event_handler_ptr;
}

// 打开客户端服务(做一些初始化工作)
bool tcp_client::open()
{
    // 创建ios
    ios_ptr_ = std::make_shared<io_service>();
    if (ios_ptr_ == nullptr) return false;
    ios_ptr_->run();

    // 创建连接器
    connector_ptr_ = std::make_shared<tcp_connector>(ios_ptr_);
    if (connector_ptr_ == nullptr) return false;
    connector_ptr_->set_event_handler(shared_from_this());

    // 创建tcp会话
    session_ptr_ = std::make_shared<tcp_session>(session_config_.msg_read_buf_size(),
                                                 session_config_.msg_write_buf_size(),
                                                 session_config_.msg_write_queue_size());
    if (session_ptr_ == nullptr) return false;
    session_ptr_->set_event_handler(shared_from_this());

    return true;
}

// 发起连接(提供URI字符串形式)
bool tcp_client::connect(const std::string remote_uri,
                         int32_t timeout_seconds/* = 0*/,
                         const std::string local_ip/* = ""*/,
                         const uint16_t local_port/* = 0*/)
{
    // 连接之前确保已初始化
    assert(connector_ptr_ != nullptr && session_ptr_ != nullptr);
    if (connector_ptr_ == nullptr || session_ptr_ == nullptr) return false;

    // 发起连接
    uri_codec uri(uri_codec::from_string(remote_uri));
    if (uri.is_valid() == false) return false;

    return connector_ptr_->connect(session_ptr_,
                                   uri.host().value(),
                                   uri.port().value(),
                                   timeout_seconds,
                                   local_ip,
                                   local_port);
}

// 发起连接(单独提供地址和端口形式)
bool tcp_client::connect(const std::string remote_addr,
                         const uint16_t remote_port,
                         int32_t timeout_seconds/* = 0*/,
                         const std::string local_ip/* = ""*/,
                         const uint16_t local_port/* = 0*/)
{
    // 连接之前确保已初始化
    assert(connector_ptr_ != nullptr && session_ptr_ != nullptr);
    if (connector_ptr_ == nullptr || session_ptr_ == nullptr) return false;

    // 发起连接
    return connector_ptr_->connect(session_ptr_,
                                   remote_addr,
                                   remote_port,
                                   timeout_seconds,
                                   local_ip,
                                   local_port);
}

// 关闭客户端服务
void tcp_client::close()
{
    // 关闭会话
    if (session_ptr_ != nullptr)
    {
        session_ptr_->close();
    }

    // 清理指针
    session_ptr_.reset();
    connector_ptr_.reset();

    // 停止ios
    if (ios_ptr_ != nullptr)
    {
        ios_ptr_->stop();
    }
}

//------------------------------------------------------------------------------
// tcp_connector_handler impl
//------------------------------------------------------------------------------

// 地址解析成功
void tcp_client::handle_resolve_success(std::shared_ptr<tcp_session> session_ptr, std::string addr, uint16_t port)
{
    // 不做处理
}

// 地址解析失败
void tcp_client::handle_resolve_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg)
{
    // 解析地址失败当成连接失败回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_connect_failed(session_ptr, err_code, err_msg);
}

// 主动连接成功
void tcp_client::handle_connect_success(std::shared_ptr<tcp_session> session_ptr)
{
    // 设置会话的socket选项
    session_ptr->set_sock_option(SOCK_OPT_RECV_BUFFER, session_config_.socket_recv_buf_size());
    session_ptr->set_sock_option(SOCK_OPT_SEND_BUFFER, session_config_.socket_send_buf_size());
    session_ptr->set_sock_option(SOCK_OPT_KEEPALIVE,   session_config_.socket_keepalive() ? 1 : 0);
    session_ptr->set_sock_option(SOCK_OPT_NODELAY,     session_config_.socket_nodelay() ? 1 : 0);
    session_ptr->set_sock_option(SOCK_OPT_LINGER,      session_config_.socket_linger());

    // 连接成功回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_connect_success(session_ptr);

    // 开始读数据
    if (session_ptr_ != nullptr)
        session_ptr_->start_read();
}

// 主动连接失败
void tcp_client::handle_connect_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg)
{
    // 连接失败回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_connect_failed(session_ptr, err_code, err_msg);

    // 关闭会话
    if (session_ptr_ != nullptr)
        session_ptr_->close();
}

// 超时处理
void tcp_client::handle_connect_timeout(std::shared_ptr<tcp_session> session_ptr)
{
    // 连接超时回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_connect_timeout(session_ptr);

    // 关闭会话
    if (session_ptr_ != nullptr)
        session_ptr_->close();
}

//------------------------------------------------------------------------------
// tcp_session_handler impl
//------------------------------------------------------------------------------

// tcp会话读完成
void tcp_client::handle_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len)
{
    // 读取会话回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_session_read(session_ptr, data_ptr, data_len);
}

// tcp会话写完成
void tcp_client::handle_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len)
{
    // 写入会话回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_session_write(session_ptr, data_ptr, data_len);
}

// tcp会话闲置
void tcp_client::handle_session_idle(std::shared_ptr<tcp_session> session_ptr, idle_type type)
{
    // 客户端不需要闲置处理
}

// tcp会话关闭
void tcp_client::handle_sessoin_close(std::shared_ptr<tcp_session> session_ptr)
{
    // 会话关闭回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_sessoin_close(session_ptr);
}

} }

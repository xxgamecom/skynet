#include "tcp_client.h"

#include "../transport/tcp_connector.h"
#include "../transport/tcp_session.h"

namespace skynet::net::impl {

tcp_client_impl::tcp_client_impl(uint32_t socket_id, std::shared_ptr<io_service> ios_ptr)
:
socket_id_(socket_id),
ios_ptr_(ios_ptr)
{
}

bool tcp_client_impl::init()
{
    // check io service
    assert(ios_ptr_ != nullptr);
    if (ios_ptr_ == nullptr)
        return false;

    // create connector
    connector_ptr_ = std::make_shared<tcp_connector_impl>(ios_ptr_);
    if (connector_ptr_ == nullptr)
        return false;
    connector_ptr_->set_event_handler(shared_from_this());

    // create tcp session
    session_ptr_ = std::make_shared<tcp_session_impl>(session_config_.read_buf_size(),
                                                      session_config_.write_buf_size(),
                                                      session_config_.write_queue_size());
    if (session_ptr_ == nullptr)
        return false;
    session_ptr_->set_event_handler(shared_from_this());

    return true;
}

void tcp_client_impl::fini()
{

}

void tcp_client_impl::set_event_handler(std::shared_ptr<tcp_client_handler> event_handler_ptr)
{
    event_handler_ptr_ = event_handler_ptr;
}

bool tcp_client_impl::connect(std::string remote_uri, int32_t timeout_seconds/* = 0*/,
                              std::string local_ip/* = ""*/, uint16_t local_port/* = 0*/)
{
    // check remote uri
    uri_codec uri(uri_codec::from_string(remote_uri));
    if (!uri.is_valid())
        return false;

    return connect(uri.host().value(), uri.port().value(), timeout_seconds, local_ip, local_port);
}

bool tcp_client_impl::connect(std::string remote_addr, uint16_t remote_port, int32_t timeout_seconds/* = 0*/,
                              std::string local_ip/* = ""*/, uint16_t local_port/* = 0*/)
{
    // ensure has been initialized
    assert(connector_ptr_ != nullptr && session_ptr_ != nullptr);
    if (connector_ptr_ == nullptr || session_ptr_ == nullptr)
        return false;

    return connector_ptr_->connect(session_ptr_, remote_addr, remote_port,
                                   timeout_seconds, local_ip, local_port);
}

void tcp_client_impl::close()
{
    // close session
    if (session_ptr_ != nullptr)
    {
        session_ptr_->close();
    }

    // clear
    session_ptr_.reset();
    connector_ptr_.reset();

    // clear io service
    ios_ptr_.reset();
}

// get client socket id
uint32_t tcp_client_impl::socket_id()
{
    return socket_id_;
}

tcp_client_session_config& tcp_client_impl::session_config()
{
    return session_config_;
}

bool tcp_client_impl::send(const char* data_ptr, int32_t data_len)
{
    assert(session_ptr_ != nullptr && session_ptr_->is_open());
    if (session_ptr_ == nullptr || !session_ptr_->is_open())
        return false;

    if (!session_ptr_->write(data_ptr, data_len))
        return false;

    return true;
}


//----------------------------------------------------
// tcp_connector_handler impl
//----------------------------------------------------

// 地址解析成功
void tcp_client_impl::handle_resolve_success(std::shared_ptr<tcp_session> session_ptr, std::string addr, uint16_t port)
{
    // 不做处理
}

// 地址解析失败
void tcp_client_impl::handle_resolve_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg)
{
    // 解析地址失败当成连接失败回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_tcp_client_connect_failed(session_ptr, err_code, err_msg);
}

// 主动连接成功
void tcp_client_impl::handle_connect_success(std::shared_ptr<tcp_session> session_ptr)
{
    // 设置会话的socket选项
    session_ptr->set_sock_option(SOCK_OPT_RECV_BUFFER, session_config_.socket_recv_buf_size());
    session_ptr->set_sock_option(SOCK_OPT_SEND_BUFFER, session_config_.socket_send_buf_size());
    session_ptr->set_sock_option(SOCK_OPT_KEEPALIVE,   session_config_.socket_keepalive() ? 1 : 0);
    session_ptr->set_sock_option(SOCK_OPT_NODELAY,     session_config_.socket_nodelay() ? 1 : 0);
    session_ptr->set_sock_option(SOCK_OPT_LINGER,      session_config_.socket_linger());

    // 连接成功回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_tcp_client_connect_success(session_ptr);

    // 开始读数据
    if (session_ptr_ != nullptr)
        session_ptr_->start_read();
}

// 主动连接失败
void tcp_client_impl::handle_connect_failed(std::shared_ptr<tcp_session> session_ptr, int32_t err_code, std::string err_msg)
{
    // 连接失败回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_tcp_client_connect_failed(session_ptr, err_code, err_msg);

    // 关闭会话
    if (session_ptr_ != nullptr)
        session_ptr_->close();
}

// 超时处理
void tcp_client_impl::handle_connect_timeout(std::shared_ptr<tcp_session> session_ptr)
{
    // 连接超时回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_tcp_client_connect_timeout(session_ptr);

    // 关闭会话
    if (session_ptr_ != nullptr)
        session_ptr_->close();
}

//----------------------------------------------------
// tcp_session_handler impl
//----------------------------------------------------

// tcp会话读完成
void tcp_client_impl::handle_tcp_session_read(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len)
{
    // 读取会话回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_tcp_client_read(session_ptr, data_ptr, data_len);
}

// tcp会话写完成
void tcp_client_impl::handle_tcp_session_write(std::shared_ptr<tcp_session> session_ptr, char* data_ptr, size_t data_len)
{
    // 写入会话回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_tcp_client_write(session_ptr, data_ptr, data_len);
}

// tcp会话闲置
void tcp_client_impl::handle_tcp_session_idle(std::shared_ptr<tcp_session> session_ptr, idle_type type)
{
    // do nothing
}

// tcp会话关闭
void tcp_client_impl::handle_tcp_sessoin_close(std::shared_ptr<tcp_session> session_ptr)
{
    // 会话关闭回调
    if (event_handler_ptr_ != nullptr)
        event_handler_ptr_->handle_tcp_client_close(session_ptr);
}

}

#include "net.h"

#include "session/io_statistics.h"
#include "session/socket_manager.h"

#include "tcp/tcp_client.h"
#include "tcp/tcp_client_config.h"
#include "tcp/tcp_server.h"
#include "tcp/tcp_server_acceptor_config.h"
#include "tcp/tcp_server_session_config.h"

#include "udp/udp_client.h"
#include "udp/udp_server.h"

#include "socket_server.h"

namespace skynet::net {

// create io service
std::shared_ptr<io_service> create_io_service()
{
    return std::make_shared<impl::io_service_impl>();
}

std::shared_ptr<io_service_pool> create_io_service_pool(uint32_t pool_size)
{
    return std::make_shared<impl::io_service_pool_impl>(pool_size);
}

// create tcp acceptor
std::shared_ptr<tcp_acceptor> create_tcp_acceptor(std::string acceptor_id, uint32_t socket_id, uint32_t svc_handle, std::shared_ptr<io_service> ios_ptr)
{
    return std::make_shared<impl::tcp_acceptor_impl>(acceptor_id, socket_id, svc_handle, ios_ptr);
}

// create tcp connector
std::shared_ptr<tcp_connector> create_tcp_connector(std::shared_ptr<io_service> ios_ptr)
{
    return std::make_shared<impl::tcp_connector_impl>(ios_ptr);
}

// create tcp session
std::shared_ptr<tcp_session> create_tcp_session(int32_t msg_read_buf_size, int32_t msg_write_buf_size, int32_t msg_write_queue_size)
{
    return std::make_shared<impl::tcp_session_impl>(msg_read_buf_size, msg_write_buf_size, msg_write_queue_size);
}

// create io statistics
std::shared_ptr<io_statistics> create_io_statistics(std::shared_ptr<session_manager> session_manager_ptr,
                                                    std::shared_ptr<io_service> ios_ptr)
{
    return std::make_shared<impl::io_statistics_impl>(session_manager_ptr, ios_ptr);
}

// create session manager
std::shared_ptr<session_manager> create_session_manager()
{
    return std::make_shared<impl::socket_manager_impl>();
}

// create tcp client
std::shared_ptr<tcp_client> create_tcp_client(uint32_t socket_id)
{
    return std::make_shared<impl::tcp_client_impl>(socket_id);
}

// create tcp server
std::shared_ptr<tcp_server> create_tcp_server(std::shared_ptr<io_service> acceptor_ios_ptr,
                                              std::shared_ptr<session_manager> session_manager_ptr,
                                              std::shared_ptr<tcp_server_acceptor_config> acceptor_config_ptr,
                                              std::shared_ptr<tcp_server_session_config> session_config_ptr)
{
    return std::make_shared<impl::tcp_server_impl>(acceptor_ios_ptr, session_manager_ptr, acceptor_config_ptr, session_config_ptr);
}

// create tcp server acceptor config
std::shared_ptr<tcp_server_acceptor_config> create_tcp_server_acceptor_config()
{
    return std::make_shared<impl::tcp_server_acceptor_config_impl>();
}

// create tcp server session config
std::shared_ptr<tcp_server_session_config> create_tcp_server_session_config()
{
    return std::make_shared<impl::tcp_server_session_config_impl>();
}

// create udp client
std::shared_ptr<udp_client> create_udp_client()
{
    return std::make_shared<impl::udp_client_impl>();
}

// create udp server
std::shared_ptr<udp_server> create_udp_server()
{
    return std::make_shared<impl::udp_server_impl>();
}


// create socket server
std::shared_ptr<socket_server> create_socket_server()
{
    return std::make_shared<impl::socket_server_impl>();
}

}


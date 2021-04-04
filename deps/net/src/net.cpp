#include "net.h"

#include "tcp/tcp_client.h"
#include "tcp/tcp_server.h"

#include "udp/udp_client.h"
#include "udp/udp_server.h"

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
std::shared_ptr<tcp_acceptor> create_tcp_acceptor(std::shared_ptr<io_service> ios_ptr)
{
    return std::make_shared<impl::tcp_acceptor_impl>(ios_ptr);
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

// create tcp client
std::shared_ptr<tcp_client> create_tcp_client()
{
    return std::make_shared<impl::tcp_client_impl>();
}

// create tcp server
std::shared_ptr<tcp_server> create_tcp_server()
{
    return std::make_shared<impl::tcp_server_impl>();
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

}


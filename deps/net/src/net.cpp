#include "net.h"

#include "tcp/tcp_client.h"
#include "tcp/tcp_server.h"

namespace skynet { namespace net {

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

} }


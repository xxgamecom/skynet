#pragma once

// tcp
#include "tcp_client_i.h"
#include "tcp_client_handler_i.h"
#include "tcp_server_i.h"
#include "tcp_server_handler_i.h"

// udp

namespace skynet { namespace net {

// create tcp client
std::shared_ptr<tcp_client> create_tcp_client();
// create tcp server
std::shared_ptr<tcp_server> create_tcp_server();

} }



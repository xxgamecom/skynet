#pragma once

// ios
#include "base/io_service_i.h"
#include "base/io_service_pool_i.h"

// statistics
#include "base/io_statistics_i.h"

// tcp
#include "tcp/tcp_connector_i.h"
#include "tcp/tcp_connector_handler_i.h"

#include "tcp/tcp_acceptor_i.h"
#include "tcp/tcp_acceptor_handler_i.h"

#include "tcp/tcp_session_i.h"
#include "tcp/tcp_session_handler_i.h"

#include "tcp/tcp_client_i.h"
#include "tcp/tcp_client_config_i.h"
#include "tcp/tcp_client_handler_i.h"

#include "tcp/tcp_server_i.h"
#include "tcp/tcp_server_config_i.h"
#include "tcp/tcp_server_handler_i.h"

// udp

namespace skynet::net {

// create io service
std::shared_ptr<io_service> create_io_service();
// create io service pool
std::shared_ptr<io_service_pool> create_io_service_pool(uint32_t pool_size);

// create tcp session
std::shared_ptr<tcp_session> create_tcp_session(int32_t msg_read_buf_size, int32_t msg_write_buf_size, int32_t msg_write_queue_size);
// create tcp connector
std::shared_ptr<tcp_connector> create_tcp_connector(std::shared_ptr<io_service> ios_ptr);

// create tcp client
std::shared_ptr<tcp_client> create_tcp_client();
// create tcp server
std::shared_ptr<tcp_server> create_tcp_server();

}



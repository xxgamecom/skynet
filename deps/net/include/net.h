#pragma once

// ios
#include "base/io_service_i.h"
#include "base/io_service_pool_i.h"

// statistics
#include "base/io_statistics_i.h"

// tcp
#include "transport/tcp_connector_i.h"
#include "transport/tcp_connector_handler_i.h"

#include "transport/tcp_acceptor_i.h"
#include "transport/tcp_acceptor_handler_i.h"

#include "transport/tcp_session_i.h"
#include "transport/tcp_session_handler_i.h"

#include "service/tcp_client_i.h"
#include "service/tcp_client_config_i.h"
#include "service/tcp_client_handler_i.h"

#include "service/tcp_server_i.h"
#include "service/tcp_server_config_i.h"
#include "service/tcp_server_handler_i.h"

// udp
#include "service/udp_client_i.h"
#include "service/udp_client_handler_i.h"

#include "service/udp_server_i.h"
#include "service/udp_server_handle_i.h"

//
#include "base/net_manager_i.h"

// network
#include "service/network_i.h"
#include "service/network_handler_i.h"

namespace skynet::net {

//-----------------------------------------------
// net components
//-----------------------------------------------

// create io service
std::shared_ptr<io_service> create_io_service();
// create io service pool
std::shared_ptr<io_service_pool> create_io_service_pool(uint32_t pool_size);

// create tcp acceptor
std::shared_ptr<tcp_acceptor> create_tcp_acceptor(std::shared_ptr<io_service> ios_ptr);
// create tcp connector
std::shared_ptr<tcp_connector> create_tcp_connector(std::shared_ptr<io_service> ios_ptr);
// create tcp session
std::shared_ptr<tcp_session> create_tcp_session(int32_t msg_read_buf_size, int32_t msg_write_buf_size, int32_t msg_write_queue_size);

// create io statistics
std::shared_ptr<io_statistics> create_io_statistics(std::shared_ptr<net_manager> net_manager_ptr,
                                                    std::shared_ptr<io_service> ios_ptr);

// create session manager
std::shared_ptr<net_manager> create_session_manager();

//-----------------------------------------------
// client & server
//-----------------------------------------------

// create tcp client
std::shared_ptr<tcp_client> create_tcp_client(uint32_t socket_id, std::shared_ptr<io_service> ios_ptr);
// create tcp server
std::shared_ptr<tcp_server> create_tcp_server(uint32_t svc_handle, uint32_t socket_id,
                                              std::shared_ptr<io_service> acceptor_ios_ptr,
                                              std::shared_ptr<io_service_pool> session_ios_pool_ptr,
                                              std::shared_ptr<net_manager> net_manager_ptr,
                                              std::shared_ptr<tcp_server_acceptor_config> acceptor_config_ptr,
                                              std::shared_ptr<tcp_server_session_config> session_config_ptr);
// create tcp server acceptor config
std::shared_ptr<tcp_server_acceptor_config> create_tcp_server_acceptor_config();
// create tcp server session config
std::shared_ptr<tcp_server_session_config> create_tcp_server_session_config();

// create udp client
std::shared_ptr<udp_client> create_udp_client();
// create udp server
std::shared_ptr<udp_server> create_udp_server();


// create network
std::shared_ptr<network> create_network();

}



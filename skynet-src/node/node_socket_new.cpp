#include "node_socket_new.h"

namespace skynet {

bool node_socket_new::init()
{
    tcp_server_ = net::create_tcp_server();
    return false;
}

void node_socket_new::fini()
{

}

void node_socket_new::handle_accept(std::shared_ptr<net::tcp_session> session_ptr)
{

}

void node_socket_new::handle_session_read(std::shared_ptr<net::tcp_session> session_ptr, char* data_ptr, size_t data_len)
{

}

void node_socket_new::handle_session_write(std::shared_ptr<net::tcp_session> session_ptr, char* data_ptr, size_t data_len)
{

}

void node_socket_new::handle_session_idle(std::shared_ptr<net::tcp_session> session_ptr, net::idle_type type)
{

}

void node_socket_new::handle_sessoin_close(std::shared_ptr<net::tcp_session> session_ptr)
{

}

}


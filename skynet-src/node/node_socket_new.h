#pragma once

#include "net.h"

namespace skynet {

class node_socket_new : public net::tcp_server_handler
{
private:
    std::shared_ptr<net::tcp_server> tcp_server_;
    std::shared_ptr<net::udp_server> udp_server_;

public:
    node_socket_new() = default;
    ~node_socket_new() override = default;

public:
    bool init();
    void fini();

public:

    // tcp_server_handler impl
public:
    void handle_accept(std::shared_ptr<net::tcp_session> session_ptr) override;
    void handle_session_read(std::shared_ptr<net::tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    void handle_session_write(std::shared_ptr<net::tcp_session> session_ptr, char* data_ptr, size_t data_len) override;
    void handle_session_idle(std::shared_ptr<net::tcp_session> session_ptr, net::idle_type type) override;
    void handle_sessoin_close(std::shared_ptr<net::tcp_session> session_ptr) override;

    // udp_server_handle_impl
public:

};

}


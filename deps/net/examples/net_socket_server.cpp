#include "net.h"

#include <iostream>

class network_handler : public skynet::net::network_tcp_server_handler
{
public:
    void handle_accept(std::shared_ptr<skynet::net::tcp_session> session_ptr) override
    {
        std::cout << "handle_accept" << std::endl;
    }
    void handle_session_read(std::shared_ptr<skynet::net::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
        std::cout << "handle_session_read" << std::endl;
    }
    void handle_session_write(std::shared_ptr<skynet::net::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
        std::cout << "handle_session_write" << std::endl;
    }
    void handle_session_idle(std::shared_ptr<skynet::net::tcp_session> session_ptr, skynet::net::idle_type type) override
    {
        std::cout << "handle_session_idle" << std::endl;
    }
    void handle_sessoin_close(std::shared_ptr<skynet::net::tcp_session> session_ptr) override
    {
        std::cout << "handle_sessoin_close" << std::endl;
    }
};

int32_t main(int32_t argc, char* argv[])
{
    auto network_ptr = skynet::net::create_network();
    auto network_handler_ptr = std::make_shared<network_handler>();
    network_ptr->set_event_handler(network_handler_ptr);
    network_ptr->init();

    uint32_t svc_handle = 1;
    auto socket_id = network_ptr->open_tcp_server("0.0.0.0:10001", svc_handle);

    ::getchar();

    network_ptr->close_tcp_server(socket_id, svc_handle);

    return 0;
}


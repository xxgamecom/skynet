#include "net.h"

class socket_server_handle : public skynet::net::socket_server_handler
{
public:
    void handle_accept(std::shared_ptr<skynet::net::tcp_session> session_ptr) override
    {

    }
    void handle_session_read(std::shared_ptr<skynet::net::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {

    }
    void handle_session_write(std::shared_ptr<skynet::net::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {

    }
    void handle_session_idle(std::shared_ptr<skynet::net::tcp_session> session_ptr, skynet::net::idle_type type) override
    {

    }
    void handle_sessoin_close(std::shared_ptr<skynet::net::tcp_session> session_ptr) override
    {

    }
};

int32_t main(int32_t argc, char* argv[])
{
    auto server_ptr = skynet::net::create_socket_server();
    server_ptr->init();

    server_ptr->listen("127.0.0.1:10001", 1);

    ::getchar();

    return 0;
}


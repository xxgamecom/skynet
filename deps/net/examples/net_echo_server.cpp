#include "net.h"

#include "fmt/format.h"
#include <iostream>

class network_handler : public skynet::net::network_tcp_server_handler
{
public:
    network_handler() = default;
    ~network_handler() override = default;

public:
    // 接收连接成功
    void handle_accept(std::shared_ptr<skynet::net::tcp_session> session_ptr) override
    {
        std::cout << "accept success" << std::endl;
    }

    // tcp会话读完成
    void handle_session_read(std::shared_ptr<skynet::net::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
        std::cout << std::endl;
        std::cout << "recv: " << data_ptr << std::endl;
        //std::cout << "recv: " << bytes_transferred << "bytes"<< std::endl;

        session_ptr->write(data_ptr, data_len);
        std::cout << "send: " << data_ptr << std::endl;
        //std::cout << "send: " << bytes_transferred << "bytes" << std::endl;
    }

    // tcp会话写完成
    void handle_session_write(std::shared_ptr<skynet::net::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
    }

    // tcp会话闲置
    void handle_session_idle(std::shared_ptr<skynet::net::tcp_session> session_ptr, skynet::net::idle_type type) override
    {
        std::cout << "session idle" << std::endl;
    }

    // tcp会话关闭
    void handle_sessoin_close(std::shared_ptr<skynet::net::tcp_session> session_ptr) override
    {
        std::cout << "session close" << std::endl;
    }
};

int32_t main(int32_t argc, char* argv[])
{
//    std::cout << "create server" << std::endl;
//
//    // acceptor ios
//    auto acceptor_ios_ptr = skynet::net::create_io_service();
//    auto session_manager_ptr = skynet::net::create_session_manager();
//    auto io_statistics_ptr = skynet::net::create_io_statistics(session_manager_ptr, acceptor_ios_ptr);
//    auto acceptor_config_ptr = skynet::net::create_tcp_server_acceptor_config();
//    auto session_config_ptr = skynet::net::create_tcp_server_session_config();
//
//    // create server
//    auto server_ptr = skynet::net::create_tcp_server(acceptor_ios_ptr, session_manager_ptr, acceptor_config_ptr, session_config_ptr);
//
//    // create server handler
//    auto server_handler_ptr = std::make_shared<tcp_server_handler>();
//    server_ptr->set_event_handler(server_handler_ptr);
//
//    // open server
//    std::cout << "open server" << std::endl;
//    if (!server_ptr->open({ std::make_pair("", 10001), std::make_pair("", 8989) }))
//    {
//        std::cout << "open server failed" << std::endl;
//        return 0;
//    }
//
//    ::getchar();
//
//    server_ptr->close();

    std::cout << "create network" << std::endl;

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

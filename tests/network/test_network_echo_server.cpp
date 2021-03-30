#include "network.h"

#include "fmt/format.h"
#include <iostream>

class tcp_server_handler : public skynet::network::tcp_server_handler
{
public:
    tcp_server_handler() = default;
    virtual ~tcp_server_handler() = default;

public:
    // 接收连接成功
    virtual void handle_accept(std::shared_ptr<skynet::network::tcp_session> session_ptr) override
    {
        std::cout << "accept success" << std::endl;
    }

    // tcp会话读完成
    virtual void handle_session_read(std::shared_ptr<skynet::network::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
        std::cout << std::endl;
        std::cout << "recv: " << data_ptr << std::endl;
        //std::cout << "recv: " << bytes_transferred << "bytes"<< std::endl;

        session_ptr->write(data_ptr, data_len);
        std::cout << "send: " << data_ptr << std::endl;
        //std::cout << "send: " << bytes_transferred << "bytes" << std::endl;
    }

    // tcp会话写完成
    virtual void handle_session_write(std::shared_ptr<skynet::network::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
    }

    // tcp会话闲置
    virtual void handle_session_idle(std::shared_ptr<skynet::network::tcp_session> session_ptr, skynet::network::idle_type type) override
    {
        std::cout << "session idle" << std::endl;
    }

    // tcp会话关闭
    virtual void handle_sessoin_close(std::shared_ptr<skynet::network::tcp_session> session_ptr) override
    {
        std::cout << "session close" << std::endl;
    }
};

int32_t main(int32_t argc, char* argv[])
{
    std::string local_ip = "";
    uint16_t local_port = 10001;

    // create server
    std::cout << "create server" << std::endl;
    std::shared_ptr<skynet::network::tcp_server> server_ptr = std::make_shared<skynet::network::tcp_server>();

    std::shared_ptr<tcp_server_handler> server_handler_ptr = std::make_shared<tcp_server_handler>();
    server_ptr->set_event_handler(server_handler_ptr);

    // open server
    std::cout << "open server" << std::endl;
    //if (server_ptr->open(local_ip, local_port) == false)
    if (server_ptr->open({std::make_pair(local_ip, local_port), std::make_pair("", 8989)}) == false)
    {
        std::cout << "open server failed" << std::endl;
        return 0;
    }

    getchar();

    server_ptr->close();

    return 0;
}

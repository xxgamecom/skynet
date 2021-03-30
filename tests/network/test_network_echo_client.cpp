#include "game.network/network.h"

#include <boost/format.hpp>
#include <iostream>

class tcp_client_handler : public skynet::newwork::tcp_client_handler
{
public:
    std::string             echo_msg_ = "A";
    int32_t                 repeat_count_ = 0;

public:
    tcp_client_handler() = default;
    virtual ~tcp_client_handler() = default;

    // tcp_client_handler impl
public:
    // 主动连接成功
    virtual void handle_connect_success(std::shared_ptr<skynet::newwork::tcp_session> session_ptr) override
    {
        std::cout << "connect success" << std::endl;

        session_ptr->write(echo_msg_.c_str(), echo_msg_.length());
        std::cout << "send: " << echo_msg_.c_str() << std::endl;
    }

    // 主动连接失败
    virtual void handle_connect_failed(std::shared_ptr<skynet::newwork::tcp_session> session_ptr, int32_t err_code, std::string err_msg) override
    {
        boost::format fmt("connec failed(%d:%s)");
        fmt % err_code
            % err_msg;
        std::cout << fmt.str() << std::endl;
    }

    // 超时处理
    virtual void handle_connect_timeout(std::shared_ptr<skynet::newwork::tcp_session> session_ptr) override
    {
        std::cout << "connect timeout" << std::endl;
    }

    // tcp会话读完成
    virtual void handle_session_read(std::shared_ptr<skynet::newwork::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
        std::cout << std::endl;
        std::cout << "recv: " << data_ptr << std::endl;

        if (--repeat_count_ > 0)
        {
            session_ptr->write(echo_msg_.c_str(), echo_msg_.length());
            std::cout << "send: " << echo_msg_.c_str() << std::endl;
        }
    }

    // tcp会话写完成
    virtual void handle_session_write(std::shared_ptr<skynet::newwork::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
    }

    // tcp会话关闭
    virtual void handle_sessoin_close(std::shared_ptr<skynet::newwork::tcp_session> session_ptr) override
    {
        std::cout << "session close" << std::endl;
    }
};

int32_t main(int32_t argc, char* argv[])
{
    if (argc < 5)
    {
        std::cout << "usage: " << std::endl;
        std::cout << argv[0] << " hostname port echo_msg repeate_cout" << std::endl << std::endl;
        return 0;
    }

    std::string remote_host = argv[1];
    uint16_t remote_port = std::stoi(argv[2]);

    // create client
    std::cout << "create client" << std::endl;

    std::shared_ptr<tcp_client_handler> client_handler_ptr = std::make_shared<tcp_client_handler>();
    client_handler_ptr->echo_msg_ = argv[3];
    client_handler_ptr->repeat_count_ = std::stoi(argv[4]);

    std::shared_ptr<skynet::newwork::tcp_client> client_ptr = std::make_shared<skynet::newwork::tcp_client>();
    client_ptr->set_event_handler(client_handler_ptr);

    // open client
    std::cout << "open client" << std::endl;
    if (client_ptr->open() == false)
    {
        std::cout << "open client failed" << std::endl;
        return 0;
    }
    
    // connect
    std::cout << "connect: " << remote_host << ":" << remote_port << std::endl;
    bool ret = client_ptr->connect(remote_host, remote_port);
    if (ret == false)
    {
        std::cout << "connect error" << std::endl;
        return 0;
    }

    getchar();

    client_ptr->close();

    return 0;
}

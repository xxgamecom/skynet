#include "net.h"

#include "fmt/format.h"
#include <iostream>

class tcp_client_handler : public skynet::net::tcp_client_handler
{
public:
    tcp_client_handler() = default;
    ~tcp_client_handler() override = default;

    // tcp_client_handler impl
public:
    // 主动连接成功
    void handle_connect_success(std::shared_ptr<skynet::net::tcp_session> session_ptr) override
    {
        std::cout << "connect success" << std::endl;

        // GET header
        std::stringstream ss;
        ss << "GET / HTTP/1.1\r\n"
           << "User-Agent: IE\r\n"
           << "Accept: */*\r\n"
           << "\r\n";

        std::cout << "send data: \r\n" << ss.str() << std::endl;

        size_t send_bytes = session_ptr->write(ss.str().c_str(), ss.str().length());

        std::cout << "send data bytes: " << send_bytes << std::endl;
    }

    // 主动连接失败
    void handle_connect_failed(std::shared_ptr<skynet::net::tcp_session> session_ptr, int32_t err_code, std::string err_msg) override
    {
        std::cout << fmt::format("connec failed({}:{})", err_code, err_msg) << std::endl;
    }

    // 超时处理
    void handle_connect_timeout(std::shared_ptr<skynet::net::tcp_session> session_ptr) override
    {
        std::cout << "connect timeout" << std::endl;
    }

    // tcp会话读完成
    void handle_session_read(std::shared_ptr<skynet::net::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
        std::cout << std::endl;
        std::cout << "read: " << std::endl << data_ptr << std::endl;
        std::cout << std::endl;
    }

    // tcp会话写完成
    void handle_session_write(std::shared_ptr<skynet::net::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
        std::cout << "write completed bytes: " << data_len  << std::endl;
    }

    // tcp会话关闭
    void handle_sessoin_close(std::shared_ptr<skynet::net::tcp_session> session_ptr) override
    {
        std::cout << "session close" << std::endl;
    }
};

int32_t main(int32_t argc, char* argv[])
{
    if (argc < 3)
    {
        std::cout << "usage: " << std::endl;
        std::cout << argv[0] << " hostname port [timeout]" << std::endl << std::endl;
        return 0;
    }

    std::string remote_host = argv[1];
    uint16_t remote_port = std::stoi(argv[2]);
    int32_t timeout_seconds = 0;
    if (argc > 3) timeout_seconds = std::stoi(argv[3]);

    // create client
    std::cout << "create client" << std::endl;
    auto service_handler_ptr = std::make_shared<tcp_client_handler>();
    auto client_ptr = skynet::net::create_tcp_client(0);
    client_ptr->set_event_handler(service_handler_ptr);

    // open client
    std::cout << "open client" << std::endl;
    if (!client_ptr->open())
    {
        std::cout << "open client failed" << std::endl;
        return 0;
    }

    // connect
    std::cout << "connect: " << remote_host << ":" << remote_port << std::endl;
    bool ret = client_ptr->connect(remote_host, remote_port, timeout_seconds);
    if (!ret)
    {
        std::cout << "connect error" << std::endl;
    }

    getchar();

    client_ptr->close();

    return 0;
}

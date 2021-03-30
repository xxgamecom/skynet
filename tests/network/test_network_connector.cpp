#include "game.network/network.h"

#include <boost/format.hpp>

class connect_handler : public skynet::newwork::tcp_connector_handler
{
public:
    connect_handler() = default;
    virtual ~connect_handler() = default;

    // connect_handler impl
public:
    // 地址解析成功
    virtual void handle_resolve_success(std::shared_ptr<skynet::newwork::tcp_session> session_ptr, std::string addr, uint16_t port) override
    {
        boost::format fmt("resolve %s:%d success");
        fmt % addr
            % port;
        std::cout << fmt.str() << std::endl;
    }

    // 地址解析失败
    virtual void handle_resolve_failed(std::shared_ptr<skynet::newwork::tcp_session> session_ptr, int32_t err_code, std::string err_msg) override
    {
        boost::format fmt("resolve failed(%d:%s)");
        fmt % err_code
            % err_msg;
        std::cout << fmt.str() << std::endl;
    }

    // 主动连接成功
    virtual void handle_connect_success(std::shared_ptr<skynet::newwork::tcp_session> session_ptr) override
    {
        boost::system::error_code ec;
        boost::format fmt("connect %s:%d success");
        fmt % session_ptr->remote_endpoint().address().to_string(ec)
            % session_ptr->remote_endpoint().port();
        std::cout << fmt.str() << std::endl;
    }

    // 主动连接失败
    virtual void handle_connect_failed(std::shared_ptr<skynet::newwork::tcp_session> session_ptr, int32_t err_code, std::string err_msg) override
    {
        boost::system::error_code ec;
        boost::format fmt("connect %s:%d failed(%d:%s)");
        fmt % session_ptr->remote_endpoint().address().to_string(ec) 
            % session_ptr->remote_endpoint().port()
            % err_code
            % err_msg;
        std::cout << fmt.str() << std::endl;
    }

    // 超时处理
    virtual void handle_connect_timeout(std::shared_ptr<skynet::newwork::tcp_session> session_ptr) override
    {
        boost::system::error_code ec;
        boost::format fmt("connect %s:%d timeout");
        fmt % session_ptr->remote_endpoint().address().to_string(ec)
            % session_ptr->remote_endpoint().port();
        std::cout << fmt.str() << std::endl;
    }
};

class stream_handler : public skynet::newwork::tcp_session_handler
{
public:
    stream_handler() = default;
    virtual ~stream_handler() = default;

public:
    // tcp会话读完成
    virtual void handle_session_read(std::shared_ptr<skynet::newwork::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
    }

    // tcp会话写完成
    virtual void handle_session_write(std::shared_ptr<skynet::newwork::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
    }

    // tcp会话闲置
    virtual void handle_session_idle(std::shared_ptr<skynet::newwork::tcp_session> session_ptr, skynet::newwork::idle_type type) override
    {
    }
    
    // tcp会话关闭
    virtual void handle_sessoin_close(std::shared_ptr<skynet::newwork::tcp_session> session_ptr) override
    {
    }
};

int32_t main(int32_t argc, char* argv[])
{
    std::string remote_host = "www.sina.com.cn";
    uint16_t remote_port = 80;
    int32_t timeout_seconds = 10;

    std::cout << "create ios pool" << std::endl;

    // ios pool
    skynet::newwork::io_service_pool ios_pool(1);
    ios_pool.run();

    // socket connector
    std::cout << "create connector" << std::endl;
    std::shared_ptr<connect_handler> connect_handler_ptr = std::make_shared<connect_handler>();
    std::shared_ptr<skynet::newwork::tcp_connector> connector_ptr = std::make_shared<skynet::newwork::tcp_connector>(ios_pool.select_one());
    connector_ptr->set_event_handler(connect_handler_ptr);

    // socket session
    std::cout << "create session" << std::endl;
    std::shared_ptr<stream_handler> stream_handler_ptr = std::make_shared<stream_handler>();
    std::shared_ptr<skynet::newwork::tcp_session> session_ptr = std::make_shared<skynet::newwork::tcp_session>(8192, 4096, 4);
    session_ptr->set_event_handler(stream_handler_ptr);
    
    std::cout << "connect: " << remote_host << ":" << remote_port << std::endl;

    // connect
    bool ret = connector_ptr->connect(session_ptr, remote_host, remote_port, timeout_seconds);
    if (ret == false)
    {
        std::cout << "connect error" << std::endl;
    }

    getchar();

    session_ptr->close();
    ios_pool.stop();

    return 0;
}

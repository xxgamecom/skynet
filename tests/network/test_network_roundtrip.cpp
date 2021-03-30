#include "network.h"

#include "fmt/format.h"
#include <iostream>

class tcp_server_handler : public skynet::network::tcp_server_handler
{
private:
    int32_t block_size_ = 0;

    std::shared_ptr<char> write_data_ptr_;
    std::shared_ptr<char> read_data_ptr_;

    size_t read_data_length_ = 0;

public:
    tcp_server_handler(int32_t block_size)
        :
        block_size_(block_size)
    {
        write_data_ptr_.reset(new char[block_size_], std::default_delete<char[]>());
        read_data_ptr_.reset(new char[block_size_], std::default_delete<char[]>());
    }

    virtual ~tcp_server_handler() = default;

    // tcp_server_handler impl
public:
    // 接收连接成功
    virtual void handle_accept(std::shared_ptr<skynet::network::tcp_session> session_ptr) override
    {
        //std::cout << "accept success" << std::endl;
        session_ptr->set_sock_option(skynet::network::SOCK_OPT_NODELAY, 1);
    }

    // tcp会话读完成
    virtual void handle_session_read(std::shared_ptr<skynet::network::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
        read_data_length_ = data_len;
        std::swap(read_data_ptr_, write_data_ptr_);
        session_ptr->write(write_data_ptr_.get(), read_data_length_);
    }

    // tcp会话写完成
    virtual void handle_session_write(std::shared_ptr<skynet::network::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
    }

    // tcp会话闲置
    virtual void handle_session_idle(std::shared_ptr<skynet::network::tcp_session> session_ptr, skynet::network::idle_type type) override
    {
        //std::cout << "session idle" << std::endl;
    }

    // tcp会话关闭
    virtual void handle_sessoin_close(std::shared_ptr<skynet::network::tcp_session> session_ptr) override
    {
        //std::cout << "session " << session_ptr->session_id() << " close" << std::endl;
    }
};

class stress_server
{
private:
    std::shared_ptr<skynet::network::tcp_server> server_ptr_;
    int32_t block_size_ = 0;

public:
    stress_server() = default;
    ~stress_server() = default;

public:
    void start(std::string local_ip, uint16_t local_port, int32_t thread_count, int32_t block_size)
    {
        server_ptr_ = std::make_shared<skynet::network::tcp_server>();

        std::shared_ptr<tcp_server_handler> server_handler_ptr = std::make_shared<tcp_server_handler>(block_size);
        server_ptr_->set_event_handler(server_handler_ptr);

        // 设置服务端会话选项
        //server_ptr_->get_session_config().msg_read_buf_size(32*1024);
        //server_ptr_->get_session_config().msg_write_buf_size(32*1024);
        server_ptr_->get_session_config().session_thread_num(thread_count - 1);
        server_ptr_->get_session_config().socket_recv_buf_size(32 * 1024);
        server_ptr_->get_session_config().socket_send_buf_size(32 * 1024);

        if (server_ptr_->open(local_ip, local_port) == false)
        {
            return;
        }
    }

    void stop()
    {
        std::cout << fmt::format("read_bytes: %lld, read_bytes_throughput: %lf, largest_read_bytes_throughput: %lf.",
                                 server_ptr_->get_io_statistics()->read_bytes(),
                                 server_ptr_->get_io_statistics()->read_bytes_throughput(),
                                 server_ptr_->get_io_statistics()->largest_read_bytes_throughput())
                  << std::endl
                  << fmt::format("write_bytes: %lld, write_bytes_throughput: %lf, largest_write_bytes_throughput: %lf.",
                                 server_ptr_->get_io_statistics()->write_bytes(),
                                 server_ptr_->get_io_statistics()->write_bytes_throughput(),
                                 server_ptr_->get_io_statistics()->largest_write_bytes_throughput())
                  << std::endl;

        server_ptr_->close();
    }

};


void run_server(uint16_t port)
{
    //stress_server ss;
    //ss.start("", port);
}

void run_client(const std::string ip, uint16_t port)
{
//    EventLoop loop;
//    TcpClient client(&loop, InetAddress(ip, port), "ClockClient");
//    client.enableRetry();
//    client.setConnectionCallback(clientConnectionCallback);
//    client.setMessageCallback(clientMessageCallback);
//    client.connect();
//    loop.runEvery(0.2, sendMyTime);
//    loop.loop();
}

int32_t main(int32_t argc, char* argv[])
{
    if (argc <= 2)
    {
        std::cout << "Usage: " << std::endl;
        std::cout << argv[0] << " <address> <port>" << std::endl;
        return 1;
    }

    if (strcmp(argv[1], "-s") == 0)
    {
        uint16_t local_port = std::stoi(argv[2]);
        run_server(local_port);
    }
    else
    {
        std::string local_ip = argv[1];
        uint16_t local_port = std::stoi(argv[2]);
        run_client(local_ip, local_port);
    }

    return 0;
}





//#include <muduo/base/Logging.h>
//#include <muduo/net/EventLoop.h>
//#include <muduo/net/TcpClient.h>
//#include <muduo/net/TcpServer.h>
//
//#include <stdio.h>
//
//using namespace muduo;
//using namespace muduo::net;
//
//const size_t frameLen = 2 * sizeof(int64_t);
//
//void serverConnectionCallback(const TcpConnectionPtr& conn)
//{
//    LOG_TRACE << conn->name() << " " << conn->peerAddress().toIpPort() << " -> "
//        << conn->localAddress().toIpPort() << " is "
//        << (conn->connected() ? "UP" : "DOWN");
//    if (conn->connected())
//    {
//        conn->setTcpNoDelay(true);
//    }
//    else
//    {
//    }
//}
//
//void serverMessageCallback(const TcpConnectionPtr& conn,
//                           Buffer* buffer,
//                           muduo::Timestamp receiveTime)
//{
//    int64_t message[2];
//    while (buffer->readableBytes() >= frameLen)
//    {
//        memcpy(message, buffer->peek(), frameLen);
//        buffer->retrieve(frameLen);
//        message[1] = receiveTime.microSecondsSinceEpoch();
//        conn->send(message, sizeof message);
//    }
//}

//TcpConnectionPtr clientConnection;
//
//void clientConnectionCallback(const TcpConnectionPtr& conn)
//{
//    LOG_TRACE << conn->localAddress().toIpPort() << " -> "
//        << conn->peerAddress().toIpPort() << " is "
//        << (conn->connected() ? "UP" : "DOWN");
//    if (conn->connected())
//    {
//        clientConnection = conn;
//        conn->setTcpNoDelay(true);
//    }
//    else
//    {
//        clientConnection.reset();
//    }
//}
//
//void clientMessageCallback(const TcpConnectionPtr&,
//                           Buffer* buffer,
//                           muduo::Timestamp receiveTime)
//{
//    int64_t message[2];
//    while (buffer->readableBytes() >= frameLen)
//    {
//        memcpy(message, buffer->peek(), frameLen);
//        buffer->retrieve(frameLen);
//        int64_t send = message[0];
//        int64_t their = message[1];
//        int64_t back = receiveTime.microSecondsSinceEpoch();
//        int64_t mine = (back + send) / 2;
//        LOG_INFO << "round trip " << back - send
//            << " clock error " << their - mine;
//    }
//}
//
//void sendMyTime()
//{
//    if (clientConnection)
//    {
//        int64_t message[2] = { 0, 0 };
//        message[0] = Timestamp::now().microSecondsSinceEpoch();
//        clientConnection->send(message, sizeof message);
//    }
//}
//


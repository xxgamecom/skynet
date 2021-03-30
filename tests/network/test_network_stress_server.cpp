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

        std::cout << fmt::format("read_bytes: {}, read_bytes_throughput: {}, largest_read_bytes_throughput: {}.",
                                 server_ptr_->get_io_statistics()->read_bytes(),
                                 server_ptr_->get_io_statistics()->read_bytes_throughput(),
                                 server_ptr_->get_io_statistics()->largest_read_bytes_throughput())
                  << std::endl
                  << fmt::format("write_bytes: {}, write_bytes_throughput: {}, largest_write_bytes_throughput: {}.",
                                 server_ptr_->get_io_statistics()->write_bytes(),
                                 server_ptr_->get_io_statistics()->write_bytes_throughput(),
                                 server_ptr_->get_io_statistics()->largest_write_bytes_throughput())
                  << std::endl;

        server_ptr_->close();
    }

};

int32_t main(int32_t argc, char* argv[])
{
    if (argc != 5)
    {
        std::cout << "usage: " << std::endl;
        std::cout << argv[0] << " <address> <port> <thread_count> <block_size>" << std::endl;
        return 1;
    }

    std::string local_ip = argv[1];
    uint16_t local_port = std::stoi(argv[2]);
    int32_t thread_count = std::stoi(argv[3]);
    int32_t block_size = std::stoi(argv[4]);

    std::cout << "create stress test server" << std::endl;
    stress_server ss;
    ss.start(local_ip, local_port, thread_count, block_size);

    getchar();

    ss.stop();

    getchar();

    return 0;
}

#include "net.h"

#include <boost/format.hpp>
#include <iostream>
#include <atomic>

// statistics
class statistics
{
private:
    std::atomic<int64_t> total_bytes_write_;
    std::atomic<int64_t> total_bytes_read_;

public:
    statistics()
        :
        total_bytes_write_(0),
        total_bytes_read_(0)
    {
    }
    ~statistics() = default;

public:
    void add(int64_t bytes_written, int64_t bytes_read)
    {
        total_bytes_write_ += bytes_written;
        total_bytes_read_ += bytes_read;
    }

    void reset()
    {
        total_bytes_write_ = 0;
        total_bytes_read_ = 0;
    }

    void print()
    {
        std::cout << (total_bytes_write_ / 1024 / 1024) << "MB written\n";
        std::cout << (total_bytes_read_ / 1024 / 1024) << "MB read\n";
    }
};

class client_handler : public skynet::net::tcp_connector_handler,
                       public skynet::net::tcp_session_handler
{
private:
    statistics& stats_ref_;
    int32_t block_size_ = 0;

    int64_t bytes_written_ = 0;             // 本client的写字节数统计量
    int64_t bytes_read_ = 0;                // 本client的读字节数统计量

    std::shared_ptr<char> write_data_ptr_;
    std::shared_ptr<char> read_data_ptr_;

    size_t read_data_length_ = 0;

public:
    client_handler(statistics& stats_ref, int32_t block_size)
        :
        stats_ref_(stats_ref),
        block_size_(block_size)
    {
        write_data_ptr_.reset(new char[block_size_], std::default_delete<char[]>());
        read_data_ptr_.reset(new char[block_size_], std::default_delete<char[]>());

        for (int32_t i = 0; i < block_size_; ++i)
        {
            write_data_ptr_.get()[i] = static_cast<char>(i % 128);
        }
    }
    virtual ~client_handler()
    {
    }

    // tcp_connector_handler impl
protected:
    // 地址解析成功
    virtual void handle_resolve_success(std::shared_ptr<skynet::net::tcp_session> session_ptr, std::string addr, uint16_t port) override
    {
    }

    // 地址解析失败
    virtual void handle_resolve_failed(std::shared_ptr<skynet::net::tcp_session> session_ptr, int32_t err_code, std::string err_msg) override
    {
    }

    // 主动连接成功
    virtual void handle_connect_success(std::shared_ptr<skynet::net::tcp_session> session_ptr) override
    {
        session_ptr->set_sock_option(skynet::net::SOCK_OPT_RECV_BUFFER, 32 * 1024);
        session_ptr->set_sock_option(skynet::net::SOCK_OPT_SEND_BUFFER, 32 * 1024);
        session_ptr->set_sock_option(skynet::net::SOCK_OPT_NODELAY, 1);

        session_ptr->write(write_data_ptr_.get(), block_size_);

        session_ptr->start_read();
    }

    // 主动连接失败
    virtual void handle_connect_failed(std::shared_ptr<skynet::net::tcp_session> session_ptr, int32_t err_code, std::string err_msg) override
    {
        boost::format fmt("connec failed(%d:%s)");
        fmt % err_code
        % err_msg;
        std::cout << fmt.str() << std::endl;

        session_ptr->close();
    }

    // 超时处理
    virtual void handle_connect_timeout(std::shared_ptr<skynet::net::tcp_session> session_ptr) override
    {
        session_ptr->close();
    }

    // tcp_session_handler impl
protected:
    // tcp会话读完成
    virtual void handle_session_read(std::shared_ptr<skynet::net::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
        bytes_read_ += data_len;
        read_data_length_ = data_len;

        std::swap(read_data_ptr_, write_data_ptr_);
        session_ptr->write(write_data_ptr_.get(), read_data_length_);
    }

    // tcp会话写完成
    virtual void handle_session_write(std::shared_ptr<skynet::net::tcp_session> session_ptr, char* data_ptr, size_t data_len) override
    {
        bytes_written_ += data_len;
    }

    // tcp会话闲置
    virtual void handle_session_idle(std::shared_ptr<skynet::net::tcp_session> session_ptr, skynet::net::idle_type type) override
    {
    }

    // tcp会话关闭
    virtual void handle_sessoin_close(std::shared_ptr<skynet::net::tcp_session> session_ptr) override
    {
        stats_ref_.add(bytes_written_, bytes_read_);
        //std::cout << "session close" << std::endl;
    }
};

class stress_client
{
private:
    struct client
    {
        std::shared_ptr<skynet::net::tcp_connector> connector_ptr_;
        std::shared_ptr<skynet::net::tcp_session> session_ptr_;
        std::shared_ptr<client_handler> client_handler_ptr_;
    };

private:
    statistics stats_;                 // 收发计数统计

    std::shared_ptr<skynet::net::io_service> ios_ptr_;               // timer的ios
    std::shared_ptr<skynet::net::io_service_pool> ios_pool_ptr_;          // 线程池

    std::vector<std::shared_ptr<client>> clients_;               // 模拟客户端
    std::shared_ptr<asio::steady_timer> stop_timer_ptr_;        //

public:
    stress_client() = default;
    ~stress_client() = default;

public:
    void start(std::string host, uint16_t port, int32_t thread_count, int32_t session_count, int32_t block_size, int32_t test_seconds)
    {
        // 重置统计量
        stats_.reset();

        ios_pool_ptr_ = std::make_shared<skynet::net::io_service_pool>(thread_count);

        // 创建client
        for (int32_t i = 0; i < session_count; ++i)
        {
            std::shared_ptr<client> client_ptr = std::make_shared<client>();

            // client handler
            client_ptr->client_handler_ptr_ = std::make_shared<client_handler>(stats_, block_size);

            // session
            client_ptr->session_ptr_ = std::make_shared<skynet::net::tcp_session>(32 * 1024, 32 * 1024, 4);
            client_ptr->session_ptr_->set_event_handler(std::dynamic_pointer_cast<skynet::net::tcp_session_handler>(client_ptr->client_handler_ptr_));

            // connector
            client_ptr->connector_ptr_ = std::make_shared<skynet::net::tcp_connector>(ios_pool_ptr_->select_one());
            client_ptr->connector_ptr_->set_event_handler(std::dynamic_pointer_cast<skynet::net::tcp_connector_handler>(client_ptr->client_handler_ptr_));

            // save client object
            clients_.push_back(client_ptr);
        }

        // 连接host
        for (auto& itr : clients_)
        {
            itr->connector_ptr_->connect(itr->session_ptr_, host, port);
        }

        // 测试结束定时器
        ios_ptr_ = std::make_shared<skynet::net::io_service>();
        stop_timer_ptr_ = std::make_shared<asio::steady_timer>(ios_ptr_->get_raw_ios());
        stop_timer_ptr_->expires_from_now(std::chrono::seconds(test_seconds));
        stop_timer_ptr_->async_wait(std::bind(&stress_client::handle_timeout, this));

        // 
        ios_ptr_->run();
        ios_pool_ptr_->run();
    }

    void stop()
    {
        stop_sessions();
        ios_ptr_->stop();
    }

private:
    void stop_sessions()
    {
        for (auto& itr : clients_)
        {
            itr->session_ptr_->close();
        }

        stats_.print();

        if (ios_pool_ptr_ != nullptr)
        {
            ios_pool_ptr_->stop();
        }
    }

private:
    void handle_timeout()
    {
        stop_sessions();
    }
};

int32_t main(int32_t argc, char* argv[])
{
    if (argc != 7)
    {
        std::cout << "usage: " << std::endl;
        std::cout << argv[0] << " <host> <port> <thread_count> <session_count> <block_size> <test_seconds>" << std::endl << std::endl;
        return 1;
    }
    std::string host = argv[1];
    uint16_t port = std::stoi(argv[2]);
    int32_t thread_count = std::stoi(argv[3]);
    int32_t session_count = std::stoi(argv[4]);
    int32_t block_size = std::stoi(argv[5]);
    int32_t test_seconds = std::stoi(argv[6]);

    stress_client sc;
    sc.start(host, port, thread_count, session_count, block_size, test_seconds);

    getchar();

    sc.stop();

    return 0;
}

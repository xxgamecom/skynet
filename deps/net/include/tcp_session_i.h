#pragma once

#include "tcp_session_def.h"
#include "socket_option_def.h"

#include "asio.hpp"

#include <cstdint>
#include <string>
#include <memory>

namespace skynet { namespace net {

// forward declare
class io_service;
class tcp_session_handler;

/**
 * tcp session, used for communication at both endpoints.
 */
class tcp_session
{
public:
    virtual ~tcp_session() = default;

public:
    // set session event handler
    virtual void set_event_handler(std::shared_ptr<tcp_session_handler> event_handler_ptr) = 0;

public:
    // open session
    virtual bool open(std::shared_ptr<io_service> ios_ptr, std::string local_ip = "", uint16_t local_port = 0) = 0;
    // close session(is_force: 是否立即关闭, 如果立即关闭不等待剩余数据发送完毕)
    virtual void close(bool is_force = true) = 0;

    // 会话ID
    virtual void session_id(session_id_t id) = 0;
    virtual session_id_t session_id() = 0;

    // 开始读取数据(通常在连接成功后调用, 用于发起异步读操作)
    virtual void start_read() = 0;

    // 写入数据, 返回写入的字节数(异步写入, 写入成功后通过handle_session_write回调)
    // 这里的内部保护只保证1个线程写的情况是安全的, 具体查看tcp_session_write_queue的说明
    virtual size_t write(const char* data_ptr, size_t data_len) = 0;

    // 用于外部对会话进行闲置测试(check_type为要检测的闲置类型, check_seconds为判断闲置的时间(秒))
    virtual void check_idle(idle_type check_type, int32_t check_seconds) = 0;

public:
    // is session open
    virtual bool is_open() = 0;

    // get session socket
    virtual std::shared_ptr<asio::ip::tcp::socket> get_socket() = 0;

    // 读写字节计数
    virtual int64_t read_bytes() = 0;
    virtual int64_t write_bytes() = 0;

    // 增量读写字节数(注意: 调用后增量重新计数)
    virtual int64_t delta_read_bytes() = 0;
    virtual int64_t delta_write_bytes() = 0;

    // 获取远端端点信息(连接后才有效)
    virtual asio::ip::tcp::endpoint remote_endpoint() = 0;
    // 获取本地端点信息(连接后才有效)
    virtual asio::ip::tcp::endpoint local_endpoint() = 0;

    // socket选项
public:
    virtual bool set_sock_option(sock_options opt, int32_t value) = 0;
    virtual bool get_sock_option(sock_options opt, int32_t& value) = 0;

//    // 异步投递
//protected:
//    // 投递一次异步读操作
//    void async_read_once();
//    // 投递一个异步写操作
//    void async_write_once();
//
//    // 处理函数
//protected:
//    // 处理完成的读操作
//    void handle_async_read(const asio::error_code& ec, size_t bytes_transferred);
//    // 处理完成的写操作
//    void handle_async_write(const asio::error_code& ec, size_t bytes_transferred);
};

} }



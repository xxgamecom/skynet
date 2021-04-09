#pragma once

#include "base/socket_option_def.h"

#include "base/session_i.h"

#include "asio.hpp"

#include <cstdint>
#include <string>
#include <memory>

namespace skynet::net {

// forward declare
class io_service;
class tcp_session_handler;

/**
 * tcp session, used for communication at both endpoints.
 */
class tcp_session : public basic_session
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

    // 开始读取数据(通常在连接成功后调用, 用于发起异步读操作)
    virtual void start_read() = 0;

    // 写入数据, 返回写入的字节数(异步写入, 写入成功后通过handle_session_write回调)
    // 这里的内部保护只保证1个线程写的情况是安全的, 具体查看tcp_session_write_queue的说明
    virtual size_t write(const char* data_ptr, size_t data_len) = 0;

public:
    // is session open
    virtual bool is_open() = 0;

    // get session socket
    virtual std::shared_ptr<asio::ip::tcp::socket> get_socket() = 0;

    // local/remote endpoint info (valid after connnected)
    virtual asio::ip::tcp::endpoint local_endpoint() = 0;
    virtual asio::ip::tcp::endpoint remote_endpoint() = 0;

    // socket选项
public:
    virtual bool set_sock_option(sock_options opt, int32_t value) = 0;
    virtual bool get_sock_option(sock_options opt, int32_t& value) = 0;
};

}



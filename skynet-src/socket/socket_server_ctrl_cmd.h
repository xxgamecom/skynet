#pragma once

#include "socket_server_def.h"
#include "buffer.h"

#include <cstdint>

namespace skynet {

// cmd - create a tcp server
struct cmd_request_listen
{
    int socket_id = 0;                          //
    int socket_fd = 0;                          //
    uint64_t svc_handle = 0;                    // skynet service handle
    char host[1] = { 0 };                       // listen address
};

// cmd - create a tcp client, open a socket connection
struct cmd_request_connect
{
    int socket_id = 0;                          //
    int port = 0;                               //
    uint64_t svc_handle = 0;                    // skynet service handle
    char host[1] = { 0 };                       // address
};

// cmd - send data
struct cmd_request_send
{
    int socket_id = 0;                          //
    size_t data_size = 0;                       // data size
    const void* data_ptr = nullptr;             // data
};

// cmd - send udp package
struct cmd_request_send_udp
{
    cmd_request_send send;                      // send data
    uint8_t address[UDP_ADDRESS_SIZE] = { 0 };  // udp address
};

// cmd - 
struct cmd_request_set_udp
{
    int socket_id = 0;                          //
    uint8_t address[UDP_ADDRESS_SIZE] = { 0 };  //
};

// cmd - close/shutdown socket
struct cmd_request_close
{
    int socket_id = 0;                          //
    int shutdown = 0;                           // 0 close, 1 shutdown
    uint64_t svc_handle = 0;                    // skynet service handle
};


// cmd - bind os fd
struct cmd_request_bind
{
    int socket_id = 0;                          //
    int os_fd = 0;                              //
    uint64_t svc_handle = 0;                    // skynet service handle
};

// cmd - 
struct cmd_request_resume_pause
{
    int socket_id = 0;                          //
    uint64_t svc_handle = 0;                    // skynet service handle
};

// cmd - set socket option
struct cmd_request_set_opt
{
    int socket_id = 0;                          //
    int what = 0;                               // socket option
    int value = 0;                              // socket option value
};

// cmd - create an udp socket (used for udp server & client)
struct cmd_request_udp_socket
{
    int socket_id = 0;                          // socket id
    int socket_fd = 0;                          // socket fd
    int family = 0;                             //
    uint64_t svc_handle = 0;                    // skynet service handle
};

/**
 * 请求数据包
 * 
 * 控制包类型: type字段
 * R - Resume socket (Start)
 * S - Pause socket
 * B - Bind socket
 * L - Listen socket
 * K - Close socket
 * O - Connect to (Open), create tcp client
 * X - Exit
 * D - Send package (high)
 * P - Send package (low)
 * A - Send UDP package
 * W - Trigger write
 * T - Set opt
 * U - Create UDP socket
 * C - Set udp address
 * Q - Query info
 */
struct ctrl_cmd_package
{
    // cmd header
    //
    // 0 ~ 5: dummy
    //     6: type
    //     7: data len
    //
    // so actually offset: &header[6]
    uint8_t header[8] = { 0 };

    // cmd data, less 256 bytes
    union
    {
        char buf[256];
        cmd_request_listen listen;
        cmd_request_connect connect;
        cmd_request_send send;
        cmd_request_send_udp send_udp;
        cmd_request_close close;
        cmd_request_bind bind;
        cmd_request_resume_pause resume_pause;
        cmd_request_set_opt set_opt;
        cmd_request_udp_socket udp_socket;
        cmd_request_set_udp set_udp;
    } u = { { 0 } };

    //
    uint8_t dummy[256] = { 0 };
};

// forward declare
class socket_addr;

// prepare start data: cmd_request_resume_pause
int prepare_ctrl_cmd_request_resume(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id);
// prepare pause data: cmd_request_resume_pause
int prepare_ctrl_cmd_request_pause(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id);
// prepare close data: cmd_request_close
int prepare_ctrl_cmd_request_close(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id);
// prepare shutdown data: cmd_request_close
int prepare_ctrl_cmd_request_shutdown(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id);

// prepare connect remote server data: cmd_request_open
int prepare_ctrl_cmd_request_connect(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id, const char* addr, int port);
// prepare os fd bind data: request_bind
int prepare_ctrl_cmd_request_bind(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id, int os_fd);
// prepare create tcp server data: cmd_request_listen
int prepare_ctrl_cmd_request_listen(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id, int listen_fd);
//
int prepare_ctrl_cmd_request_send(ctrl_cmd_package& cmd, int socket_id, const send_buffer* buf_ptr, bool is_high);
// let socket thread enable write event
int prepare_ctrl_cmd_request_trigger_write(ctrl_cmd_package& cmd, int socket_id);

// 准备 request_set_opt 请求数据
int prepare_ctrl_cmd_request_set_opt(ctrl_cmd_package& cmd, int socket_id);

// prepare create an udp socket data: cmd_request_udp_socket
int prepare_ctrl_cmd_request_udp_socket(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id, int socket_fd, int family);
//
int prepare_ctrl_cmd_request_set_udp(ctrl_cmd_package& cmd, int socket_id, int socket_type, const socket_addr* sa);
//
int prepare_ctrl_cmd_request_send_udp(ctrl_cmd_package& cmd, int socket_id, const send_buffer* buf_ptr, const uint8_t* udp_address, int addr_sz);

}

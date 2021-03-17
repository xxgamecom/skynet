#pragma once

#include "socket_server_def.h"
#include "buffer.h"

#include <cstdint>

namespace skynet {


// cmd - open a socket connection
struct request_open
{
    int                         socket_id = 0;                      //
    int                         port = 0;                           //
    uint64_t                    svc_handle = 0;                     // skynet service handle
    char                        host[1] = { 0 };                    // address
};

// cmd - send data
struct request_send
{
    int                         socket_id = 0;                      //
    size_t                      sz = 0;                             // data size
    const void*                 data_ptr = nullptr;                 // data
};

// cmd - send udp package
struct request_send_udp
{
    request_send                send;                               // send data
    uint8_t                     address[UDP_ADDRESS_SIZE] = { 0 };  // udp address
};

// cmd - 
struct request_set_udp
{
    int                         socket_id = 0;                      //
    uint8_t                     address[UDP_ADDRESS_SIZE] = { 0 };  //
};

// cmd - close/shutdown socket
struct request_close
{
    int                         socket_id = 0;                      //
    int                         shutdown = 0;                       // 0 close, 1 shutdown
    uint64_t                    svc_handle = 0;                     // skynet service handle
};

// cmd - 
struct request_listen
{
    int                         socket_id = 0;                      //
    int                         fd = 0;                             //
    uint64_t                    svc_handle = 0;                     // skynet service handle
    char                        host[1] = { 0 };                    // listen address
};

// cmd - 
struct request_bind
{
    int                         socket_id = 0;                      //
    int                         fd = 0;                             //
    uint64_t                    svc_handle = 0;                     // skynet service handle
};

// cmd - 
struct request_resume_pause
{
    int                         socket_id = 0;                      //
    uint64_t                    svc_handle = 0;                     // skynet service handle
};

// cmd - set socket option
struct request_set_opt
{
    int                         socket_id = 0;                      //
    int                         what = 0;                           // socket option
    int                         value = 0;                          // socket option value
};

// cmd - 
struct request_udp
{
    int                         socket_id = 0;                      //
    int                         fd = 0;                             //
    int                         family = 0;                         //
    uint64_t                    svc_handle = 0;                     // skynet service handle
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
 * O - Connect to (Open)
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
    uint8_t                     header[8] = { 0 };

    // cmd data, less 256 bytes
    union
    {
        char                    buf[256];
        request_open            open;
        request_send            send;
        request_send_udp        send_udp;
        request_close           close;
        request_listen          listen;
        request_bind            bind;
        request_resume_pause    resume_pause;
        request_set_opt         set_opt;
        request_udp             udp;
        request_set_udp         set_udp;
    } u = { { 0 } };

    //
    uint8_t                     dummy[256] = { 0 };
};

// forward declare
union socket_addr;

// 准备 request_resume_pause 请求数据
int prepare_ctrl_cmd_request_resume(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id);
// 准备 request_resume_pause 请求数据
int prepare_ctrl_cmd_request_pause(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id);
// 准备 request_close 请求数据
int prepare_ctrl_cmd_request_close(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id);
// 准备 request_close 请求数据
int prepare_ctrl_cmd_request_shutdown(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id);

// 准备 request_open 请求数据, 返回实际所占数据长度
int prepare_ctrl_cmd_request_open(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id, const char* addr, int port);
// 准备 request_bind 请求数据
int prepare_ctrl_cmd_request_bind(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id, int fd);
// 
int prepare_ctrl_cmd_request_listen(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id, int fd);
//
int prepare_ctrl_cmd_request_send(ctrl_cmd_package& cmd, int socket_id, const send_buffer* buf_ptr, bool is_high);
// let socket thread enable write event
int prepare_ctrl_cmd_request_trigger_write(ctrl_cmd_package& cmd, int socket_id);

// 准备 request_set_opt 请求数据
int prepare_ctrl_cmd_request_set_opt(ctrl_cmd_package& cmd, int socket_id);

// udp
int prepare_ctrl_cmd_request_set_udp(ctrl_cmd_package& cmd, int socket_id, int protocol, const socket_addr* sa);
//
int prepare_ctrl_cmd_request_udp(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id, int fd, int family);
//
int prepare_ctrl_cmd_request_send_udp(ctrl_cmd_package& cmd, int socket_id, const send_buffer* buf_ptr, const uint8_t* udp_address, int addr_sz);

}

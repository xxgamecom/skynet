#include "socket_server_ctrl_cmd.h"
#include "socket_endpoint.h"

#include "../log/log.h"

#include <iostream>
#include <cstring>
#include <netinet/tcp.h>

namespace skynet {


int prepare_ctrl_cmd_request_resume(ctrl_cmd_package& cmd, uint32_t svc_handle, int socket_id)
{
    // cmd data
    cmd.u.resume_pause.socket_id = socket_id;
    cmd.u.resume_pause.svc_handle = svc_handle;

    // actually data len
    int len = sizeof(cmd.u.resume_pause);

    // set header - cmd type & data len
    cmd.header[6] = (uint8_t)'R';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_pause(ctrl_cmd_package& cmd, uint32_t svc_handle, int socket_id)
{
    // cmd data
    cmd.u.resume_pause.socket_id = socket_id;
    cmd.u.resume_pause.svc_handle = svc_handle;

    // actually data len
    int len = sizeof(cmd.u.resume_pause);

    // set header - cmd type & data len
    cmd.header[6] = (uint8_t)'S';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_close(ctrl_cmd_package& cmd, uint32_t svc_handle, int socket_id)
{
    // cmd data
    cmd.u.close.socket_id = socket_id;
    cmd.u.close.shutdown = 0;       // 0 - close
    cmd.u.close.svc_handle = svc_handle;

    // actually length
    int len = sizeof(cmd.u.close);

    // cmd header - cmd type & data len
    cmd.header[6] = (uint8_t)'K';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_shutdown(ctrl_cmd_package& cmd, uint32_t svc_handle, int socket_id)
{
    // cmd data
    cmd.u.close.socket_id = socket_id;
    cmd.u.close.shutdown = 1;       // 1 - shutdown
    cmd.u.close.svc_handle = svc_handle;

    // actually length
    int len = sizeof(cmd.u.close);

    // cmd header - cmd type & data len
    cmd.header[6] = (uint8_t)'K';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_connect(ctrl_cmd_package& cmd, uint32_t svc_handle, int socket_id, const char* remote_ip, uint16_t remote_port)
{
    int len = ::strlen(remote_ip);

    // request_open结构体尾部的host字段为地址数据, 整体长度不能超过256
    if (sizeof(cmd.u.connect) + len >= 256)
    {
        log_error(nullptr, fmt::format("socket-server : Invalid addr {}.", remote_ip));
        return -1;
    }

    // cmd data
    cmd.u.connect.svc_handle = svc_handle;
    cmd.u.connect.socket_id = socket_id;
    cmd.u.connect.port = remote_port;
    
    // append udp address
    ::memcpy(cmd.u.connect.host, remote_ip, len);
    cmd.u.connect.host[len] = '\0';

    // actually length
    len += sizeof(cmd.u.connect);

    // cmd header
    cmd.header[6] = (uint8_t)'O';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_bind(ctrl_cmd_package& cmd, uint32_t svc_handle, int socket_id, int os_fd)
{
    // cmd data
    cmd.u.bind_os_fd.svc_handle = svc_handle;
    cmd.u.bind_os_fd.socket_id = socket_id;
    cmd.u.bind_os_fd.os_fd = os_fd;

    // actually length
    int len = sizeof(cmd.u.bind_os_fd);

    // cmd header
    cmd.header[6] = (uint8_t)'B';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_listen(ctrl_cmd_package& cmd, uint32_t svc_handle, int socket_id, int listen_fd)
{
    // cmd data
    cmd.u.listen.svc_handle = svc_handle;
    cmd.u.listen.socket_id = socket_id;
    cmd.u.listen.socket_fd = listen_fd;

    // actually length
    int len = sizeof(cmd.u.listen);

    // cmd header
    cmd.header[6] = (uint8_t)'L';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_send(ctrl_cmd_package& cmd, int socket_id, const send_data* sd_ptr, bool is_high)
{
    // cmd data
    cmd.u.send.socket_id = socket_id;
    cmd.u.send.data_ptr = sd_ptr;

    // actually length
    int len = sizeof(cmd.u.send);

    // cmd header
    cmd.header[6] = (uint8_t)is_high ? 'D' : 'P';    // high/low priority
    cmd.header[7] = (uint8_t)len;

    return len;
}

// let socket thread enable write event
int prepare_ctrl_cmd_request_trigger_write(ctrl_cmd_package& cmd, int socket_id)
{
    cmd.u.send.socket_id = socket_id;
    cmd.u.send.data_size = 0;
    cmd.u.send.data_ptr = nullptr;

    // actually length
    int len = sizeof(cmd.u.send);

    // cmd header
    cmd.header[6] = 'W';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_set_opt(ctrl_cmd_package& cmd, int socket_id)
{
    // cmd data
    cmd.u.set_opt.socket_id = socket_id;
    cmd.u.set_opt.what = TCP_NODELAY;
    cmd.u.set_opt.value = 1;

    // actually length
    int len = sizeof(cmd.u.set_opt);
    
    // cmd header
    cmd.header[6] = (uint8_t)'T';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_udp_socket(ctrl_cmd_package& cmd, uint32_t svc_handle, int socket_id, int socket_fd, int family)
{
    // cmd data
    cmd.u.udp_socket.svc_handle = svc_handle;
    cmd.u.udp_socket.socket_id = socket_id;
    cmd.u.udp_socket.socket_fd = socket_fd;
    cmd.u.udp_socket.family = family;

    // actually length
    int len = sizeof(cmd.u.udp_socket);

    // cmd header
    cmd.header[6] = (uint8_t)'U';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_set_udp(ctrl_cmd_package& cmd, int socket_id, int socket_type, const socket_endpoint* endpoint_ptr)
{
    // cmd data
    cmd.u.set_udp.socket_id = socket_id;
    int addr_sz = endpoint_ptr->to_udp_address(socket_type, cmd.u.set_udp.address);

    // actually length
    int len = sizeof(cmd.u.set_udp) - sizeof(cmd.u.set_udp.address) + addr_sz;
    
    // cmd header
    cmd.header[6] = (uint8_t)'C';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_send_udp(ctrl_cmd_package& cmd, int socket_id, const send_data* sd_ptr, const uint8_t* udp_address, int addr_sz)
{
    // cmd data
    cmd.u.send_udp.send.socket_id = socket_id;
    cmd.u.send_udp.send.data_ptr = sd_ptr;
    ::memcpy(cmd.u.send_udp.address, udp_address, addr_sz);

    // actually length
    int len = sizeof(cmd.u.send_udp.send) + addr_sz;

    // cmd header
    cmd.header[6] = (uint8_t)'A';
    cmd.header[7] = (uint8_t)len;

    return len;
}

}

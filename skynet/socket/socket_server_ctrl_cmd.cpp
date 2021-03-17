#include "socket_server_ctrl_cmd.h"
#include "socket_addr.h"

#include "../log/log.h"

#include <iostream>
#include <netinet/tcp.h>

namespace skynet {


int prepare_ctrl_cmd_request_resume(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id)
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

int prepare_ctrl_cmd_request_pause(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id)
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

int prepare_ctrl_cmd_request_close(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id)
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

int prepare_ctrl_cmd_request_shutdown(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id)
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

int prepare_ctrl_cmd_request_open(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id, const char* addr, int port)
{
    int len = ::strlen(addr);

    // request_open结构体尾部的host字段为地址数据, 整体长度不能超过256
    if (sizeof(cmd.u.open) + len >= 256)
    {
        log(nullptr, "socket-server : Invalid addr %s.", addr);
        return -1;
    }

    // cmd data
    cmd.u.open.svc_handle = svc_handle;
    cmd.u.open.socket_id = socket_id;
    cmd.u.open.port = port;
    
    // append udp address
    ::memcpy(cmd.u.open.host, addr, len);
    cmd.u.open.host[len] = '\0';

    // actually length
    len += sizeof(cmd.u.open);    

    // cmd header
    cmd.header[6] = (uint8_t)'O';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_bind(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id, int fd)
{
    // cmd data
    cmd.u.bind.svc_handle = svc_handle;
    cmd.u.bind.socket_id = socket_id;
    cmd.u.bind.fd = fd;

    // actually length
    int len = sizeof(cmd.u.bind);

    // cmd header
    cmd.header[6] = (uint8_t)'B';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_listen(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id, int fd)
{
    // cmd data
    cmd.u.listen.svc_handle = svc_handle;
    cmd.u.listen.socket_id = socket_id;
    cmd.u.listen.fd = fd;

    // actually length
    int len = sizeof(cmd.u.listen);

    // cmd header
    cmd.header[6] = (uint8_t)'L';
    cmd.header[7] = (uint8_t)len;


    return len;
}

int prepare_ctrl_cmd_request_send(ctrl_cmd_package& cmd, int socket_id, const send_buffer* buf_ptr, bool is_high)
{
    // cmd data
    cmd.u.send.socket_id = socket_id;
    cmd.u.send.data_ptr = buf_ptr;

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
    cmd.u.send.sz = 0;
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

int prepare_ctrl_cmd_request_set_udp(ctrl_cmd_package& cmd, int socket_id, int protocol, const socket_addr* sa)
{
    // cmd data
    cmd.u.set_udp.socket_id = socket_id;
    int addr_sz = socket_addr_to_udp_address(protocol, sa, cmd.u.set_udp.address);

    // actually length
    int len = sizeof(cmd.u.set_udp) - sizeof(cmd.u.set_udp.address) + addr_sz;
    
    // cmd header
    cmd.header[6] = (uint8_t)'C';
    cmd.header[7] = (uint8_t)len;

    return len;
}


int prepare_ctrl_cmd_request_udp(ctrl_cmd_package& cmd, uint64_t svc_handle, int socket_id, int fd, int family)
{
    // cmd data
    cmd.u.udp.svc_handle = svc_handle;
    cmd.u.udp.socket_id = socket_id;
    cmd.u.udp.fd = fd;
    cmd.u.udp.family = family;

    // actually length
    int len = sizeof(cmd.u.udp);

    // cmd header
    cmd.header[6] = (uint8_t)'U';
    cmd.header[7] = (uint8_t)len;

    return len;
}

int prepare_ctrl_cmd_request_send_udp(ctrl_cmd_package& cmd, int socket_id, const send_buffer* buf_ptr, const uint8_t* udp_address, int addr_sz)
{
    // cmd data
    cmd.u.send_udp.send.socket_id = socket_id;
    cmd.u.send_udp.send.data_ptr = buf_ptr;
    ::memcpy(cmd.u.send_udp.address, udp_address, addr_sz);

    // actually length
    int len = sizeof(cmd.u.send_udp.send) + addr_sz;

    // cmd header
    cmd.header[6] = (uint8_t)'A';
    cmd.header[7] = (uint8_t)len;

    return len;
}

}

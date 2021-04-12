#pragma once

#include "../socket/socket_info.h"
#include "../socket/socket_buffer.h"

#include <memory>
#include <list>

namespace skynet {

// skynet node socket event type
enum skynet_socket_event
{
    SKYNET_SOCKET_EVENT_DATA = 1,           // data event
    SKYNET_SOCKET_EVENT_CONNECT = 2,        // connect event
    SKYNET_SOCKET_EVENT_CLOSE = 3,          // socket close event
    SKYNET_SOCKET_EVENT_ACCEPT = 4,         // accept connection event
    SKYNET_SOCKET_EVENT_ERROR = 5,          // socket error event
    SKYNET_SOCKET_EVENT_UDP = 6,            //
    SKYNET_SOCKET_EVENT_WARNING = 7,        //
};

// skynet socket message
struct skynet_socket_message
{
    int socket_event;                       // skynet socket event type
    int socket_id;                          //

    int ud;                                 // user data
                                            // - for accept, ud is new connection id;
                                            // - for data, ud is size of data.
    char* buffer;                           // message data
};

// forward declare
class socket_server;

// skynet node socket
class node_socket final
{
private:
    static node_socket* instance_;
public:
    static node_socket* instance();

private:
    std::shared_ptr<socket_server> socket_server_;

public:
    bool init();
    void fini();

public:
    //
    void exit();
    //
    void update_time();

    // poll socket event
    int poll_socket_event();

    int listen(uint32_t svc_handle, const char* local_ip, int local_port, int backlog);
    int connect(uint32_t svc_handle, const char* remote_addr, int remote_port);
    void close(uint32_t svc_handle, int socket_id);
    void shutdown(uint32_t svc_handle, int socket_id);
    void start(uint32_t svc_handle, int socket_id);
    void pause(uint32_t svc_handle, int socket_id);
    void nodelay(uint32_t svc_handle, int socket_id);
    int bind_os_fd(uint32_t svc_handle, int os_fd);

    int send(uint32_t svc_handle, send_data* sd_ptr);
    int send_low_priority(uint32_t svc_handle, send_data* sd_ptr);

    //
    int udp_socket(uint32_t svc_handle, const char* local_ip, int local_port);
    int udp_connect(uint32_t svc_handle, int socket_id, const char* remote_ip, int remote_port);
    int udp_sendbuffer(uint32_t svc_handle, const char* address, send_data* sd_ptr);
    const char* udp_address(skynet_socket_message*, int* addrsz);

    void get_socket_info(std::list<socket_info>& si_list);
};

}


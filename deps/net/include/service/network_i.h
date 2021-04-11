#pragma once

#include <string>
#include <cstdint>
#include <memory>

namespace skynet::net {

// forward declare
class network_tcp_server_handler;
class network_tcp_client_handler;
class network_udp_server_handler;
class network_udp_client_handler;

/**
 * network interface
 */
class network
{
public:
    virtual ~network() = default;

public:
    // init/fini
    virtual bool init() = 0;
    virtual void fini() = 0;

    // set network event handler
    virtual void set_event_handler(std::shared_ptr<network_tcp_server_handler> event_handler_ptr) = 0;
    virtual void set_event_handler(std::shared_ptr<network_tcp_client_handler> event_handler_ptr) = 0;
    virtual void set_event_handler(std::shared_ptr<network_udp_server_handler> event_handler_ptr) = 0;
    virtual void set_event_handler(std::shared_ptr<network_udp_client_handler> event_handler_ptr) = 0;

    // tcp
public:
    /**
     * create tcp server
     *
     * @param local_uri local listen uri, `ip:port`
     * @param svc_handle skynet service handle
     * @return socket id
     */
    virtual int open_tcp_server(std::string local_uri, uint32_t svc_handle) = 0;

    /**
     * create tcp server
     * @param local_ip local listen ip
     * @param local_port local listen port
     * @param svc_handle skynet service handle
     * @return socket id
     */
    virtual int open_tcp_server(std::string local_ip, uint16_t local_port, uint32_t svc_handle) = 0;

    /**
     *
     */
    virtual void close_tcp_server(uint32_t socket_id, uint32_t svc_handle) = 0;

    /**
     * create tcp client
     * @param remote_uri remote server uri, `host:port`
     * @param svc_handle skynet service handle
     * @return socket id
     */
    virtual int open_tcp_client(std::string remote_uri, uint32_t svc_handle) = 0;

    /**
     * create tcp client
     * @param remote_uri remote server ip or domain name
     * @param remote_uri remote server port
     * @param svc_handle skynet service handle
     * @return socket id
     */
    virtual int open_tcp_client(std::string remote_host, uint16_t remote_port, uint32_t svc_handle) = 0;

    /**
     *
     */
    virtual void close_tcp_client(uint32_t socket_id, uint32_t svc_handle) = 0;

    // udp
public:
    // create udp server
    virtual int udp_socket(std::string local_uri, uint32_t svc_handle) = 0;
    virtual int udp_socket(std::string local_ip, uint16_t local_port, uint32_t svc_handle) = 0;
    virtual void close_udp_server(uint32_t socket_id, uint32_t svc_handle) = 0;

    // create udp client
    virtual int open_udp_client(std::string remote_uri, uint32_t svc_handle) = 0;
    virtual int open_udp_client(std::string remote_host, uint16_t remote_port, uint32_t svc_handle) = 0;
    virtual void close_udp_client(uint32_t socket_id, uint32_t svc_handle) = 0;
};

}

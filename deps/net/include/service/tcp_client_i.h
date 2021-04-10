#pragma once

#include <string>
#include <cstdint>
#include <memory>

namespace skynet::net {

// forward declare
class tcp_client_handler;
class tcp_client_session_config;

// tcp client
class tcp_client
{
public:
    virtual ~tcp_client() = default;

public:
    //
    virtual bool init() = 0;
    virtual void fini() = 0;

    /**
     * set tcp client event handler
     *
     * @param event_handler_ptr
     */
    virtual void set_event_handler(std::shared_ptr<tcp_client_handler> event_handler_ptr) = 0;

    /**
     * connect remote server (async), success or failed will use tcp_client_handler callback
     *
     * @param remote_uri remote server uri, `host:port`
     * @param timeout_seconds connect timeout, include resolve host & connect timeout (seconds)
     * @param local_ip local bind ip, empty means don't bind ip
     * @param local_port local bind port, 0 meants don't bind port
     * @return true if exec connect success
     */
    virtual bool connect(std::string remote_uri, int32_t timeout_seconds = 0,
                         std::string local_ip = "", uint16_t local_port = 0) = 0;

    /**
     * connect remote server (async), success or failed will use tcp_client_handler callback
     *
     * @param remote_addr remote server ip or domain name
     * @param remote_port remote server port
     * @param timeout_seconds connect timeout, include resolve host & connect timeout (seconds)
     * @param local_ip local bind ip, empty means don't bind ip
     * @param local_port local bind port, 0 meants don't bind port
     * @return true if exec connect success
     */
    virtual bool connect(std::string remote_addr, uint16_t remote_port, int32_t timeout_seconds = 0,
                         std::string local_ip = "", uint16_t local_port = 0) = 0;

    /**
     * close tcp client
     */
    virtual void close() = 0;

    /**
     * get client socket id
     */
    virtual uint32_t socket_id() = 0;

    /**
     * get client config
     */
    virtual tcp_client_session_config& session_config() = 0;

    /**
     * send data (async)
     *
     * @param data_ptr
     * @param data_len
     * @return
     */
    virtual bool send(const char* data_ptr, int32_t data_len) = 0;
};

}


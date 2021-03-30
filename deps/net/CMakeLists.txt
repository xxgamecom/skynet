set(SKYNET_NET_HEADER
    uri/uri_codec.h
    uri/uri_codec.inl
    uri/uri_host.h
    uri/uri_host.inl
    uri/uri_port.h
    uri/uri_port.inl
    uri/uri_scheme.h
    uri/uri_scheme.inl
    core/object_pool.h
    core/object_pool.inl
    core/io_buffer.h
    core/io_buffer.inl
    core/io_service.h
    core/io_service.inl
    core/io_service_pool.h
    core/io_service_pool.inl
    core/socket_option_def.h
    transport/tcp_acceptor_def.h
    transport/tcp_acceptor.h
    transport/tcp_acceptor.inl
    transport/tcp_connector.h
    transport/tcp_connector.inl
    transport/tcp_connector_handler.h
    transport/tcp_session_def.h
    transport/tcp_session.h
    transport/tcp_session.inl
    transport/tcp_session_handler.h
    transport/tcp_session_write_queue.h
    transport/tcp_session_write_queue.inl
    service/io_statistics.h
    service/io_statistics.inl
    service/tcp_client.h
    service/tcp_client.inl
    service/tcp_client_config.h
    service/tcp_client_config.inl
    service/tcp_client_handler.h
    service/tcp_server.h
    service/tcp_server.inl
    service/tcp_server_config.h
    service/tcp_server_config.inl
    service/tcp_server_handler.h
    service/tcp_session_idle_checker.h
    service/tcp_session_manager.h
    service/tcp_session_manager.inl
    network.h
)

set(SKYNET_NET_SRC
    uri/uri_codec.cpp
    uri/uri_host.cpp
    uri/uri_port.cpp
    uri/uri_scheme.cpp
    core/io_buffer.cpp
    core/io_service.cpp
    core/io_service_pool.cpp
    transport/tcp_acceptor.cpp
    transport/tcp_connector.cpp
    transport/tcp_session.cpp
    transport/tcp_session_write_queue.cpp
    service/io_statistics.cpp
    service/tcp_client.cpp
    service/tcp_server.cpp
    service/tcp_session_idle_checker.cpp
    service/tcp_session_manager.cpp
)

list(APPEND SKYNET_NET_SRC ${SKYNET_NET_HEADER})

# target libnet.a
add_library(net STATIC ${SKYNET_NET_SRC})
target_link_libraries(net )

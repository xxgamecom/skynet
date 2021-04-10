set(SKYNET_NET_TCP_HEADER
    src/tcp/tcp_acceptor.h
    src/tcp/tcp_client.h
    src/tcp/tcp_client.inl
    src/tcp/tcp_client_config.h
    src/tcp/tcp_client_config.inl
    src/tcp/tcp_connector.h
    src/tcp/tcp_connector.inl
    src/tcp/tcp_server.h
    src/tcp/tcp_server_acceptor_config.h
    src/tcp/tcp_server_acceptor_config.inl
    src/tcp/tcp_server_session_config.h
    src/tcp/tcp_server_session_config.inl
    src/tcp/tcp_session.h
    src/tcp/tcp_session.inl
    src/tcp/tcp_session_write_queue.h
    src/tcp/tcp_session_write_queue.inl
)

set(SKYNET_NET_TCP_SRC
    src/tcp/tcp_acceptor.cpp
    src/tcp/tcp_client.cpp
    src/tcp/tcp_connector.cpp
    src/tcp/tcp_server.cpp
    src/tcp/tcp_session.cpp
    src/tcp/tcp_session_write_queue.cpp
)


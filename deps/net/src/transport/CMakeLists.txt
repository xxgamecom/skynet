set(SKYNET_NET_TRANSPORT_HEADER
    src/transport/tcp_acceptor.h
    src/transport/tcp_connector.h
    src/transport/tcp_connector.inl
    src/transport/tcp_session.h
    src/transport/tcp_session.inl
    src/transport/tcp_session_write_queue.h
    src/transport/tcp_session_write_queue.inl
    src/transport/udp_session.h
)

set(SKYNET_NET_TRANSPORT_SRC
    src/transport/tcp_acceptor.cpp
    src/transport/tcp_connector.cpp
    src/transport/tcp_session.cpp
    src/transport/tcp_session_write_queue.cpp
    src/transport/udp_session.cpp
)


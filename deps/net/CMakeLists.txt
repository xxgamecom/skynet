include(src/core/CMakeLists.txt)
include(src/tcp/CMakeLists.txt)
include(src/udp/CMakeLists.txt)

# target libnet.a
add_library(net STATIC
    src/net.cpp
    ${SKYNET_NET_CORE_SRC}
    ${SKYNET_NET_TCP_SRC}
    ${SKYNET_NET_UDP_SRC}
)
target_link_libraries(net )

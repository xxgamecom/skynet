include(base/CMakeLists.txt)
include(uri/CMakeLists.txt)
include(core/CMakeLists.txt)
include(tcp/CMakeLists.txt)
include(udp/CMakeLists.txt)

set(SKYNET_NET_HEADER
    ${SKYNET_NET_BASE_HEADER}
    ${SKYNET_NET_URI_HEADER}
    ${SKYNET_NET_CORE_HEADER}
    ${SKYNET_NET_TCP_HEADER}
    ${SKYNET_NET_UDP_HEADER}
    net.h
)

set(SKYNET_NET_SRC
    ${SKYNET_NET_BASE_SRC}
    ${SKYNET_NET_URI_SRC}
    ${SKYNET_NET_CORE_SRC}
    ${SKYNET_NET_TCP_SRC}
    ${SKYNET_NET_UDP_SRC}
)

list(APPEND SKYNET_NET_SRC ${SKYNET_NET_HEADER})

# target libnet.a
add_library(net STATIC ${SKYNET_NET_SRC})
target_link_libraries(net )

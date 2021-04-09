include(src/base/CMakeLists.txt)
include(src/tcp/CMakeLists.txt)
include(src/udp/CMakeLists.txt)
include(src/session/CMakeLists.txt)
#include(src/uri/CMakeLists.txt)

set(SKYNET_NET_HEADER
    ${SKYNET_NET_CORE_HEADER}
    ${SKYNET_NET_TCP_HEADER}
    ${SKYNET_NET_UDP_HEADER}
    ${SKYNET_NET_SESSION_HEADER}
#    ${SKYNET_NET_URI_HEADER}
)
set(SKYNET_NET_SRC
    ${SKYNET_NET_CORE_SRC}
    ${SKYNET_NET_TCP_SRC}
    ${SKYNET_NET_UDP_SRC}
    ${SKYNET_NET_SESSION_SRC}
#    ${SKYNET_NET_URI_SRC}
)
list(APPEND SKYNET_NET_SRC ${SKYNET_NET_HEADER})

# target libnet.a
add_library(net STATIC
    src/net.cpp
    src/socket_server.cpp
    ${SKYNET_NET_SRC}
)
target_link_libraries(net )

# examples
add_subdirectory(${SKYNET_ROOT_PATH}/deps/net/examples)

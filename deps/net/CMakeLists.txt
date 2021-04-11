include(src/base/CMakeLists.txt)
include(src/uri/CMakeLists.txt)
include(src/transport/CMakeLists.txt)
include(src/session/CMakeLists.txt)
include(src/service/CMakeLists.txt)

set(SKYNET_NET_HEADER
    ${SKYNET_NET_BASE_HEADER}
    ${SKYNET_NET_URI_HEADER}
    ${SKYNET_NET_TRANSPORT_HEADER}
    ${SKYNET_NET_SESSION_HEADER}
    ${SKYNET_NET_SERVICE_HEADER}
)
set(SKYNET_NET_SRC
    ${SKYNET_NET_BASE_SRC}
    ${SKYNET_NET_URI_SRC}
    ${SKYNET_NET_TRANSPORT_SRC}
    ${SKYNET_NET_SESSION_SRC}
    ${SKYNET_NET_SERVICE_SRC}
)
list(APPEND SKYNET_NET_SRC ${SKYNET_NET_HEADER})

# target libnet.a
add_library(net STATIC src/net.cpp ${SKYNET_NET_SRC})
target_link_libraries(net )

# examples
add_subdirectory(${SKYNET_ROOT_PATH}/deps/net/examples)

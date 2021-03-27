# include path
include_directories("${SKYNET_ROOT_PATH}/skynet-src")

#
set(SKYNET_LUACLIB_HEADER
    lsha1.h
    lua-seri.h
)
set(SKYNET_LUACLIB_SRC
    lsha1.c
    lua-crypt.cpp
    lua-seri.cpp
    lua-memory.cpp
    lua-core.cpp
    lua-profile.cpp
    lua-socket.cpp
    lua-netpack.cpp
    lua-netpack_tcp.cpp
    lua-netpack_ws.cpp
    lua-cluster.cpp
    lua-mongo.cpp
    lua-clientsocket.cpp
    lua-clientsocket_tcp.cpp
    lua-clientsocket_ws.cpp
    lua-stm.cpp
    lua-sharetable.cpp
    lua-debugchannel.cpp
)
list(APPEND SKYNET_LUACLIB_SRC ${SKYNET_LUACLIB_HEADER})

# target module lib (skynet.so)
add_library(skynet_luaclib MODULE ${SKYNET_LUACLIB_SRC})
set_target_properties(skynet_luaclib
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "skynet"
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB}"
)
target_link_libraries(skynet_luaclib ${SKYNET_SHARED})

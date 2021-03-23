# include path
include_directories("${SKYNET_ROOT_PATH}/skynet")

#
set(SKYNET_LUACLIB_HEADER
    lsha1.h
    lua-seri.h
)

set(SKYNET_LUACLIB_SRC
    lsha1.c
    lua-skynet_crypt.cpp
    lua-seri.cpp
    lua-skynet_core.cpp
    lua-skynet_netpack.cpp
    lua-skynet_netpack_tcp.cpp
    lua-skynet_netpack_ws.cpp
    lua-skynet_socket.cpp
    lua-skynet_cluster.cpp
    lua-skynet_mongo.cpp
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
target_link_libraries(skynet_luaclib skynet_core)

# include path
INCLUDE_DIRECTORIES("${SKYNET_ROOT_PATH}/skynet")

#
set(SKYNET_LUACLIB_HEADER
    lua-seri.h
)

set(SKYNET_LUACLIB_SRC
    lua-seri.cpp
    lua-netpack.cpp
    lua-socket.cpp
    lua-skynet.cpp
)

LIST(APPEND SKYNET_LUACLIB_SRC ${SKYNET_LUACLIB_HEADER})

# target shared lib (skynet.so)
ADD_LIBRARY(libskynet MODULE
    ${SKYNET_LUACLIB_SRC}
    $<TARGET_OBJECTS:skynet_o>
)
SET_TARGET_PROPERTIES(libskynet
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "skynet"
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
)
TARGET_LINK_LIBRARIES(libskynet liblua)

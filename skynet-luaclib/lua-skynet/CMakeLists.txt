# include path
INCLUDE_DIRECTORIES("${SKYNET_ROOT_PATH}/skynet")

# source file list
AUX_SOURCE_DIRECTORY("${SKYNET_ROOT_PATH}/skynet-luaclib/lua-skynet" SKYNET_SRC)

# target shared lib (skynet.so)
ADD_LIBRARY(libskynet MODULE 
    ${SKYNET_SRC}
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

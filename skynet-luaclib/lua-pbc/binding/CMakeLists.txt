# header file
INCLUDE_DIRECTORIES("${SKYNET_ROOT_PATH}/skynet-luaclib/lua-pbc")

# target shared lib (protobuf.so)
ADD_LIBRARY(protobuf MODULE
    "${SKYNET_ROOT_PATH}/skynet-luaclib/lua-pbc/binding/lua53/pbc-lua53.c"
)
SET_TARGET_PROPERTIES(protobuf
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
)
TARGET_LINK_LIBRARIES(protobuf liblua pbc)

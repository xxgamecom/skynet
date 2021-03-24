# header file
include_directories("${SKYNET_ROOT_PATH}/luaclib-src/lua-pbc")

# target shared lib (protobuf.so)
add_library(protobuf MODULE
    "${SKYNET_ROOT_PATH}/luaclib-src/lua-pbc/binding/lua53/pbc-lua53.c"
)
set_target_properties(protobuf
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB}"
)
target_link_libraries(protobuf pbc)

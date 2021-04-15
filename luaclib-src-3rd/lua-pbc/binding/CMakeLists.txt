# header file
include_directories("${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-pbc")

# target shared lib (protobuf.so)
add_library(protobuf MODULE
    "${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-pbc/binding/lua53/pbc-lua53.c"
)
set_target_properties(protobuf
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
)
target_link_libraries(protobuf ${SKYNET_SHARED} pbc)

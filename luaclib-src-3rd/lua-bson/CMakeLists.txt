# target shared lib (bson.so)
add_library(bson MODULE
    "${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-bson/lua-bson.cpp"
)
set_target_properties(bson
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
)
target_link_libraries(bson ${SKYNET_SHARED})

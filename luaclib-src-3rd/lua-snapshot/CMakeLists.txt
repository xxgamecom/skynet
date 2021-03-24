# target shared lib (snapshot.so)
add_library(snapshot MODULE
    "${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-snapshot/snapshot.c"
)
set_target_properties(snapshot
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
)
target_link_libraries(snapshot ${SKYNET_SHARED})

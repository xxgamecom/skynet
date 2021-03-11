# target shared lib (snapshot.so)
ADD_LIBRARY(snapshot MODULE 
    "${SKYNET_ROOT_PATH}/skynet-luaclib/lua-snapshot/snapshot.c"
)
SET_TARGET_PROPERTIES(snapshot 
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
)
TARGET_LINK_LIBRARIES(snapshot liblua)

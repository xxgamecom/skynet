# target shared lib (md5.so)
ADD_LIBRARY(md5 MODULE  
    "${SKYNET_ROOT_PATH}/skynet-luaclib/lua-md5/compat-5.2.c"
    "${SKYNET_ROOT_PATH}/skynet-luaclib/lua-md5/md5.c"
    "${SKYNET_ROOT_PATH}/skynet-luaclib/lua-md5/md5lib.c"
)
SET_TARGET_PROPERTIES(md5
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
)
TARGET_LINK_LIBRARIES(md5 liblua)

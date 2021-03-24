#
set(SKYNET_MD5_HEADER
    compat-5.2.h
    md5.c
)
set(SKYNET_MD5_SRC
    compat-5.2.c
    md5.c
    md5lib.c
)
list(APPEND SKYNET_MD5_SRC ${SKYNET_MD5_HEADER})

# target shared lib (md5.so)
add_library(md5 MODULE ${SKYNET_MD5_SRC})
set_target_properties(md5
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
)
target_link_libraries(md5 ${SKYNET_SHARED})

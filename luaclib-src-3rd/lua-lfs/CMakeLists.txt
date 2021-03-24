# source file list
AUX_SOURCE_DIRECTORY("${SKYNET_ROOT_PATH}/luaclib-src/lua-lfs/src" LUA_LFS_SRC)

#
set(LUA_LFS_HEADER
    src/lfs.h
)
set(LUA_LFS_SRC
    src/lfs.c
)
list(APPEND LUA_LFS_SRC ${LUA_LFS_HEADER})

# target shared lib
add_library(lfs MODULE  ${LUA_LFS_SRC})
set_target_properties(lfs
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
)
target_link_libraries(lfs ${SKYNET_SHARED})

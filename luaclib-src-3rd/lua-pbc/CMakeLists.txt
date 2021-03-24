# header file
include_directories("${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-pbc")
include_directories("${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-pbc/src")

# source file list
aux_source_directory("${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-pbc/src" LUA_PBC_SRC)

# target shared lib (libpbc.a)
add_library(pbc STATIC  ${LUA_PBC_SRC})
#target_link_libraries(pbc liblua)
target_link_libraries(pbc )

# header file
include_directories("${SKYNET_ROOT_PATH}/luaclib-src/lua-pbc")
include_directories("${SKYNET_ROOT_PATH}/luaclib-src/lua-pbc/src")

# source file list
AUX_SOURCE_DIRECTORY("${SKYNET_ROOT_PATH}/luaclib-src/lua-pbc/src" LUA_PBC_SRC)

# target shared lib (libpbc.a)
add_library(pbc STATIC  ${LUA_PBC_SRC})
target_link_libraries(pbc liblua)

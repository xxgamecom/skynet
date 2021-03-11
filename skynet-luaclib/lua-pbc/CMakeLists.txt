# header file
INCLUDE_DIRECTORIES("${SKYNET_ROOT_PATH}/skynet-luaclib/lua-pbc")
INCLUDE_DIRECTORIES("${SKYNET_ROOT_PATH}/skynet-luaclib/lua-pbc/src")

# source file list
AUX_SOURCE_DIRECTORY("${SKYNET_ROOT_PATH}/skynet-luaclib/lua-pbc/src" LUA_PBC_SRC)

# target shared lib (libpbc.a)
ADD_LIBRARY(pbc STATIC  ${LUA_PBC_SRC})
TARGET_LINK_LIBRARIES(pbc liblua)

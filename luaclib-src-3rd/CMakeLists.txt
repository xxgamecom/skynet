# lpeg
add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-lpeg)
# lua-md5
add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-md5)
# lua-lfs
add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-lfs)
# lua-iconv
add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-iconv)
# lua-cjson
add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-cjson)
# lua-bson
add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-bson)
# lua-snapshot
add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-snapshot)

#------------------------------------
# protocol
#------------------------------------

# lua-pbc
add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-pbc)
add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-pbc/binding)
# sproto support
add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/sproto)

#------------------------------------
# need openssl support
#------------------------------------

if (BUILD_SKYNET_SUPPORT_OPENSSL)
    # lua-tls
    add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-tls)
    # lua-codec
    add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-codec)
    # lua-curl
    add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-curl)
    # lua-httpsc
    add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-httpsc)
endif()

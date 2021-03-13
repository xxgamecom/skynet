# lpeg
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-lpeg)

# lua-bson
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-bson)

# lua-md5
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-md5)

# lua-codec
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-codec)

# lua-curl
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-curl)

# lua-iconv
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-iconv)

# lua-cjson
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-cjson)

# lua-lfs
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-lfs)

# lua-snapshot
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-snapshot)

# lua-httpsc
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-httpsc)

# lua-epoll
if (LINUX)
    ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-epoll)
endif()

# lua-pbc
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-pbc)
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-pbc/binding)

# sproto support
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/sproto)

# lua-skynet
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib/lua-skynet)

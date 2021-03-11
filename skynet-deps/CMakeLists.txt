# lua
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-deps/lua)

# jemalloc
if (LINUX)
    ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-deps/jemalloc)
endif()

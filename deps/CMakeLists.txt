# lua
ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/deps/lua)

# jemalloc
if (LINUX)
    ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/deps/jemalloc)
endif()

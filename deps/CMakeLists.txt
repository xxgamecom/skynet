# lua
add_subdirectory(${SKYNET_ROOT_PATH}/deps/lua)

# jemalloc
if (LINUX)
    add_subdirectory(${SKYNET_ROOT_PATH}/deps/jemalloc)
endif()

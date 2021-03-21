project(skynet++ C CXX)
cmake_minimum_required(VERSION 3.12)

# skynet++ root & parent path
set(SKYNET_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
get_filename_component(SKYNET_PARENT_DIR ${SKYNET_ROOT_PATH} DIRECTORY)

# skynet++ bin, lib, service path
set(SKYNET_BIN_PATH             ${SKYNET_PARENT_DIR}/bin)
set(SKYNET_BIN_PATH_LUACLIB     ${SKYNET_PARENT_DIR}/bin/luaclib)
set(SKYNET_BIN_PATH_LUALIB      ${SKYNET_PARENT_DIR}/bin/lualib)
set(SKYNET_BIN_PATH_SERVICE_C   ${SKYNET_PARENT_DIR}/bin/service-c)
set(SKYNET_BIN_PATH_SERVICE_LUA ${SKYNET_PARENT_DIR}/bin/service-lua)
set(SKYNET_BIN_PATH_TESTS       ${SKYNET_PARENT_DIR}/bin/tests)


# cmake module path
set(CMAKE_MODULE_PATH ${SKYNET_ROOT_PATH}/cmake/modules/)

# disallow build in-source
include(DisallowBuildInSource)

#------------------------------------------------------
# compiler
#------------------------------------------------------

# set common compiler define & options
include(ConfigDefine)

#
message(STATUS "PROJECT_NAME              : " ${PROJECT_NAME})
message(STATUS "SKYNET_BUILD_TYPE         : " ${CMAKE_BUILD_TYPE})
message(STATUS "SKYNET_ROOT_PATH          : " ${SKYNET_ROOT_PATH})
message(STATUS "SKYNET_BIN_PATH           : " ${SKYNET_BIN_PATH})
message(STATUS "SKYNET_BIN_PATH_LUACLIB   : " ${SKYNET_BIN_PATH_LUACLIB})
message(STATUS "SKYNET_BIN_PATH_SERVICE_C : " ${SKYNET_BIN_PATH_SERVICE_C})
message(STATUS "SKYNET_BIN_PATH_TESTS     : " ${SKYNET_BIN_PATH_TESTS})
message(STATUS "CMAKE_MODULE_PATH         : " ${CMAKE_MODULE_PATH})

# rebuild: delete build dir if you hope a full clean rebuild
message(STATUS "SKYNET_BUILD_DIR          : " ${PROJECT_BINARY_DIR})

# use jemalloc
if (MACOSX)
    add_definitions(-DDONT_USE_JEMALLOC)
endif()

#------------------------------------------------------
# default output & include path
#------------------------------------------------------

# output path
set(EXECUTABLE_OUTPUT_PATH "${SKYNET_BIN_PATH}")

# include path
include_directories("${SKYNET_ROOT_PATH}/deps/lua")
include_directories("${SKYNET_ROOT_PATH}/deps/asio/include")

#------------------------------------------------------
# build
#------------------------------------------------------

# build options
option(BUILD_SKYNET_DEPS "Build skynet deps" ON)
option(BUILD_SKYNET "Build skynet" ON)
option(BUILD_SKYNET_LUACLIB "Build skynet luaclib" ON)
option(BUILD_SKYNET_CSERVICE "Build skynet c service" ON)
option(BUILD_SKYNET_TESTS "Build skynet tests" OFF)

# build deps
if (BUILD_SKYNET_DEPS)
    add_subdirectory(${SKYNET_ROOT_PATH}/deps)
endif()

# build skynet
if (BUILD_SKYNET)
    add_subdirectory(${SKYNET_ROOT_PATH}/skynet)
endif()

# build skynet-luaclib
if (BUILD_SKYNET_LUACLIB)
    add_subdirectory(${SKYNET_ROOT_PATH}/skynet-luaclib)
endif()

# build skynet-service-c
if (BUILD_SKYNET_CSERVICE)
    add_subdirectory(${SKYNET_ROOT_PATH}/skynet-service-c)
endif()

# build skynet tests
if (BUILD_SKYNET_TESTS)
    add_subdirectory(${SKYNET_ROOT_PATH}/tests)
endif()

# copy lualib to bin/lualib
#add_custom_command(TARGET skynet POST_BUILD         # Adds a post-build event to MyTest
#    COMMAND ${CMAKE_COMMAND} -E copy_if_different   # which executes "cmake - E copy_if_different..."
#    "${SKYNET_ROOT_PATH}/lualib"                    # <--this is in-file
#    $<TARGET_FILE_DIR:${SKYNET_BIN_PATH}/lualib>)   # <--this is out-file path

# copy lua service to bin/service-lua
project(skynet++ C CXX)
cmake_minimum_required(VERSION 3.12)

# skynet++ root & parent path
set(SKYNET_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
#get_filename_component(SKYNET_PARENT_DIR ${SKYNET_ROOT_PATH} DIRECTORY)

# skynet++ bin, lib, service path
set(SKYNET_BIN_PATH             ${SKYNET_ROOT_PATH})
set(SKYNET_BIN_PATH_CSERVICE    ${SKYNET_BIN_PATH}/cservice)
set(SKYNET_BIN_PATH_LUACLIB     ${SKYNET_BIN_PATH}/luaclib)
set(SKYNET_BIN_PATH_LUACLIB_3RD ${SKYNET_BIN_PATH}/luaclib)
set(SKYNET_BIN_PATH_TESTS       ${SKYNET_BIN_PATH}/tests)


# cmake module path
set(CMAKE_MODULE_PATH ${SKYNET_ROOT_PATH}/cmake/modules/)

# disallow build in-source
include(DisallowBuildInSource)

#------------------------------------------------------
# compiler
#------------------------------------------------------

# set common compiler define & options
include(ConfigDefine)

if(MACOSX)
    set(SKYNET_EXPORT )
    set(SKYNET_SHARED -Wl,-undefined,dynamic_lookup)  # -dynamiclib
elseif(LINUX OR FREEBSD)
    set(SKYNET_EXPORT -Wl,-E)
    set(SKYNET_SHARED --shared)
endif()

# Debug/Release support
set(CMAKE_CXX_FLAGS_DEBUG   "$ENV{CXXFLAGS} -O0 -Wall -g -ggdb")
set(CMAKE_CXX_FLAGS_RELEASE "$ENV{CXXFLAGS} -O3 -Wall")

#
message(STATUS "PROJECT_NAME                : " ${PROJECT_NAME})
message(STATUS "SKYNET_BUILD_TYPE           : " ${CMAKE_BUILD_TYPE})
message(STATUS "SKYNET_ROOT_PATH            : " ${SKYNET_ROOT_PATH})
message(STATUS "SKYNET_BIN_PATH             : " ${SKYNET_BIN_PATH})
message(STATUS "SKYNET_BIN_PATH_CSERVICE    : " ${SKYNET_BIN_PATH_CSERVICE})
message(STATUS "SKYNET_BIN_PATH_LUACLIB     : " ${SKYNET_BIN_PATH_LUACLIB})
message(STATUS "SKYNET_BIN_PATH_LUACLIB_3RD : " ${SKYNET_BIN_PATH_LUACLIB_3RD})
message(STATUS "SKYNET_BIN_PATH_TESTS       : " ${SKYNET_BIN_PATH_TESTS})
message(STATUS "CMAKE_MODULE_PATH           : " ${CMAKE_MODULE_PATH})

# rebuild: delete build dir if you hope a full clean rebuild
message(STATUS "SKYNET_BUILD_DIR            : " ${PROJECT_BINARY_DIR})

#------------------------------------------------------
# default output & include path
#------------------------------------------------------

# output path
set(EXECUTABLE_OUTPUT_PATH "${SKYNET_BIN_PATH}")

# include path
include_directories("${SKYNET_ROOT_PATH}/deps/lua")
include_directories("${SKYNET_ROOT_PATH}/deps/fmt/include")
include_directories("${SKYNET_ROOT_PATH}/deps/asio/include")
include_directories("${SKYNET_ROOT_PATH}/deps/spdlog/include")
include_directories("${SKYNET_ROOT_PATH}/deps/net/include")

#------------------------------------------------------
# build
#------------------------------------------------------

# fmt header-only
add_definitions(-DFMT_HEADER_ONLY)

# build options
option(BUILD_SKYNET_DEPS "Build skynet deps" ON)
option(BUILD_SKYNET "Build skynet" ON)
option(BUILD_SKYNET_SUPPORT_OPENSSL "Build skynet support openssl" OFF)
option(BUILD_SKYNET_CSERVICE "Build skynet c service" ON)
option(BUILD_SKYNET_LUACLIB "Build skynet luaclib" ON)
option(BUILD_SKYNET_LUACLIB_3RD "Build skynet luaclib-3rd" ON)
option(BUILD_SKYNET_TESTS "Build skynet tests" OFF)

# build deps
if (BUILD_SKYNET_DEPS)
    add_subdirectory(${SKYNET_ROOT_PATH}/deps)
endif()

# build skynet
if (BUILD_SKYNET)
    add_subdirectory(${SKYNET_ROOT_PATH}/skynet-src)
endif()

# build service-src
if (BUILD_SKYNET_CSERVICE)
    add_subdirectory(${SKYNET_ROOT_PATH}/service-src)
endif()

# build luaclib-src
if (BUILD_SKYNET_LUACLIB)
    add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src)
endif()

# build luaclib-src-3rd
if (BUILD_SKYNET_LUACLIB_3RD)
    add_subdirectory(${SKYNET_ROOT_PATH}/luaclib-src-3rd)
endif()

# build skynet tests
if (BUILD_SKYNET_TESTS)
    add_subdirectory(${SKYNET_ROOT_PATH}/tests)
endif()

#add_subdirectory(${SKYNET_ROOT_PATH}/tests/net)


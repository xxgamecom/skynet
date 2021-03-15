PROJECT(skynet++ C CXX)
CMAKE_MINIMUM_REQUIRED(VERSION 3.12)

# skynet++ root & parent path
SET(SKYNET_ROOT_PATH ${CMAKE_CURRENT_SOURCE_DIR})
GET_FILENAME_COMPONENT(SKYNET_PARENT_DIR ${SKYNET_ROOT_PATH} DIRECTORY)

# skynet++ bin & lib path
SET(SKYNET_BIN_PATH ${SKYNET_PARENT_DIR}/bin)
SET(SKYNET_LUACLIB_BIN_PATH ${SKYNET_PARENT_DIR}/bin/luaclib)
SET(SKYNET_CSERVICE_BIN_PATH ${SKYNET_PARENT_DIR}/bin/cservice)


# cmake module path
SET(CMAKE_MODULE_PATH ${SKYNET_ROOT_PATH}/cmake/modules/)

# disallow build in-source
INCLUDE(DisallowBuildInSource)

#------------------------------------------------------
# compiler
#------------------------------------------------------

# set common compiler define & options
INCLUDE(ConfigDefine)

#
MESSAGE(STATUS "PROJECT_NAME             : " ${PROJECT_NAME})
MESSAGE(STATUS "SKYNET_BUILD_TYPE        : " ${CMAKE_BUILD_TYPE})
MESSAGE(STATUS "SKYNET_ROOT_PATH         : " ${SKYNET_ROOT_PATH})
MESSAGE(STATUS "SKYNET_BIN_PATH          : " ${SKYNET_BIN_PATH})
MESSAGE(STATUS "SKYNET_LUACLIB_BIN_PATH  : " ${SKYNET_LUACLIB_BIN_PATH})
MESSAGE(STATUS "SKYNET_CSERVICE_BIN_PATH : " ${SKYNET_CSERVICE_BIN_PATH})
MESSAGE(STATUS "CMAKE_MODULE_PATH        : " ${CMAKE_MODULE_PATH})

# rebuild: delete build dir if you hope a full clean rebuild
MESSAGE(STATUS "SKYNET_BUILD_DIR         : " ${PROJECT_BINARY_DIR})

# use jemalloc
if (MACOSX)
    ADD_DEFINITIONS(-DDONT_USE_JEMALLOC)
endif()

#------------------------------------------------------
# default output & include path
#------------------------------------------------------

# output path
SET(EXECUTABLE_OUTPUT_PATH "${SKYNET_BIN_PATH}")

# include path
INCLUDE_DIRECTORIES("${SKYNET_ROOT_PATH}/deps/lua")

#------------------------------------------------------
# build
#------------------------------------------------------

# build options
OPTION(BUILD_SKYNET_DEPS "Build skynet deps" ON)
OPTION(BUILD_SKYNET "Build skynet" ON)
OPTION(BUILD_SKYNET_LUACLIB "Build skynet luaclib" ON)
OPTION(BUILD_SKYNET_CSERVICE "Build skynet c service" ON)
OPTION(BUILD_SKYNET_TESTS "Build skynet tests" OFF)

# build deps
if (BUILD_SKYNET_DEPS)
    ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/deps)
endif()

# build skynet
if (BUILD_SKYNET)
    ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet)
endif()

# build skynet-luaclib
if (BUILD_SKYNET_LUACLIB)
    ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-luaclib)
endif()

# build skynet-service-c
if (BUILD_SKYNET_CSERVICE)
    ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/skynet-service-c)
endif()

# build skynet tests
if (BUILD_SKYNET_TESTS)
    ADD_SUBDIRECTORY(${SKYNET_ROOT_PATH}/tests)
endif()
#
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif()

# Use internal number conversion routines
ADD_DEFINITIONS(-DUSE_INTERNAL_FPCONV)

INCLUDE(TestBigEndian)
TEST_BIG_ENDIAN(IEEE_BIG_ENDIAN)
if(IEEE_BIG_ENDIAN)
    ADD_DEFINITIONS(-DIEEE_BIG_ENDIAN)
endif()

# multiple threads
SET(CMAKE_THREAD_PREFER_PTHREAD TRUE)
#
FIND_PACKAGE(Threads REQUIRED)
if(NOT CMAKE_USE_PTHREADS_INIT)
    MESSAGE(FATAL_ERROR "Pthreads not found")
endif()
ADD_DEFINITIONS(-DMULTIPLE_THREADS)


# Handle platforms missing isinf() macro (Eg, some Solaris systems).
INCLUDE(CheckSymbolExists)
CHECK_SYMBOL_EXISTS(isinf math.h HAVE_ISINF)
if(NOT HAVE_ISINF)
    ADD_DEFINITIONS(-DUSE_INTERNAL_ISINF)
endif()

if(APPLE)
    SET(CMAKE_SHARED_MODULE_CREATE_C_FLAGS "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -undefined dynamic_lookup")
endif()


# target shared lib
ADD_LIBRARY(cjson MODULE 
    "${SKYNET_ROOT_PATH}/skynet-luaclib/lua-cjson/lua_cjson.c"
    "${SKYNET_ROOT_PATH}/skynet-luaclib/lua-cjson/strbuf.c"
    "${SKYNET_ROOT_PATH}/skynet-luaclib/lua-cjson/g_fmt.c"
    "${SKYNET_ROOT_PATH}/skynet-luaclib/lua-cjson/dtoa.c"
)
SET_TARGET_PROPERTIES(cjson
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
)
TARGET_LINK_LIBRARIES(cjson liblua ${CMAKE_THREAD_LIBS_INIT})

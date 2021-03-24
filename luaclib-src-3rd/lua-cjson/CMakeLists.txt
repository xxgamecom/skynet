#
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING
        "Choose the type of build, options are: None Debug Release RelWithDebInfo MinSizeRel."
        FORCE)
endif()

# Use internal number conversion routines
add_definitions(-DUSE_INTERNAL_FPCONV)

include(TestBigEndian)
test_big_endian(IEEE_BIG_ENDIAN)
if(IEEE_BIG_ENDIAN)
    add_definitions(-DIEEE_BIG_ENDIAN)
endif()

# multiple threads
set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
#
find_package(Threads REQUIRED)
if(NOT CMAKE_USE_PTHREADS_INIT)
    message(FATAL_ERROR "Pthreads not found")
endif()
add_definitions(-DMULTIPLE_THREADS)


# Handle platforms missing isinf() macro (Eg, some Solaris systems).
include(CheckSymbolExists)
check_symbol_exists(isinf math.h HAVE_ISINF)
if(NOT HAVE_ISINF)
    add_definitions(-DUSE_INTERNAL_ISINF)
endif()

if(MACOSX)
    SET(CMAKE_SHARED_MODULE_CREATE_C_FLAGS "${CMAKE_SHARED_MODULE_CREATE_C_FLAGS} -undefined dynamic_lookup")
endif()


# target shared lib
add_library(cjson MODULE
    "${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-cjson/lua_cjson.c"
    "${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-cjson/strbuf.c"
    "${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-cjson/g_fmt.c"
    "${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-cjson/dtoa.c"
)
set_target_properties(cjson
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
)
target_link_libraries(cjson ${SKYNET_SHARED} ${CMAKE_THREAD_LIBS_INIT})

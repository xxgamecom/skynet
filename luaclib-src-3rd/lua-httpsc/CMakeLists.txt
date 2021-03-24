# use static
set(OPENSSL_USE_STATIC_LIBS ON)

# find openssl
find_package(OpenSSL COMPONENTS SSL Crypto REQUIRED)
if (OpenSSL_FOUND)
    # include
    include_directories("${OPENSSL_INCLUDE_DIR}")

    #message(STATUS "OpenSSL version      : " ${OPENSSL_VERSION})
    #message(STATUS "OpenSSL include path : " ${OPENSSL_INCLUDE_DIR})
    #message(STATUS "OpenSSL ssl path     : " ${OPENSSL_SSL_LIBRARIES})
    #message(STATUS "OpenSSL crypto path  : " ${OPENSSL_CRYPTO_LIBRARIES})

    # target shared lib
    add_library(httpsc MODULE
        "${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-httpsc/lua_httpsc.c"
    )
    #
    set_target_properties(httpsc
        PROPERTIES
        PREFIX ""
        ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
        LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
    )
    target_link_libraries(httpsc ${SKYNET_SHARED} OpenSSL::SSL OpenSSL::Crypto idn z)
else()
    message(WARNING "lua-httpsc need OpenSSL, but not found OpenSSL, SKIP...")
endif()

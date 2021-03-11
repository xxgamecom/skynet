# use static
SET(OPENSSL_USE_STATIC_LIBS ON)

# find openssl
FIND_PACKAGE(OpenSSL COMPONENTS SSL Crypto REQUIRED)
if (OpenSSL_FOUND)
    # include
    INCLUDE_DIRECTORIES("${OPENSSL_INCLUDE_DIR}")

    #MESSAGE(STATUS "OpenSSL version      : " ${OPENSSL_VERSION})
    #MESSAGE(STATUS "OpenSSL include path : " ${OPENSSL_INCLUDE_DIR})
    #MESSAGE(STATUS "OpenSSL ssl path     : " ${OPENSSL_SSL_LIBRARIES})
    #MESSAGE(STATUS "OpenSSL crypto path  : " ${OPENSSL_CRYPTO_LIBRARIES})

    # target shared lib
    ADD_LIBRARY(httpsc MODULE  
        "${SKYNET_ROOT_PATH}/skynet-luaclib/lua-httpsc/lua_httpsc.c"
    )
    #
    SET_TARGET_PROPERTIES(httpsc
        PROPERTIES
        PREFIX ""
        ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
        LIBRARY_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
    )
    TARGET_LINK_LIBRARIES(httpsc liblua OpenSSL::SSL OpenSSL::Crypto idn z)
else()
    MESSAGE(WARNING "lua-httpsc need OpenSSL, but not found OpenSSL, SKIP...")
endif()

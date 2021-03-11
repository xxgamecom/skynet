# use static
#SET(ICONV_USE_STATIC_LIBS ON)

# find iconv
FIND_PACKAGE(iconv REQUIRED)
if (Iconv_FOUND)
    MESSAGE(STATUS "iconv found")
    # include
    INCLUDE_DIRECTORIES("${Iconv_INCLUDE_DIR}")

    MESSAGE(STATUS "iconv include path       : " ${Iconv_INCLUDE_DIR})
    MESSAGE(STATUS "iconv lib path           : " ${Iconv_LIBRARIES})

    # target shared lib
    ADD_LIBRARY(iconv MODULE 
        "${SKYNET_ROOT_PATH}/skynet-luaclib/lua-iconv/luaiconv.c"
    )
    SET_TARGET_PROPERTIES(iconv
        PROPERTIES
        PREFIX ""
        ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
        LIBRARY_OUTPUT_DIRECTORY "${SKYNET_LUACLIB_BIN_PATH}"
    )
    TARGET_LINK_LIBRARIES(iconv liblua Iconv::Iconv)
else()
    MESSAGE(WARNING "lua-iconv need iconv, but not found iconv, SKIP...")
endif()


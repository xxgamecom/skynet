# use static
#SET(ICONV_USE_STATIC_LIBS ON)

# find iconv
FIND_PACKAGE(iconv REQUIRED)
if (Iconv_FOUND)
    MESSAGE(STATUS "iconv found")
    # include
    include_directories("${Iconv_INCLUDE_DIR}")

    MESSAGE(STATUS "iconv include path       : " ${Iconv_INCLUDE_DIR})
    MESSAGE(STATUS "iconv lib path           : " ${Iconv_LIBRARIES})

    # target shared lib
    add_library(iconv MODULE
        "${SKYNET_ROOT_PATH}/luaclib-src-3rd/lua-iconv/luaiconv.c"
    )
    set_target_properties(iconv
        PROPERTIES
        PREFIX ""
        ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
        LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
    )
    target_link_libraries(iconv ${SKYNET_SHARED} Iconv::Iconv)
else()
    MESSAGE(WARNING "lua-iconv need iconv, but not found iconv, SKIP...")
endif()


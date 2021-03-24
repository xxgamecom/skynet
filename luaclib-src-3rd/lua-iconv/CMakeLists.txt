# use static
#set(ICONV_USE_STATIC_LIBS ON)

# find iconv
find_package(Iconv REQUIRED)
if (Iconv_FOUND)
    message(STATUS "iconv found")

    if (NOT Iconv_IS_BUILT_IN)
        message(STATUS "iconv include path       : " ${Iconv_INCLUDE_DIRS})
        message(STATUS "iconv lib path           : " ${Iconv_LIBRARIES})
        # include
        include_directories("${Iconv_INCLUDE_DIRS}")
    endif()

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
    message(WARNING "lua-iconv need iconv, but not found iconv, SKIP...")
endif()


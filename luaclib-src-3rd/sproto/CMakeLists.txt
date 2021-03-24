#
set(SKYNET_SPROTO_HEADER
    sproto.h
    msvcint.h
)
set(SKYNET_SPROTO_SRC
    sproto.c
    lsproto.c
)
list(APPEND SKYNET_SPROTO_SRC ${SKYNET_SPROTO_HEADER})

# target shared lib
add_library(sproto MODULE  ${SKYNET_SPROTO_SRC})
set_target_properties(sproto
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
)
target_link_libraries(sproto ${SKYNET_SHARED})

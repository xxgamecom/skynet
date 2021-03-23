# include path
include_directories("${SKYNET_ROOT_PATH}/skynet-src")

#
set(SKYNET_SNLUA_HEADER
    snlua_service.h
)

set(SKYNET_SNLUA_SRC
    snlua_service.cpp
    mod.cpp
)

list(APPEND SKYNET_SNLUA_SRC ${SKYNET_SNLUA_HEADER})

# target module lib (snlua.so)
add_library(snlua MODULE ${SKYNET_SNLUA_SRC})
set_target_properties(snlua
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_CSERVICE}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_CSERVICE}"
)
target_link_libraries(snlua skynet_core)

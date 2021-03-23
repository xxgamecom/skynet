# include path
include_directories("${SKYNET_ROOT_PATH}/skynet-src")

#
set(SKYNET_LOGGER_HEADER
    logger_service.h
)

set(SKYNET_LOGGER_SRC
    logger_service.cpp
    mod.cpp
)

list(APPEND SKYNET_LOGGER_SRC ${SKYNET_LOGGER_HEADER})

# target module lib (logger.so)
add_library(logger MODULE ${SKYNET_LOGGER_SRC})
set_target_properties(logger
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_CSERVICE}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_CSERVICE}"
)
target_link_libraries(logger skynet_core)

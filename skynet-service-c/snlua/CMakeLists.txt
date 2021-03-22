# include path
INCLUDE_DIRECTORIES("${SKYNET_ROOT_PATH}/skynet")

#
set(SKYNET_SNLUA_HEADER
    snlua_mod.h
    profile.h
    snlua_service.h
)

set(SKYNET_SNLUA_SRC
    profile.cpp
    snlua_service.cpp
    mod.cpp
)

LIST(APPEND SKYNET_SNLUA_SRC ${SKYNET_SNLUA_HEADER})

# target module lib (snlua.so)
ADD_LIBRARY(snlua MODULE ${SKYNET_SNLUA_SRC})
SET_TARGET_PROPERTIES(snlua
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_SERVICE_C}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_SERVICE_C}"
)
TARGET_LINK_LIBRARIES(snlua liblua skynet_core)

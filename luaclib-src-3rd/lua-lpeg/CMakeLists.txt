#
set(SKYNET_LPEG_HEADER
    lptypes.h
    lpcap.h
    lpcode.h
    lpprint.h
    lptree.h
    lpvm.h
)
set(SKYNET_LPEG_SRC
    lpcap.c
    lpcode.c
    lpprint.c
    lptree.c
    lpvm.c
)
list(APPEND SKYNET_LPEG_SRC ${SKYNET_LPEG_HEADER})

# target shared lib (lpeg.so)
add_library(lpeg MODULE ${SKYNET_LPEG_SRC})
set_target_properties(lpeg
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_LUACLIB_3RD}"
)
target_link_libraries(lpeg ${SKYNET_SHARED})

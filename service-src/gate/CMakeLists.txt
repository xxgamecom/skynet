# include path
include_directories("${SKYNET_ROOT_PATH}/skynet-src")

#
set(SKYNET_GATE_HEADER
    hash_id.h
    data_buffer.h
    gate_ctrl_cmd.h
    gate_service.h
)
set(SKYNET_GATE_SRC
    hash_id.cpp
    data_buffer.cpp
    gate_ctrl_cmd.cpp
    gate_service.cpp
    mod.cpp
)
list(APPEND SKYNET_GATE_SRC ${SKYNET_GATE_HEADER})

# target module lib (gate.so)
add_library(gate MODULE ${SKYNET_GATE_SRC})
set_target_properties(gate
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_CSERVICE}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_CSERVICE}"
)
target_link_libraries(gate "-undefined dynamic_lookup")
# Meh. main should have been module. You could also try to add it to CMAKE_SHARED_LINKER_FLAGS.
# include path
include_directories("${SKYNET_ROOT_PATH}/skynet")

#
set(SKYNET_GATE_HEADER
    hash_id.h
    data_buffer.h
    gate_mod.h
    gate_ctrl_cmd.h
    gate_service.h
)

set(SKYNET_GATE_SRC
    hash_id.cpp
    data_buffer.cpp
    gate_ctrl_cmd.cpp
    service_gate.cpp
    gate_service.cpp
    service.cpp
)

list(APPEND SKYNET_GATE_SRC ${SKYNET_GATE_HEADER})

# target shared lib (gate.so)
add_library(gate MODULE
    ${SKYNET_GATE_SRC}
    $<TARGET_OBJECTS:skynet_o>
)
set_target_properties(gate
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_CSERVICE_BIN_PATH}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_CSERVICE_BIN_PATH}"
)
target_link_libraries(gate liblua)

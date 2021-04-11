include(mq/CMakeLists.txt)
include(log/CMakeLists.txt)
include(timer/CMakeLists.txt)
include(mod/CMakeLists.txt)
include(service/CMakeLists.txt)
include(socket/CMakeLists.txt)
include(node/CMakeLists.txt)
include(memory/CMakeLists.txt)
include(utils/CMakeLists.txt)

set(SKYNET_CORE_HEADER skynet.h
    ${SKYNET_MEMORY_HEADER}
    ${SKYNET_MQ_HEADER}
    ${SKYNET_LOG_HEADER}
    ${SKYNET_TIMER_HEADER}
    ${SKYNET_MOD_HEADER}
    ${SKYNET_SERVICE_HEADER}
    ${SKYNET_SOCKET_HEADER}
    ${SKYNET_NODE_HEADER}
    ${SKYNET_UTILS_HEADER}
)
set(SKYNET_CORE_SRC skynet.cpp
    ${SKYNET_MEMORY_SRC}
    ${SKYNET_MQ_SRC}
    ${SKYNET_LOG_SRC}
    ${SKYNET_TIMER_SRC}
    ${SKYNET_MOD_SRC}
    ${SKYNET_SERVICE_SRC}
    ${SKYNET_SOCKET_SRC}
    ${SKYNET_NODE_SRC}
    ${SKYNET_UTILS_SRC}
)
list(APPEND SKYNET_CORE_SRC ${SKYNET_CORE_HEADER})

# output path
set(EXECUTABLE_OUTPUT_PATH "${SKYNET_BIN_PATH}")

# target executable (skynet)
add_executable(skynet ${SKYNET_CORE_SRC})
if (MACOSX)
    target_link_libraries(skynet ${SKYNET_EXPORT} pthread m dl liblua net)
elseif(LINUX OR FREEBSD)
    target_link_libraries(skynet ${SKYNET_EXPORT} pthread m dl rt liblua net)
endif()



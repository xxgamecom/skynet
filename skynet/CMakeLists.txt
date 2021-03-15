include(mq/CMakeLists.txt)
include(log/CMakeLists.txt)
include(timer/CMakeLists.txt)
include(service/CMakeLists.txt)
include(mod/CMakeLists.txt)
include(socket/CMakeLists.txt)
include(node/CMakeLists.txt)
include(memory/CMakeLists.txt)
include(utils/CMakeLists.txt)

# source file list
SET(SKYNET_HEADER skynet.h
    ${SKYNET_MEMORY_HEADER}
    ${SKYNET_MQ_HEADER}
    ${SKYNET_LOG_HEADER}
    ${SKYNET_TIMER_HEADER}
    ${SKYNET_SERVICE_HEADER}
    ${SKYNET_MOD_HEADER}
    ${SKYNET_SOCKET_HEADER}
    ${SKYNET_NODE_HEADER}
    ${SKYNET_UTILS_HEADER}
)
SET(SKYNET_SRC skynet.cpp
    ${SKYNET_MEMORY_SRC}
    ${SKYNET_MQ_SRC}
    ${SKYNET_LOG_SRC}
    ${SKYNET_TIMER_SRC}
    ${SKYNET_SERVICE_SRC}
    ${SKYNET_MOD_SRC}
    ${SKYNET_SOCKET_SRC}
    ${SKYNET_NODE_SRC}
    ${SKYNET_UTILS_SRC}
)

LIST(APPEND SKYNET_SRC ${SKYNET_HEADER})

# target
ADD_LIBRARY(skynet_o OBJECT ${SKYNET_SRC})
ADD_EXECUTABLE(skynet $<TARGET_OBJECTS:skynet_o>)
TARGET_LINK_LIBRARIES(skynet liblua)

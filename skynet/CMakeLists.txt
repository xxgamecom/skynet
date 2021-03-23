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
set(SKYNET_CORE_SRC
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

#----------------------------------------------------
# target shared lib (skynet_core.dylib)
#----------------------------------------------------

add_library(skynet_core SHARED ${SKYNET_CORE_SRC})
set_target_properties(skynet_core
    PROPERTIES
    PREFIX ""
    OUTPUT_NAME "skynet_core"
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH}"
)
target_link_libraries(skynet_core pthread m dl liblua)

#----------------------------------------------------
# target executable (skynet)
#----------------------------------------------------

add_definitions(-Wl,-E)

add_executable(skynet skynet.cpp)
target_link_libraries(skynet pthread m dl liblua skynet_core)

#$(SKYNET_BUILD_PATH)/skynet : $(foreach v, $(SKYNET_SRC), skynet-src/$(v)) $(LUA_LIB) $(MALLOC_STATICLIB)
#    $(CC) $(CFLAGS) -o $@ $^ -Iskynet-src -I$(JEMALLOC_INC) $(LDFLAGS) $(EXPORT) $(SKYNET_LIBS) $(SKYNET_DEFINES)

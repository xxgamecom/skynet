# target test_socket_server
include_directories("${SKYNET_ROOT_PATH}/skynet")


# test foo
#add_executable(test_foo test_foo.cpp)
#target_link_libraries(test_foo)
#set_target_properties(test_foo
#    PROPERTIES
#    PREFIX ""
#    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_TESTS}"
#    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_TESTS}"
#)

# test hash id
add_executable(test_hash_id hash_id/test_hash_id.cpp hash_id/hash_id.h hash_id/hash_id.cpp)
target_link_libraries(test_hash_id)
set_target_properties(test_hash_id
    PROPERTIES
    PREFIX ""
    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_TESTS}"
    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_TESTS}"
)

#
#add_executable(test_socket_server test_socket_server.cpp)
#target_link_libraries(test_socket_server skynet_core)
#set_target_properties(test_socket_server
#    PROPERTIES
#    PREFIX ""
#    ARCHIVE_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_TESTS}"
#    LIBRARY_OUTPUT_DIRECTORY "${SKYNET_BIN_PATH_TESTS}"
#)

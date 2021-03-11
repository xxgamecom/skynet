# target test_socket_server
INCLUDE_DIRECTORIES("${SKYNET_ROOT_PATH}/skynet")

ADD_EXECUTABLE(test_socket_server test_socket_server.cpp)
TARGET_LINK_LIBRARIES(test_socket_server liblua skynet)

#

# Set output path
set(EXECUTABLE_OUTPUT_PATH "${SKYNET_BIN_PATH_TESTS}/network")

# Set link libraries
if (MACOSX)
    set(MY_LINK_LIBRARIES pthread net)
elseif(LINUX)
    set(MY_LINK_LIBRARIES rt pthread net)
endif ()


# Targets

# test network connector
add_executable(test_network_connector test_network_connector.cpp)
target_link_libraries(test_network_connector ${MY_LINK_LIBRARIES})

# test network http client
add_executable(test_network_http_client test_network_http_client.cpp)
target_link_libraries(test_network_http_client  ${MY_LINK_LIBRARIES})

# test network echo
add_executable(test_network_echo_client test_network_echo_client.cpp)
target_link_libraries(test_network_echo_client  ${MY_LINK_LIBRARIES})

add_executable(test_network_echo_server test_network_echo_server.cpp)
target_link_libraries(test_network_echo_server  ${MY_LINK_LIBRARIES})

# test boost asio stress
add_executable(test_network_stress_asio_client test_network_stress_asio_client.cpp)
target_link_libraries(test_network_stress_asio_client ${MY_LINK_LIBRARIES})

add_executable(test_network_stress_asio_server test_network_stress_asio_server.cpp)
target_link_libraries(test_network_stress_asio_server ${MY_LINK_LIBRARIES})

# test network stress
add_executable(test_network_stress_client test_network_stress_client.cpp)
target_link_libraries(test_network_stress_client ${MY_LINK_LIBRARIES})

add_executable(test_network_stress_server test_network_stress_server.cpp)
target_link_libraries(test_network_stress_server ${MY_LINK_LIBRARIES})


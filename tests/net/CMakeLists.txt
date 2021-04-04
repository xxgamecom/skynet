# Set output path
set(EXECUTABLE_OUTPUT_PATH "${SKYNET_BIN_PATH_TESTS}/net")

# Set link libraries
if (MACOSX)
    set(MY_LINK_LIBRARIES pthread net)
elseif(LINUX)
    set(MY_LINK_LIBRARIES rt pthread net)
endif ()


# Targets

## test network connector
#add_executable(test_net_connector test_net_connector.cpp)
#target_link_libraries(test_net_connector ${MY_LINK_LIBRARIES})
#
## test network http client
#add_executable(test_net_http_client test_net_http_client.cpp)
#target_link_libraries(test_net_http_client  ${MY_LINK_LIBRARIES})
#
## test network echo
#add_executable(test_net_echo_client test_net_echo_client.cpp)
#target_link_libraries(test_net_echo_client  ${MY_LINK_LIBRARIES})
#
#add_executable(test_net_echo_server test_net_echo_server.cpp)
#target_link_libraries(test_net_echo_server  ${MY_LINK_LIBRARIES})

## test boost asio stress
#add_executable(test_net_stress_asio_client test_net_stress_asio_client.cpp)
#target_link_libraries(test_net_stress_asio_client ${MY_LINK_LIBRARIES})
#
#add_executable(test_net_stress_asio_server test_net_stress_asio_server.cpp)
#target_link_libraries(test_net_stress_asio_server ${MY_LINK_LIBRARIES})

## test network stress
#add_executable(test_net_stress_client test_net_stress_client.cpp)
#target_link_libraries(test_net_stress_client ${MY_LINK_LIBRARIES})
#
#add_executable(test_net_stress_server test_net_stress_server.cpp)
#target_link_libraries(test_net_stress_server ${MY_LINK_LIBRARIES})


# Set link libraries
if (MACOSX)
    set(MY_LINK_LIBRARIES pthread net)
elseif(LINUX)
    set(MY_LINK_LIBRARIES rt pthread net)
endif ()

# Targets

# test network connector
add_executable(net_connector net_connector.cpp)
target_link_libraries(net_connector ${MY_LINK_LIBRARIES})

# test network http client
add_executable(net_http_client net_http_client.cpp)
target_link_libraries(net_http_client  ${MY_LINK_LIBRARIES})

# test network echo
add_executable(net_echo_client net_echo_client.cpp)
target_link_libraries(net_echo_client  ${MY_LINK_LIBRARIES})

add_executable(net_echo_server net_echo_server.cpp)
target_link_libraries(net_echo_server  ${MY_LINK_LIBRARIES})

# test network stress
add_executable(net_stress_client net_stress_client.cpp)
target_link_libraries(net_stress_client ${MY_LINK_LIBRARIES})

add_executable(net_stress_server net_stress_server.cpp)
target_link_libraries(net_stress_server ${MY_LINK_LIBRARIES})


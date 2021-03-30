#---------------------------------------------------------------------------------
# Set output path
#---------------------------------------------------------------------------------

if((${CMAKE_BUILD_TYPE} MATCHES "debug") OR (${CMAKE_BUILD_TYPE} MATCHES "Debug"))
    SET(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin/debug/test")
else()
    SET(EXECUTABLE_OUTPUT_PATH "${CMAKE_SOURCE_DIR}/bin/release/test")
endif()

#---------------------------------------------------------------------------------
# Set link libraries
#---------------------------------------------------------------------------------

if((${CMAKE_BUILD_TYPE} MATCHES "debug") OR (${CMAKE_BUILD_TYPE} MATCHES "Debug"))
    SET(MY_LINK_LIBRARIES rt pthread game.common_d game.network_d ${Boost_LIBRARIES})
else()
    SET(MY_LINK_LIBRARIES rt pthread game.common   game.network   ${Boost_LIBRARIES})
endif()

#---------------------------------------------------------------------------------
# Targets
#---------------------------------------------------------------------------------

# test network connector
ADD_EXECUTABLE(test_network_connector test_network_connector.cpp)
TARGET_LINK_LIBRARIES(test_network_connector ${MY_LINK_LIBRARIES})

# test network http client
ADD_EXECUTABLE(test_network_http_client test_network_http_client.cpp)
TARGET_LINK_LIBRARIES(test_network_http_client  ${MY_LINK_LIBRARIES})

# test network echo
ADD_EXECUTABLE(test_network_echo_client test_network_echo_client.cpp)
TARGET_LINK_LIBRARIES(test_network_echo_client  ${MY_LINK_LIBRARIES})

ADD_EXECUTABLE(test_network_echo_server test_network_echo_server.cpp)
TARGET_LINK_LIBRARIES(test_network_echo_server  ${MY_LINK_LIBRARIES})

# test boost asio stress
ADD_EXECUTABLE(test_network_stress_asio_client test_network_stress_asio_client.cpp)
TARGET_LINK_LIBRARIES(test_network_stress_asio_client ${MY_LINK_LIBRARIES})

ADD_EXECUTABLE(test_network_stress_asio_server test_network_stress_asio_server.cpp)
TARGET_LINK_LIBRARIES(test_network_stress_asio_server ${MY_LINK_LIBRARIES})

# test network stress
ADD_EXECUTABLE(test_network_stress_client test_network_stress_client.cpp)
TARGET_LINK_LIBRARIES(test_network_stress_client ${MY_LINK_LIBRARIES})

ADD_EXECUTABLE(test_network_stress_server test_network_stress_server.cpp)
TARGET_LINK_LIBRARIES(test_network_stress_server ${MY_LINK_LIBRARIES})


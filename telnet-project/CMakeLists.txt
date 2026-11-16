cmake_minimum_required(VERSION 3.16)
project(TelnetProject CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

# Common static library
add_library(telnet_lib STATIC
    src/socket.cpp
    src/nvt.cpp
    src/telnet_protocol.cpp
    src/terminal_session.cpp
    src/telnet_server.cpp
    src/telnet_client.cpp
)

target_compile_options(telnet_lib PRIVATE -Wall -Wextra -Wno-unused-parameter)

if(WIN32)
    target_link_libraries(telnet_lib ws2_32)
else()
    target_compile_options(telnet_lib PRIVATE -pthread)
    target_link_libraries(telnet_lib pthread)
endif()

# Server executable
add_executable(telnet_server src/server.cpp)
target_link_libraries(telnet_server telnet_lib)

# Client executable
add_executable(telnet_client src/client.cpp)
target_link_libraries(telnet_client telnet_lib)

# Tests
enable_testing()

foreach(TEST_NAME test_nvt test_protocol test_session integration_test)
    add_executable(${TEST_NAME} tests/${TEST_NAME}.cpp)
    target_link_libraries(${TEST_NAME} telnet_lib)
    add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
endforeach()

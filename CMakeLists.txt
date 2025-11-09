cmake_minimum_required(VERSION 3.20)
project(TinyLoader CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server server.cpp)

if(WIN32)
    add_executable(client client.cpp)
    target_link_libraries(server ws2_32)
    target_link_libraries(client ws2_32)
else()
    # server only on Linux
endif()

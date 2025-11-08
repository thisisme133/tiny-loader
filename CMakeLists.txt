cmake_minimum_required(VERSION 3.25)
project(TinyLoader CXX)

# C++23 standard
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Include directories
include_directories(${CMAKE_SOURCE_DIR}/src)

# === SERVER (Cross-platform) ===
add_executable(server
    src/server/main.cpp
    src/server/server.cpp
    src/core/packet.cpp
    src/core/session.cpp
    src/core/encryption.cpp
    src/handlers/opcode_handler.cpp
)

# === CLIENT (Cross-platform) ===
add_executable(client
    src/client/main.cpp
    src/client/client.cpp
    src/core/packet.cpp
    src/core/session.cpp
    src/core/encryption.cpp
)

# Platform-specific libraries
if(WIN32)
    target_link_libraries(server ws2_32)
    target_link_libraries(client ws2_32)
else()
    target_link_libraries(server pthread)
    target_link_libraries(client pthread)
endif()

# Compiler warnings
if(MSVC)
    target_compile_options(server PRIVATE /W4 /permissive-)
    target_compile_options(client PRIVATE /W4 /permissive-)
else()
    target_compile_options(server PRIVATE -Wall -Wextra -pedantic)
    target_compile_options(client PRIVATE -Wall -Wextra -pedantic)
endif()

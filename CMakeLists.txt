cmake_minimum_required(VERSION 3.10)
project(ClientApp C)

# Définit le standard C
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Ajoute les fichiers sources
add_executable(client
    client.c
)

# Inclut le dossier courant pour accéder à common.h
target_include_directories(client PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

# Si tu utilises des sockets sous Windows
if(WIN32)
    target_link_libraries(client ws2_32)
endif()

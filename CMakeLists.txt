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

# Bibliothèques Windows requises
if(WIN32)
    target_link_libraries(client
        ws2_32      # Winsock
        ntdll       # NT API
        advapi32    # Registry & Security
        tlhelp32    # Toolhelp (Process enumeration)
    )

    # Options de compilation pour éviter les warnings
    target_compile_options(client PRIVATE /W3)
endif()

# === SERVEUR (Linux/Unix) ===
# Compile uniquement sur les plateformes Unix-like
if(UNIX)
    add_executable(server
        server.c
    )

    # Inclut le dossier courant pour accéder à common.h
    target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})

    # Bibliothèques Linux requises
    target_link_libraries(server
        pthread     # POSIX threads
    )

    # Options de compilation
    target_compile_options(server PRIVATE -Wall -Wextra)
endif()

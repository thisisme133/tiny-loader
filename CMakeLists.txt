cmake_minimum_required(VERSION 3.20)
project(TinyLoader CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server server.cpp)

if(WIN32)
    # ImGui sources
    set(IMGUI_DIR ${CMAKE_CURRENT_SOURCE_DIR}/imgui)
    set(IMGUI_SOURCES
        ${IMGUI_DIR}/imgui.cpp
        ${IMGUI_DIR}/imgui_draw.cpp
        ${IMGUI_DIR}/imgui_tables.cpp
        ${IMGUI_DIR}/imgui_widgets.cpp
        ${IMGUI_DIR}/backends/imgui_impl_win32.cpp
        ${IMGUI_DIR}/backends/imgui_impl_dx11.cpp
    )

    add_executable(client client.cpp ${IMGUI_SOURCES})

    target_include_directories(client PRIVATE ${IMGUI_DIR} ${IMGUI_DIR}/backends)
    target_link_libraries(server ws2_32)
    target_link_libraries(client ws2_32 d3d11 d3dcompiler)
else()
    # server only on Linux
endif()

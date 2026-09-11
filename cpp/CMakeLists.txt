cmake_minimum_required(VERSION 3.20)
project(HexMapping VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Compile options
if (MSVC)
    add_compile_options(/W4 /permissive- /Zc:preprocessor /EHsc)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# Core Math & Model Source Files
set(CORE_SOURCES
    core/math/Mat3.cpp
    core/math/Homography.cpp
    core/model/WarpCornerPin.cpp
)

# UI Layer Source Files
set(UI_SOURCES
    ui/Theme.cpp
    ui/panels/OutputPanel.cpp
    ui/panels/LayerPanel.cpp
    ui/ControlPanel.cpp
)

# Include directories
include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/core
    ${CMAKE_CURRENT_SOURCE_DIR}/ui
)

# Core library (Zero external dependencies)
add_library(HexMapCore STATIC ${CORE_SOURCES})

# Target executable (Khi tích hợp GLFW / openFrameworks / Dear ImGui)
# add_executable(HexMapping app/main.cpp ${UI_SOURCES})
# target_link_libraries(HexMapping PRIVATE HexMapCore)

message(STATUS "HexMapping C++20 Core & UI configured successfully.")

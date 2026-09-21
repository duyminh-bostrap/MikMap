# MikMap UI (Dear ImGui + GLFW), built against the repo's own engine libraries.
#
# Standalone on purpose: it globs ../engine/core and ../engine/io itself instead of
# touching the root CMakeLists.txt, so this target can never conflict with the
# engine+test build. This directory was promoted from newui/ to be the current
# src/ — the old openFrameworks+MSBuild UI it replaced lives on the `legacy-oF-ui`
# git branch, not in this working tree.
#   cmake -S src -B src/build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build src/build
cmake_minimum_required(VERSION 3.20)
project(MikMapUI CXX)

set(CMAKE_CXX_STANDARD 20)          # matches the engine (hexmap_core is C++20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO "${CMAKE_CURRENT_SOURCE_DIR}/..")
set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.tools/imgui-1.92.9b")
set(GLFW_DIR  "${CMAKE_CURRENT_SOURCE_DIR}/.tools/glfw-3.5.1.bin.WIN64")

option(MIKMAP_CONSOLE "Keep the console window (debugging)" OFF)
option(MIKMAP_WITH_ENGINE "Compile the repo's core/io engine into this target" ON)

# ── the engine, straight from the repo (pure C++20, no openFrameworks/GL/ImGui) ──
if(MIKMAP_WITH_ENGINE)
  file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${REPO}/engine/core/*.cpp" "${REPO}/engine/io/*.cpp")
else()
  set(ENGINE_SOURCES "")
endif()

file(GLOB NEWUI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/src/*.cpp")

add_executable(mikmap
  ${NEWUI_SOURCES}
  ${ENGINE_SOURCES}
  ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)

target_include_directories(mikmap PRIVATE
  src ${REPO}/engine ${IMGUI_DIR} ${IMGUI_DIR}/backends ${GLFW_DIR}/include third_party/stb)

if(MIKMAP_WITH_ENGINE)
  target_compile_definitions(mikmap PRIVATE MIKMAP_WITH_ENGINE=1)
endif()

target_link_libraries(mikmap PRIVATE
  ${GLFW_DIR}/lib-mingw-w64/libglfw3.a opengl32 gdi32 dwmapi shell32 imm32
  ws2_32)   # io/sources/OscSource.cpp opens a UDP socket

target_compile_options(mikmap PRIVATE -Wall -Wno-unused-function -Wno-unused-variable)
target_link_options(mikmap PRIVATE -static -static-libgcc -static-libstdc++)
if(NOT MIKMAP_CONSOLE)
  target_link_options(mikmap PRIVATE -mwindows)
endif()

add_custom_command(TARGET mikmap POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:mikmap>/assets")

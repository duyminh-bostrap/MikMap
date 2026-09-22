# MikMap UI (Dear ImGui + GLFW), built against the repo's own engine libraries.
#
# Standalone on purpose: it globs ../engine/core and ../engine/io itself instead of
# touching the root CMakeLists.txt, so this target can never conflict with the
# engine+test build. This directory was promoted from newui/ to be the current
# src/ — the old openFrameworks+MSBuild UI it replaced lives on the `legacy-oF-ui`
# git branch, not in this working tree.
#
# Needs only CMake + a C++20 compiler on any of Windows / macOS / Linux:
#   cmake -S src -B src/build -DCMAKE_BUILD_TYPE=Release
#   cmake --build src/build
#
# Dear ImGui is always fetched at configure time (it ships no CMakeLists, so its
# sources are compiled straight into this target). GLFW is used from the system
# if installed (brew/apt/vcpkg), otherwise it is fetched and built from source —
# so no prebuilt toolchain directory has to be assembled by hand.
cmake_minimum_required(VERSION 3.20)
project(MikMapUI CXX)

set(CMAKE_CXX_STANDARD 20)          # matches the engine (hexmap_core is C++20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)   # feeds VS Code IntelliSense, no hardcoded compiler path

set(REPO "${CMAKE_CURRENT_SOURCE_DIR}/..")

option(MIKMAP_CONSOLE "Keep the console window (debugging, Windows only)" OFF)
option(MIKMAP_WITH_ENGINE "Compile the repo's core/io engine into this target" ON)

include(FetchContent)

# ── the engine, straight from the repo (pure C++20, no GL/ImGui) ──
if(MIKMAP_WITH_ENGINE)
  file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${REPO}/engine/core/*.cpp" "${REPO}/engine/io/*.cpp")
else()
  set(ENGINE_SOURCES "")
endif()

file(GLOB NEWUI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# ── Dear ImGui (no CMakeLists upstream → populated only, compiled by us) ──
FetchContent_Declare(imgui_src
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG v1.92.9b
  GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(imgui_src)
set(IMGUI_DIR "${imgui_src_SOURCE_DIR}")

set(IMGUI_SOURCES
  ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)

# ── GLFW: system install first, source build as the fallback ──
find_package(glfw3 3.3 QUIET)
if(NOT glfw3_FOUND)
  message(STATUS "MikMap: no system GLFW found — building GLFW 3.5.1 from source")
  set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_TESTS    OFF CACHE BOOL "" FORCE)
  set(GLFW_BUILD_DOCS     OFF CACHE BOOL "" FORCE)
  set(GLFW_INSTALL        OFF CACHE BOOL "" FORCE)
  FetchContent_Declare(glfw_src
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.5.1
    GIT_SHALLOW TRUE)
  FetchContent_MakeAvailable(glfw_src)
endif()

find_package(OpenGL REQUIRED)
find_package(Threads REQUIRED)

add_executable(mikmap ${NEWUI_SOURCES} ${ENGINE_SOURCES} ${IMGUI_SOURCES})

target_include_directories(mikmap PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${REPO}/engine ${IMGUI_DIR} ${IMGUI_DIR}/backends third_party/stb)

if(MIKMAP_WITH_ENGINE)
  target_compile_definitions(mikmap PRIVATE MIKMAP_WITH_ENGINE=1)
endif()

target_link_libraries(mikmap PRIVATE glfw OpenGL::GL Threads::Threads)

# ── per-platform extras (glfw's own target already carries gdi32/Cocoa/X11 etc.) ──
if(WIN32)
  target_link_libraries(mikmap PRIVATE dwmapi ws2_32)   # dark titlebar (main.cpp) + UDP socket (OscSource.cpp)
  # GUI subsystem: no console window. Works for both MSVC (/SUBSYSTEM:WINDOWS)
  # and MinGW (-mwindows), unlike a hardcoded linker flag.
  if(NOT MIKMAP_CONSOLE)
    set_target_properties(mikmap PROPERTIES WIN32_EXECUTABLE TRUE)
  endif()
elseif(NOT APPLE)
  target_link_libraries(mikmap PRIVATE dl)
endif()

# ── warnings/linking per COMPILER, not per OS: Windows may be MSVC or MinGW ──
if(MSVC)
  target_compile_options(mikmap PRIVATE /W3 /utf-8)
else()
  target_compile_options(mikmap PRIVATE -Wall -Wno-unused-function -Wno-unused-variable)
  if(MINGW)
    # ship one self-contained .exe instead of needing the MinGW runtime DLLs
    target_link_options(mikmap PRIVATE -static -static-libgcc -static-libstdc++)
  endif()
endif()

# Keep the binary at <build>/mikmap(.exe) under every generator. Multi-config
# generators (Visual Studio, Xcode) would otherwise bury it in <build>/Debug/,
# which breaks a fixed launch.json "program" path.
set_target_properties(mikmap PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY                "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_DEBUG          "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_RELEASE        "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO "${CMAKE_BINARY_DIR}"
  RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL     "${CMAKE_BINARY_DIR}")

add_custom_command(TARGET mikmap POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:mikmap>/assets")

# Headless self-check of project save/load/undo (needs no window or GPU): `ctest --test-dir src/build`
enable_testing()
add_test(NAME project_roundtrip COMMAND mikmap --roundtrip ${CMAKE_CURRENT_BINARY_DIR}/roundtrip.mikmap)

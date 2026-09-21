# MikMap UI (Dear ImGui + GLFW), built against the repo's own engine libraries.
#
# Standalone on purpose: it globs ../engine/core and ../engine/io itself instead of
# touching the root CMakeLists.txt, so this target can never conflict with the
# engine+test build. This directory was promoted from newui/ to be the current
# src/ — the old openFrameworks+MSBuild UI it replaced lives on the `legacy-oF-ui`
# git branch, not in this working tree. The app's own sources used to live one
# level deeper at newui/src/*.cpp; flattened to src/*.cpp directly after the
# promotion so the tree doesn't read as src/src/.
#
# Windows (primary target, prebuilt toolchain in .tools/, not in git):
#   cmake -S src -B src/build -G Ninja -DCMAKE_BUILD_TYPE=Release
#   cmake --build src/build
#
# Linux/macOS (built and launched for real on both; GLFW must be
# installed system-wide, e.g. `apt install libglfw3-dev` or `brew install glfw`;
# Dear ImGui is fetched at configure time, pinned to the same v1.92.9b the
# Windows .tools/ bundle uses):
#   cmake -S src -B src/build -DCMAKE_BUILD_TYPE=Release
#   cmake --build src/build
cmake_minimum_required(VERSION 3.20)
project(MikMapUI CXX)

set(CMAKE_CXX_STANDARD 20)          # matches the engine (hexmap_core is C++20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(REPO "${CMAKE_CURRENT_SOURCE_DIR}/..")

option(MIKMAP_CONSOLE "Keep the console window (debugging, Windows only)" OFF)
option(MIKMAP_WITH_ENGINE "Compile the repo's core/io engine into this target" ON)

# ── the engine, straight from the repo (pure C++20, no GL/ImGui) ──
if(MIKMAP_WITH_ENGINE)
  file(GLOB_RECURSE ENGINE_SOURCES CONFIGURE_DEPENDS "${REPO}/engine/core/*.cpp" "${REPO}/engine/io/*.cpp")
else()
  set(ENGINE_SOURCES "")
endif()

file(GLOB NEWUI_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

# ── Dear ImGui: bundled prebuilt on Windows (.tools/), fetched at configure
# time everywhere else — the sources aren't vendored into the repo either way ──
if(WIN32)
  set(IMGUI_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.tools/imgui-1.92.9b")
else()
  include(FetchContent)
  FetchContent_Declare(imgui_src
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.92.9b
    GIT_SHALLOW TRUE)
  FetchContent_Populate(imgui_src)
  set(IMGUI_DIR "${imgui_src_SOURCE_DIR}")
endif()

set(IMGUI_SOURCES
  ${IMGUI_DIR}/imgui.cpp ${IMGUI_DIR}/imgui_draw.cpp ${IMGUI_DIR}/imgui_tables.cpp ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp ${IMGUI_DIR}/backends/imgui_impl_opengl3.cpp)

add_executable(mikmap ${NEWUI_SOURCES} ${ENGINE_SOURCES} ${IMGUI_SOURCES})

target_include_directories(mikmap PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR} ${REPO}/engine ${IMGUI_DIR} ${IMGUI_DIR}/backends third_party/stb)

if(MIKMAP_WITH_ENGINE)
  target_compile_definitions(mikmap PRIVATE MIKMAP_WITH_ENGINE=1)
endif()

target_compile_options(mikmap PRIVATE -Wall -Wno-unused-function -Wno-unused-variable)

if(WIN32)
  # Prebuilt MinGW GLFW under .tools/ (not in git) — see src/build.ps1.
  set(GLFW_DIR "${CMAKE_CURRENT_SOURCE_DIR}/.tools/glfw-3.5.1.bin.WIN64")
  target_include_directories(mikmap PRIVATE ${GLFW_DIR}/include)
  target_link_libraries(mikmap PRIVATE
    ${GLFW_DIR}/lib-mingw-w64/libglfw3.a opengl32 gdi32 dwmapi shell32 imm32
    ws2_32)   # io/sources/OscSource.cpp opens a UDP socket
  target_link_options(mikmap PRIVATE -static -static-libgcc -static-libstdc++)
  if(NOT MIKMAP_CONSOLE)
    target_link_options(mikmap PRIVATE -mwindows)
  endif()
else()
  # System-installed GLFW (apt/brew) + system OpenGL.
  find_package(glfw3 3.3 REQUIRED)
  find_package(OpenGL REQUIRED)
  find_package(Threads REQUIRED)
  target_link_libraries(mikmap PRIVATE glfw OpenGL::GL Threads::Threads)
  if(APPLE)
    target_link_libraries(mikmap PRIVATE "-framework Cocoa" "-framework IOKit" "-framework CoreVideo")
  else()
    target_link_libraries(mikmap PRIVATE dl)
  endif()
endif()

add_custom_command(TARGET mikmap POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory "${CMAKE_CURRENT_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:mikmap>/assets")

# Headless self-check of project save/load/undo (needs no window or GPU): `ctest --test-dir src/build`
enable_testing()
add_test(NAME project_roundtrip COMMAND mikmap --roundtrip ${CMAKE_CURRENT_BINARY_DIR}/roundtrip.mikmap)

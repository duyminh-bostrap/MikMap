# ════════════════════════════════════════════════════════════════════════
#  MikMap — build cho engine/ (core + io + i18n) + tests
#
#  QUAN TRỌNG: File này KHÔNG build ứng dụng. Ứng dụng (GLFW + Dear ImGui)
#  ở src/, build riêng bằng src/CMakeLists.txt (xem src/README.md). Bản
#  engine cũ dùng openFrameworks Project Generator (.vcxproj) đã lưu trữ ở
#  nhánh git `legacy-oF-ui`, không còn trên nhánh này.
#
#  Mục đích: engine/core là C++20 thuần (chỉ phụ thuộc STL) nên build và
#  test được độc lập — không cần openFrameworks, không cần GPU.
#  ⇒ Unit test toán học chạy trong vài giây, hợp cho CI.
#
#  Cách dùng:
#     cmake -S . -B build
#     cmake --build build --config Debug
#     ctest --test-dir build -C Debug --output-on-failure
# ════════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.20)
project(MikMap LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # /W4      cảnh báo mức cao
    # /permissive-  tuân thủ chuẩn chặt chẽ (chỉ có nghĩa với C++)
    # /fp:precise   KHÔNG dùng /fp:fast — toán homography cần độ chính xác
    # C cũng phải /W4 /utf-8 (plugin giả build bằng C thuần).
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:/W4>"
        "$<$<COMPILE_LANGUAGE:CXX>:/permissive->"
        "$<$<COMPILE_LANGUAGE:CXX>:/fp:precise>"
        "$<$<COMPILE_LANGUAGE:CXX>:/utf-8>"
        "$<$<COMPILE_LANGUAGE:C>:/W4>"
        "$<$<COMPILE_LANGUAGE:C>:/utf-8>")
else()
    # -ffp-contract=off là bản tương đương /fp:precise của GCC/Clang: cấm
    # gộp a*b+c thành FMA. Thiếu nó, cùng một ma trận homography cho kết
    # quả lệch bit giữa Windows-MSVC và macOS/Linux, và test so sánh số
    # với epsilon nhỏ sẽ đỏ trên đúng một nền tảng.
    add_compile_options(
        "$<$<COMPILE_LANGUAGE:CXX>:-Wall>"
        "$<$<COMPILE_LANGUAGE:CXX>:-Wextra>"
        "$<$<COMPILE_LANGUAGE:CXX>:-Wpedantic>"
        "$<$<COMPILE_LANGUAGE:CXX>:-ffp-contract=off>"
        "$<$<COMPILE_LANGUAGE:C>:-Wall>"
        "$<$<COMPILE_LANGUAGE:C>:-Wextra>"
        "$<$<COMPILE_LANGUAGE:C>:-Wpedantic>")
endif()

# Test engine xuất file chạy thẳng vào <build>/ (giống src/CMakeLists.txt),
# với mọi generator — Ninja/Makefiles lẫn Visual Studio/Xcode (multi-config,
# vốn mặc định thêm thư mục Debug/). launch.json nhờ vậy chỉ cần MỘT đường dẫn.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}")
foreach(_cfg IN LISTS CMAKE_CONFIGURATION_TYPES)
    string(TOUPPER "${_cfg}" _cfgUpper)
    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_${_cfgUpper} "${CMAKE_BINARY_DIR}")
endforeach()

# ── Thư viện core: C++ thuần, KHÔNG GL / oF / ImGui ─────────────────────
file(GLOB_RECURSE MIKMAP_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/core/*.cpp"
)
file(GLOB_RECURSE MIKMAP_CORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/core/*.h"
)

add_library(mikmap_core STATIC ${MIKMAP_CORE_SOURCES} ${MIKMAP_CORE_HEADERS})
target_include_directories(mikmap_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/engine")

# ── Thư viện io: thu thập sensor. Phụ thuộc core, KHÔNG phụ thuộc GL/oF ─
#
# Tách thành library riêng KHÔNG phải để cho gọn — nó ÉP quy tắc phụ thuộc
# ở architecture.md §1: io → core, không bao giờ ngược lại. Nếu ai đó lỡ
# viết #include "io/..." trong core/, linker sẽ báo lỗi ngay.
file(GLOB_RECURSE MIKMAP_IO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/io/*.cpp"
)
file(GLOB_RECURSE MIKMAP_IO_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/io/*.h"
)

find_package(Threads REQUIRED)

add_library(mikmap_io STATIC ${MIKMAP_IO_SOURCES} ${MIKMAP_IO_HEADERS})
target_include_directories(mikmap_io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/engine")
target_link_libraries(mikmap_io PUBLIC mikmap_core Threads::Threads ${CMAKE_DL_LIBS})

# Winsock: PHẢI khai ở đây, không chỉ dựa vào `#pragma comment(lib, ...)`
# trong OscSource.cpp — pragma đó là đặc sản của MSVC, MinGW/clang trên
# Windows bỏ qua nó và linker báo thiếu __imp_socket, __imp_WSAStartup...
# PUBLIC vì test_osc.cpp cũng tự mở socket.
if(WIN32)
    target_link_libraries(mikmap_io PUBLIC ws2_32 iphlpapi winhttp)   # iphlpapi: GetAdaptersAddresses (NetScan.cpp); winhttp: HttpGet.cpp
endif()

# ── Thư viện i18n: bảng chuỗi giao diện ────────────────────────────────
#
# KHÔNG chạm ImGui — chỉ std + core, nên tách khỏi bất kỳ UI cụ thể nào
# (cả UI cũ lẫn src/ hiện tại đều có thể dùng lại). Target riêng để test
# đối chiếu được hai bảng ngôn ngữ mà không phải kéo cả một UI vào bản
# build test.
add_library(mikmap_i18n STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/i18n/Localization.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/engine/i18n/Localization.h"
)
target_include_directories(mikmap_i18n PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/engine")
target_link_libraries(mikmap_i18n PUBLIC mikmap_core)

# ── Plugin cảm biến giả (C thuần) ──────────────────────────────────────
# Cùng một file .c build hai lần: bản đúng API và bản lệch phiên bản để
# test loader từ chối. test_plugin.cpp nhận đường dẫn qua FAKE_PLUGIN_PATH
# / FAKE_PLUGIN_V999_PATH (bên dưới).
add_library(mikmap_fake_scan_plugin SHARED tests/plugins/fake_scan_plugin.c)
target_include_directories(mikmap_fake_scan_plugin PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/engine/io/plugin")
if(UNIX)
    target_link_libraries(mikmap_fake_scan_plugin PRIVATE m)
endif()

add_library(mikmap_fake_scan_plugin_v999 SHARED tests/plugins/fake_scan_plugin.c)
target_include_directories(mikmap_fake_scan_plugin_v999 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/engine/io/plugin")
target_compile_definitions(mikmap_fake_scan_plugin_v999 PRIVATE FAKE_API_VERSION=999)
if(UNIX)
    target_link_libraries(mikmap_fake_scan_plugin_v999 PRIVATE m)
endif()

# ── Unit tests ─────────────────────────────────────────────────────────
enable_testing()

file(GLOB MIKMAP_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

add_executable(mikmap_tests ${MIKMAP_TEST_SOURCES})
target_link_libraries(mikmap_tests PRIVATE mikmap_core mikmap_io mikmap_i18n)
target_include_directories(mikmap_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")
target_compile_definitions(mikmap_tests PRIVATE
    "FAKE_PLUGIN_PATH=\"$<TARGET_FILE:mikmap_fake_scan_plugin>\""
    "FAKE_PLUGIN_V999_PATH=\"$<TARGET_FILE:mikmap_fake_scan_plugin_v999>\"")
add_dependencies(mikmap_tests mikmap_fake_scan_plugin mikmap_fake_scan_plugin_v999)

add_test(NAME core_tests COMMAND mikmap_tests)

# ── Công cụ đóng gói & ký .mikpack ──────────────────────────────────────
add_executable(mikmap_packtool "${CMAKE_CURRENT_SOURCE_DIR}/tools/packs/packtool.cpp")
target_link_libraries(mikmap_packtool PRIVATE mikmap_io)
if(WIN32)
    target_link_libraries(mikmap_packtool PRIVATE bcrypt)
endif()

# ── Báo cáo cấu hình ───────────────────────────────────────────────────
list(LENGTH MIKMAP_CORE_SOURCES _n_core)
list(LENGTH MIKMAP_IO_SOURCES   _n_io)
list(LENGTH MIKMAP_TEST_SOURCES _n_test)
message(STATUS "MikMap: ${_n_core} core + ${_n_io} io source(s), ${_n_test} test file(s)")

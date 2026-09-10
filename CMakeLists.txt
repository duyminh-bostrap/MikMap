# ════════════════════════════════════════════════════════════════════════
#  HexMapping — build cho core/ + tests
#
#  QUAN TRỌNG: File này KHÔNG build ứng dụng chính.
#  Ứng dụng chính dùng openFrameworks Project Generator (.vcxproj).
#
#  Mục đích: core/ là C++20 thuần (chỉ phụ thuộc STL) nên build và test
#  được độc lập — không cần openFrameworks, không cần GPU.
#  ⇒ Unit test toán học chạy trong vài giây, hợp cho CI.
#
#  Cách dùng:
#     cmake -S . -B build
#     cmake --build build --config Debug
#     ctest --test-dir build -C Debug --output-on-failure
# ════════════════════════════════════════════════════════════════════════

cmake_minimum_required(VERSION 3.20)
project(HexMapping LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(MSVC)
    # /W4      cảnh báo mức cao
    # /permissive-  tuân thủ chuẩn chặt chẽ
    # /fp:precise   KHÔNG dùng /fp:fast — toán homography cần độ chính xác
    add_compile_options(/W4 /permissive- /fp:precise /utf-8)
else()
    add_compile_options(-Wall -Wextra -Wpedantic)
endif()

# ── Thư viện core: C++ thuần, KHÔNG GL / oF / ImGui ─────────────────────
file(GLOB_RECURSE HEXMAP_CORE_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.cpp"
)
file(GLOB_RECURSE HEXMAP_CORE_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/core/*.h"
)

add_library(hexmap_core STATIC ${HEXMAP_CORE_SOURCES} ${HEXMAP_CORE_HEADERS})
target_include_directories(hexmap_core PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")

# ── Thư viện io: thu thập sensor. Phụ thuộc core, KHÔNG phụ thuộc GL/oF ─
#
# Tách thành library riêng KHÔNG phải để cho gọn — nó ÉP quy tắc phụ thuộc
# ở architecture.md §1: io → core, không bao giờ ngược lại. Nếu ai đó lỡ
# viết #include "io/..." trong core/, linker sẽ báo lỗi ngay.
file(GLOB_RECURSE HEXMAP_IO_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/io/*.cpp"
)
file(GLOB_RECURSE HEXMAP_IO_HEADERS CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/src/io/*.h"
)

find_package(Threads REQUIRED)

add_library(hexmap_io STATIC ${HEXMAP_IO_SOURCES} ${HEXMAP_IO_HEADERS})
target_include_directories(hexmap_io PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(hexmap_io PUBLIC hexmap_core Threads::Threads)

# ── Thư viện i18n: bảng chuỗi giao diện ────────────────────────────────
#
# Nằm trong ui/ nhưng KHÔNG chạm ImGui — chỉ std + core. Tách ra thành
# target riêng để test đối chiếu được hai bảng ngôn ngữ mà không phải
# kéo cả openFrameworks vào bản build test.
add_library(hexmap_i18n STATIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Localization.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ui/Localization.h"
)
target_include_directories(hexmap_i18n PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src")
target_link_libraries(hexmap_i18n PUBLIC hexmap_core)

# ── Unit tests ─────────────────────────────────────────────────────────
enable_testing()

file(GLOB HEXMAP_TEST_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp"
)

add_executable(hexmap_tests ${HEXMAP_TEST_SOURCES})
target_link_libraries(hexmap_tests PRIVATE hexmap_core hexmap_io hexmap_i18n)
target_include_directories(hexmap_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/tests")

add_test(NAME core_tests COMMAND hexmap_tests)

# ── Báo cáo cấu hình ───────────────────────────────────────────────────
list(LENGTH HEXMAP_CORE_SOURCES _n_core)
list(LENGTH HEXMAP_IO_SOURCES   _n_io)
list(LENGTH HEXMAP_TEST_SOURCES _n_test)
message(STATUS "HexMapping: ${_n_core} core + ${_n_io} io source(s), ${_n_test} test file(s)")

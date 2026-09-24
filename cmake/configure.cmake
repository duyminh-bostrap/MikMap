# ════════════════════════════════════════════════════════════════════════
#  cmake/configure.cmake — bước configure dùng chung cho mọi máy
#
#     cmake [-DSRC=<thư mục nguồn>] -DBUILD=<thư mục build> [-DTYPE=Debug] -P cmake/configure.cmake
#
#  Tương đương `cmake -S <SRC> -B <BUILD> -DCMAKE_BUILD_TYPE=<TYPE>`, chỉ khác
#  một chỗ: trên Windows KHÔNG có Visual Studio, CMake mặc định chọn
#  "NMake Makefiles" rồi hỏng vì thiếu nmake. Script này khi đó tự chuyển sang
#  Ninja (hoặc "MinGW Makefiles") — nên VS Code task, hook pre-push và người
#  gõ tay đều chạy được trên máy mới mà không phải đặt biến môi trường nào.
#
#  Không đụng tới generator khi:
#    · macOS / Linux — mặc định (Unix Makefiles) luôn chạy được;
#    · Windows có Visual Studio — mặc định là generator Visual Studio;
#    · đã đặt biến môi trường CMAKE_GENERATOR — ý người dùng được ưu tiên;
#    · thư mục build đã configure rồi — generator đã ghi trong cache, đổi
#      giữa chừng thì CMake báo lỗi "generator does not match".
# ════════════════════════════════════════════════════════════════════════

# SRC mặc định = gốc repo (thư mục chứa cmake/). Có giá trị mặc định này là để
# lệnh configure engine KHÔNG phải viết `-DSRC=.`: Windows PowerShell 5.1 (shell
# mặc định của VS Code task trên Windows) tách đối số trần `-DSRC=.` thành hai,
# `-DSRC=` và `.` — SRC thành rỗng. Vì cùng lý do, giá trị -D truyền vào đây
# không nên chứa dấu chấm nếu gõ trần trong PowerShell.
if(NOT DEFINED SRC)
    get_filename_component(SRC "${CMAKE_CURRENT_LIST_DIR}/.." ABSOLUTE)
endif()
if(SRC STREQUAL "" OR NOT DEFINED BUILD OR BUILD STREQUAL "")
    message(FATAL_ERROR
        "Dung: cmake [-DSRC=<nguon>] -DBUILD=<build> [-DTYPE=Debug] -P cmake/configure.cmake\n"
        "(SRC rong? PowerShell 5.1 tach doi so tran chua dau cham, vd -DSRC=. — bo -DSRC, hoac boc trong nhay: '-DSRC=.')")
endif()
if(NOT EXISTS "${SRC}/CMakeLists.txt")
    message(FATAL_ERROR "Khong thay ${SRC}/CMakeLists.txt")
endif()
if(NOT DEFINED TYPE)
    set(TYPE Debug)
endif()

set(_gen "")
if(CMAKE_HOST_WIN32
   AND "$ENV{CMAKE_GENERATOR}" STREQUAL ""
   AND NOT EXISTS "${BUILD}/CMakeCache.txt")
    # Hỏi chính CMake generator mặc định của máy này là gì (dòng có dấu "*"
    # trong `cmake --help`) thay vì tự đoán Visual Studio có cài hay không.
    execute_process(COMMAND "${CMAKE_COMMAND}" --help OUTPUT_VARIABLE _help)
    string(REGEX MATCH "\n[*] ([^=\n]*[^ =\n])" _m "${_help}")
    set(_default "${CMAKE_MATCH_1}")

    if(_default MATCHES "^NMake")
        find_program(_nmake nmake)
        if(NOT _nmake)
            find_program(_ninja ninja)
            find_program(_mingw_make mingw32-make)
            if(_ninja)
                set(_genName Ninja)
            elseif(_mingw_make)
                set(_genName "MinGW Makefiles")
            else()
                message(FATAL_ERROR
                    "Khong co Visual Studio, cung khong tim thay ninja hay mingw32-make tren PATH.\n"
                    "Cai mot trong hai:  winget install Ninja-build.Ninja   (khuyen dung)\n"
                    "                    hoac Visual Studio / VS Build Tools (MSVC).")
            endif()
            set(_gen -G "${_genName}")
            message(STATUS "configure.cmake: khong co Visual Studio/nmake -> dung generator ${_genName}")
        endif()
    endif()
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" -S "${SRC}" -B "${BUILD}" ${_gen} "-DCMAKE_BUILD_TYPE=${TYPE}"
    RESULT_VARIABLE _rc)
if(NOT _rc EQUAL 0)
    message(FATAL_ERROR "cmake configure that bai (ma ${_rc})")
endif()

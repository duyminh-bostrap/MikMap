# Yêu cầu hệ thống để build MikMap

> Tổng hợp lại từ `README.md`/`.claude/rules/tech-defaults.md` — đây không phải
> tài liệu thứ hai độc lập, chỉ gom riêng phần "cần cài gì trên máy nào" cho dễ
> tra cứu khi setup máy mới. Nếu hai bên lệch nhau, `README.md` là bản đúng.

Có **hai hệ build tách biệt**, cài đặt yêu cầu khác nhau:

| Hệ build | Cần gì tối thiểu | GPU/GLFW? |
|---|---|---|
| `engine/` (`engine/core`+`engine/io`+`engine/i18n`) + `mikmap_tests` | CMake + trình biên dịch C++20 | Không — chạy được trên CI headless |
| `src/` (app GLFW + Dear ImGui) | CMake + trình biên dịch C++20 (+ GLFW nếu muốn dùng bản hệ thống) | Có — cần OpenGL, cần cửa sổ để *chạy* (build thì không) |

Cả hai đều dùng **C++20**, `CMAKE_CXX_EXTENSIONS OFF` (không dùng extension
ngoài chuẩn). Dear ImGui (v1.92.9b) và GLFW (fallback 3.5.1) được CMake tự tải
qua `FetchContent` lúc configure nếu không có sẵn — không cần dựng thủ công
thư mục `.tools/` trên bất kỳ OS nào.

---

## macOS

| | |
|---|---|
| Trình biên dịch | Xcode Command Line Tools: `xcode-select --install` |
| CMake | `brew install cmake` |
| GLFW | Không bắt buộc — `brew install glfw` để dùng bản hệ thống, thiếu thì CMake tự build từ nguồn |
| Kiến trúc đã kiểm chứng | Apple Silicon, SDK 15.0 |
| Cờ cảnh báo non-MSVC | `-Wall -Wextra -Wpedantic`, mục tiêu 0 cảnh báo (riêng OpenGL deprecated trên macOS thì bỏ qua — xem dưới) |

```bash
xcode-select --install
brew install cmake        # glfw tuỳ chọn: brew install glfw
cmake -S . -B build && cmake --build build --target mikmap_tests -j && ctest --test-dir build
cmake -S src -B src/build && cmake --build src/build
./src/build/mikmap
```

Ghi chú riêng của macOS (đã kiểm chứng thật, không phải suy đoán):

- `main.cpp` xin context **OpenGL 3.2 core + forward-compatible** và GLSL
  `#version 150` — macOS từ chối context 3.0 thường (app thoát mã 2 nếu xin
  sai).
- `clipart.cpp` tự định nghĩa `APIENTRY` rỗng (macro này không có sẵn trên
  macOS như trên Windows).
- Sẽ thấy cảnh báo `'glXxx' is deprecated` khi build — vô hại, Apple ngừng hỗ
  trợ OpenGL từ macOS 10.14, không phải lỗi code. Muốn tắt: định nghĩa
  `GL_SILENCE_DEPRECATION`.
- Output máy chiếu (F2/I1) đã mở và vẽ đúng, chụp được bằng `--outshot`; chưa
  thử nhiều màn hình vật lý thật.
- Debug: F5 trong VS Code dùng `lldb`, build vào `src/build-debug/` (đã
  gitignore).

---

## Linux

| | |
|---|---|
| Trình biên dịch | GCC/Clang hỗ trợ C++20 |
| CMake | `apt install cmake` |
| GL/GLFW | `apt install libgl-dev` bắt buộc; `libglfw3-dev` tuỳ chọn (thiếu thì CMake tự build GLFW từ nguồn) |
| Đã kiểm chứng | Build native + chạy dưới **Xvfb** (không cần màn hình vật lý để build/kiểm), giao diện render đúng |

```bash
sudo apt install cmake g++ libgl-dev      # libglfw3-dev tuỳ chọn
cmake -S . -B build && cmake --build build --target mikmap_tests -j && ctest --test-dir build
cmake -S src -B src/build && cmake --build src/build
./src/build/mikmap
```

Debug F5 trong VS Code dùng `gdb`, cùng cấu hình `src/build-debug/` như
Windows.

---

## Windows

| | |
|---|---|
| CMake | `winget install Kitware.CMake` |
| Trình biên dịch | **MSVC** (Visual Studio 2022 / VS Build Tools, component "Desktop development with C++") **hoặc** MinGW-w64 qua MSYS2: `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gdb` (hoặc w64devkit) — cả hai đã kiểm chứng thật. CMake tự chọn compiler nào có trên PATH (mở "Developer PowerShell for VS 2022" để có `cl.exe`, hoặc dùng generator mặc định `Visual Studio 17 2022` — cả hai cách đều không cần vào MSYS2 shell). |
| GLFW | Không bắt buộc cài — CMake tự dựng từ nguồn nếu không có |
| Đã kiểm chứng | **MinGW-w64**: cross-compile từ máy trắng (không có `.tools/`) ra `mikmap.exe` PE32+ thật, chạy qua Wine, chụp màn hình xác nhận giao diện + tiếng Việt có dấu render đúng. **MSVC**: biên dịch + chạy thật trên Windows thật (2026-09-23) — Release và Debug, dòng lệnh và task/F5 VS Code; ban đầu build gãy 2 chỗ (thiếu `NOMINMAX`, sai entry point cho `int main()` dưới `/SUBSYSTEM:WINDOWS`), đã sửa trong `src/CMakeLists.txt`, xem README.md để biết chi tiết. |
| Cờ MSVC (nếu build bằng MSVC) | `/W4 /permissive- /fp:precise /utf-8` — **`/fp:precise` bắt buộc** cho `engine/core/math` (toán homography/calibration cần độ chính xác, không đổi sang `/fp:fast`) |

```powershell
winget install Kitware.CMake

# Cách A — MSVC (đã kiểm chứng): mở "Developer PowerShell for VS 2022", rồi
cmake -S src -B src/build -DCMAKE_BUILD_TYPE=Release
cmake --build src/build --config Release
.\src\build\mikmap.exe

# Cách B — MinGW-w64 (đã kiểm chứng): trong MSYS2 shell
pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gdb
cmake -S src -B src/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build src/build
.\src\build\mikmap.exe
```

Ghi chú:

- Macro `near`/`far` của `windows.h` — không đặt tên biến/lambda trùng, lỗi
  báo ra khó hiểu (đã từng mắc lỗi này).
- `<windows.h>` (qua `glfw3native.h`/`dwmapi.h` trong `main.cpp`, và trực tiếp
  trong `output.cpp`/`clipart.cpp`) định nghĩa macro `min`/`max` nếu thiếu
  `NOMINMAX` — đè lên `std::min/std::max/std::clamp`, chỉ lộ ra khi build bằng
  MSVC thật (MinGW không dính). `src/CMakeLists.txt` đã tự thêm `NOMINMAX` cho
  mọi build Windows, không cần làm gì thêm.
- Debug F5 trong VS Code: `.vscode/launch.json` có **2 config riêng** —
  `"MikMap (Debug)"` dùng `gdb` (cho MinGW-w64) và
  `"MikMap (Debug, MSVC)"` dùng `cppvsdbg` (cho MSVC, không cần cài gdb).
  Chọn đúng config theo trình biên dịch đang dùng ở dropdown Run and Debug
  trước khi bấm F5, không thì debugger không khởi động được dù build vẫn qua.
- `.claude/hooks/pre-push.sh` build+test bằng CMake trước mỗi `git push` — chỉ
  chạy hệ `engine/`, không phụ thuộc GLFW nên chạy được trên mọi OS kể cả
  Windows.

---

## Tắt engine, chỉ build riêng giao diện

Dùng khi chỉ muốn thử UI, không cần link `engine/core`+`engine/io`:

```bash
cmake -S src -B src/build -DMIKMAP_WITH_ENGINE=OFF
```

## Kiểm tự động sau khi build `src/`

```bash
./src/build/mikmap --roundtrip <file.mikmap>   # lưu/mở/undo, không cần cửa sổ
ctest --test-dir src/build --output-on-failure  # test project_roundtrip
```

Test này không cần GPU/màn hình thật trên cả 3 OS.

# MikMap

Projection mapping engine kết hợp hệ thống calibration sensor: **chạm vào vật
thể thật, hiệu ứng nổ đúng chỗ đó**.

> **Lưu ý về tên:** repo, namespace (`mikmap`) và target CMake
> (`mikmap_core`/`mikmap_io`/`mikmap_i18n`/`mikmap_tests`) nay dùng thống nhất
> một tên `MikMap`/`mikmap` — trước đây mã nguồn dùng tên nội bộ riêng
> `HexMapping`/`hexmap`, đã đổi hết.

```
C++20 · Dear ImGui + GLFW (app) · CMake (engine + app, hai project riêng)
```

---

## Trạng thái hiện tại

`src/` (app GLFW+ImGui, được đổi tên từ `newui/`) là **bản đang phát triển**,
còn ở giai đoạn prototype:

| | |
|---|---|
| Giao diện | 3 màn Composition · Advanced Mapping · Sensor I/O + cửa sổ Cài đặt, bám bản thiết kế `MikMap Workspace.dc.html` |
| Engine dùng chung (`engine/core`, `engine/io`) | Đã biên dịch & link vào app; `src/` mới chỉ gọi `core/util/Json` (cho lưu/mở dự án), **chưa gọi model/calib/io thật** |
| Mô hình dữ liệu app hiện tại | Struct riêng trong `src/app.h`, chưa dùng `core/model` thật |
| Đối chiếu chi tiết | `features.md` — 56 mục `[x]` hành vi thật · 17 mục `[~]` một phần · 62 mục `[ ]`, trên 135 mục Resolume-parity |
| Build đa nền tảng | **Đã kiểm chứng thật**: macOS (2026-09-22, build+chạy trên máy Mac thật) · Linux (2026-09-22, build native + chạy dưới Xvfb, chụp màn hình) · Windows — **hai đường kiểm chứng riêng**: cross-compile MinGW-w64 (2026-09-22, ra `mikmap.exe` PE32+ thật, chạy qua Wine, chụp màn hình) **và MSVC thật trên Windows thật** (2026-09-23, build Debug lẫn Release qua cả dòng lệnh lẫn task/F5 của VS Code, chạy được) — cả 3 OS cùng render đúng giao diện, cùng tiếng Việt có dấu |

Xem [`features.md`](features.md) để biết chính xác cái gì thật/cái gì chỉ có
UI, và [`architecture.md`](architecture.md) để biết đích đến kiến trúc (chuỗi
`H_w⁻¹·H_s`, hợp đồng `IWarp`, mô hình thread) mà prototype này đang được ghép
dần vào.

> **Bản engine cũ (oF + MSBuild + ImGui-trên-oF, 313+ unit test, HAP 4K thật,
> giao diện hoàn chỉnh hơn) đã được lưu trữ nguyên vẹn ở nhánh git
> [`legacy-oF-ui`](../../tree/legacy-oF-ui)** — tham khảo hoặc khôi phục từ đó
> nếu cần, đừng tìm trong `main`/`new_UI` nữa.

---

## Bố cục thư mục

```
engine/     core/ + io/ + i18n — C++20 THUẦN, build bằng CMake gốc, không cần GPU/GLFW
            (dùng chung, không thuộc riêng UI nào)
src/        App GLFW + Dear ImGui hiện tại — CMake riêng (src/CMakeLists.txt)
tests/      Unit test cho engine/ — chạy được trên CI, không cần GPU
architecture.md   Kiến trúc đích: quy tắc phụ thuộc, chuỗi biến đổi toạ độ, mô hình thread
features.md       Backlog 135 mục kiểu Resolume, chấm điểm theo code thật của src/
```

---

## Build

Hai hệ build **tách biệt**, đọc chung `engine/`:

### `engine/` + unit test — CMake thuần, chạy trên Linux/macOS/Windows

```bash
cmake -DBUILD=build -P cmake/configure.cmake
cmake --build build --target mikmap_tests -j
ctest --test-dir build --output-on-failure
```

Không cần GPU, không cần GLFW/ImGui. Toàn bộ toán học mapping và calibration
nằm ở `engine/core`, test trong vài giây.

**Đã kiểm chứng bộ test bằng MinGW-w64 GCC 16 trên Windows (2026-09-24):**
416 test / 5706 assertion đạt, build không cảnh báo, F5 *Engine tests (Debug)*
dừng đúng breakpoint bằng gdb. Lần đầu chạy bằng GCC lộ ra 3 lỗi mà MSVC giấu
(nay đã sửa): `test_filter.cpp` thiếu `#include <algorithm>` (không biên dịch
được); winsock chỉ được nối bằng `#pragma comment(lib)` của MSVC (link thiếu
`ws2_32`); và `if (!f)` không bắt được file không tồn tại trên libstdc++ của
MinGW (dùng `is_open()`). Cũng thêm `-ffp-contract=off` cho GCC/Clang để toán
homography không bị gộp FMA. **Chưa chạy** bộ test này trên macOS/Linux thật.

### `src/` — app GLFW + ImGui, **một lệnh giống nhau cho Windows/Linux/macOS**

```bash
cmake -DSRC=src -DBUILD=src/build -DTYPE=Release -P cmake/configure.cmake
cmake --build src/build --config Release
./src/build/mikmap          # Windows: .\src\build\mikmap.exe
```

Chỉ cần **CMake + một trình biên dịch C++20**, không cần dựng sẵn thư mục
`.tools/` nào:

| OS | Cần cài |
|---|---|
| Windows | CMake (`winget install Kitware.CMake`) + **MSVC** (Visual Studio / VS Build Tools, đã kiểm chứng thật) hoặc MinGW-w64 (MSYS2: `pacman -S mingw-w64-x86_64-gcc mingw-w64-x86_64-gdb`, hoặc w64devkit — cũng đã kiểm chứng). CMake tự chọn trình biên dịch nào có trên PATH. |
| macOS | `xcode-select --install` + `brew install cmake` (GLFW qua `brew install glfw` nếu muốn, không bắt buộc) |
| Linux | `apt install cmake g++ libgl-dev` (GLFW qua `apt install libglfw3-dev` nếu muốn, không bắt buộc) |

> **Vì sao configure qua `cmake/configure.cmake` thay vì `cmake -S … -B …`:**
> hai lệnh tương đương, trừ một chỗ — trên Windows **không có Visual Studio**,
> CMake mặc định chọn `NMake Makefiles` rồi hỏng vì thiếu `nmake`. Script tự
> chuyển sang Ninja (hoặc `MinGW Makefiles`), nên máy mới clone về chạy được
> ngay, không phải đặt biến môi trường nào. Trên macOS, Linux, hoặc Windows có
> Visual Studio thì nó không đổi gì — gõ `cmake -S … -B …` trần cũng được.
> VS Code task (F5), `src/build.ps1` và hook pre-push đều đi qua script này.

**Dear ImGui** (v1.92.9b) luôn được CMake tải lúc configure. **GLFW** dùng bản
hệ thống nếu có (brew/apt/vcpkg), không có thì CMake tự tải và dựng GLFW 3.5.1
từ nguồn — nên máy trắng vẫn build được, chỉ tốn thêm ~1 phút lần đầu.

**VS Code:** bấm `F5` là xong (`.vscode/tasks.json` configure+build vào
`src/build-debug`, `.vscode/launch.json` chạy debugger). Build task dùng
chung một cấu hình cho cả 3 OS. Debugger thì **Windows có 2 config** vì gdb và
cppvsdbg không thay thế nhau được: `"MikMap (Debug)"` (gdb — cho ai build bằng
MinGW-w64) và `"MikMap (Debug, MSVC)"` (`cppvsdbg`, debugger gốc của VS, đọc
được PDB của MSVC, không cần cài gdb) — chọn đúng config theo trình biên dịch
đang dùng ở dropdown Run and Debug. macOS dùng `lldb`, Linux dùng `gdb`, đều
chỉ 1 config như cũ.

Đã kiểm chứng thật: **Linux** (2026-09-22, build native + chạy dưới Xvfb) ·
**macOS** (2026-09-22, Apple Silicon, Xcode SDK 15.0, GLFW qua Homebrew — build
sạch, app chạy ổn định, F5 hoạt động; app xin OpenGL 3.2 core + GLSL 150 vì
macOS không cấp context 3.0 thường, có vài cảnh báo `deprecated` vô hại; cửa sổ
output máy chiếu F2/I1 mở và vẽ đúng, chụp bằng `--outshot`, chưa thử đa màn
hình thật) · **Windows** — hai đường kiểm chứng riêng:
- *MinGW-w64* (2026-09-22, cross-compile từ máy trắng không có `.tools/`: CMake
  tự dựng GLFW từ nguồn, ra `mikmap.exe` PE32+ chạy đúng qua Wine).
- *MSVC* (2026-09-23, biên dịch thật trên Windows thật bằng Visual Studio 2022
  — cả Release lẫn Debug, cả dòng lệnh lẫn task/F5 VS Code, app chạy được).
  Build ban đầu **gãy 2 chỗ**, cả hai đã sửa trong `src/CMakeLists.txt`:
  thiếu `NOMINMAX` (`<windows.h>` qua `glfw3native.h`/`dwmapi.h` định nghĩa
  macro `min`/`max`, đè lên `std::min/max/clamp` — MinGW không dính lỗi này
  nên chưa từng lộ ra), và entry point (`WIN32_EXECUTABLE TRUE` bắt MSVC tìm
  `WinMain`, nhưng `main.cpp` dùng `int main()` chuẩn — MinGW's `-mwindows` tự
  xử lý được còn MSVC cần `/ENTRY:mainCRTStartup` tường minh).

Tắt engine để dựng riêng phần giao diện: `-DMIKMAP_WITH_ENGINE=OFF`.

---

## Việc còn lại để ghép trọn engine thật vào `src/`

`src/` hiện giữ mô hình dữ liệu riêng (`src/app.h`); bước tiếp theo là
chuyển sang mô hình thật của `engine/`, làm từng mảng để luôn build được:

1. `core/model/ProjectIO` → thay `src/project.cpp` (hiện lưu `.mikmap` bằng schema riêng của `src/`, đã chạy được — I2); còn thiếu preset output/calibration dùng chung giữa dự án (F8, G8).
2. `core/calib/*` → thay phép tính homography (DLT+RANSAC tự viết) và
   SensorMapper trong `src/calib.cpp` (G5, G7).
3. `core/model/Slice` + `WarpCornerPin`/`WarpMesh`/`WarpBezier` → thay phép
   warp trong `src/mapping.cpp`, qua đó có luôn Bezier (F10) và mặt nạ
   bezier (F12).
4. `io/*` → nguồn sensor thật (G1–G4, G14) thay cho radar mô phỏng trong
   `src/sensor.cpp`.
5. `core/model/Composition` · `Layer` · `Clip` · `Transport` → thay mô hình
   deck trong `src/app.h`.

---

## Đa ngôn ngữ

`engine/i18n/Localization.{h,cpp}` là bảng chuỗi Tiếng Việt/English dùng
chung, KHÔNG chạm ImGui — build và test độc lập (`tests/test_localization.cpp`
báo ngay nếu thiếu khoá hoặc lệch định dạng `%d`/`%s` giữa các bảng). Thêm
ngôn ngữ mới: chép một bảng trong `Localization.cpp`, dịch giá trị, thêm vào
`enum class Language`.

---

## Kiến trúc

```
core   ──▶ (không phụ thuộc ai)      engine/core — math, model, calib, filter
io     ──▶ core                     engine/io — thread sensor
ui/render/app  ──▶ core (+io)       tuỳ UI — hiện là src/ (GLFW+ImGui)
```

`core/` không phụ thuộc gì ngoài STL. Nhờ vậy toàn bộ toán học mapping và
calibration test được trong vài giây mà không cần GPU hay GLFW.

Công thức trung tâm:

```
p_content = H_w⁻¹ · H_s · p_sensor
```

`H_s` (calibration sensor) và `H_w` (keystone của slice) là **hai ma trận
tách rời** — chỉnh lại keystone không làm hỏng calibration. Chi tiết ở
`architecture.md` §4.

**Video HAP** (không phải H.264/MP4) là bắt buộc khi engine phát video thật —
lý do và băng thông đo được ở `architecture.md`; `src/` hiện chưa phát video
thật (B1 trong `features.md`), nên chưa áp dụng cho prototype.

---

## Giấy phép

Chưa chọn.

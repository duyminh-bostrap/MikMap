# MikMap

Projection mapping engine kết hợp hệ thống calibration sensor: **chạm vào vật
thể thật, hiệu ứng nổ đúng chỗ đó**.

> **Lưu ý về tên:** repo tên `MikMap`, mã nguồn dùng tên nội bộ **HexMapping**
> (namespace `hexmap`, target `hexmap_core`/`hexmap_io`). Hai tên này chỉ khác
> nhau ở nhãn, không phải hai thứ khác nhau.

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
| Đối chiếu chi tiết | `features.md` — 38 mục `[x]` hành vi thật · 26 mục `[~]` một phần · 71 mục `[ ]`, trên 135 mục Resolume-parity |

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
cmake -S . -B build
cmake --build build --target hexmap_tests -j
ctest --test-dir build --output-on-failure
```

Không cần GPU, không cần GLFW/ImGui. Toàn bộ toán học mapping và calibration
nằm ở `engine/core`, test trong vài giây.

### `src/` — app GLFW + ImGui, cả Windows/Linux/macOS

```powershell
# Windows — bộ công cụ (MinGW GCC, CMake, Ninja, Dear ImGui, GLFW) nằm trong
# src/.tools/, không vào git. Xem src/build.ps1 để build nhanh.
cmake -S src -B src/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build src/build
.\src\build\mikmap.exe
```

```bash
# Linux/macOS — cần GLFW cài qua package manager hệ thống
# (Linux: apt install libglfw3-dev libgl-dev · macOS: brew install glfw).
# Dear ImGui (v1.92.9b, khớp bản Windows) được CMake tự tải lúc configure,
# không vendor vào repo.
cmake -S src -B src/build -DCMAKE_BUILD_TYPE=Release
cmake --build src/build
./src/build/mikmap
```

Đã build+chạy thật kiểm chứng trên Linux (không chỉ compile — chạy dưới Xvfb,
giao diện render đúng) và trên macOS (Apple Silicon, Xcode SDK 15.0, GLFW 3.5.1
qua Homebrew: build sạch, app mở cửa sổ và chạy ổn định, F5 trong VS Code dùng
`.vscode/launch.json`). Trên macOS app xin OpenGL 3.2 core (+ GLSL 150) vì hệ
điều hành không cấp context 3.0 thường; sẽ có vài cảnh báo `deprecated` của
OpenGL khi build — vô hại. Cửa sổ output máy chiếu (F2/I1) cũng đã mở và vẽ đúng trên macOS
(chụp bằng `--outshot`); chưa thử với nhiều màn hình thật. Tắt engine để dựng riêng phần giao diện:
`-DMIKMAP_WITH_ENGINE=OFF`.

---

## Việc còn lại để ghép trọn engine thật vào `src/`

`src/` hiện giữ mô hình dữ liệu riêng (`src/app.h`); bước tiếp theo là
chuyển sang mô hình thật của `engine/`, làm từng mảng để luôn build được:

1. `core/model/ProjectIO` → thay `src/project.cpp` (hiện lưu `.mikmap` bằng schema riêng của `src/`, đã chạy được — I2); còn thiếu preset output/calibration dùng chung giữa dự án (F8, G8).
2. `core/calib/*` → thay phép tính homography (DLT tự viết, chưa RANSAC) và
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

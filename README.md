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
| Giao diện | 2 trang Composition · Mapping (Mapping gộp máy chiếu và sensor; cây chia mỗi screen thành hai phần Slices / Sensors, kéo-thả slice/sensor sang screen khác; thêm sensor là popup trên mọi trang) + cửa sổ Cài đặt, bám bản thiết kế `MikMap Workspace.dc.html` |
| Engine dùng chung (`engine/core`, `engine/io`) | Đã biên dịch & link vào app; `src/` gọi `core/util/Json` (lưu/mở dự án), `core/util/Sha2` + `Ed25519` (xác minh gói), `core/calib/LidarPlacement`, `core/math` (EdgeBlend/Triangulate), `io/net`, `io/sources`, `io/plugin`, `io/packs` (thread sensor + gói cảm biến tải khi cần); **chưa gọi model/warp/calib DLT/ProjectIO thật** |
| Mô hình dữ liệu app hiện tại | Struct riêng trong `src/app.h`, chưa dùng `core/model` thật |
| Đối chiếu chi tiết | `features.md` — 77 mục `[x]` hành vi thật · 15 mục `[~]` một phần · 57 mục `[ ]`, trên 149 mục Resolume-parity |
| Build đa nền tảng | **Đã kiểm chứng thật**: macOS (2026-09-22, build+chạy trên máy Mac thật) · Linux (2026-09-22, build native + chạy dưới Xvfb, chụp màn hình) · Windows — **hai đường kiểm chứng riêng**: cross-compile MinGW-w64 (2026-09-22, ra `mikmap.exe` PE32+ thật, chạy qua Wine, chụp màn hình) **và MSVC thật trên Windows thật** (2026-09-23, build Debug lẫn Release qua cả dòng lệnh lẫn task/F5 của VS Code, chạy được) — cả 3 OS cùng render đúng giao diện, cùng tiếng Việt có dấu. **Chưa kiểm:** build Linux/macOS của code trang Sensor (2026-10-28: `engine/io/net` NetScan/TouchSender, `src/sources.cpp`) — mới build/chạy trên Windows MSVC |

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
tools/      Công cụ đi kèm (pose_bridge Python, mikmap_packtool đóng gói/ký .mikpack)
tests/      Unit test cho engine/ — chạy được trên CI, không cần GPU
architecture.md   Kiến trúc đích: quy tắc phụ thuộc, chuỗi biến đổi toạ độ, mô hình thread
features.md       Backlog 149 mục kiểu Resolume, chấm điểm theo code thật của src/
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
4. `io/*` → nguồn sensor thật (G3, G14) thay cho radar mô phỏng trong
   `src/sensor.cpp`. OSC/UDP (G4), camera skeleton (G20), Hokuyo 2D lidar
   (G21, SCIP 2.0/TCP) và thiết bị Plugin DLL (G22, ABI C) đã chạy qua
   `src/sources.cpp`; sensor chọn và chỉnh ngay trên trang Mapping (tab INPUT/OUTPUT), đặt lidar theo mm, scan IP thêm nhiều sensor và gửi toạ độ OSC/UDP (G18, G24–G27) (gốc toạ độ gửi ra mặc định dưới-trái, chọn được trên-trái; project cũ giữ trên-trái) cũng đã chạy; Camera 2D native Off/Body/Hand/Face/Full đã nối vào app (xem dưới). Serial/Arduino (G3) chưa làm; YDLIDAR (G23) còn chờ plugin/phần cứng thật, mới kiểm ABI bằng plugin giả; lidar 3D/depth còn chờ.
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

## Camera 2D: Off · Body · Hand · Face · Full (2026-11-01)

Mapping → + Sensor → **Camera 2D**, bật thiết bị, chọn **INPUT → Mode**.
Mỗi camera chỉ chạy một mode; "Pose" là Body. Camera thật dùng Media Foundation
trên Windows. Mỗi mode nhận dạng cần `onnx-runtime-cuda` cùng gói trong bảng;
xem [gói mẫu, nguồn mô hình và cách ký](tools/packs/examples/README.md).

| Mode (`cam.mode`) | Mô hình / gói | Điểm mỗi đối tượng | Trần đối tượng |
|---|---|---:|---:|
| Off (0) | Chỉ preview, không cần model | 0 | 0 |
| Body (1) | RTMO s/m / `pose-rtmo` | 17 | 30 người |
| Hand (2) | Detector tay + RTMPose-hand / `hand-rtm` | 21 | 24 bàn tay |
| Face (3) | Detector mặt + RTMPose-face / `face-rtm` | 106 | 4 mặt |
| Full (4) | Detector người + RTMW / `wholebody-rtmw` | 133 | 3 người |

Trần tính theo khung 512 điểm, trước khi lọc confidence/nhóm; Properties hiện số
đối tượng sau khi kẹp. Full gồm 17 thân + 6 bàn chân + 68 mặt + 2×21 tay;
tắt nhóm chỉ lọc điểm xuất ra, không giảm số lần chạy mô hình điểm theo người.
Full có năm ô Body/Left hand/Right hand/Face/Feet. Project lưu `cam.mode` 0..4
và `cam.groups` là bitmask `1 << group`; thiếu `groups` nạp thành 31 (bật cả năm).
Đổi mode/nhóm khởi động lại nguồn, reset slot và gửi frame 0 trước luồng mới;
thiếu gói vẫn giữ mode đã chọn, hiện PACK/Cần gói và đường vào Cài đặt.

Body giữ lookup model đã xác minh `rtmo-s*.onnx`/`rtmo-m*.onnx`; ba mode mới dùng
`detector.onnx` + `keypoints.onnx` từ manifest đã xác minh. App gọi `CameraSource`
qua `src/sources.cpp`, dùng `PoseModel` cho Body, chung `TopDownPose` cho Hand/Face/Full
và `PersonTracker` cho slot. Hand độc lập không gắn tay với người: trái/phải là
heuristic dấu tích có hướng từ cổ tay–gốc ngón trỏ–gốc ngón út (0/5/17), có xét
Flip và bỏ phiếu cửa sổ năm quan sát; hoà giữ bên cũ. Lật lòng/mu tay, che khuất,
góc nhìn nghiêng hoặc điểm sai có thể làm nhầm bên; vote chỉ giảm rung. Full lấy
bên theo layout mô hình. INPUT vẽ xương thân/tay, chấm mặt/bàn chân theo màu nhóm.

OUTPUT giữ calibration/gửi/trigger. Capture chọn **cả group và id**: Body/Full
là body id 16 (cổ tay phải slot 0), Hand là hand-right id 58 (đầu ngón trỏ slot 0).
Full tắt Body thì Capture bị vô hiệu cùng gợi ý bật lại. Face chỉ Capture chóp mũi
khi ONNX `keypoints.onnx` có custom metadata `mikmap.face.nose_tip_index`: chuỗi
chỉ chữ số thập phân 0..105, do nhà xuất bản xác minh đúng layout và giải phẫu.
Chưa xác minh được chỉ số chóp mũi của mô hình Face thật; thiếu/sai metadata thì
Capture bị vô hiệu, gợi ý calibrate bằng Body trước. Ma trận Body hoặc chỉnh tay
đã có vẫn dùng qua các mode trên cùng camera/cấu hình ảnh chuẩn hoá; kiểm lại khi
đổi camera, Flip hoặc cách đóng khung ảnh.

GPU không dùng được thì thử CPU, trạng thái vàng cảnh báo chậm. Camera 3D chưa
có driver, không có trong popup; Gesture còn mờ. Crop/NDI/Spout chưa làm.
Khởi tạo, đóng camera và session chạy trên một worker 16 slot;
worker chậm có thể làm camera khác chờ Connecting, không chặn frame render.
Gói đã nạp được bảo vệ khỏi cập nhật cả khi tắt thiết bị; khởi động lại để cập nhật.
Mọi đường cài online/offline kiểm bảo vệ gói phụ thuộc. Không suy ra toàn bộ app
không lock/alloc: snapshot của PackInstaller vẫn có mutex có sẵn.

ORT chạy trong tiến trình app: crash native có thể làm tắt cả app. Show cần tiếp
tục khi inference hỏng nên dùng **Skeleton OSC + [pose_bridge](tools/pose_bridge/README.md)**
ở tiến trình riêng. `--replay <file.mikpose>` kiểm UI/luồng điểm bằng dữ liệu ghi sẵn,
không chạy inference; `--camera-mode 0|1|2|3|4` chọn Off/Body/Hand/Face/Full.
Replay `.mikpose`: mỗi dòng `tSec slot joint x y confidence [group]`, thiếu group
là Body (0); joint là chỉ số cục bộ trong nhóm (tay phải 0..20, chưa cộng 50).

Ghi chú kiểm tra và giới hạn đo đạc ở cuối [features.md](features.md).

### Giao thức gửi điểm theo nhóm (Plan B, tương thích Body Plan A)

OSC gửi bundle; mỗi điểm camera có `i:id f:x f:y f:z`, z là confidence 0–1,
không phải mét. Nguồn cũ không có z vẫn gửi `i:id f:x f:y`. x/y đi qua `H_s`,
chuẩn hoá theo sân khấu Screen và Origin đã chọn; có thể ra ngoài 0–1 sau calib.
Danh tính điểm là **(group, id)**, không dùng id đơn lẻ vì các nhóm có thể trùng id.

| Group | Địa chỉ OSC | Id |
|---|---|---|
| 0 body | `/mikmap/<device>/touch` | `slot*100 + i`, i theo MediaPipe (17 khớp COCO ánh xạ sang 0,2,5,7,8,11,12,13,14,15,16,23,24,25,26,27,28) |
| 1 hand-left | `/mikmap/<device>/hand` | `slot*100 + i`, i = 0..20 |
| 2 hand-right | `/mikmap/<device>/hand` | `slot*100 + 50 + i`, i = 0..20 |
| 3 face | `/mikmap/<device>/face` | **`slot*1000 + i`**, i = 0..105 ở Face, 0..67 ở Full |
| 4 feet | `/mikmap/<device>/feet` | `slot*100 + i`, i = 0..5 (Full) |

Full dùng chung slot người cho mọi nhóm; Hand dùng slot của riêng từng bàn tay,
Face dùng slot khuôn mặt. Face106 và Full68 là hai layout riêng, không suy ra
ý nghĩa giải phẫu từ cùng số i. Các địa chỉ hand/face/feet là **sibling** của
touch, không phải `/touch/hand`. Receiver `OscSource` dùng prefix touch đã cấu
hình để nhận các sibling này và tách hai bên tay qua `id % 100`.

Chỉ chunk đầu có `/mikmap/<device>/frame i:count`: count là **tổng số điểm**,
không phải sequence hay số đối tượng. Chunk sau chỉ chứa điểm. `frame 0` xoá
mọi nhóm của thiết bị (kể cả khi đổi sang mode thiếu gói). Mỗi datagram ≤1200
byte, không cắt đôi message/line; UDP có thể rơi hoặc đảo gói, **không có sequence
để ghép/khôi phục frame**. Gửi frame 0 không bảo đảm nó tới nơi qua mạng mất gói.

UDP text giữ `<device> frame <count>` và dòng Body `<device> <id> <x> <y> [<z>]`;
nhóm khác thêm tên: `<device> hand|face|feet <id> <x> <y> [<z>]`.
Bytes Body OSC/text không đổi so với Plan A, có golden từ trước Plan B và kiểm
replay qua `SensorsTick` → `SendOut` → UDP thật so từng byte với golden bất biến.

# MikMap / HexMapping — bộ não dự án

> File này Claude Code tự nạp mỗi phiên làm việc trong repo. Nội dung dưới đây
> lấy trực tiếp từ `architecture.md` (kiến trúc + nguyên tắc bất di bất dịch),
> `README.md` và `features.md` (tình trạng THẬT của app hiện tại) — các tài
> liệu có thẩm quyền cao nhất khi có mâu thuẫn với suy đoán.
>
> **Đã tái cấu trúc:** `core/`/`io/` chuyển từ `src/core`,`src/io` sang
> `engine/core`,`engine/io` (dùng chung, không thuộc riêng UI nào).
> `newui/` (prototype GLFW+ImGui) đã đổi tên thành `src/` — đây là app **hiện
> tại**. Bản UI cũ (oF + MSBuild + ImGui-trên-oF, đầy đủ hơn nhiều) đã lưu trữ
> nguyên vẹn ở nhánh git **`legacy-oF-ui`**, không còn trong working tree này.

## Dự án là gì

Projection mapping engine kiểu Resolume (deck · layer · column) → composition
canvas ảo → slice có keystone/mesh warp → máy chiếu, kèm chuỗi ánh xạ ngược từ
sensor về toạ độ nội dung: **chạm vào vật thể thật, hiệu ứng nổ đúng chỗ đó**.
Repo tên `MikMap`, mã nguồn tên nội bộ `HexMapping`.

```
C++20 · Dear ImGui + GLFW (app hiện tại, src/) · CMake (engine + app, hai project riêng)
Bản cũ (nhánh legacy-oF-ui): openFrameworks 0.12.x · OpenGL 4.x · MSVC 2022
```

## Bố cục thư mục hiện tại

```
engine/core/    C++20 THUẦN — math, model, calib, filter, util. Zero GL/GLFW/ImGui
engine/io/      thread sensor — không biết Slice/Layer tồn tại
engine/i18n/    bảng chuỗi Tiếng Việt/English (Localization.h/cpp), chỉ std+core
src/            app GLFW + Dear ImGui HIỆN TẠI (đổi tên từ newui/) — CMake riêng
tests/          unit test cho engine/, chạy trên CI không cần GPU
architecture.md kiến trúc ĐÍCH (xem ghi chú vị trí vật lý ở đầu file đó)
features.md     backlog 135 mục, chấm điểm theo code thật của src/
```

`src/` hiện là struct riêng (`src/app.h` và các file phẳng
`calib.cpp`/`deck.cpp`/`mapping.cpp`/`sensor.cpp`/`output.cpp`/`clipart.cpp`/`project.cpp`/...), **link `engine/core`+
`engine/io` vào, nhưng chỉ gọi `core/util/Json` (ở `project.cpp`); phần còn lại CHƯA gọi tới** — xem "Việc còn lại để ghép trọn" ở
`README.md`. Đừng nhầm đây là cây thư mục layered đầy đủ ở `architecture.md`
§2 (`app/`,`ui/`,`render/`,`io/`,`core/`) — cây đó mô tả bản cũ trên nhánh
`legacy-oF-ui` và là đích mà `src/` đang được ghép dần vào.

## Quy tắc phụ thuộc BẤT KHẢ XÂM PHẠM (`architecture.md` §1)

```
core   ──▶ (không phụ thuộc ai)      engine/core
io     ──▶ core                     engine/io — không biết Slice/Layer tồn tại
(ui/render/app) ──▶ core [+io]      hiện là src/ (GLFW+ImGui) — chỉ đọc model
```

- `engine/core/` không bao giờ `#include` `<GL/...>`, GLFW, `imgui.h`.
- `engine/io/` không bao giờ biết `Slice`, `Layer`, hay ma trận mapping tồn tại.
- `engine/core/` chỉ được phụ thuộc STL. Nếu cần OpenCV (RANSAC), đặt sau macro
  `HEXMAP_USE_OPENCV` kèm fallback DLT tự viết.
- `.claude/hooks/pre-push.sh` chạy một bản kiểm nhẹ bằng grep trước khi push
  (đường dẫn đã cập nhật theo `engine/core`, `engine/io`).

## Chuỗi biến đổi toạ độ trung tâm (`architecture.md` §4)

```
p_content = H_w⁻¹ · H_s · p_sensor
```

`H_s` (calibration sensor, DLT+RANSAC) và `H_w` (keystone/warp của slice) là
**hai ma trận tách rời mãi mãi** — trộn chung là lỗi kiến trúc phổ biến nhất
của loại hệ thống này. Chuỗi đầy đủ có thêm tầng **Composition Canvas** (không
gian ảo, độc lập độ phân giải máy chiếu) ở giữa:

```
p_sensor → ×H_s → p_output → tìm slice chứa điểm (duyệt z-order, điểm đầu
tiên khớp thắng) → slice.warp.inverse() → UV cục bộ → ×inputRect → p_canvas
→ ×transform⁻¹ layer/clip → p_clip
```

Mặt nạ bezier **không phải một phép biến đổi** — là hàm che alpha ở
`contentUV`, nên tự động đi theo `H_w` khi kéo lại keystone.

**Lưu ý về `src/` hiện tại:** homography ở đây (`src/calib.cpp`) là
DLT + chuẩn hoá Hartley tự viết, **chưa có RANSAC** (bản đủ DLT+RANSAC là
`engine/core/calib/` — đã có, đúng thuật toán, nhưng `src/` chưa gọi tới). Đừng tưởng nhầm `src/`
đã có calibration chuẩn chỉ vì `engine/core/calib` tồn tại trong repo.

## Hợp đồng `IWarp` — bài học đã trả giá (`architecture.md` §4.2)

```cpp
class IWarp {
public:
    virtual Vec2 forward(const Vec2& contentUV) const = 0;
    virtual bool inverse(const Vec2& outputPx, Vec2& outUV) const = 0;  // BẮT BUỘC
    virtual bool isInvertible() const = 0;
    virtual void tessellate(int cols, int rows, WarpGeometry& out) const = 0;
};
```

`inverse()` trả `bool` + tham số ra, **không phải `Vec2`**: phép nghịch đảo có
thể thất bại hợp lệ (điểm ngoài vùng warp, ô lưới suy biến). Trả thẳng `Vec2`
buộc phải bịa giá trị — thường `{0,0}`, nhưng đó lại là toạ độ hợp lệ (góc
trên-trái), nên người gọi không phân biệt được "chạm góc" với "trượt ra
ngoài". Định nghĩa ở `engine/core/model/IWarp.h`; mọi `IWarp` mới phải theo
đúng chữ ký này ngay từ đầu.

## Mô hình thread (`architecture.md` §3, §6)

- Mỗi sensor (Serial/OSC/Depth) là **1 thread riêng, được phép chặn** ở
  `read()`/`recvfrom()`/SDK — nhưng **không bao giờ chia sẻ dữ liệu với render
  thread bằng mutex**, chỉ qua `TripleBuffer` (state, được phép rơi frame cũ)
  hoặc `SpscRingBuffer` (event, KHÔNG được rơi).
- **Quyết định thiết kế cố ý:** sensor thread gửi toạ độ THÔ; render thread
  mới áp `H_w⁻¹·H_s`. *"Dữ liệu thô đi qua biên thread; ngữ nghĩa được áp dụng
  tại nơi sử dụng."*
- Độ trễ đo bằng `tCaptureNs` đóng dấu lúc thu thập, không đoán.
- `SensorFrame`/`TouchPoint` (`engine/io/SensorFrame.h`) là POD kích thước cố
  định, memcpy-able.

## 10 nguyên tắc bất di bất dịch (`architecture.md` §10)

1. Render thread không bao giờ lock, alloc, hay chạm đĩa.
2. `engine/core/` không bao giờ include GL / GLFW / ImGui.
3. Mọi `IWarp` phải cài đặt được `inverse()`.
4. `H_s` và `H_w` là hai ma trận tách rời, mãi mãi.
5. Toạ độ thô đi qua biên thread; ngữ nghĩa áp dụng tại nơi sử dụng.
6. Độ trễ phải được ĐO, không được ĐOÁN.
7. Mọi tính năng phải chạy được với `MockSource` — không cần phần cứng thật.
8. Hình học người dùng vẽ ra sống ở không gian gắn với NỘI DUNG
   (`contentUV`/canvas), không phải với máy chiếu.
9. Thành viên khó sao chép thì bọc riêng (`WarpPtr::clone()`), không viết tay
   copy constructor liệt kê từng trường.
10. (Ứng dụng thực tế của #6) PerfPanel là công dân hạng nhất của UI.

## `src/` — tình trạng THẬT (theo `features.md`, không theo vẻ ngoài)

38 mục `[x]` (hành vi thật) · 26 mục `[~]` (chỉ UI/một phần — đọc "Ghi chú
kiểm tra" cuối `features.md` để biết chính xác cái gì còn giả) · 71 mục `[ ]`,
trên 135 mục Resolume-parity (đếm lại 2026-09-21 từ các dòng bảng).

**Đã có thật sau đợt bổ sung P0** (đã grep `src/*.cpp`): output ra cửa sổ máy
chiếu riêng, không viền (`F2`/`I1`, `output.cpp`); canvas ảo 1920×1080 (`A1`);
transform clip D1–D6; transport LOOP/BOUN/HOLD/ONCE + REV + tốc độ (`C2/C4/C5`);
blend mode dùng hàm trộn GL thật, 8 mode (`A7`/`D4`, `clipart.cpp` — riêng
Overlay tạm dùng Screen); bấm đúp header cột = bắn cả cột (`A6`); PerfPanel
FPS/P99/frame rớt (`G9`, `calib.cpp`).

**Đợt UX 2026-09-21** (`project.cpp`, xem `ux-current.md`): lưu/mở/mới dự án `.mikmap` + cài đặt máy tách riêng,
undo/redo toàn app theo snapshot, Show Mode (`Tab`), phím tắt Composition, đổi tên layer/cột/clip, scrub Timeline,
ROI kéo được. Kiểm tự động: `mikmap --roundtrip <file>` (không cần cửa sổ). `Clip::style` cố định hình vẽ khi đổi tên.

**Vẫn còn giả/thiếu — dễ bị đánh giá cao hơn thực tế:** `G5` homography là
DLT+Hartley nhưng **chưa RANSAC** nên `[~]`; `B3` generator vẽ bằng CPU, chưa
phải shader GLSL; `A4` thumbnail là gradient tĩnh (cố ý — thumbnail động làm
deck tụt còn ~16s/khung); chưa có nguồn video/ảnh thật (`B1`), chưa có thread sensor thật (`G1`).
`I2` (lưu/mở dự án) **đã chạy** qua `src/project.cpp` (định dạng `.mikmap`, schema riêng của
`src/`) nhưng **vẫn chưa gọi `ProjectIO`/`.hexmap` của `engine/`**; cài đặt máy lưu ở
thư mục config OS, tách khỏi dự án.

**Việc còn lại để ghép engine thật vào `src/`** (chi tiết ở `README.md`): (1)
`ProjectIO`, (2) `core/calib/*` thay `calib.cpp` tự viết, (3) `Slice`+
`Warp*` thay `mapping.cpp`, (4) `io/*` thay radar mô phỏng trong `sensor.cpp`,
(5) `Composition`/`Layer`/`Clip`/`Transport` thay `app.h`.

Khi tick một mục hoặc báo "đã làm", grep đúng `src/*.cpp` để xác nhận,
đừng suy diễn từ tên file hay từ những gì `engine/core` đã hỗ trợ.

## Build & test nhanh

```bash
# engine (core+io+i18n) + unit test — build được ngay trên máy này, không cần GPU/GLFW
cmake -S . -B build && cmake --build build --target hexmap_tests -j
ctest --test-dir build --output-on-failure
```

```powershell
# app (src/) — Windows: cần MinGW/GLFW/.tools trong src/.tools/
cmake -S src -B src/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build src/build
```

```bash
# app (src/) — Linux/macOS: cần GLFW hệ thống (apt/brew), ImGui tự tải lúc
# configure. Đã build+chạy thật kiểm chứng (Xvfb) — xem README.md.
cmake -S src -B src/build && cmake --build src/build
```

Quy ước build/test/style mặc định ở `.claude/rules/tech-defaults.md`.

## Sổ rủi ro đáng nhớ (`architecture.md` §8)

- **R3 (Cao):** Azure Kinect DK đã EOL (8/2023) — khuyến nghị Orbbec Femto
  Bolt/Mega (SDK tương thích K4A).
- **R6 (TB):** Input lag máy chiếu (16–80ms) — bật "Low Latency/Fast Mode",
  tắt keystone nội bộ của máy chiếu.
- **R1 đã gỡ (bản cũ, nhánh legacy-oF-ui):** HAP 4K multi-layer đã spike-test
  thật, 1–5 luồng giữ đúng fps gốc. `src/` hiện tại **chưa** phát HAP/video
  thật (`B1` trong `features.md`).

## Đọc thêm theo việc đang làm

| Đang làm gì | Đọc |
|---|---|
| Quy trình làm việc, commit, nhánh, PR, hook | `.claude/rules/workflow.md` |
| Sửa UI ImGui trong `src/`, đối chiếu bản thiết kế | `.claude/rules/design.md` |
| Quy ước code/test/build C++ mặc định | `.claude/rules/tech-defaults.md` |
| Vì sao kiến trúc chia tầng thế này, toán học đầy đủ | `architecture.md` |
| Tình trạng tính năng thật của `src/` | `README.md` + `features.md` |
| Cần tham khảo/khôi phục bản engine oF cũ (313+ test, HAP thật) | nhánh git `legacy-oF-ui` |

## Việc KHÔNG được làm

- Không thêm include phá quy tắc phụ thuộc `engine/core` ← không ai,
  `engine/io` ← `core`, UI (`src/`) ← `core`[+`io`].
- Không viết `IWarp::inverse()` trả `Vec2` — phải là `bool` + tham số ra.
- Không chia sẻ trạng thái giữa sensor thread và render thread bằng mutex.
- Không áp `H_w`/`H_s` ở sensor thread — luôn áp ở nơi dùng (render thread).
- Không viết tay copy constructor liệt kê từng trường cho lớp có thành viên
  đa hình (dùng `clone()` bọc riêng).
- Không báo một mục trong `features.md` là xong dựa trên việc `engine/core`
  đã hỗ trợ — phải xác nhận `src/` thật sự GỌI tới, không chỉ link vào.
- Không tìm/dùng API `theme::*` trong `src/` — đó là API của bản cũ (nhánh
  `legacy-oF-ui`); `src/` dùng `namespace ui::` (`src/ui.h`).
- Không đưa file mô tả *máy này và người này* vào git
  (`.claude/CLAUDE.local.md`, `.claude/settings.local.json`).

## Sub-agent & hook riêng của repo

- `.claude/agents/researcher.md` — điều tra kiến trúc/tính năng trước khi sửa.
- `.claude/agents/reviewer.md` — review diff theo checklist riêng của dự án.
- `.claude/hooks/pre-commit.sh` / `pre-push.sh` — chạy tự động qua
  `.claude/settings.json` trước `git commit`/`git push`, có thể chặn.

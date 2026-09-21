# MikMap / HexMapping — bộ não dự án

> File này Claude Code tự nạp mỗi phiên làm việc trong repo. Nội dung dưới đây
> lấy trực tiếp từ `architecture.md` (kiến trúc + nguyên tắc bất di bất dịch),
> `newui/README.md` và `newui/features.md` (tình trạng THẬT của prototype giao
> diện mới) — ba tài liệu có thẩm quyền cao nhất khi có mâu thuẫn với suy đoán.
> Chi tiết theo từng mảng nằm ở `.claude/rules/`.

## Dự án là gì

Projection mapping engine kiểu Resolume (deck · layer · column) → composition
canvas ảo → slice có keystone/mesh warp → máy chiếu, kèm chuỗi ánh xạ ngược từ
sensor về toạ độ nội dung: **chạm vào vật thể thật, hiệu ứng nổ đúng chỗ đó**.
Repo tên `MikMap`, mã nguồn tên nội bộ `HexMapping`.

```
C++20 · openFrameworks 0.12.x · OpenGL 4.x · Dear ImGui · OpenCV (tuỳ chọn)
Target: Windows 10/11 · MSVC 2022
core/ build thuần bằng CMake — chạy được trên CI không cần GPU/oF.
```

## Quy tắc phụ thuộc BẤT KHẢ XÂM PHẠM (`architecture.md` §1)

```
core   ──▶ (không phụ thuộc ai)      C++20 THUẦN: math, model, calib, filter
io     ──▶ core                     thread sensor — không biết Slice/Layer tồn tại
render ──▶ core                     OpenGL + oF — chỉ đọc model, ghi lên GPU
ui     ──▶ core                     Dear ImGui — không chạm GL state
app    ──▶ core, io, render, ui     AppController — nơi DUY NHẤT 4 tầng gặp nhau
```

- `core/` không bao giờ `#include` `ofMain.h`, `<GL/...>`, `imgui.h`.
- `render/` không bao giờ gọi hàm của `ui/`.
- `io/` không bao giờ biết `Slice`, `Layer`, hay ma trận mapping tồn tại.
- Kiểm tự động: `tools/check_layering.ps1`; `.claude/hooks/pre-push.sh` chạy
  thêm một bản kiểm nhẹ bằng grep trước khi push.
- `core/` chỉ được phụ thuộc STL. Nếu cần OpenCV (RANSAC), đặt sau macro
  `HEXMAP_USE_OPENCV` kèm fallback DLT tự viết — để test vẫn build được ở
  môi trường tối giản (`architecture.md` §7).

## Chuỗi biến đổi toạ độ trung tâm (`architecture.md` §4)

```
p_content = H_w⁻¹ · H_s · p_sensor
```

`H_s` (calibration sensor, DLT+RANSAC) và `H_w` (keystone/warp của slice) là
**hai ma trận tách rời mãi mãi** — trộn chung là lỗi kiến trúc phổ biến nhất
của loại hệ thống này: mỗi lần tinh chỉnh mapping sẽ phải calibrate sensor
lại. Chuỗi đầy đủ có thêm tầng **Composition Canvas** (không gian ảo, độc lập
độ phân giải máy chiếu) ở giữa:

```
p_sensor → ×H_s → p_output → tìm slice chứa điểm (duyệt z-order, điểm đầu
tiên khớp thắng) → slice.warp.inverse() → UV cục bộ → ×inputRect → p_canvas
→ ×transform⁻¹ layer/clip → p_clip
```

Bốn ma trận trong chuỗi nên hợp nhất thành 1 `Mat3` mỗi slice, tính lại chỉ
khi `dirty` — rẻ hơn (1 phép nhân thay vì 4) mà không mất độ chính xác.

Mặt nạ bezier **không phải một phép biến đổi** — là hàm che alpha ở
`contentUV`, nên tự động đi theo `H_w` khi kéo lại keystone. `outputToContent()`
(dùng cho wizard calibration) **cố ý bỏ qua mặt nạ**; ngược lại
`Screen::hitTest` phải tôn trọng mặt nạ (điểm bị cắt = không có ánh sáng =
không được báo "chạm trúng").

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
ngoài". Mọi `IWarp` mới (kể cả Bezier F10) phải theo đúng chữ ký này ngay từ
đầu — thêm muộn nghĩa là viết lại cả `render/WarpGeometry` và `model/Slice`.

## Mô hình thread (`architecture.md` §3, §6)

- Mỗi sensor (Serial/OSC/Depth) là **1 thread riêng, được phép chặn** ở
  `read()`/`recvfrom()`/SDK — nhưng **không bao giờ chia sẻ dữ liệu với render
  thread bằng mutex**, chỉ qua `TripleBuffer` (state, được phép rơi frame cũ —
  vd vị trí ngón tay) hoặc `SpscRingBuffer` (event, KHÔNG được rơi — vd
  TOUCH_DOWN/UP).
- **Quyết định thiết kế cố ý:** sensor thread gửi toạ độ THÔ; render thread
  mới áp `H_w⁻¹·H_s`. Lý do: `H_w` đổi bất cứ lúc nào người dùng kéo góc, chia
  sẻ nó với sensor thread sẽ bắt buộc phải lock. *"Dữ liệu thô đi qua biên
  thread; ngữ nghĩa được áp dụng tại nơi sử dụng."*
- Độ trễ đo bằng `tCaptureNs` đóng dấu lúc thu thập, không đoán — PerfPanel
  hiển thị p99, không phải tính năng phụ.
- `SensorFrame`/`TouchPoint` là POD kích thước cố định (memcpy-able) — không
  `std::vector`, không con trỏ, không alloc trong hot path.

## 10 nguyên tắc bất di bất dịch (`architecture.md` §10)

1. Render thread không bao giờ lock, alloc, hay chạm đĩa.
2. `core/` không bao giờ include GL / oF / ImGui.
3. Mọi `IWarp` phải cài đặt được `inverse()`.
4. `H_s` và `H_w` là hai ma trận tách rời, mãi mãi.
5. Toạ độ thô đi qua biên thread; ngữ nghĩa áp dụng tại nơi sử dụng.
6. Độ trễ phải được ĐO, không được ĐOÁN.
7. Mọi tính năng phải chạy được với `MockSource` — không cần phần cứng thật.
8. Hình học người dùng vẽ ra sống ở không gian gắn với NỘI DUNG
   (`contentUV`/canvas), không phải với máy chiếu.
9. Thành viên khó sao chép thì bọc riêng (`WarpPtr::clone()`), không viết tay
   copy constructor liệt kê từng trường — sót một trường mới là mất dữ liệu
   lặng lẽ, không phải lỗi biên dịch (`Slice::mask` đã từng mất kiểu này).
10. (Ứng dụng thực tế của #6) PerfPanel là công dân hạng nhất của UI, không
    phải màn hình debug phụ.

## `newui/` — tình trạng THẬT, đừng nhầm với engine chính

`newui/` là bản dựng lại giao diện bằng **GLFW + Dear ImGui thuần** (không
openFrameworks), CMake riêng tự gom `../src/core` + `../src/io` (thật) vào
cùng target — nhưng **chưa gọi tới engine đó**:

- Mô hình dữ liệu giao diện hiện tại là **struct riêng trong `newui/src/app.h`**
  (deck/layer/clip/sensor tự viết lại), KHÔNG phải `core/model/Composition`
  thật.
- `core/` + `io/` đã biên dịch & link vào (31 đối tượng) nhưng UI chưa gọi.
- Kế hoạch ghép trọn (5 bước, `newui/README.md`): (1) `ProjectIO` thay lưu/mở
  tự viết, (2) `core/calib/*` thay `src/calib.cpp` tự viết (homography hiện
  tại chỉ là affine 2 tỉ lệ + dịch, chưa DLT/RANSAC thật), (3) `core/model/Slice`
  + `WarpCornerPin`/`WarpMesh`/`WarpBezier` thay `src/mapping.cpp`, (4) `io/*`
  thay radar mô phỏng trong `src/sensor.cpp`, (5) `core/model/Composition`/
  `Layer`/`Clip`/`Transport` thay mô hình deck trong `src/app.h`.
- Vì không đụng `src/` hay `HexMapping.vcxproj` gốc, nhánh này **không thể
  conflict** với engine chính — đây là lý do nó tồn tại song song.

**`newui/features.md` chấm điểm RIÊNG cho prototype này** (không phải cho
engine chính) — 18 mục `[x]` (hành vi thật) · 34 mục `[~]` (chỉ có UI/một phần,
xem "Ghi chú kiểm tra" cuối file để biết chính xác cái gì còn giả) · 83 mục
`[ ]`, trên tổng 135 mục Resolume-parity. Ví dụ đã bị đánh giá lại thấp hơn vẻ
ngoài: `A7`/`D4` blend mode chọn được 8 mode nhưng chỉ Add/Screen thật sự cộng
sáng; `G5` homography là affine tạm, chưa DLT+RANSAC; `B3` generator vẽ bằng
CPU, chưa phải shader GLSL; `A4` thumbnail là gradient tĩnh (thumbnail động
làm deck tụt còn ~16s/khung — cố ý không làm trong đợt này).

**Repo có HAI file `features.md` với tiêu chí khác nhau — đừng lẫn:**

| | Theo dõi | Độ hoàn thiện |
|---|---|---|
| `/features.md` (gốc) | Engine chính `src/` (oF thật, HAP thật) | Xa hơn nhiều — 313+ test xanh, đã có output thật |
| `/newui/features.md` | Prototype `newui/` (GLFW, model giả) | 18 xong / 34 một phần / 83 chưa |

Khi tick một mục hoặc báo "đã làm", **luôn nói rõ đang nói về track nào** và
grep đúng thư mục nguồn tương ứng (`src/` vs `newui/src/`) để xác nhận.

## Build & test nhanh

```bash
# core/ engine chính + unit test — build được ngay trên máy này, không cần oF/GPU
cmake -S . -B build && cmake --build build --target hexmap_tests -j
ctest --test-dir build --output-on-failure
```

```powershell
# newui/ prototype — chỉ Windows, cần MinGW/Ninja trong newui/.tools/
cmake -S newui -B newui/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build newui/build
```

App chính (`HexMapping.exe`, MSBuild) chỉ build đầy đủ trên Windows/MSVC. Quy
ước build/test/style mặc định ở `.claude/rules/tech-defaults.md`.

## Sổ rủi ro đáng nhớ (`architecture.md` §8)

- **R3 (Cao):** Azure Kinect DK đã EOL (8/2023) — trừu tượng hoá sau
  `DepthSource`, khuyến nghị Orbbec Femto Bolt/Mega (SDK tương thích K4A).
- **R6 (TB):** Input lag máy chiếu (16–80ms) có thể lấn át toàn bộ pipeline
  phần mềm — bật "Low Latency/Fast Mode", **tắt keystone nội bộ của máy
  chiếu** (dùng warp phần mềm thay thế).
- **R1 đã gỡ:** HAP 4K multi-layer đã spike-test thật (`tools/spike_hap/`),
  1–5 luồng giữ đúng fps gốc.

## Đọc thêm theo việc đang làm

| Đang làm gì | Đọc |
|---|---|
| Quy trình làm việc, commit, nhánh, PR, hook | `.claude/rules/workflow.md` |
| Sửa UI ImGui, đối chiếu bản thiết kế | `.claude/rules/design.md` |
| Quy ước code/test/build C++ mặc định | `.claude/rules/tech-defaults.md` |
| Vì sao kiến trúc chia tầng thế này, toán học đầy đủ | `architecture.md` |
| Sửa trong `newui/` | `newui/README.md` + `newui/features.md` (KHÔNG phải `/features.md` gốc) |
| Sửa engine chính `src/` | `/features.md`, `UI_UPDATE_PROGRESS.md`, `newui/SKILL.md` |

## Việc KHÔNG được làm

- Không thêm include phá quy tắc phụ thuộc core/io/render/ui.
- Không viết `IWarp::inverse()` trả `Vec2` — phải là `bool` + tham số ra.
- Không chia sẻ trạng thái giữa sensor thread và render thread bằng mutex.
- Không áp `H_w`/`H_s` ở sensor thread — luôn áp ở nơi dùng (render thread).
- Không viết tay copy constructor liệt kê từng trường cho lớp có thành viên
  đa hình (dùng `clone()` bọc riêng).
- Không báo một mục trong `newui/features.md` là xong dựa theo `/features.md`
  gốc hay ngược lại — hai file chấm điểm hai thứ khác nhau.
- Không đưa file mô tả *máy này và người này* vào git (`bin/data/settings.json`,
  `.vs/`, `imgui.ini`, `.claude/CLAUDE.local.md`, `.claude/settings.local.json`).

## Sub-agent & hook riêng của repo

- `.claude/agents/researcher.md` — điều tra kiến trúc/tính năng trước khi sửa.
- `.claude/agents/reviewer.md` — review diff theo checklist riêng của dự án.
- `.claude/hooks/pre-commit.sh` / `pre-push.sh` — chạy tự động qua
  `.claude/settings.json` trước `git commit`/`git push`, có thể chặn.

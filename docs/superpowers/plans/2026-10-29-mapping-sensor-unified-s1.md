# Trang Mapping gộp (S1: UI/UX và luồng) Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sensor được căn chỉnh ngay trong trang Mapping (đổi tên từ Advanced Mapping): sensor là một node trong cây Screen; tab INPUT/OUTPUT dùng chung với slice; nền là hình live; nút thêm sensor mở popup; mỗi sensor có chip tiến độ. Trang Sensor riêng bị bỏ.

**Architecture:**
- Thêm một loại lựa chọn mới, `selKind == 3` (sensor), vào mô hình lựa chọn sẵn có của Mapping.
- `Stage()` và `PropsPanel()` của `mapping.cpp` rẽ sang phần nội dung sensor hiện nằm trong `sensor.cpp`. Phần này được giữ nguyên logic và chỉ đổi chỗ gọi.
- Không đổi dữ liệu hay hành vi của sensor (đặt vị trí mm, 4 góc, OSC/UDP). Lưới, mask dữ liệu và gộp điểm thuộc S2.

**Tech Stack:** C++20, Dear ImGui + GLFW (`src/`), `--roundtrip` self-test (`MappingSelfTest`, `SensorSelfTest`), `--shot`.

**Spec:** `docs/superpowers/specs/2026-10-29-mapping-sensor-unified-design.md` (§5 và §7 phần S1). Plan này chỉ làm S1.

**Nền:** nhánh `feat/sensor-skeleton-hokuyo`, HEAD `d90a176`.

## Global Constraints

- Tab trên cùng còn đúng hai tab: `Composition · Mapping`. Tab "Sensor I/O" bị bỏ. Tab Effect chưa xuất hiện (S3).
- Tab **INPUT / OUTPUT dùng chung** cho slice và sensor, đổi nội dung theo node đang chọn trong cây. Không có tab SENSOR riêng.
- **Màu:**
  - Sensor đang chọn dùng đúng tone của trang như slice đang chọn: INPUT cyan (`pal::cyan`), OUTPUT coral (`pal::coral`).
  - Các sensor khác cùng screen vẽ **xám mờ** và không bấm được trên stage.
  - Icon và dòng sensor trong cây vẫn dùng `pal::mint`.
- Nền INPUT và OUTPUT của sensor là **hình live của screen** (`DrawScreenBackdrop`).
- Thêm sensor bằng **popup**: không đổi trang, không đổi stage.
- Hành vi của slice, mask và screen trong Mapping **không đổi**: so ảnh `--shot` trước và sau.
- `H_s` và `H_w` tách rời; render thread không lock, không cấp phát trong vòng lặp nóng, không IO.
- MSVC `/W4 /permissive- /fp:precise /utf-8`, 0 cảnh báo mới. Dùng helper `ui::` có sẵn (`.claude/rules/design.md`). Hex literal phải tách chuỗi sau escape nếu ký tự kế tiếp là chữ số hex.
- Lệnh build và test:
  ```
  cmake --build src/build --config Release -j
  ctest --test-dir src/build -C Release --output-on-failure
  ```
  Nếu đụng `engine/` thì chạy thêm bộ engine.
- Commit không có dòng attribution / Co-Authored-By.

## Review Focus

1. **Xoá sensor đang được chọn** (menu chuột phải → Remove): lựa chọn lùi về screen chứa nó, stage hiện phần screen, không crash, không còn trỏ index cũ. Test ở Task 1.
2. **Chọn sensor có screen đã bị xoá:** stage dùng screen dự phòng (`DeviceScreen`), Properties hiện cảnh báo vàng, cây vẫn hiện sensor dưới screen dự phòng. Test ở Task 1.
3. **Phím tắt của Mapping khi đang chọn sensor** (Delete, Ctrl+C/X/V/D, mũi tên nudge ở `main.cpp` ≈1940): không được xoá hay dịch slice và screen đang bị "chọn ngầm". Test ở Task 1.
4. **Mở bằng `--screen 2`, hoặc code cũ đặt `A.screen = 2`:** vào trang Mapping với sensor đầu tiên được chọn, không vẽ trang rỗng. Test ở Task 1.
5. **Bấm xuyên popup:** khi popup mở, click vào cây, stage hay Properties phía sau không có tác dụng. `Esc` ở bước 1–2 huỷ và không tạo thiết bị; ở bước 3 đóng nhưng giữ các thiết bị đã tạo. Test ở Task 3.

---

### Task 1: Một trang Mapping, sensor là một lựa chọn trong đó

**Files:**
- Modify: `src/app.h`:
  - `selKind` thêm giá trị `3 = sensor`;
  - bỏ `spage`, `sensorTool`, `sensorTreeCollapsed`, `sensorRail`;
  - bỏ `TreeMode::Sensors`, và bỏ tham số `TreeMode` của `DrawTree` / `BuildTree` nếu chỉ còn một mode.
- Modify: `src/main.cpp`:
  - tab bar ở dòng ≈109 còn `{"Composition"}, {"Mapping"}`;
  - `switch (A.screen)` ở ≈1987 bỏ `DrawSensor`;
  - cờ `--screen` / `--spage` / `--wizard`;
  - khối phím tắt ≈1918–1960.
- Modify: `src/mapping.cpp`:
  - `TreeRow` (≈1427, ≈1471): highlight và click của `SensorN`;
  - `BuildTree`, `TreePanel`, `RailPanel`, `RailPopover`: bỏ mode Sensors;
  - `OpenSensorWizard` (≈1530): không đặt `A.screen = 2`;
  - `Stage` (≈1945): tab dùng `A.mpage`, công cụ dùng `A.outTool`, rẽ sang sensor;
  - `PropsPanel` (≈2823): rẽ sang sensor.
- Modify: `src/sensor.cpp`:
  - bỏ `DrawSensor` và `StageBar`;
  - `SensorStage` chỉ còn vẽ vùng nội dung (bỏ thanh 44 px riêng);
  - `SensorProps`, `SensorOutputView` và `LidarPlacementView` dùng `A.mpage` / `A.outTool`.
- Modify: `src/sources.cpp` (`SensorSelfTest`): bỏ hoặc đổi các chỗ dùng `A.screen == 2` và `spage`.

**Interfaces:**
- Produces:
  ```cpp
  // app.h
  int selKind = -1;   // -1 auto, 0 screen, 1 slice, 2 mask, 3 sensor (A.selDevice; selSc = its screen)
  void SelectDevice(const std::string& deviceId);   // selKind=3, selDevice, selSc = DeviceScreen(d); clears selSl/selMk/mapMulti;
                                                    // A.screen = 1; wizardStep = 0; editRoi = false. Unknown id: no change.
  void SensorStageBody(ImRect area, Device& d);     // sensor.cpp: what was SensorStage minus its own tab bar (uses A.mpage)
  void SensorProps(ImRect r, Device* d);            // sensor.cpp: now non-static, called from PropsPanel
  bool MappingEditsSlices();                        // false while selKind == 3: Mapping shortcuts (delete/copy/paste/duplicate/nudge) do nothing
  ```
- `A.screen`:
  - chỉ còn 0 (Composition) và 1 (Mapping);
  - giá trị lớn hơn 1 (cờ `--screen 2`) được đổi thành 1, kèm `SelectDevice(A.devices[0].id)` nếu có thiết bị;
  - `--spage N` đặt `A.mpage = N`;
  - `--wizard N` mở trang Mapping cùng popup (popup được dựng ở Task 3; ở Task 1 cờ chỉ đặt trạng thái).
- `Stage()`, khi `MapKind() == 3`:
  - thanh 44 px giữ đúng các nút trang INPUT/OUTPUT với màu cyan/coral như cho slice;
  - nhóm công cụ Edit points / Transform hiện khi `A.mpage == 1`;
  - nhóm zoom điều khiển `A.sensorView[A.mpage]`;
  - vùng dưới thanh gọi `SensorStageBody`.

  Các nút chỉ dành cho slice (magnet, focus, fit…) ẩn đi khi đang chọn sensor.

- [ ] **Step 1: Ảnh trước khi đổi.** Chụp Mapping hai trang với slice được chọn vào scratchpad:
  ```
  mikmap --screen 1 --page 0 --frames 30 --shot <scratch>/before_in.png
  mikmap --screen 1 --page 1 --frames 30 --shot <scratch>/before_out.png
  ```
  Kiểm cờ thật trong `main.cpp`: `--vout` nuốt `--screen`.
- [ ] **Step 2: Viết các check thất bại** trong `SensorSelfTest` (`src/sources.cpp`):
  - `SelectDevice(A.devices[2].id)`: `A.MapKind() == 3`, `A.screen == 1`, `A.curDevice() == &A.devices[2]`, `A.selSc` là id của `DeviceScreen(A.devices[2])`, `!MappingEditsSlices()`.
  - Chọn lại một slice bằng đường chọn sẵn có của cây (hàm chọn slice mà `TreeRow` đang dùng; tách ra nếu cần): `A.MapKind() == 1`, `MappingEditsSlices()`.
  - **Review Focus 1:** `SelectDevice(id)` rồi xoá thiết bị đó bằng đúng đường Remove của menu cây (tách thành hàm `RemoveDevice(const std::string&)` nếu đang là lambda). Sau đó `A.MapKind() == 0`, `A.selSc` vẫn là screen cũ, `A.curDevice()` không trỏ tới thiết bị đã xoá.
  - **Review Focus 2:** thiết bị có `screenId` trỏ tới screen đã xoá. `SelectDevice` cho `A.selSc == A.screens[0].id`, và `DeviceScreen(d, &fb)` có `fb == true`.
  - **Review Focus 3:** với sensor đang chọn, gọi đúng hàm xử lý Delete / nudge của khối phím tắt Mapping (tách thành `MappingShortcut(ImGuiKey)` nếu cần để gọi được từ test). Số slice và `q` của slice đang chọn ngầm không đổi.
  - **Review Focus 4:** hàm xử lý cờ khởi động (tách `ApplyStartScreen(int)` nếu cần) với 2 cho `A.screen == 1 && A.MapKind() == 3`.
- [ ] **Step 3: Chạy test, thấy đỏ** (thiếu `SelectDevice` hoặc check thất bại).
- [ ] **Step 4: Cài đặt** theo Interfaces.
  - Click vào `SensorN` trong cây gọi `SelectDevice`; highlight khi `MapKind() == 3 && curDevice()->id == n.dv`.
  - Mọi chỗ còn đặt `A.screen = 2` đổi sang `SelectDevice` hoặc để nguyên `A.screen`.
  - Xoá code chết: `DrawSensor`, `StageBar`, mode Sensors của cây, `spage`, `sensorTool`, `sensorTreeCollapsed`, `sensorRail`.
  - Grep chứng minh không còn tham chiếu.
- [ ] **Step 5: Chạy test, thấy xanh.** Chụp lại `before_in` / `before_out` (slice được chọn) và đặt cạnh ảnh cũ: phải giống hệt, trừ nhãn tab "Mapping" và nội dung động. Chụp thêm `--screen 2 --spage 0` và `--spage 1`: trang Mapping, sensor được chọn, nội dung sensor nằm dưới thanh tab của Mapping. Mở các ảnh bằng Read và ghi lại đã thấy gì.
- [ ] **Step 6: Commit**
  ```
  git commit -m "mapping: sensors are selected and edited on the Mapping page; the separate Sensor page is gone"
  ```

### Task 2: Nền live và màu chọn trên stage sensor

**Files:**
- Modify: `src/app.h`: `StageXf` thêm `float sy = 0` (0 nghĩa là bằng `s`). `toPx` và `fromPx` dùng `sy > 0 ? sy : s` cho trục y.
- Modify: `src/stage.cpp`: `DrawScreenBackdrop` vẽ được với `sy` khác `s`.
- Modify: `src/sensor.cpp`: `LidarPlacementView`, `SensorOutputView`, `Radar`, `SkeletonView`.

**Interfaces:**
- Consumes: `SelectDevice`, `SensorStageBody` (Task 1).
- Produces:
  ```cpp
  uint32_t SensorSelTone(int page);   // page 0 -> pal::cyan, 1 -> pal::coral (the slice selection tone of that page)
  ```

**Cách vẽ:**
- **INPUT, lidar:** vẽ `DrawScreenBackdrop(*DeviceScreen(d), X, t, 1.f)` vào đúng khung screen tính bằng mm. `X.cv` là khung screen trên màn hình, `X.s = cv.width / 1920`, `X.sy = cv.height / 1080`, vì hình chiếu phủ kín bề mặt thật `realW × realH`. Lúc screen chưa có kích thước thật thì giữ ô nhập như hiện tại, không vẽ nền.
- **INPUT, Sim / Skeleton:** giữ view cố định hiện tại, không thêm nền.
- **OUTPUT:** đã dùng `DrawScreenBackdrop`; giữ nguyên.
- **Sensor đang chọn:** khung, tứ giác, biểu tượng sensor và tay nắm dùng `SensorSelTone(A.mpage)` thay cho `pal::mint`. Điểm scan và blob giữ màu hiện tại để phân biệt với khung.
- **Sensor khác cùng screen:**
  - vẽ trước sensor đang chọn, màu `pal::t66` alpha 0.45;
  - INPUT: biểu tượng tại vị trí đặt của chúng (lidar đã có `placed` hoặc có pose) và các điểm scan;
  - OUTPUT: tứ giác `calib` của chúng và các touch qua `DeviceH`;
  - không có hit-test.

- [ ] **Step 1: Test thất bại.**
  - `StageXf x; x.cv = ImRect(10, 20, 210, 120); x.s = 0.1f; x.sy = 0.05f;` → `x.toPx({1000, 1000}) == (110, 70)` và `x.fromPx(x.toPx({123, 456}))` ≈ `(123, 456)`.
  - Khi `sy == 0` thì `toPx` giống hệt cách cũ: `x.sy = 0` → `toPx({1000, 1000}) == (110, 120)`.
  - `SensorSelTone(0) == pal::cyan`, `SensorSelTone(1) == pal::coral`.

  Đặt trong `MappingSelfTest`.
- [ ] **Step 2: Chạy, thấy đỏ. Step 3: Cài. Step 4: Chạy, thấy xanh.**
- [ ] **Step 5: Ảnh kiểm.** Dự án có 2 lidar trên cùng screen (dùng thiết bị Sim/Hokuyo giả mà self-test và `--wizard 3` đã tạo, hoặc thêm cờ test nếu cần). Chụp INPUT và OUTPUT với một lidar được chọn.
  - Thấy được: hình live dưới khung screen mm; lidar chọn màu cyan (INPUT) hoặc coral (OUTPUT); lidar kia xám mờ.
  - Chụp lại ảnh slice của Task 1 để chắc Mapping không đổi.
- [ ] **Step 6: Commit**
  ```
  git commit -m "mapping: sensors are aligned over the live picture; the selected one takes the page's selection tone, the others go grey"
  ```

### Task 3: Popup thêm sensor

**Files:**
- Modify: `src/sensor.cpp`:
  - `SensorWizardView` vẽ trong một khung modal giữa cửa sổ, thay cho vùng stage;
  - thêm `DrawSensorWizardPopup()`;
  - `SensorWizardDone`;
  - thêm `CloseSensorWizard`.
- Modify: `src/mapping.cpp`: `OpenSensorWizard` chỉ mở popup.
- Modify: `src/main.cpp`: gọi `DrawSensorWizardPopup()` sau khi vẽ trang, ở mọi trang. Áp `g.blocked` (cơ chế chặn click sẵn có của `ui::`) cho mọi thứ vẽ trước nó khi popup mở. `--wizard N` mở popup trên trang Mapping.
- Modify: `src/sources.cpp`: các check wizard trong `SensorSelfTest`.

**Interfaces:**
- Consumes: `SelectDevice` (Task 1).
- Produces:
  ```cpp
  void DrawSensorWizardPopup();   // no-op when A.wizard.step == 0; otherwise a centred modal (dim the window behind, block it)
  void CloseSensorWizard();       // step 1-2: cancel (no device made; a running scan is left to be joined later, as today);
                                  // step 3: close, keep every device already made
  // SensorWizardDone(): closes, SelectDevice(first added id), A.mpage = 0
  ```

**Bố cục popup:**
- Khung khoảng 720×520 (thu nhỏ theo cửa sổ, giữ lề 40 px), bo góc 6, nền `pal::g14`, viền `pal::g2a`.
- Đầu khung có tiêu đề "Add sensor" và nút `X` (gọi `CloseSensorWizard`). `Esc` cũng gọi `CloseSensorWizard`.
- Bên trong là đúng ba bước hiện tại, bố cục đổi theo chiều rộng khung. Danh sách kết quả quét vẫn cuộn được; mục "already added" giữ như cũ.

- [ ] **Step 1: Test thất bại.**
  - `OpenSensorWizard(A.screens[0].id)` → `A.screen` không đổi (đặt 0 trước, sau vẫn 0), `A.wizard.step == 1`.
  - Bước 1 rồi `CloseSensorWizard()`: `step == 0`, số thiết bị không đổi.
  - Đi tới bước 3 với một thiết bị (đường self-test wizard sẵn có) rồi `CloseSensorWizard()`: `step == 0`, thiết bị còn đó.
  - `SensorWizardDone()`: `A.MapKind() == 3`, `A.curDevice()->id` là thiết bị vừa thêm, `A.mpage == 0`, `A.screen == 1`.
  - Sửa các check wizard cũ đang kỳ vọng `A.screen == 2`.
- [ ] **Step 2: Chạy, thấy đỏ. Step 3: Cài. Step 4: Chạy, thấy xanh.**
- [ ] **Step 5: Ảnh kiểm.** `--wizard 1`, `--wizard 2`, `--wizard 3` (chạy cả trên `--screen 0` để thấy popup nổi trên trang Composition). Popup nằm giữa, phía sau mờ, trang phía sau không đổi. Ghi rõ là click xuyên popup và `Esc` chỉ kiểm bằng test, chưa kiểm bằng chuột thật.
- [ ] **Step 6: Commit**
  ```
  git commit -m "sensor: adding a sensor is a popup over the page you are on"
  ```

### Task 4: Chip tiến độ của sensor trong cây

**Files:**
- Modify: `src/app.h`:
  - `Device` thêm `double lastSentAt = -1;` (runtime, giây theo `ImGui::GetTime()`, không serialize, không có trong snapshot undo);
  - thêm `SensorProgress`, `GoToSensorStep`.
- Modify: `src/sources.cpp`: `SendOut` ghi `d.lastSentAt` khi `push` thành công. `SendOut` hiện nhận `const Device&`; đổi để ghi được, hoặc ghi ở chỗ gọi trong `SensorsTick`.
- Modify: `src/mapping.cpp`: `TreeRow` của `SensorN` vẽ chip 4 chấm bên phải dòng, thay chỗ meta fps; fps chuyển vào tooltip.
- Modify: `src/sensor.cpp`: `GoToSensorStep` mở đúng mục Properties (dùng `sensorSecOpen` sẵn có).

**Interfaces:**
- Produces:
  ```cpp
  // number of steps reached, 0..4, in order; a step only counts when every earlier one does:
  // 1 connected  = enabled && (kind == Sim || status == Running) && error.empty()
  // 2 placed     = !IsLidar(d) || d.placed || d.calibrated      (Custom 4-point counts as placed)
  // 3 aligned    = d.calibrated
  // 4 sending    = d.outOn && d.lastSentAt >= 0 && now - d.lastSentAt <= 2.0
  int  SensorProgress(const Device& d, double now);
  void GoToSensorStep(const std::string& deviceId, int step);   // SelectDevice, then: 1 -> mpage 0 + open Connection;
                                                                // 2 -> mpage 0; 3 -> mpage 1; 4 -> mpage 1 + open Send out
  ```
- Chip có 4 chấm đường kính 5 px, cách nhau 4 px:
  - chấm đã đạt tô `pal::mint`;
  - chấm đầu tiên chưa đạt viền `pal::yellow`, hoặc `pal::red` nếu thiết bị đang lỗi;
  - các chấm còn lại viền `pal::t44`.
- Di chuột lên chip hiện tooltip `"Next: Connect" / "Next: Place" / "Next: Align" / "Next: Send out" / "Ready"` cùng fps.
- Bấm vào chip gọi `GoToSensorStep(id, SensorProgress + 1)`; khi đủ 4 thì chỉ chọn sensor.

- [ ] **Step 1: Test thất bại** (bảng trong `SensorSelfTest`, `Device` dựng tay, `now = 100.0`):

  | Device (dựng tay) | `SensorProgress(d, 100.0)` phải bằng |
  |---|---|
  | Hokuyo đang lỗi | 0 |
  | Hokuyo Running, `placed = false`, `calibrated = false` | 1 |
  | như trên với `placed = true` | 2 |
  | thêm `calibrated = true` | 3 |
  | thêm `outOn = true`, `lastSentAt = 99.0` | 4 |
  | như trên với `lastSentAt = 97.5` | 3 |
  | Sim, `calibrated = true` | 3 |
  | Hokuyo `placed = false`, `calibrated = true` (Custom) | 3 |

  Thêm hai check:
  - `GoToSensorStep(id, 3)` → `A.mpage == 1` và `A.MapKind() == 3`;
  - chạy `SensorsTick` hai lần không làm `ProjectDirty()` thành true (giữ guard đã có).
- [ ] **Step 2: Chạy, thấy đỏ. Step 3: Cài. Step 4: Chạy, thấy xanh.**
- [ ] **Step 5: Ảnh kiểm.** Chụp cây Mapping có ít nhất 3 sensor ở các bước khác nhau (Sim đã căn, Hokuyo giả đã kết nối nhưng chưa đặt vị trí, một thiết bị lỗi). Chip hiển thị đúng như bảng trên. Mở ảnh bằng Read.
- [ ] **Step 6: Commit**
  ```
  git commit -m "tree: each sensor shows how far its setup has got, and a click on the chip goes to the next step"
  ```

### Task 5: Tài liệu

**Files:** `features.md`, `README.md`, `.claude/CLAUDE.md`, `.claude/rules/design.md`.

- [ ] **Step 1:** Grep `src/*.cpp` xác nhận `SelectDevice`, `SensorStageBody`, `DrawSensorWizardPopup`, `SensorProgress`, `GoToSensorStep` đều được gọi trên đường chạy của app.
- [ ] **Step 2:** Sửa tài liệu.
  - Đổi mọi chỗ mô tả "Advanced Mapping" / "Sensor I/O" thành trang **Mapping** (hai tab).
  - Dòng G24 trong `features.md` mô tả trang gộp; ghi chú kiểm tra nói rõ phần nào chỉ kiểm bằng ảnh hoặc self-test.
  - Bảng "Thao tác chuẩn" của `design.md` thêm: chọn sensor trong cây; chip tiến độ; nút `Sensor` mở popup; `Esc` đóng popup.
  - Mục "Bố cục" của `design.md` đổi từ ba trang thành `Composition · Mapping` (cùng cửa sổ Cài đặt), Effect ghi là kế hoạch sau.
- [ ] **Step 3: Commit**
  ```
  git commit -m "docs: one Mapping page for projector and sensor setup"
  ```

## Kiểm cuối

- Hai bộ test xanh, chạy mỗi bộ hai lần; 0 cảnh báo mới.
- Ảnh Mapping với slice được chọn trước và sau giống nhau.
- Luồng §5.7 của spec đi được trên ảnh: popup → sensor xuất hiện trong cây → INPUT → OUTPUT → Send out, không rời trang Mapping. Phần chưa thử bằng chuột thật phải ghi rõ.
- Agent `reviewer` của repo duyệt toàn nhánh.

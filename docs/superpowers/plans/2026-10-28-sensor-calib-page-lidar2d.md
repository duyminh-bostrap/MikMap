# Trang Sensor kiểu Advanced Output + Lidar 2D theo mm — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Sensor thuộc Screen (như slice), trang Sensor dựng lại theo bố cục 3 cột của Advanced Mapping (cây thu gọn được · stage Input/Output · Properties), có wizard thêm sensor (chọn loại → kết nối/scan IP → kiểm tín hiệu), và lidar 2D được calib bằng cách đặt vị trí thật theo mm trên screen, có blob track và gửi toạ độ chuẩn hoá 0–1 qua OSC/UDP.

**Architecture:**
- **Calib sensor vẫn là `H_s` từ 4 cặp điểm** (`Device::calib`, giữ nguyên).
- **Tab Output** của trang Sensor chỉnh 4 đích `tx,ty` bằng chính các mảnh stage được tách ra từ Advanced Mapping: khung pan/zoom, nền screen + slice, hộp Transform. Không viết lại.
- **Đặt lidar theo mm** là một phép hình học thuần (`engine/core/calib/LidarPlacement`). Nó **ghi ra** 4 cặp `calib`, nên mọi chỗ đang dùng `DeviceH` (overlay, capture, gửi OSC) chạy tiếp mà không đổi gì.
- **Gửi toạ độ ra ngoài** đi qua một thread riêng (`engine/io/net/TouchSender`): render thread chỉ đẩy một gói POD vào ring.

**Tech Stack:** C++20, Dear ImGui + GLFW (`src/`), engine STL thuần, `tests/TestHarness.h`.

**Spec:** Chưa có file spec. Yêu cầu của người dùng (2026-10-28) cùng các câu trả lời đã chốt:
- Cột 1 Advanced Mapping: *"Mỗi Screen sẽ có nhiều sensor như slice · Thêm nút add sensor, bấm sẽ chuyển qua màn hình sensor"*.
- Trang Sensor: *"bố cục như advance output bao gồm 3 phần"*. Cột 1 thu gọn được, nút thêm sensor *"tương tự nút thêm screen và slice nhưng khác màu"*. Cột 2: wizard *"chọn loại sensor … check tín hiệu, nếu là sensor kết nối qua ip thì sẽ cần scan ip, có thể chọn nhiều sensor để add cùng 1 lúc"*; lidar 2D *"input: kéo thả vị trí, xoay sensor như vị trí thực tế (mm) so với screen được chọn, sau đó bật blob track"*, *"output: transform edit point … như advanced output (dùng chung function không viết lại)"*. Cột 3 lidar: *"Chọn screen, kích thước thực tế (mm) của screen · toạ độ vị trí thực tế của sensor, góc xoay, giới hạn đầu vào (mm) · IP, port, reconnect, active · blob track · Truyền toạ độ điểm qua OSC hoặc UDP"*.
- Đã chốt: lidar dùng cho **cả tường chiếu lẫn sàn** (cùng một phép hình học 2D trong mặt phẳng quét); toạ độ gửi ra **chuẩn hoá 0–1 theo screen**; plan này gồm **khung + Lidar 2D**.
- Để các plan sau: camera 2D (crop kiểu iPhone Straighten/Dọc/Ngang, các mode nhận dạng, luồng ảnh xem trước, NDI/Spout); lidar 3D / depth camera.

**Nền:** làm tiếp trên nhánh `feat/sensor-skeleton-hokuyo`, đã có thiết bị thật (`Device`, `DevKind::{Sim,Skeleton,Hokuyo,Plugin}`, `SensorsTick`, `ScanSource`, `LidarParams`, `HokuyoSource`, plugin DLL). Nên chạy xong review cuối của plan cũ (`2026-10-27-sensor-skeleton-hokuyo.md`) trước khi bắt đầu.

## Global Constraints

- Quy tắc phụ thuộc giữ nguyên: `engine/core` chỉ STL; `engine/io` chỉ STL + `engine/core` + API mạng/thư viện động của hệ điều hành; không biết `Slice`/`Layer`/`Screen`/ImGui.
- Render thread không lock / alloc trong vòng lặp nóng / IO. Sensor thread ↔ render thread chỉ qua `TripleBuffer`/`SpscRingBuffer`. Gửi UDP ra ngoài chỉ từ thread của `TouchSender`.
- Toạ độ sensor đi qua biên thread vẫn thô (lidar: mm, x = phía trước, y = bên trái của sensor). `H_s` và phép đặt vị trí chỉ áp ở render thread.
- Không gian output của stage là **1920×1080** (`StageSpace`, `mapping.cpp`). `calib[i].tx,ty` luôn ở không gian này, trên screen mà thiết bị thuộc về.
- Màu: sensor = `pal::mint` (đã là màu sensor trong app), screen = coral, slice = cyan, mask = yellow.
- Toạ độ gửi ra: `x = ox / 1920`, `y = oy / 1080` (không kẹp, điểm ngoài screen vẫn gửi).
- MSVC `/W4 /permissive- /fp:precise /utf-8`, 0 cảnh báo mới. Engine thêm gì thì có test trong `tests/`. `src/` có test ở `--roundtrip` (`SensorSelfTest`, `MappingSelfTest`).
- Không thêm dependency ngoài.
- Cập nhật `features.md` chỉ sau khi grep `src/*.cpp` thấy code được gọi thật.

## Review Focus

1. **Screen bị xoá trong Advanced Mapping mà sensor còn trỏ tới:** kỳ vọng sensor chuyển về screen đầu tiên, card báo cảnh báo, không mất calib, không crash. Test ở Task 1.
2. **Chưa nhập kích thước thật của screen (0 mm) mà đã đặt lidar:** kỳ vọng stage hiện ô nhập kích thước, `PlacementCalib` trả false, không ghi calib NaN/vô hạn. Test ở Task 2 và Task 6.
3. **Đã chỉnh tay 4 điểm ở tab Output rồi kéo sensor ở tab Input:** kỳ vọng calib tay **không** bị ghi đè âm thầm. Thiết bị chuyển sang chế độ "Custom (4-point)"; muốn áp lại vị trí thì phải bấm "Re-apply placement". Test ở Task 6.
4. **Scan IP trên máy có card mạng /16 hoặc nhiều card:** kỳ vọng từ chối dải > 1024 host kèm thông báo, và scan chạy trên thread riêng nên UI không đứng. Test ở Task 7.
5. **Đích gửi OSC/UDP sai hoặc không tới được:** kỳ vọng render thread không bao giờ chặn; ring đầy thì bỏ gói cũ nhất và đếm số gói bị bỏ; card hiện lỗi. Test ở Task 8.

## Ngoài phạm vi

Camera 2D mới (crop/perspective, Face/Hand/Gesture, độ phân giải/zoom/focus, xuất hình ra Screen/NDI/Spout), lidar 3D / depth camera, hàm `discover` trong ABI plugin (plugin vẫn thêm tay qua JSON), chọn hệ toạ độ gửi ra theo từng sensor, vẽ sensor lên cửa sổ máy chiếu thật (vẫn chỉ là overlay debug G13 như cũ).

---

### Task 1: Sensor thuộc Screen + kích thước thật của Screen

**Files:**
- Modify: `src/app.h` (`Device`, `Screen`, khai báo mới)
- Modify: `src/project.cpp` (serialize, migrate, `UndoApply` không cần đổi vì là trường cấu hình)
- Modify: `src/calib.cpp` (`DeviceScreen`)
- Modify: `src/mapping.cpp` (overlay G13 ở stage Output: vẽ touch của thiết bị trên **screen của nó**, không còn cố định `A.screens[0]`)
- Modify: `src/output.cpp` (overlay trên cửa sổ output: tương tự)
- Modify: `src/main.cpp` (`--roundtrip`)

**Interfaces:**
- Produces:
  ```cpp
  struct Screen { /* … */ float realW = 0, realH = 0; };   // mm của bề mặt chiếu thật; 0 = chưa nhập
  struct Device { /* … */ std::string screenId; };        // Screen::id; rỗng hoặc không tìm thấy ⇒ screen đầu tiên
  Screen* DeviceScreen(const Device& d, bool* fellBack = nullptr);   // không bao giờ nullptr khi A.screens không rỗng
  ```
- Schema: `Screen` thêm `"realW"`, `"realH"`; device thêm `"screen"`. File cũ thiếu `"screen"` ⇒ `screenId` = id của screen đầu tiên.
- Nếu `fellBack` thì card thiết bị ở trang Sensor hiện dòng màu yellow: `"Screen not found — using <tên>"`.

- [ ] **Step 1: Viết check thất bại trong `--roundtrip`**
  - Gán `A.screens[1].realW = 4000; A.screens[1].realH = 2250;` và `A.devices[2].screenId = A.screens[1].id` → lưu → nạp → hai giá trị mm còn nguyên, `screenId` còn nguyên.
  - File cũ không có `"screen"` → `A.devices[0].screenId == A.screens[0].id`.
  - `deleteScreen(A.screens[1].id)` → `bool fb; DeviceScreen(A.devices[2], &fb) == &A.screens[0] && fb`, `A.devices[2].calibrated` vẫn giữ giá trị cũ.
- [ ] **Step 2: Chạy, xác nhận thất bại:** `cmake --build src/build --config Release -j && ctest --test-dir src/build -C Release --output-on-failure`. Expected: lỗi biên dịch hoặc `project_roundtrip` FAIL.
- [ ] **Step 3: Cài.** Overlay ở `mapping.cpp`/`output.cpp` đổi điều kiện `sc == &A.screens[0]` thành `DeviceScreen(d) == sc`. Thêm hai ô `IntField` "Real width / height (mm)" vào Properties của Screen ở Advanced Mapping, đặt cạnh kích thước pixel.
- [ ] **Step 4: Chạy lại, xác nhận đạt.**
- [ ] **Step 5: Commit** `git commit -m "sensor: devices belong to a screen; screens know their real size in mm (lidar placement needs both)"`

### Task 2: Phép đặt lidar theo mm (engine, thuần)

**Files:**
- Create: `engine/core/calib/LidarPlacement.h`, `engine/core/calib/LidarPlacement.cpp`
- Create: `tests/test_lidarplacement.cpp`
- Modify: `src/calib.cpp`, `src/app.h` (`PlacementCalib`)

**Interfaces:**
- Produces:
  ```cpp
  // Bề mặt (tường hoặc sàn) trong mm: gốc ở góc trên-trái screen, x sang phải, y xuống dưới.
  // angleDeg = hướng "phía trước" của lidar trên bề mặt: 0 = +x, tăng theo chiều kim đồng hồ (vì y hướng xuống).
  // mirror = lidar lắp úp ngược (trục "trái" của sensor bị lật).
  struct LidarPose { double x = 0, y = 0, angleDeg = 0; bool mirror = false; };
  Vec2 lidarToSurface(const LidarPose& p, const Vec2& sensorMm);   // F = (cos a, sin a); L = mirror ? (-sin a, cos a) : (sin a, -cos a); out = P + x·F + y·L
  Vec2 surfaceToLidar(const LidarPose& p, const Vec2& surfaceMm);  // nghịch đảo: d = S − P; x = d·F; y = d·L
  // src/
  bool PlacementCalib(Device& d, const Screen& sc);   // false (không đổi d) nếu sc.realW <= 0 || sc.realH <= 0.
       // Ghi 4 cặp: tx,ty = (0,0) (1920,0) (1920,1080) (0,1080); mx,my = surfaceToLidar của 4 góc screen
       // (0,0) (realW,0) (realW,realH) (0,realH); calibrated = true.
  ```

- [ ] **Step 1: Viết test thất bại** (`tests/test_lidarplacement.cpp`) với `LidarPose{2000, 2250, -90, false}` (lidar ở giữa mép dưới một tường 4000×2250 mm, quay mặt lên):
  - `lidarToSurface(p, {1000, 0})` ≈ `(2000, 1250)`.
  - `lidarToSurface(p, {1000, 500})` ≈ `(1500, 1250)`.
  - Với `mirror = true`: `{1000, 500}` → `(2500, 1250)`.
  - Khứ hồi `surfaceToLidar(p, lidarToSurface(p, v)) ≈ v` (±1e-9) cho 3 pose ngẫu nhiên cố định seed.
- [ ] **Step 2: Chạy, xác nhận thất bại:** `cmake --build build --config Release --target mikmap_tests -j && ctest --test-dir build -C Release --output-on-failure`.
- [ ] **Step 3: Cài `LidarPlacement.cpp`; cài `PlacementCalib` trong `src/calib.cpp`.** Thêm check `--roundtrip`:
  - `PlacementCalib` với screen `realW = 0` trả false và calib không đổi.
  - Với 4000×2250 và pose ở trên: áp `DeviceH` cho điểm sensor `(1000, 0)` ra ≈ `(960, 1080·1250/2250)` px (±0.5).
- [ ] **Step 4: Chạy lại cả hai bộ test, xác nhận đạt.**
- [ ] **Step 5: Commit** `git commit -m "calib: place a lidar by its real position on the screen in mm — it writes the 4 calibration pairs, so H_s users need no change"`

### Task 3: Giới hạn tầm + bật/tắt blob, theo từng thiết bị

**Files:**
- Modify: `engine/io/LidarBlobs.{h,cpp}`, `tests/test_lidarblobs.cpp`
- Modify: `src/app.h` (`Device`), `src/project.cpp`, `src/sources.cpp` (đẩy `setParams` từ thiết bị)

**Interfaces:**
- Produces:
  ```cpp
  struct LidarParams { /* … */ double maxRangeMm = 0; bool trackBlobs = true; };   // 0 = không giới hạn; false ⇒ 0 blob, scan vẫn publish
  struct Device { /* … */ LidarPose pose; bool placed = false; float rangeMm = 0;
                  bool blobOn = true; float blobMinMm = 15, blobMaxMm = 600, blobGapMm = 50; };
  ```
- Schema device: `"pose":{"x","y","a","mirror"}`, `"placed"`, `"range"`, `"blob":{"on","min","max","gap"}`. `sources.cpp` dựng `LidarParams` từ thiết bị: ROI = `d.roi`, các ngưỡng blob, `maxRangeMm = d.rangeMm`. Global `A.blobSize`/`A.noise` chỉ còn dùng cho Sim.

- [ ] **Step 1: Test thất bại**
  - `scanWith` (helper đã có trong test) có vật ở (1000, 0) và `maxRangeMm = 800` → 0 blob; `maxRangeMm = 1500` → 1 blob.
  - `trackBlobs = false` → 0 blob.
  - `--roundtrip`: các trường mới khứ hồi đúng; `ProjectDirty()` không bật khi chỉ chạy `SensorsTick`.
- [ ] **Step 2: Chạy, thấy đỏ.** **Step 3: Cài.** **Step 4: Chạy, thấy xanh.**
- [ ] **Step 5: Commit** `git commit -m "lidar: range limit and blob tracking switch per device"`

### Task 4: Tách các mảnh stage dùng chung ra khỏi Advanced Mapping (hành vi Mapping không đổi)

Đây là phần "dùng chung function không viết lại". Mảnh nào trong `Stage()` của `mapping.cpp` mà trang Sensor cần thì được **chuyển** (không chép) sang `src/stage.cpp`, và Mapping gọi lại chính nó.

**Files:**
- Create: `src/stage.cpp`
- Modify: `src/app.h` (`RectXf` chuyển từ `mapping.cpp` sang đây, kèm các khai báo bên dưới)
- Modify: `src/mapping.cpp`, `src/project.cpp` (chỗ đang đọc `A.mapZ`…)

**Interfaces:**
- Produces:
  ```cpp
  struct StageView { float z = 1, scrollX = 0, scrollY = 0, cx = 0.5f, cy = 0.5f; struct { bool valid = false; float z = 1, cx = 0.5f, cy = 0.5f; } req;
                     void setZoom(float z_); void setZoom(float z_, float cx_, float cy_); };
  struct StageXf { ImRect cv; float s = 1; ImVec2 toPx(ImVec2 p) const; ImVec2 fromPx(ImVec2 px) const; };
  // Chuyển nguyên khối pan/zoom của Stage(): từ `float baseW = StageBaseW(area);` tới hết `auto toPx = …`
  // (wheel quanh con trỏ, Shift+wheel, wheel ngang, kéo chuột phải/hand tool, cuộn khi kéo sát mép, kẹp −4000..8000).
  StageXf StageViewUpdate(StageView& v, ImRect area, float SW, float SH, float baseW, bool handTool, bool edgeScroll, bool* rClick);
  RectXf BoxOfPoints(const ImVec2* p, int n, float rotDeg);                         // lõi của OutputBox
  void   MapBoxPoints(ImVec2* p, int n, const RectXf& from, const RectXf& to);      // lõi của lambda trong ApplyBox (kẹp −4000..8000)
  void   DrawScreenBackdrop(const Screen& sc, const StageXf& x, float t, float alpha);   // khung 1920×1080 + mọi slice hiện của screen, vẽ như trang Output của Mapping
  // Edit Points: 4 ô vuông lớn kéo được (Shift = khoá trục). Transform: hộp có hướng quanh 4 điểm — dời / co giãn / xoay
  // (Shift: khoá trục, giữ tỉ lệ, bước 45°). Trả true ở frame thả chuột sau khi có thay đổi (để pushHist).
  bool   EditQuad(const char* id, const StageXf& x, ImVec2 q[4], int tool, uint32_t tone);
  ```
- `App::mapZ/mapScrollX/mapScrollY/mapCx/mapCy/mapReq/setZoom` gộp thành `StageView mapView` (khoảng 28 chỗ tham chiếu).
- Khối vẽ và kéo hộp Transform của slice trong `Stage()` được chuyển vào nhánh Transform của `EditQuad`, và Mapping gọi `EditQuad` cho slice lưới. **Ngoại lệ được phép:** nếu khối đó dính chặt vào trạng thái riêng của slice (bezier, mesh, chọn nhiều slice) tới mức tách ra sẽ đổi hành vi Mapping, thì giữ nguyên khối đó trong Mapping, và để `EditQuad` dùng chung phần toán `BoxOfPoints`/`MapBoxPoints`. Báo lại trong report là đã đi nhánh nào.

- [ ] **Step 1: Ghi nhận trạng thái trước khi đổi.** Chụp `mikmap --vout --screen 1 --frames 30 --shot before_out.png` và `--shot before_in.png` (trang Input) vào scratchpad. Thêm vào `MappingSelfTest` một check: `BoxOfPoints` của 4 góc slice mặc định bằng `OutputBox` cũ (±1e-3) trước khi xoá `OutputBox`.
- [ ] **Step 2: Chuyển code** theo Interfaces. Không đổi một con số hay một hằng nào trong khối được chuyển.
- [ ] **Step 3: Kiểm.** `ctest --test-dir src/build -C Release` xanh (`MappingSelfTest` gồm cả Transform). Chụp lại hai ảnh, so với ảnh trước: phải giống nhau (bằng mắt, ảnh đặt cạnh nhau).
- [ ] **Step 4: Commit** `git commit -m "stage: pan/zoom view, box math and quad editing move out of Mapping so the Sensor page uses the same code"`

### Task 5: Cây có Sensor (Mapping + trang Sensor), nút "+ Sensor" màu mint

**Files:**
- Modify: `src/mapping.cpp` (`Node`, `TreeRow`, `BuildTree`, `TreePanel`, `RailPanel`, `RailPopover`)
- Modify: `src/app.h` (`TreeMode`, `DrawTree`, `App::sensorTreeCollapsed`, `App::sensorRail`)
- Modify: `src/sensor.cpp` (cột 1 gọi `DrawTree`)

**Interfaces:**
- Produces:
  ```cpp
  enum class TreeMode { Mapping, Sensors };
  void DrawTree(ImRect r, TreeMode m);   // panel đầy đủ, hoặc rail 46 px khi cờ thu gọn của trang đó bật
  void OpenSensorWizard(const std::string& screenId);   // A.screen = 2; wizard bước 1 với screen đã chọn sẵn (Task 7)
  ```
- `Node` thêm `SensorN` (icon `radar` cho lidar, `camera` cho Skeleton, `activity` cho Sim; màu mint; meta = `fps` khi Running hoặc `ERR`).
  - Mode Mapping: sensor là con của Screen, nằm sau các slice.
  - Mode Sensors: Screen → sensor; slice gom thành một dòng mờ `"N slices"`, không bấm được.
- Bấm node sensor ở Mapping ⇒ `A.screen = 2`, `A.selDevice` = thiết bị đó. Chuột phải ⇒ menu Enable/Disable · Rename · Move to screen › · Remove.
- Footer: Mapping có 3 nút `Screen` (coral), `Slice`, `Sensor` (mint, viền mint); trang Sensor chỉ có `Sensor` (mint). Nút `Sensor` gọi `OpenSensorWizard(A.selSc hoặc screen của thiết bị đang chọn)`.
- Rail của trang Sensor: icon screen như Mapping; popover liệt kê sensor của screen đó. Cờ thu gọn riêng `A.sensorTreeCollapsed`.

- [ ] **Step 1: Check thất bại trong `SensorSelfTest`:** `BuildTree` (đưa ra ngoài `static` để test) ở mode Mapping chứa đúng 1 node `SensorN` dưới screen của `A.devices[2]` sau khi gán `screenId`; ở mode Sensors không chứa `SliceN`.
- [ ] **Step 2: Chạy đỏ. Step 3: Cài. Step 4: Chạy xanh**, rồi chụp `--screen 1` và `--screen 2`: cây có dòng sensor mint, footer có nút mint.
- [ ] **Step 5: Commit** `git commit -m "tree: sensors live under their screen in Mapping and on the Sensor page; + Sensor in mint"`

### Task 6: Trang Sensor 3 cột — stage Input/Output và Properties

**Files:**
- Modify: `src/sensor.cpp` (`DrawSensor` dựng lại; `Radar`/`SkeletonView` giữ cho Sim/Skeleton ở tab Input; thêm `LidarPlacementView`, `SensorOutputView`, `SensorProps`; `Routing` chuyển thành mục "Triggers" trong Properties; `Devices()` cũ bị bỏ — card trạng thái gọn lại thành đầu mục "Connection")
- Modify: `src/app.h` (`App::spage`, `App::sensorView`, `App::sensorTool`, `App::sensorPanelCollapsed`)
- Modify: `src/sources.cpp` (`RestartSource`), `src/main.cpp` (`SensorSelfTest`)

**Interfaces:**
- Consumes: `DrawTree` (Task 5), `StageView`/`StageViewUpdate`/`DrawScreenBackdrop`/`EditQuad` (Task 4), `LidarPose`/`lidarToSurface`/`surfaceToLidar`/`PlacementCalib` (Task 2), `DeviceScreen` (Task 1), `DeviceScan`/`DeviceSource` (đã có).
- Produces:
  ```cpp
  int App::spage = 0;                 // 0 Input, 1 Output (như mpage)
  StageView App::sensorView[2];       // view riêng cho mỗi tab
  int App::sensorTool = 0;            // tab Output: 0 Edit points, 1 Transform
  void RestartSource(const std::string& deviceId);   // xoá khỏi g_live; tick sau tạo lại (nút Reconnect)
  enum class PropSection { Connection, ScreenSize, Placement, Blobs, Calibration, SendOut, Triggers };
  std::vector<PropSection> SensorPropSections(DevKind k, int spage);   // mục Properties theo loại + tab (thuần, test được)
  ```
- Bố cục `DrawSensor`: trái `DrawTree(…, Sensors)` (236 px, hoặc 46 px khi thu gọn); phải `SensorProps` (280 px); giữa là stage có thanh 44 px mang 2 tab `INPUT` / `OUTPUT` (mint / coral, cùng kiểu với `Stage()` của Mapping) và nhóm zoom.
- **Input, lidar** (`ScanSource` có mặt hoặc kind Hokuyo/Plugin), không gian = mm của screen (`SW = realW`, `SH = realH`):
  - Nếu `realW/realH == 0`: giữa stage hiện ô nhập hai số mm, ghi thẳng vào `Screen::realW/H`.
  - Ngược lại vẽ: khung screen; điểm scan (`lidarToSurface`) chấm 1 px màu `pal::t66`, điểm quá `rangeMm` mờ đi; blob màu coral kèm `ID:n`; biểu tượng sensor tại `pose` có mũi tên hướng trước và vòng tầm `rangeMm`.
  - Thao tác: kéo thân sensor ⇒ dời (làm tròn 10 mm, Shift = khoá trục); kéo chấm xoay đặt cách 60 px theo hướng trước ⇒ xoay (Shift = bước 15°). Thả chuột ⇒ `pushHist`.
  - Khi `d.placed`: mọi thay đổi pose / mm của screen ⇒ `PlacementCalib`.
- **Input, Sim/Skeleton:** giữ `Radar` / `SkeletonView` như hiện tại.
- **Output** (mọi loại), không gian 1920×1080:
  - Nền là `DrawScreenBackdrop(*DeviceScreen(d))`.
  - Tứ giác `calib[i].tx,ty` chỉnh bằng `EditQuad` (màu mint), có công cụ Edit points / Transform.
  - Touch của thiết bị qua `DeviceH` vẽ chồng lên.
  - Thả chuột sau khi chỉnh ⇒ `d.placed = false` (sang chế độ Custom), `pushHist`.
- **`SensorProps`** — bố cục lai giống Properties của Advanced Mapping (`mapping.cpp`, `bool output = A.mpage != 0`): một khối đầu **luôn hiện**, rồi các mục **đổi theo tab**. Quy tắc: Input = sensor *nhìn thấy gì* và *lọc ra sao*; Output = điểm *rơi vào đâu trên screen* và *đi đâu tiếp*. Mục dùng `PanelHeader`/`PropertyRow` có sẵn; mỗi mục thu gọn được, trạng thái mở/đóng nhớ theo `DevKind` (runtime, `std::map<int, uint32_t> App::sensorSecOpen`, bit theo mục).
  - **Luôn hiện (đầu panel):** tên (`TextField`); chấm trạng thái + fps / latency (`d.error` màu red nếu có); Active; chọn screen (dropdown `A.screens`) + dòng cảnh báo yellow khi `DeviceScreen` phải dùng screen dự phòng.
  - **Tab Input** (lidar):
    - **Connection:** host, port, nút `Reconnect` (`RestartSource`), nút `Scan again` (mở wizard bước 2 cho đúng thiết bị này), packets.
    - **Screen size:** Real width / height mm — ghi thẳng `Screen::realW/H` (cùng giá trị ở Properties của Screen bên Mapping).
    - **Placement:** X, Y mm; Angle °; Mirror; Range limit mm; nút `ROI = screen` (`roi` = `surfaceToLidar` của 4 góc screen).
    - **Blob tracking:** On, Min, Max, Gap mm; số blob đang thấy (đọc, không sửa).
  - **Tab Output** (lidar):
    - **Calibration:** badge `Placed` (mint) hoặc `Custom (4-point)` (yellow); nút `Re-apply placement` (đặt `placed = true` rồi gọi `PlacementCalib`) — đặt ở đây, cạnh badge, vì badge là nơi người dùng thấy calib tay đang được giữ; 4 hàng số `tx, ty` của các điểm (sửa được, sửa ⇒ `placed = false`); wizard chạm 4 điểm (chạm dấu thập, giữ nguyên); RMS.
    - **Send out:** chỗ giữ cho Task 8, chưa vẽ ở task này.
    - **Triggers:** các patch cord cũ.
  - **Sim / Skeleton** (camera mới để plan sau): Input = Connection (port, lệnh `pose_bridge` cho Skeleton); Output = Calibration (4 điểm + wizard + RMS), Send out, Triggers.

- [ ] **Step 1: Check thất bại trong `SensorSelfTest`** (Hokuyo nối fake SCIP server như Task 8 cũ, screen 4000×2250):
  - Đặt `d.placed = true`, `d.pose = {2000, 2250, -90}`, gọi hàm cập nhật placement ⇒ `d.calibrated` và `DeviceH` đưa blob ở (1000, 0) ra ≈ (960, 600) px (±2).
  - Đặt `d.placed = false`, sửa `calib[0].tx += 50`, đổi `pose.x = 2500`, chạy lại hàm cập nhật ⇒ `calib[0].tx` vẫn giữ giá trị tay (Review Focus #3).
  - `ROI = screen` ⇒ `d.roi[0]` ≈ `surfaceToLidar(pose, {0,0})`.
  - Danh sách mục Properties là dữ liệu kiểm được: tách hàm thuần `std::vector<PropSection> SensorPropSections(DevKind k, int spage)` (`enum class PropSection { Connection, ScreenSize, Placement, Blobs, Calibration, SendOut, Triggers }`) mà `SensorProps` dùng để vẽ; check: Hokuyo + Input = `{Connection, ScreenSize, Placement, Blobs}`, Hokuyo + Output = `{Calibration, SendOut, Triggers}`, Skeleton + Input = `{Connection}`, Skeleton + Output = `{Calibration, SendOut, Triggers}`; `DevKind::Plugin` cho đúng kết quả như Hokuyo (plugin hiện chỉ là lidar 2D).
- [ ] **Step 2: Chạy đỏ. Step 3: Cài. Step 4: Chạy xanh.** Chụp `--screen 2` ở cả hai tab (thêm cờ `--spage N` nếu cần, giống `--tab`): tab Input có khung screen mm + sensor + scan; tab Output có nền screen + tứ giác mint.
- [ ] **Step 5: Commit** `git commit -m "sensor page: Mapping-style three columns — place a lidar in mm on Input, fine-tune its quad on Output with the Mapping tools"`

### Task 7: Wizard thêm sensor + scan IP

**Files:**
- Create: `engine/io/net/NetScan.h`, `engine/io/net/NetScan.cpp`, `tests/test_netscan.cpp`
- Modify: `src/sensor.cpp` (`SensorWizardView` thay stage khi `A.wizard.step > 0`), `src/sources.cpp` (`StartIpScan`/`IpScanDone`), `src/app.h`, `src/main.cpp`
- Modify: `CMakeLists.txt` gốc và `src/CMakeLists.txt`: Windows link `iphlpapi`

**Interfaces:**
- Produces:
  ```cpp
  struct Ipv4Iface { std::string name, address; int prefix = 24; };
  std::vector<Ipv4Iface> localIpv4Interfaces();     // card đang up, bỏ loopback (GetAdaptersAddresses / getifaddrs)
  std::vector<std::string> hostsInSubnet(const std::string& address, int prefix);   // bỏ địa chỉ mạng, broadcast, và chính address; prefix < 22 ⇒ rỗng
  std::vector<std::string> scanTcpPort(const std::vector<std::string>& hosts, uint16_t port, int timeoutMs);
       // connect non-blocking theo lô 256, chờ bằng select; trả các host nhận kết nối, theo thứ tự đầu vào
  // src/
  struct SensorWizard { int step = 0; DevKind kind = DevKind::Hokuyo; std::string plugin, screenId, manualHost, config;
                        int iface = 0; std::vector<std::string> found; std::vector<char> pick; std::vector<std::string> added; bool scanning = false; };
  SensorWizard App::wizard;           // runtime, không serialize
  void StartIpScan(const std::string& address, int prefix, uint16_t port);   // một std::thread; kết quả qua atomic<bool> done (release/acquire)
  bool IpScanDone(std::vector<std::string>& out);
  ```
- **Bước 1 — chọn loại** (thẻ 2 cột): `Camera 2D` (Skeleton), `Lidar 2D · Hokuyo`, `Lidar 2D · <tên plugin>` (một thẻ cho mỗi plugin của `SensorPlugins()`), `Simulated`. Thêm hai thẻ mờ `Lidar 3D`, `Depth camera`: bấm vào thì toast `"Not supported yet — planned"`.
- **Bước 2 — kết nối:**
  - Hokuyo: chọn card mạng (`localIpv4Interfaces`), nút `Scan` (cổng 10940, timeout 300 ms), danh sách kết quả có checkbox (chọn nhiều được), ô nhập IP tay + `Add`. Dải quá lớn thì báo `"Subnet too large to scan (/<prefix>) — type the IP"`.
  - Camera: port (mặc định = cổng trống nhỏ nhất ≥ 9000 chưa có thiết bị dùng) + dòng lệnh `pose_bridge`.
  - Plugin: ô JSON điền sẵn `defaultConfig`.
  - Sim: bỏ qua bước này.
- **Bước 3 — kiểm tín hiệu:** tạo một thiết bị cho mỗi mục đã chọn (`enabled = true`, `screenId = wizard.screenId`, tên `"Hokuyo 192.168.0.10"`…), rồi hiện bảng trạng thái sống (chấm, fps, lỗi). Nút `Done` ⇒ `step = 0`, chọn thiết bị đầu tiên mới thêm. Thiết bị lỗi vẫn giữ lại để người dùng sửa.

- [ ] **Step 1: Test thất bại** (`tests/test_netscan.cpp`):
  - `hostsInSubnet("192.168.0.23", 24)`: `size() == 253`, có `"192.168.0.1"` và `"192.168.0.254"`, không có `".0"`, `".255"`, `".23"`.
  - `hostsInSubnet("10.0.0.21", 30) == {"10.0.0.22"}`.
  - `hostsInSubnet("10.0.0.1", 16)` rỗng.
  - `scanTcpPort({"127.0.0.1"}, <cổng của một socket đang listen trong test>, 300) == {"127.0.0.1"}`; cổng không ai nghe ⇒ rỗng.
  - `localIpv4Interfaces()` không ném lỗi và không chứa `127.0.0.1`.
- [ ] **Step 2: Chạy đỏ. Step 3: Cài.** Thêm check `SensorSelfTest`: chạy wizard tự động (bước 1 Hokuyo → bước 2 nhập tay `127.0.0.1` cổng fake SCIP → bước 3) ⇒ có thêm 1 thiết bị trên đúng screen, trong 2 s chuyển sang Running.
- [ ] **Step 4: Chạy xanh;** chụp ba bước bằng `--shot`.
- [ ] **Step 5: Commit** `git commit -m "sensor: add-sensor wizard — pick a type, scan the subnet for lidars, add several at once, watch them come up"`

### Task 8: Gửi toạ độ ra ngoài (OSC / UDP text), chuẩn hoá 0–1

**Files:**
- Create: `engine/io/net/TouchSender.h`, `engine/io/net/TouchSender.cpp`, `tests/test_touchsender.cpp`
- Modify: `src/app.h` (`Device`), `src/project.cpp`, `src/sources.cpp`, `src/sensor.cpp` (mục Send out)

**Interfaces:**
- Produces:
  ```cpp
  struct OutPacket {                                   // POD, memcpy-able
    char host[64]; uint16_t port; uint8_t proto;       // 0 OSC, 1 text
    char device[32]; uint8_t count; uint32_t ids[kMaxTouchPoints]; float xy[kMaxTouchPoints * 2];
  };
  size_t encodeOsc(const OutPacket& p, uint8_t* buf, size_t cap);   // bundle: /mikmap/<device>/frame i:count, rồi mỗi điểm /mikmap/<device>/touch i:id f:x f:y
  size_t encodeText(const OutPacket& p, uint8_t* buf, size_t cap);  // "<device> frame <count>\n" rồi mỗi điểm "<device> <id> <x> <y>\n" (x, y 4 chữ số thập phân)
  class TouchSender {                                  // một thread; ring SPSC 64 gói; đầy ⇒ bỏ gói cũ nhất
  public: bool start(); void stop(); bool push(const OutPacket& p); uint64_t dropped() const; uint64_t sent() const; std::string lastError() const; };
  struct Device { /* … */ bool outOn = false; int outProto = 0; std::string outHost = "127.0.0.1"; int outPort = 7000; };
  ```
- `SensorsTick`: mỗi khi thiết bị có frame mới **và** `outOn` **và** `DeviceH` hợp lệ ⇒ dựng `OutPacket` (`x = ox/1920`, `y = oy/1080`) rồi `push`. Đây là `push` không cấp phát trên render thread. Một `TouchSender` toàn cục, `start` lúc khởi động, `stop` trong `SensorsShutdown`.
- Schema device: `"out":{"on","proto","host","port"}`. Mục **Send out** trong Properties: On, OSC/UDP text, Host, Port, số gói đã gửi/bỏ, lỗi.

- [ ] **Step 1: Test thất bại**
  - `encodeOsc` của một gói 2 điểm ⇒ đưa vào `OscSource::feedPacket` (prefix `"/mikmap/dev1/touch"`) ⇒ frame có 2 điểm, đúng id và x/y (±1e-6). Đây là bằng chứng ta nói đúng OSC của chính mình.
  - `encodeText` ⇒ chuỗi đúng từng ký tự với ví dụ cố định.
  - `TouchSender` gửi loopback tới một `OscSource` đang chạy ⇒ nhận đủ trong 1 s.
  - Đẩy 1000 gói tới `127.0.0.1:9` (discard) liên tục ⇒ `push` luôn trả nhanh (tổng < 50 ms), `dropped() + sent() == 1000`.
  - Host không phân giải được ⇒ `lastError()` không rỗng, không ném lỗi.
- [ ] **Step 2: Chạy đỏ. Step 3: Cài.** Thêm check `SensorSelfTest`: Hokuyo đặt vị trí + `outOn` tới một `OscSource` loopback ⇒ nhận `x` ≈ 0.5, `y` ≈ 0.556 (±0.01).
- [ ] **Step 4: Chạy xanh.**
- [ ] **Step 5: Commit** `git commit -m "sensor: send mapped touches out as OSC or plain UDP, normalised to the screen, from their own thread"`

### Task 9: Tài liệu

**Files:** `features.md`, `README.md`, `.claude/CLAUDE.md`, `.claude/rules/design.md` (bảng "Thao tác chuẩn" thêm các thao tác ở trang Sensor)

- [ ] **Step 1:** Grep `src/*.cpp` xác nhận `DrawTree(…, TreeMode::Sensors)`, `PlacementCalib`, `EditQuad`, `scanTcpPort`, `TouchSender` đều được gọi. Thêm/đổi dòng ở nhóm G:
  - `G18` (nhiều sensor nhiều screen) ⇒ `[x]`.
  - Thêm `G24 Trang Sensor kiểu Advanced Output`, `G25 Đặt lidar theo mm`, `G26 Scan IP thêm nhiều sensor`, `G27 Gửi toạ độ OSC/UDP`.
  - Ghi chú kiểm tra: phần nào chưa kiểm với phần cứng thật.
- [ ] **Step 2: Commit** `git commit -m "docs: the Sensor page, lidar placement, IP scan and touch output"`

---

## Kiểm cuối

- `ctest --test-dir build -C Release` và `ctest --test-dir src/build -C Release` đều xanh, 0 cảnh báo mới.
- So ảnh `--shot` trang Mapping trước và sau Task 4: không khác.
- Agent `reviewer` của repo duyệt toàn nhánh.
- Báo rõ phần nào chưa chạy với Hokuyo/YDLIDAR thật.

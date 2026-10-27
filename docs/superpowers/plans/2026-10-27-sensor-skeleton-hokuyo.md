# Sensor thật: Camera skeleton (MediaPipe) → Hokuyo 2D lidar — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Trang Sensor I/O của `src/` chạy được với nguồn thật: trước là camera + MediaPipe Pose (tracking khung xương), sau là lidar 2D Hokuyo (SCIP 2.0 qua TCP), rồi tới plugin cảm biến dạng DLL (ABI C riêng của MikMap) với plugin đầu tiên là YDLIDAR. Mỗi thiết bị có calibration `H_s` riêng.

**Architecture:** MediaPipe chạy trong một **sidecar Python** (`tools/pose_bridge/`), gửi các khớp xương qua OSC/UDP tới `OscSource` đã có trong `engine/io` (phương ngữ `/mikmap/touch`). Hokuyo là một `ISensorSource` mới trong `engine/io`: thread riêng đọc TCP, giải mã SCIP 2.0, ra `LaserScan`, rồi cắt cụm (blob) + `PointTracker` thành `SensorFrame`/`TouchEvent`. Plugin DLL (`plugins/sensors/*.dll`) đi vào cùng pipeline lidar đó qua `PluginSource`. `src/` lần đầu giữ `ISensorSource` thật (`src/sources.cpp`). Render thread chỉ `consume()` TripleBuffer / `pop` ring, và áp `H_s` tại nơi dùng.

**Tech Stack:** C++20, Dear ImGui + GLFW (`src/`), engine STL thuần, `tests/TestHarness.h`; Python 3.10–3.12 + `mediapipe==0.10.14` + `opencv-python==4.10.0.84` (chỉ cho sidecar).

**Bổ sung 2026-10-27 (sau khi đọc `YDLidar.dll` của người dùng):** thêm Phần D. Người dùng đã chốt: *"Thêm task plugin ABI của MikMap vào plan, plugin đầu tiên là YDLIDAR bọc ydlidar_sdk.dll. Tôi sẽ gửi file .cpp của YDLidarCHOP."*

**Spec:** Chưa có file spec. Yêu cầu của người dùng (2026-10-27): *"Tạo plan với trang sensor, đầu tiên với camera sử dụng mediapipe để tracking khung xương trước, sau đó là calib 2d lidar: điển hình là hokuyo, và 1 số model sensor Tàu như ydlidar nhận tín hiệu qua udp"*. Hai quyết định người dùng chốt thêm: **Hokuyo trước, lidar UDP (YDLIDAR…) làm sau** (ngoài plan này), và MediaPipe chọn **cách tốn ít tài nguyên nhất / fps cao nhất**.

**Vì sao sidecar Python mà không nhúng MediaPipe C++:** phần tốn tài nguyên là suy luận TFLite, và nó giống hệt nhau ở cả hai cách vì wheel Python gọi đúng lõi C++ đó. Nhúng C++ chỉ bớt được bước đổi frame của Python (cỡ 1–3 ms) cùng chặng UDP loopback (dưới 0,1 ms), nhưng đổi lại phải build Bazel trên Windows, kéo TFLite và OpenCV vào `src/`, và mất GPU delegate trên Windows (bản Python trên Windows cũng không có GPU delegate, nên hai cách hoà nhau về khoản này). Sidecar chạy suy luận trên một core riêng, không ăn vào ngân sách khung hình của render thread; MediaPipe có sập cũng không kéo buổi diễn sập theo. Nút vặn hiệu năng thật sự là `--model lite` và độ phân giải capture 640×480, đều là tham số của bridge.

## Global Constraints

- `engine/core` chỉ STL; `engine/io` chỉ STL + `engine/core` + socket hệ điều hành. Không include GL/GLFW/ImGui, không biết `Slice`/`Layer` (nguyên tắc #1, #2).
- Sensor thread → render thread chỉ qua `TripleBuffer` (state) / `SpscRingBuffer` (event). **Không mutex** chia sẻ dữ liệu giữa hai bên. Tham số UI → sensor thread cũng đi qua `TripleBuffer` (UI là writer duy nhất).
- Toạ độ đi qua biên thread là **thô**: camera = toạ độ ảnh chuẩn hoá `[0,1]` (y hướng xuống); Hokuyo = mm trong hệ của lidar (x = phía trước, y = bên trái, θ ngược chiều kim đồng hồ tính từ phía trước); Sim = toạ độ radar chuẩn hoá như hiện tại. `H_s` chỉ áp ở render thread (#5).
- Render thread không lock/alloc/IO. Sensor thread không cấp phát trong vòng lặp nóng: buffer được `reserve` một lần lúc khởi động.
- `tCaptureNs` đóng dấu bằng `steady_clock` lúc nhận dữ liệu. Độ trễ hiển thị trên card thiết bị là số **đo được**, không phải số gõ tay (#6).
- MSVC `/W4 /permissive- /fp:precise /utf-8`, 0 cảnh báo mới. GCC/Clang `-Wall -Wextra -Wpedantic`.
- `SensorFrame` giữ `kMaxTouchPoints = 64`. Bridge phải từ chối chạy khi `num_poses × số khớp > 64`.
- Mọi tính năng phải chạy được khi không có phần cứng: Sim device vẫn còn; Hokuyo được test bằng fake SCIP server chạy loopback; skeleton được test bằng gói OSC gửi loopback (#7).
- Ranh giới plugin là **C thuần**: không class, không `std::*`, không exception đi qua biên DLL. Dữ liệu là mảng `float` do host cấp phát. Không include header của TouchDesigner ở bất kỳ đâu trong repo.
- Làm trên nhánh mới `feat/sensor-skeleton-hokuyo` tách từ `main`. Commit message nói **vì sao**.
- Đánh dấu `features.md` chỉ sau khi grep `src/*.cpp` thấy code được GỌI tới thật.

## Review Focus

1. **Số liệu runtime lọt vào snapshot undo**: nếu fps/latency/packets/touches của thiết bị bị serialize thì dự án bị đánh dấu "dirty" ở mọi khung hình khi có thiết bị đang chạy. Kỳ vọng: không. Test nằm ở Task 2 (`SensorSelfTest`: `ProjectDirty()` vẫn false sau khi có touch).
2. **Rút cáp hoặc tắt Hokuyo giữa buổi diễn**: kỳ vọng card chuyển sang Error/Connecting và tự nối lại, mọi blob đang sống phát `Up`, không có touch nào kẹt trên output. Test ở Task 7.
3. **TCP cắt block SCIP ở chỗ bất kỳ** (giữa nhóm 3 ký tự, giữa `\n\n`): kỳ vọng nạp từng byte một vẫn cho đúng kết quả như nạp cả khối. Test ở Task 5.
4. **Mở file dự án hoặc calib profile cũ** (không có `kind`, calib nằm ở `sensor.calib` cấp trên): kỳ vọng nạp thành một Sim device đã calibrate, hình dạng không đổi; profile Hokuyo không nạp được vào camera. Test ở Task 1.
5. **Mở dự án trên máy diễn chưa cài plugin** (hoặc thiếu `ydlidar_sdk.dll` cạnh plugin): kỳ vọng thiết bị vẫn giữ nguyên cấu hình, card đỏ, báo rõ tìm file ở đâu. Không im lặng, không xoá thiết bị. Test ở Task 10 (thiếu plugin) và Task 11 Step 4 (DLL phụ thuộc). Trường hợp cổng OSC bị chiếm cũng có test, ở Task 2.

## Ngoài phạm vi (cố ý bỏ)

- Decoder UDP tự viết cho lidar Tàu: YDLIDAR đi đường plugin bọc SDK của hãng (Task 11). Các hãng khác (LakiBeam, LSLiDAR…) sau này cũng thành plugin, hoặc thành `ScanSource` mới khi có tài liệu giao thức.
- Plugin dạng touch/skeleton (không phải scan 2D); mô tả tham số plugin theo từng trường (v1 dùng JSON thô); chạy plugin ở tiến trình riêng để cô lập lỗi sập.
- Hokuyo qua USB/serial (URG-04LX-UG01): plan chỉ làm Ethernet.
- App tự khởi chạy sidecar Python (spawn process): v1 cho người dùng chạy tay, card hiện sẵn lệnh cần chạy.
- Lidar quay 360° cắt cụm qua mốc 0°/360° (Hokuyo chỉ quét 270°, không gặp trường hợp này).
- `G18` gắn calibration với screen khác screen 0: overlay vẫn chỉ vẽ ở Screen đầu tiên như hiện tại.

---

## Phần A — Thiết bị thật trên trang Sensor (nền chung)

### Task 1: Device có loại + calibration riêng từng thiết bị

Hiện mọi thứ dùng chung một `A.calib`/`A.roi`. Camera (`[0,1]`) và lidar (mm) không thể chung một `H_s`, nên calib + ROI phải chuyển vào `Device`.

**Files:**
- Modify: `src/app.h:199-203` (struct `Device`), `src/app.h:377-388` (bỏ `calib`, `roi`; thêm `selDevice`), `src/app.h:507-514`, `src/app.h:607` (khai báo)
- Modify: `src/project.cpp:333-346` (Serialize), `src/project.cpp:450-460` (Load + migrate), `src/project.cpp:640-662` (calib profile), `src/project.cpp:727-728`, `src/project.cpp:788`
- Modify: `src/deck.cpp:114-122` (thiết bị mặc định), `src/calib.cpp` (thêm `DeviceH`), `src/sensor.cpp` (`FitAffine`, mọi chỗ `A.calib`/`A.roi` → thiết bị đang chọn), `src/mapping.cpp:2654-2668`, `src/output.cpp:468-480` (overlay lặp qua các thiết bị), `src/main.cpp:157` (đếm thiết bị online), `src/main.cpp:701-707,1128-1140` (roundtrip)

**Interfaces:**
- Produces:
  ```cpp
  enum class DevKind { Sim = 0, Skeleton = 1, Hokuyo = 2 };
  struct Device {
    std::string id, name; DevKind kind = DevKind::Sim;
    std::string host;  int port = 0;   // Skeleton: port nghe OSC (9000), host bỏ qua · Hokuyo: 192.168.0.10:10940
    bool enabled = false;              // người dùng muốn nó chạy (lưu vào dự án)
    bool calibrated = false;
    std::vector<Calib> calib;          // đúng 4 điểm: tx,ty = px output Screen 0; mx,my = đơn vị THÔ của thiết bị này
    ImVec2 roi[4];                     // đơn vị thô của thiết bị
    // ── runtime, KHÔNG serialize ──
    std::vector<Touch> touches; int status = 0; std::string error; int fps = 0; float latencyMs = 0; uint64_t packets = 0;
  };
  int  App::selDevice = 0;                                        // runtime, kẹp vào [0, devices.size())
  Device* App::curDevice();                                        // nullptr nếu danh sách rỗng
  void FitAffine(const std::vector<Calib>& c, float out[9], float* rms);   // thay chữ ký cũ đọc A.calib
  bool DeviceH(const Device& d, float H[9]);                        // false nếu !calibrated; FitHomography, lỗi thì FitAffine
  Device MakeDevice(DevKind k, const std::string& id);              // mặc định theo loại (bảng dưới)
  bool SaveCalibProfile(const std::string& path, const Device& d, float noise, float blobSize, std::string& err);
  bool LoadCalibProfile(const std::string& path, Device& d, float& noise, float& blobSize, std::string& err);
  ```
- Mặc định `MakeDevice`. Calib targets của mọi loại là `(100,100) (1820,100) (1820,980) (100,980)`, `mx,my = 0`.

  | kind | name | host:port | roi | calibrated |
  |---|---|---|---|---|
  | Sim | "Simulated radar" | — | ROI cũ trong `deck.cpp:122` | true, calib cũ `deck.cpp:121` |
  | Skeleton | "Camera skeleton" | `:9000` | không dùng | false |
  | Hokuyo | "Hokuyo 2D lidar" | `192.168.0.10:10940` | `(3000,1500) (3000,-1500) (500,-1500) (500,1500)` mm | false |

- Schema device trong `.mikmap`: `{"id","name","kind","host","port","enabled","calibrated","calib":[4×{tx,ty,mx,my}],"roi":[[x,y]×4]}`. Không còn ghi `sensor.calib`/`sensor.roi` ở cấp trên. File profile `.mikmap-calib` thêm trường `"kind"`.

- Danh sách mặc định ở `deck.cpp`: `devices = {MakeDevice(Sim,"dev1"), MakeDevice(Skeleton,"dev2"), MakeDevice(Hokuyo,"dev3")}`, chỉ Sim có `enabled = true`.

- [ ] **Step 1: Viết check thất bại trong `--roundtrip` (`src/main.cpp`, cạnh `MappingSelfTest`)**
  - Đặt `A.devices[2].calib[2].mx = 0.123f` (thiết bị Hokuyo mặc định) → lưu → nạp → check `std::fabs(A.devices[2].calib[2].mx - 0.123f) < 1e-6f`, `A.devices[2].kind == DevKind::Hokuyo` và `A.devices[2].port == 10940`.
  - Ghi tay một file dự án cũ có `"sensor":{"calib":[...4 điểm...],"roi":[...],"devices":[{"id":"d","name":"L","endpoint":"udp://x","connected":true,"fps":40,"latency":12,"packets":"1k"}]}` → nạp → check thiết bị `d` có `kind == DevKind::Sim`, `calibrated == true`, `calib` trùng 4 điểm cũ, `enabled == false`.
  - Lưu profile từ thiết bị Hokuyo → `LoadCalibProfile` vào thiết bị Skeleton → check trả `false` và `err` chứa `"Hokuyo"`.
  - `MarkSaved()`, rồi đổi `A.devices[0].fps = 99` và `A.devices[0].touches.push_back({1, 0.1f, 0.1f})` → check `ProjectDirty() == false` (số liệu runtime không vào snapshot).

- [ ] **Step 2: Chạy, xác nhận thất bại**
  Run: `cmake --build src/build --config Release -j && ctest --test-dir src/build -C Release --output-on-failure`
  Expected: lỗi biên dịch (`DevKind` chưa có) hoặc `project_roundtrip` FAIL.

- [ ] **Step 3: Cài đặt** theo Interfaces. Migrate trong Load: device không có `"kind"` thì là Sim, `enabled=false`, nhận `sensor.calib`/`sensor.roi` cũ (nếu file có), `calibrated=true`. `sensor.cpp`: mọi chỗ `A.calib`/`A.roi` đổi thành `A.curDevice()->...`; đổi `selDevice` thì đặt `A.wizardStep = 0`; bấm vào card để chọn thiết bị (viền coral cho thiết bị đang chọn). Overlay ở `mapping.cpp`/`output.cpp` lặp qua `A.devices`, chỉ vẽ thiết bị có `DeviceH(d, H)` trả true và `!d.touches.empty()`. Radar click (Sim) ghi vào `curDevice()->touches` thay cho `A.touchPts`; xoá `A.touchPts`. Profile: `LoadCalibProfile` từ chối khi `"kind"` có mặt mà khác `d.kind` (`err = "profile is for a Hokuyo sensor"`, dùng tên loại); thiếu `"kind"` thì chỉ nhận vào Sim.

- [ ] **Step 4: Chạy lại, xác nhận đạt**
  Run: lệnh build + `ctest` của Step 2. Expected: `project_roundtrip` PASS, 0 cảnh báo mới.

- [ ] **Step 5: Commit**
  ```bash
  git add src/app.h src/project.cpp src/deck.cpp src/calib.cpp src/sensor.cpp src/mapping.cpp src/output.cpp src/main.cpp
  git commit -m "sensor: calibration per device — camera [0,1] and lidar mm cannot share one H_s"
  ```

### Task 2: Nguồn sống trong `src/` + Skeleton device qua OscSource

**Files:**
- Create: `src/sources.cpp`
- Modify: `src/app.h` (khai báo bên dưới), `src/main.cpp` (gọi `SensorsTick` ngay sau dòng `A.sweep = ...` ở `main.cpp:1741`, `SensorsShutdown()` trước khi huỷ cửa sổ, `SensorSelfTest` trong `--roundtrip`)
- Modify: `src/sensor.cpp` (card thiết bị lấy số thật, nút "+ Add device", menu chuột phải "Remove", ô host/port, nút Capture trong wizard; `FireTouchRoutes` bỏ `static` và nhận `Down` từ mọi thiết bị)
- Modify: `engine/io/sources/OscSource.{h,cpp}` (`/mikmap/meta`)
- Test: `tests/test_osc.cpp`

**Interfaces:**
- Consumes: `Device`, `DevKind`, `App::curDevice()` (Task 1); `OscSource`, `OscConfig` (có sẵn).
- Produces:
  ```cpp
  // src/app.h
  void SensorsTick(double now);       // render thread: tạo/khởi động/dừng nguồn theo enabled + host/port; consume frame → d.touches; pop event Down → FireTouchRoutes(); cập nhật status/error/fps/latencyMs/packets; chạy capture wizard
  void SensorsShutdown();
  mikmap::ISensorSource* DeviceSource(const Device& d);   // nullptr nếu Sim hoặc chưa chạy
  void StartCapture(int step);        // wizard: lấy trung bình điểm ứng viên của curDevice() qua 30 frame liên tiếp
  bool SensorSelfTest(std::string& why);
  void FireTouchRoutes();             // từ sensor.cpp, bỏ static
  inline constexpr int kCalibJoint = 16;   // MediaPipe RIGHT_WRIST, người slot 0 → id 16
  // engine/io/sources/OscSource.h
  float senderLatencyMs() const;      // giá trị cuối từ "/mikmap/meta <frame:int> <latency_ms:float> <fps:float>"
  ```
- Nguồn được giữ ở `static std::map<std::string, Live> g_live;` trong `sources.cpp`, khoá theo `Device::id`. `Live` gồm `std::unique_ptr<mikmap::ISensorSource>` và chuỗi chữ ký `kind|host|port`; chữ ký đổi thì dừng rồi tạo lại nguồn. Thiết bị bị xoá thì dừng nguồn của nó. Không có gì của `g_live` nằm trong `App`, nên snapshot undo không chạm tới.
- Skeleton: `OscConfig{port = d.port, protocol = Mikmap, addressPrefix = "/mikmap/touch", normalizedInput = false, pointTimeoutSec = 0.25}`.
- Điểm ứng viên khi capture: Hokuyo dùng `touches` (phải có đúng 1); Skeleton dùng touch có `id == kCalibJoint`. Nếu một frame bất kỳ không có đúng 1 ứng viên thì huỷ capture và `A.notify("Need exactly one point — seen N")`. Capture xong ghi `calib[step].mx/my`, sang bước kế; xong bước 4 thì `calibrated = true`. Sim vẫn calibrate bằng cách bấm vào radar như cũ.
- Card: FPS là số frame mới consume được mỗi giây. LATENCY = `now − tCaptureNs` của frame mới nhất, cộng `senderLatencyMs()` với Skeleton. PACKETS là `packetsReceived()` (OSC) / `scansReceived()` (Hokuyo, Task 7). Chấm trạng thái: Running màu mint, Connecting màu yellow, Error màu red, có `TextEll` hiện `error`. Skeleton đang Running mà 0 gói thì card hiện dòng lệnh `python tools/pose_bridge/pose_bridge.py --port <port>`.

- [ ] **Step 1: Test engine thất bại (`tests/test_osc.cpp`)**
  - `TEST_CASE("★ OSC: vector chung voi pose_bridge.py")`: giải hex `2f6d696b6d61702f746f756368000000` `2c69666666000000` `00000010` `3f000000` `3e800000` `00000000` (40 byte, `/mikmap/touch 16 0.5 0.25 0.0`) → `feedPacket` → `REQUIRE(src.frames().consume())`, `count == 1`, `points[0].id == 16`, `CHECK_NEAR(x, 0.5, 1e-6)`, `CHECK_NEAR(y, 0.25, 1e-6)`. Chuỗi hex này **phải trùng từng byte** với test Python ở Task 3.
  - `TEST_CASE("OSC: /mikmap/meta cap nhat senderLatencyMs, khong tao diem")`: message `/mikmap/meta` với `i=7, f=12.5, f=30.0` → `CHECK_NEAR(src.senderLatencyMs(), 12.5, 1e-4)`. Frame đã publish (nếu có) thì `count == 0`.

- [ ] **Step 2: Chạy, xác nhận thất bại**
  Run: `cmake --build build --target mikmap_tests -j && ctest --test-dir build --output-on-failure`
  Expected: lỗi biên dịch `senderLatencyMs`.

- [ ] **Step 3: Cài `/mikmap/meta` trong `OscSource::handleMessages`** (kiểm trước nhánh tiền tố; giá trị cất trong `std::atomic<float>`).

- [ ] **Step 4: Chạy lại, xác nhận đạt**. Expected: mọi test PASS.

- [ ] **Step 5: Viết `SensorSelfTest` thất bại** (gọi trong `--roundtrip`). Thêm Skeleton device cổng 39517 với `enabled=true` → `MarkSaved()` → `SensorsTick` → gửi gói 40 byte ở Step 1 qua UDP tới `127.0.0.1:39517` → lặp `SensorsTick(now)` tối đa 2 s.
  - Check: thiết bị có `touches.size() == 1`, `touches[0].id == 16`, `status == (int)SourceStatus::Running`, và `ProjectDirty() == false`.
  - Thêm Skeleton device thứ hai cùng cổng 39517 → sau tick: `status == (int)SourceStatus::Error` và `!error.empty()`.
  - Đặt `enabled=false` cho cả hai → tick → `DeviceSource(d) == nullptr`.

- [ ] **Step 6: Chạy, xác nhận thất bại.** Run: build app + `ctest --test-dir src/build -C Release --output-on-failure`. Expected: `project_roundtrip` FAIL ("SensorSelfTest").

- [ ] **Step 7: Cài `src/sources.cpp`** + phần UI card/Add/Remove/Capture trong `sensor.cpp` theo Interfaces. Ô host/port dùng `ui::TextField`; menu dùng `A.openCtx`.

- [ ] **Step 8: Chạy lại, xác nhận đạt**, rồi chụp thử: `src/build/mikmap --shot sensor.png --screen 2 --frames 30`. Expected: `ctest` PASS; ảnh có 3 card (Sim online; Camera skeleton và Hokuyo offline), không còn số liệu giả "184.2k".

- [ ] **Step 9: Commit**
  ```bash
  git add src/sources.cpp src/app.h src/main.cpp src/sensor.cpp engine/io/sources/OscSource.h engine/io/sources/OscSource.cpp tests/test_osc.cpp
  git commit -m "sensor: src owns real ISensorSource threads; device cards show measured fps/latency"
  ```

---

## Phần B — Camera + MediaPipe skeleton

### Task 3: Sidecar `pose_bridge.py`

**Files:**
- Create: `tools/pose_bridge/pose_bridge.py`, `tools/pose_bridge/test_pose_bridge.py`, `tools/pose_bridge/requirements.txt`, `tools/pose_bridge/README.md`
- Modify: `.gitignore` (thêm `tools/pose_bridge/*.task`, `tools/pose_bridge/.venv/`)

**Interfaces:**
- Produces (giao thức dây — Task 2 và Task 4 dựa vào):
  - Mỗi khung camera gửi **một OSC bundle** (timetag `0x0000000000000001`) tới `--host 127.0.0.1 --port 9000`. Bundle gồm `/mikmap/meta i:frame f:latency_ms f:fps`, rồi `/mikmap/touch i:id f:x f:y f:z` cho từng khớp, rồi `/mikmap/touch/up i:id` cho từng id có ở khung trước mà khung này không còn.
  - `id = slot * 100 + landmark` (landmark 0..32 theo MediaPipe Pose). `x, y` là toạ độ ảnh chuẩn hoá thô của MediaPipe, `z` là landmark z.
  - Bỏ khớp có `visibility < --min-visibility` (mặc định 0.5).
- Hàm thuần (không import mediapipe/cv2 ở đầu module, để test chạy được mà không cần chúng):
  ```python
  def encode_msg(address: str, args: list[int | float]) -> bytes        # int → 'i', float → 'f', big-endian, pad 4
  def encode_bundle(msgs: list[bytes]) -> bytes
  def assign_slots(prev: list[tuple[float, float] | None], centres: list[tuple[float, float]], max_dist: float = 0.2) -> list[int]
  def frame_messages(people: dict[int, list[tuple[float, float, float, float]]], joints: list[int], min_vis: float, prev_ids: set[int]) -> tuple[list[bytes], set[int]]
  ```
  `assign_slots`: centre là trung điểm hông (landmark 23, 24). Ghép tham lam theo khoảng cách tăng dần vào slot cũ trong phạm vi `max_dist`; người mới nhận slot trống nhỏ nhất.
- CLI: `--camera 0 --width 640 --height 480 --model lite|full|heavy (mặc định lite) --num-poses 1 --joints all|key --min-visibility 0.5 --host --port`.
  - `key` = `[0, 11, 12, 15, 16, 23, 24, 27, 28]`.
  - Nếu `num_poses * len(joints) > 64` thì thoát mã 2, kèm thông báo nêu giới hạn 64.
  - Model `.task` tải về cạnh script nếu thiếu, từ `https://storage.googleapis.com/mediapipe-models/pose_landmarker/pose_landmarker_<model>/float16/latest/pose_landmarker_<model>.task`.
  - Dùng `RunningMode.VIDEO` + `detect_for_video` (đồng bộ: capture đã chặn theo fps camera, nên độ trễ chỉ còn là thời gian suy luận). Windows dùng `cv2.CAP_DSHOW`.
  - Mất camera (`read()` false 30 lần liên tiếp) thì thoát mã 3 kèm thông báo. Mỗi giây in ra một dòng `fps / latency`.

- [ ] **Step 1: Viết test thất bại** (`unittest`):
  - `test_vector_matches_cpp`: `encode_msg("/mikmap/touch", [16, 0.5, 0.25, 0.0]).hex()` bằng đúng chuỗi hex 40 byte ở Task 2 Step 1.
  - `test_bundle_header`: `encode_bundle([m])` bắt đầu bằng `b"#bundle\0" + bytes(7) + b"\x01"`, tiếp theo là `len(m)` dạng `>i`.
  - `test_slots_stable_when_order_swaps`: `assign_slots([(0.2,0.5),(0.8,0.5)], [(0.79,0.5),(0.21,0.5)]) == [1, 0]`.
  - `test_new_person_gets_lowest_free_slot`: `assign_slots([None,(0.8,0.5)], [(0.8,0.5),(0.3,0.5)]) == [1, 0]`.
  - `test_low_visibility_dropped_and_up_sent`: người slot 0 có landmark 16 `vis=0.9`, landmark 15 `vis=0.1`; `prev_ids={15, 16, 116}` → các id trả về `== {16}`; trong msgs có đúng hai `/mikmap/touch/up` (cho 15 và 116).

- [ ] **Step 2: Chạy, xác nhận thất bại.** Run: `python -m unittest discover -s tools/pose_bridge -v`. Expected: `ModuleNotFoundError`/`AttributeError`.

- [ ] **Step 3: Cài `pose_bridge.py`** theo Interfaces (OSC tự mã hoá bằng `struct`, không thêm `python-osc`). `requirements.txt`: `mediapipe==0.10.14`, `opencv-python==4.10.0.84`. README: tạo venv, `pip install -r`, lệnh chạy, bảng id khớp, bảng cờ.

- [ ] **Step 4: Chạy lại, xác nhận đạt.** Expected: 5 test OK.

- [ ] **Step 5: Kiểm tay với app** (cần webcam thật): chạy app, bật "Camera skeleton", chạy `python tools/pose_bridge/pose_bridge.py`. Expected: card Running, FPS > 0, LATENCY có số. Máy không có webcam thì ghi rõ trong báo cáo là bước này **chưa kiểm**.

- [ ] **Step 6: Commit**
  ```bash
  git add tools/pose_bridge .gitignore
  git commit -m "pose_bridge: MediaPipe in a sidecar — same TFLite cost, no Bazel in src/, a crash cannot take the show down"
  ```

### Task 4: Khung nhìn skeleton trên trang Sensor

**Files:**
- Modify: `src/sensor.cpp` (`Radar` rẽ nhánh theo `curDevice()->kind`; thêm `static void SkeletonView(ImRect area, const Device& d)`)
- Modify: `features.md` (bảng G + "Ghi chú kiểm tra"), `README.md` (mục "Việc còn lại" (4)), `.claude/CLAUDE.md` (đoạn "Vẫn còn giả/thiếu")

**Interfaces:**
- Consumes: `Device::touches` (id theo Task 3), `kCalibJoint`.
- Bones: `static const int kPoseBones[35][2] = {{0,1},{1,2},{2,3},{3,7},{0,4},{4,5},{5,6},{6,8},{9,10},{11,12},{11,13},{13,15},{15,17},{15,19},{15,21},{17,19},{12,14},{14,16},{16,18},{16,20},{16,22},{18,20},{11,23},{12,24},{23,24},{23,25},{24,26},{25,27},{26,28},{27,29},{28,30},{29,31},{30,32},{27,31},{28,32}};` (`POSE_CONNECTIONS` của MediaPipe). Chỉ vẽ một xương khi cả hai đầu khớp đều có mặt, cùng `slot = id / 100`.
- Khung camera: hình chữ nhật 4:3 căn giữa vùng radar, ánh xạ `[0,1]²`. `// shortcut: aspect cố định 4:3 khớp mặc định 640×480 của bridge; bridge đổi độ phân giải thì gửi kích thước trong /mikmap/meta`.
- Màu mỗi slot lấy theo `RoleHex(slot % 3)`; khớp `kCalibJoint` có vòng vàng khi wizard đang chạy. Nhãn thanh trên: `"CAMERA VIEW"`, text phụ là `"<fps> fps · <N> people"`. Ẩn nút Edit ROI với Skeleton.

- [ ] **Step 1: Kiểm hình bằng `--shot`.** Tạm thời trong `SensorSelfTest` (hoặc gửi bằng chính `pose_bridge.encode_*` từ một script Python ngắn ở scratchpad), gửi một bundle 33 khớp tư thế đứng tới cổng của thiết bị → `src/build/mikmap --shot skel.png --screen 2 --frames 60`. Expected: thấy người que đủ 35 xương trong khung 4:3.
- [ ] **Step 2: Chạy `ctest --test-dir src/build -C Release`**. Expected: PASS (không hỏng Task 1–2).
- [ ] **Step 3: Cập nhật tài liệu.** Grep `src/sources.cpp` thấy `OscSource` được tạo và `consume()` → `G1 [x]`, `G4 [x]`. Thêm dòng mới `G20 Camera skeleton (MediaPipe sidecar)` = `[x]`. Ghi chú kiểm tra nêu rõ: id người có thể nhảy khi hai người cắt nhau (bridge chỉ ghép slot theo khoảng cách hông), và chưa thử webcam thật nếu Task 3 Step 5 chưa chạy.
- [ ] **Step 4: Commit**
  ```bash
  git add src/sensor.cpp features.md README.md .claude/CLAUDE.md
  git commit -m "sensor: camera skeleton view; G1/G4 now real in src"
  ```

---

## Phần C — Hokuyo 2D lidar (SCIP 2.0 qua TCP)

Tham chiếu giao thức: *URG Series Communication Protocol Specification SCIP 2.0* (C-42-03320B) và *UST-10LX/20LX SCIP specifications* (C-42-04076). Chỗ nào plan này lệch với spec thì **spec thắng**: sửa theo spec, rồi ghi chỗ lệch vào commit message.

### Task 5: Bộ giải mã SCIP 2.0 (thuần, không socket)

**Files:**
- Create: `engine/io/proto/Scip2.h`, `engine/io/proto/Scip2.cpp`
- Test: `tests/test_scip2.cpp`

**Interfaces:**
- Produces:
  ```cpp
  namespace mikmap::scip2 {
  uint32_t decode(const char* p, int nChars);            // nChars ∈ {2,3,4}: mỗi ký tự (c − 0x30) & 0x3F, ghép 6 bit, big-endian
  bool lineChecksumOk(std::string_view line);            // ký tự cuối == ((Σ byte trước đó) & 0x3F) + 0x30
  std::string mdCommand(int startStep, int endStep);     // "MD" + %04d start + %04d end + "01" + "0" + "00" + "\n"
  struct DeviceParams { int dmin = 0, dmax = 0, ares = 0, amin = 0, amax = 0, afrt = 0; };
  bool parsePP(std::string_view block, DeviceParams& out); // các dòng "KEY:VALUE;<sum>"; KHÔNG kiểm checksum (đọc một lần lúc kết nối; giá trị sai sẽ bị bắt ở kiểm tra ares>0, amax>amin)
  enum class MdResult { None, Scan, Error };
  class MdStream {
  public:
    MdStream();                                          // reserve buffer 64 KiB một lần
    void feed(const char* data, size_t n);
    MdResult next(uint32_t* ranges, int maxOut, int& count, uint32_t& timestamp);
    const std::string& error() const;                    // khi Error: "MD status XX" hoặc "checksum"
    void reset();
  };
  }
  ```
- Khung một block MD: các dòng kết thúc bằng `\n`, block kết thúc bằng dòng rỗng (`\n\n`). Dòng 0 là echo bắt đầu bằng `MD`. Dòng 1 là status 2 ký tự + checksum: `00P` là ack, không có dữ liệu → bỏ qua, gọi tiếp; `99b` là có dữ liệu. Với `99b`: dòng 2 là timestamp (4 ký tự + checksum, `decode(…,4)`), các dòng còn lại là dữ liệu, mỗi dòng ≤ 64 ký tự + 1 checksum. **Nối phần dữ liệu (đã bỏ checksum) của mọi dòng rồi mới cắt thành nhóm 3 ký tự**, vì một nhóm có thể vắt qua hai dòng. Status khác thì trả `Error`; dòng dữ liệu sai checksum thì trả `Error` với `"checksum"` và bỏ block đó.

- [ ] **Step 1: Viết test thất bại** (`tests/test_scip2.cpp`; helper trong test: `withSum(s)` nối thêm ký tự checksum, `enc3(v)` mã hoá 3 ký tự):
  - `decode("1Dh", 3) == 5432` và `decode("0m", 2) == 61`.
  - `lineChecksumOk("00P")`, `lineChecksumOk("99b")`, `!lineChecksumOk("00Q")`.
  - `mdCommand(0, 1080) == "MD0000108001000\n"`.
  - `parsePP` với các dòng `MODL:UST-10LX;` `DMIN:20;` `DMAX:30000;` `ARES:1440;` `AMIN:0;` `AMAX:1080;` `AFRT:540;` `SCAN:2400;` (đã `withSum`) → `ares==1440, amin==0, amax==1080, afrt==540, dmin==20, dmax==30000`.
  - `"★ SCIP: nhom 3 ky tu vat qua 2 dong"`: block `99b` có 30 giá trị `1000 + i` (90 ký tự → dòng 64 + dòng 26) → `next()==Scan`, `count==30`, `ranges[21]==1021`.
  - `"★ SCIP: nap tung byte cho cung ket qua"`: cùng block, kèm một block ack `00P` đứng trước, nạp từng byte một và gọi `next()` sau mỗi byte → đúng 1 lần `Scan`, giá trị giống hệt.
  - `"SCIP: sai checksum dong du lieu → Error, block sau van doc duoc"`.

- [ ] **Step 2: Chạy, xác nhận thất bại.** Run: `cmake -DBUILD=build -P cmake/configure.cmake && cmake --build build --target mikmap_tests -j && ctest --test-dir build --output-on-failure`. Expected: lỗi biên dịch `Scip2.h`.
- [ ] **Step 3: Cài `Scip2.cpp`.**
- [ ] **Step 4: Chạy lại, xác nhận đạt.** Expected: PASS, 0 cảnh báo.
- [ ] **Step 5: Commit** `git add engine/io/proto/Scip2.* tests/test_scip2.cpp && git commit -m "io: SCIP 2.0 decoder that survives TCP splitting a 3-char group across reads"`

### Task 6: `LaserScan` + cắt cụm + publish touch (dùng chung cho mọi lidar 2D)

**Files:**
- Create: `engine/io/LaserScan.h`, `engine/io/LidarBlobs.h`, `engine/io/LidarBlobs.cpp`
- Test: `tests/test_lidarblobs.cpp`

**Interfaces:**
- Consumes: `PointTracker` (`engine/core/filter`), `pointInQuad` (`core/math/BilinearInverse.h`), `ISensorSource::frames()/events()`.
- Produces:
  ```cpp
  inline constexpr int kMaxScanPoints = 2880;           // đủ cho 360° @ 0.125°
  struct LaserScan {                                    // POD, memcpy-able
    uint64_t seq = 0; int64_t tCaptureNs = 0; uint16_t count = 0;
    float angleRad[kMaxScanPoints]; float rangeMm[kMaxScanPoints];   // rangeMm == 0 ⇒ không hợp lệ
  };
  struct LidarParams {
    Vec2 roi[4]{};                                      // mm, hệ lidar; cả 4 điểm = 0 ⇒ không lọc ROI
    double minBlobMm = 15, maxBlobMm = 600, maxGapMm = 50;
  };
  int extractBlobs(const LaserScan& s, const LidarParams& p, Vec2* out, int maxOut);   // trả số blob, out = trọng tâm (mm)
  class LidarTouchPublisher {
  public:
    explicit LidarTouchPublisher(PointTrackerParams tp = {100.0, 0.15});
    void publish(const LaserScan& s, const LidarParams& p, double nowSec, ISensorSource& dst);
    void clear(double nowSec, ISensorSource& dst);      // Up cho mọi track + publish frame rỗng (dùng khi mất kết nối)
  };
  ```
- Thuật toán `extractBlobs`: duyệt theo thứ tự index. Điểm hợp lệ thì `x = r·cosθ`, `y = r·sinθ`, bỏ điểm ngoài ROI. Khoảng cách Euclid tới điểm trước > `maxGapMm` (hoặc có điểm không hợp lệ chen giữa) thì mở cụm mới. Giữ cụm có `minBlobMm ≤ |đầu − cuối| ≤ maxBlobMm`, trọng tâm là trung bình các điểm.
- `publish`: track `isNew` → event `Down`, `justLost()` → event `Up` (x, y = 0), frame chứa các track không coasting (`state` Down/Move), `tCaptureNs = s.tCaptureNs`, `sourceId = dst.sourceId()`. Vector quan sát được `reserve(kMaxTouchPoints)` một lần trong constructor.

- [ ] **Step 1: Viết test thất bại.** Helper `scanWith(wallMm, objects)` dựng 1081 tia (−135°…+135°, bước 0.25°): tường cách 2000 mm, cộng các vật hình cung:
  - Một vật rộng 40 mm ở (1000, 0) → `extractBlobs == 1`, trọng tâm `x` nằm trong ±25 mm quanh 1000, `y` trong ±25 mm quanh 0 (tường dài hơn `maxBlobMm` nên bị loại).
  - ROI `(1500,500) (1500,−500) (500,−500) (500,500)` → vẫn 1 blob. ROI dời sang `x ∈ [2500, 3000]` → 0 blob.
  - Hai vật ở (1000, −300) và (1000, 300) → 2 blob.
  - Vật rộng 8 mm (< `minBlobMm`) → 0 blob.
  - `"★ Lidar: id ben vung, Down roi Up"`: publish scan có vật ở (1000, 0), rồi (1000, 30) → cùng một id; ring có đúng 1 `Down`. Publish 3 scan rỗng cách nhau 0.1 s → ring có 1 `Up` cùng id, frame cuối `count == 0`.
  - `clear()` sau 1 track → 1 `Up`, frame `count == 0`.
- [ ] **Step 2: Chạy, xác nhận thất bại** (lệnh như Task 5 Step 2).
- [ ] **Step 3: Cài.**
- [ ] **Step 4: Chạy lại, xác nhận đạt.**
- [ ] **Step 5: Commit** `git add engine/io/LaserScan.h engine/io/LidarBlobs.* tests/test_lidarblobs.cpp && git commit -m "io: lidar scan → blobs → tracked touches, shared by Hokuyo now and UDP lidars later"`

### Task 7: `HokuyoSource` (TCP) + fake SCIP server trong test

**Files:**
- Create: `engine/io/net/Socket.h` (dời `SocketHandle`, `kInvalidSocket`, `closeSocketHandle`, `ensureWinsock` ra khỏi namespace ẩn danh của `OscSource.cpp`, rồi thêm `tcpConnect`)
- Modify: `engine/io/sources/OscSource.cpp` (include `io/net/Socket.h`, xoá bản sao)
- Create: `engine/io/sources/ScanSource.h`, `engine/io/sources/ScanSource.cpp` (phần chung của mọi lidar 2D — Hokuyo bây giờ, plugin ở Task 9)
- Create: `engine/io/sources/HokuyoSource.h`, `engine/io/sources/HokuyoSource.cpp`
- Test: `tests/test_hokuyo.cpp`

**Interfaces:**
- Consumes: `scip2::*` (Task 5), `LaserScan`, `LidarParams`, `LidarTouchPublisher` (Task 6).
- Produces:
  ```cpp
  // io/net/Socket.h (inline)
  SocketHandle tcpConnect(const std::string& host, uint16_t port, int timeoutMs, std::string& err);   // kInvalidSocket nếu lỗi
  // io/sources/ScanSource.h — lớp cha trừu tượng; src/ chỉ biết tới lớp này (dynamic_cast) để vẽ scan và đẩy ROI
  class ScanSource : public ISensorSource {
  public:
    void setParams(const LidarParams& p);               // CHỈ gọi từ UI thread → TripleBuffer<LidarParams>
    TripleBuffer<LaserScan>& scans();                    // scan thô cho radar
    uint64_t scansReceived() const; uint64_t scansMalformed() const;
  protected:                                             // gọi từ thread của lớp con
    void onScan(LaserScan& s);                           // đóng dấu tCaptureNs = Clock::nowNs(), seq, publish scans() + LidarTouchPublisher::publish với params mới nhất
    void onLost();                                       // LidarTouchPublisher::clear — mọi blob đang sống phát Up
    void countMalformed();
  };
  // io/sources/HokuyoSource.h
  struct HokuyoConfig { std::string host = "192.168.0.10"; uint16_t port = 10940; double reconnectSec = 1.0; };
  class HokuyoSource final : public ScanSource {
  public:
    explicit HokuyoSource(HokuyoConfig cfg = {});
    const char* typeName() const override { return "Hokuyo"; }
    bool start() override; void stop() override;
    SourceStatus status() const override; std::string lastError() const override;
  };
  ```
- Vòng thread:
  1. `tcpConnect`, đặt `SO_RCVTIMEO` = 100 ms. Status là `Connecting` trong lúc nối.
  2. Gửi `"PP\n"`, đọc tới `\n\n`, `parsePP`. Fail khi `ares <= 0 || amax <= amin`.
  3. Gửi `mdCommand(amin, amax)`, status `Running`. Mỗi `Scan` quy đổi: `angleRad[i] = (amin + i − afrt) · 2π / ares`; `rangeMm = r` nếu `max(dmin, 20) ≤ r ≤ dmax`, ngoài ra là 0; rồi `onScan(scan)`. Block sai checksum thì `countMalformed()`.
  4. Lỗi socket/EOF/`Error` liên tiếp thì gọi `onLost()`, status `Error` + `lastError`, đóng socket, chờ `reconnectSec` rồi quay về bước 1.
  5. `stop()`: gửi `"QT\n"`, đóng socket, join.

- [ ] **Step 1: Viết test thất bại** (`tests/test_hokuyo.cpp`). Fake server trên thread: `listen` ở `127.0.0.1:0` (port do OS cấp), nhận `PP`, trả block UST-10LX của Task 5, nhận `MD`, trả `00P`, rồi phát 5 block `99b` có một vật ở (1000, 0), cắt mỗi lần `send` thành mẩu 7 byte:
  - `"Hokuyo: ket noi, nhan scan, ra 1 touch"`: chờ tối đa 2 s → `status()==Running`, `scansReceived() >= 3`, `scans().consume()` có `count == 1081`, frame có `count == 1`, `x` trong khoảng 975…1025 mm.
  - `"★ Hokuyo: server dong ket noi → Up, het touch, tu noi lai"`: server đóng socket sau 5 scan rồi `listen` lại trên cùng port → ring có `Up` cho id đã `Down`; trong vòng 3 s `status()` quay lại `Running` (lần kết nối thứ hai được đếm ở phía server).
  - `"Hokuyo: khong co server → Error co lastError"`: port không ai nghe → trong 2 s `status()` là `Error` hoặc `Connecting`, `!lastError().empty()`.
  - `"Hokuyo: setParams ROI loai vat"`: `setParams` với ROI ở `x ∈ [2500, 3000]` → các frame sau có `count == 0`.
- [ ] **Step 2: Chạy, xác nhận thất bại** (lệnh Task 5 Step 2).
- [ ] **Step 3: Tách `Socket.h`, chạy lại `test_osc` để xác nhận OSC không đổi hành vi, rồi cài `ScanSource` và `HokuyoSource`.**
- [ ] **Step 4: Chạy lại, xác nhận đạt.** Expected: toàn bộ `mikmap_tests` PASS, 0 cảnh báo; hook `pre-push.sh` (layering grep) không báo gì.
- [ ] **Step 5: Commit** `git add engine/io/net engine/io/sources tests/test_hokuyo.cpp && git commit -m "io: Hokuyo SCIP 2.0 source on a shared ScanSource; drops live blobs and reconnects when the cable goes"`

### Task 8: Hokuyo trên trang Sensor + calib lidar

**Files:**
- Modify: `src/sources.cpp` (`DevKind::Hokuyo` → `HokuyoSource{host, port}`; mỗi khi ROI/blob slider đổi thì `setParams` qua `dynamic_cast<mikmap::ScanSource*>`; thêm `const mikmap::LaserScan* DeviceScan(const Device&)`, trả bản `readSlot()` mới nhất của bất kỳ `ScanSource` nào, hoặc nullptr). Từ đây mọi nhánh UI lidar kiểm "nguồn là `ScanSource`" chứ không kiểm `kind == Hokuyo`, để plugin ở Task 10 dùng lại nguyên khung nhìn này.
- Modify: `src/sensor.cpp` (`Radar` nhánh Hokuyo; panel Routing: "Min blob size" áp vào `minBlobMm`, dải 5–300 mm)
- Modify: `src/app.h` (khai báo `DeviceScan`, `inline constexpr float kRadarRangeMm = 4000.f;`)
- Modify: `features.md`, `README.md`, `.claude/CLAUDE.md`

**Interfaces:**
- Consumes: `HokuyoSource`, `LidarParams`, `LaserScan` (Task 5–7); `StartCapture` (Task 2).
- Ánh xạ radar ↔ mm (phía trước lidar hướng lên màn hình): `screen = c + ImVec2(−y, −x) · (maxR / kRadarRangeMm)`, chiều ngược `x = −(screen.y − c.y)·kRadarRangeMm/maxR`, `y = −(screen.x − c.x)·kRadarRangeMm/maxR`. Vòng 1–4 m giữ nguyên nhãn hiện có. `// shortcut: tầm nhìn cố định 4 m; UST-20LX quét tới 20 m thì thêm zoom radar`.
- Vẽ: các điểm `rangeMm > 0` của scan thành chấm 1 px màu `pal::t66`; ROI tính bằng mm (kéo handle dùng ánh xạ ngược, không kẹp vào đĩa); touch là blob màu coral, nhãn `ID:n [x,y mm]`. Thanh trên: `"LIDAR VIEW"` + `"<fps> Hz · <N> blobs"`. Bấm vào radar **không** chèn touch giả với Hokuyo.
- `blobSize` toàn cục (vẫn lưu trong profile) nghĩa là `minBlobMm`. `noise` không dùng cho Hokuyo. `maxGapMm` cố định 50. `// shortcut: gap cố định 50 mm; thêm slider khi gặp vật mảnh sát nhau bị gộp`.

- [ ] **Step 1: Mở rộng `SensorSelfTest`.** Thêm Hokuyo device trỏ vào fake SCIP server (dời fake server của Task 7 sang `tests/FakeScipServer.h` header-only để cả `tests/` lẫn `src/main.cpp` cùng include, đường dẫn `${REPO}/tests` đã có trong include path của app thì dùng luôn, chưa có thì thêm vào `target_include_directories` của `src/CMakeLists.txt`).
  - Check: sau tối đa 2 s, `DeviceScan(d) != nullptr` và `d.touches.size() == 1`.
  - Gọi `StartCapture(0)` rồi tick đủ 30 frame → `std::fabs(d.calib[0].mx − 1000) < 25`.
- [ ] **Step 2: Chạy, xác nhận thất bại** (build app + `ctest --test-dir src/build -C Release`).
- [ ] **Step 3: Cài** theo Interfaces.
- [ ] **Step 4: Chạy lại, xác nhận đạt**, rồi chụp `--shot lidar.png --screen 2 --frames 60` với Hokuyo nối vào fake server. Expected: có vòng cung tường, một blob coral, ROI vàng tính bằng mm.
- [ ] **Step 5: Kiểm với Hokuyo thật** (UST-10LX/20LX, IP máy tính `192.168.0.x/24`): bật thiết bị → Running ~40 Hz, đi vào ROI thấy blob. Chạy wizard 4 điểm, đứng vào từng dấu thập → overlay trên Output đặt đúng chỗ chân. Không có máy thật thì ghi **chưa kiểm phần cứng** vào features.md và báo cáo.
- [ ] **Step 6: Tài liệu.** Thêm `G21 Hokuyo 2D lidar (SCIP 2.0/TCP)` `[x]` (hoặc `[~]` nếu Step 5 chưa chạy). `G12 [x]` (PointTracker được gọi qua `LidarTouchPublisher`). `G19` giữ nguyên `[ ]` vì Livox/Ouster là 3D. Ghi chú kiểm tra: lidar UDP của Tàu chưa làm, pipeline chờ sẵn ở `LidarBlobs.h`.
- [ ] **Step 7: Commit**
  ```bash
  git add src tests/FakeScipServer.h features.md README.md .claude/CLAUDE.md
  git commit -m "sensor: Hokuyo on the Sensor page — scan view in mm, ROI, 4-point capture calibration"
  ```

---

## Phần D — Plugin cảm biến dạng DLL (ABI riêng của MikMap) + YDLIDAR

Giống cách TouchDesigner nạp Custom CHOP, nhưng **không** host API của TouchDesigner: header `CHOP_CPlusPlusBase.h` có "Shared Use License", chỉ được dùng kèm TouchDesigner. MikMap có ABI C thuần của riêng mình. v1 chỉ có plugin **dạng scan 2D** (lidar), vì thứ người dùng cần bây giờ là YDLIDAR. Plugin dạng touch/skeleton để khi có nhu cầu thật.

### Task 9: ABI plugin + loader + plugin giả để test

**Files:**
- Create: `engine/io/plugin/mikmap_sensor_plugin.h` (C thuần, không include C++; đây là file duy nhất người viết plugin cần)
- Create: `engine/io/plugin/PluginLoader.h`, `engine/io/plugin/PluginLoader.cpp` (`LoadLibraryExW` / `dlopen`)
- Create: `engine/io/sources/PluginSource.h`, `engine/io/sources/PluginSource.cpp`
- Create: `tests/plugins/fake_scan_plugin.c`
- Modify: `CMakeLists.txt` gốc (target `mikmap_fake_scan_plugin` dạng `SHARED`, cộng bản `mikmap_fake_scan_plugin_v999` biên dịch với `-DFAKE_API_VERSION=999`; `mikmap_tests` nhận `FAKE_PLUGIN_PATH="$<TARGET_FILE:mikmap_fake_scan_plugin>"` và `FAKE_PLUGIN_V999_PATH=...` qua `target_compile_definitions`, kèm `add_dependencies`; Linux cần thêm `${CMAKE_DL_LIBS}` cho `mikmap_io`)
- Test: `tests/test_plugin.cpp`

**Interfaces:**
- Consumes: `ScanSource`, `LaserScan`, `kMaxScanPoints` (Task 6–7).
- Produces — ABI (`mikmap_sensor_plugin.h`), mọi hàm `extern "C"`, kiểu C thuần, chuỗi UTF-8:
  ```c
  #define MIKMAP_SENSOR_API 1
  int         mikmap_sensor_api_version(void);            /* phải trả MIKMAP_SENSOR_API */
  const char* mikmap_sensor_name(void);                   /* hiện trong menu "+ Add device", vd "YDLIDAR" */
  const char* mikmap_sensor_default_config(void);         /* JSON, điền sẵn vào ô config của thiết bị mới */
  void*       mikmap_sensor_create(const char* configJson);   /* NULL nếu config sai */
  int         mikmap_sensor_start(void* s);               /* 0 = ok, khác 0 = lỗi (đọc mikmap_sensor_error) */
  int         mikmap_sensor_poll(void* s, float* angleRad, float* rangeMm, int maxPoints);
                /* >0: số điểm của MỘT scan mới · 0: chưa có scan mới · <0: lỗi, host sẽ dừng rồi tạo lại.
                   Được phép chặn, nhưng phải trả về trong ≤ 1000 ms. rangeMm == 0 nghĩa là điểm không hợp lệ.
                   angleRad: 0 = phía trước, tăng ngược chiều kim đồng hồ (cùng quy ước với Hokuyo). */
  const char* mikmap_sensor_error(void* s);               /* chuỗi sống tới lần gọi kế tiếp; "" nếu không lỗi */
  void        mikmap_sensor_stop(void* s);
  void        mikmap_sensor_destroy(void* s);
  ```
  Host chỉ gọi các hàm này từ **một** thread (thread của `PluginSource`); riêng `api_version`/`name`/`default_config` gọi từ thread chính lúc nạp. Plugin không cần thread-safe.
- Loader / nguồn:
  ```cpp
  struct SensorPlugin { std::string path, name, defaultConfig; /* + 9 con trỏ hàm + handle thư viện */ };
  bool loadSensorPlugin(const std::string& path, SensorPlugin& out, std::string& err);
       // err nêu đường dẫn; thiếu hàm → "missing export <tên>"; lệch phiên bản → "API version <n>, MikMap needs 1"
  std::vector<SensorPlugin> scanPluginDir(const std::string& dir, std::vector<std::string>& errors);   // *.dll / *.so / *.dylib
  struct PluginConfig { std::string configJson; double reconnectSec = 1.0; };
  class PluginSource final : public ScanSource {
  public:
    PluginSource(const SensorPlugin& p, PluginConfig cfg);   // SensorPlugin phải sống lâu hơn nguồn
    const char* typeName() const override;                   // = p.name
    bool start() override; void stop() override;
    SourceStatus status() const override; std::string lastError() const override;
  };
  ```
- Windows: nạp bằng `LoadLibraryExW(path, nullptr, LOAD_WITH_ALTERED_SEARCH_PATH)` với **đường dẫn tuyệt đối**, để DLL phụ thuộc nằm cạnh plugin (vd `ydlidar_sdk.dll`) được tìm thấy. Không làm thế thì plugin chỉ chạy khi `ydlidar_sdk.dll` nằm cạnh `mikmap.exe`.
- Vòng thread `PluginSource`: `create(config)` (NULL thì báo Error "plugin rejected config") → `start` → lặp `poll` vào hai mảng `float[kMaxScanPoints]` là thành viên của lớp (không cấp phát trong vòng lặp). Mỗi lần `>0` thì chép vào `LaserScan` rồi gọi `onScan`. `<0` hoặc `start` lỗi thì `onLost()`, status `Error` + chép `mikmap_sensor_error`, gọi `stop` + `destroy`, chờ `reconnectSec`, tạo lại. `stop()` của host: hạ cờ chạy, join (poll trả về trong ≤ 1 s), `stop` + `destroy`.
- Plugin giả (`fake_scan_plugin.c`): name `"Fake scan"`, default config `{"fail_after":0}`. Mỗi `poll` ngủ 10 ms rồi trả 1081 tia (−135°…+135°, bước 0.25°), tường 2000 mm, cộng một vật rộng 40 mm ở (1000, 0). Với `fail_after = N > 0`: sau N scan thì `poll` trả −1, error `"fake: cable pulled"`. Đọc `fail_after` bằng `strstr` + `atoi` (plugin C không có parser JSON, mà cũng không cần).

- [ ] **Step 1: Viết test thất bại** (`tests/test_plugin.cpp`):
  - `"Plugin: nap duoc, dung ten va config mac dinh"`: `loadSensorPlugin(FAKE_PLUGIN_PATH)` → true, `name == "Fake scan"`, `defaultConfig` chứa `"fail_after"`.
  - `"Plugin: lech phien ban API bi tu choi"`: nạp `FAKE_PLUGIN_V999_PATH` → false, `err` chứa `"API version 999"`.
  - `"Plugin: duong dan khong ton tai"`: → false, `err` chứa đường dẫn đó.
  - `"★ Plugin: scan → 1 touch qua cung pipeline voi Hokuyo"`: `PluginSource` với `{"fail_after":0}` → trong 2 s `status()==Running`, `scans().consume()` có `count == 1081`, frame có `count == 1` và `x` trong khoảng 975…1025.
  - `"★ Plugin: poll loi → Up, Error co thong bao, tu tao lai"`: `{"fail_after":5}` → ring có `Up` cho id đã `Down`, `lastError()` chứa `"cable pulled"`, và trong 3 s `scansReceived()` vượt quá 5 (instance mới đã được tạo).
- [ ] **Step 2: Chạy, xác nhận thất bại.** Run: `cmake -DBUILD=build -P cmake/configure.cmake && cmake --build build --target mikmap_tests -j && ctest --test-dir build --output-on-failure`. Expected: lỗi biên dịch `PluginLoader.h`.
- [ ] **Step 3: Cài** theo Interfaces.
- [ ] **Step 4: Chạy lại, xác nhận đạt.** Expected: PASS, 0 cảnh báo. Hook layering không báo gì (`engine/io` chỉ dùng API hệ điều hành, giống socket).
- [ ] **Step 5: Commit** `git add engine/io/plugin engine/io/sources/PluginSource.* tests/plugins tests/test_plugin.cpp CMakeLists.txt && git commit -m "io: C-only sensor plugin ABI — a DLL built with any compiler can feed the lidar pipeline"`

### Task 10: Plugin trong app (thư mục plugin, loại thiết bị Plugin)

**Files:**
- Modify: `src/app.h` (`DevKind::Plugin = 3`; `Device` thêm `std::string plugin, config;`), `src/project.cpp` (serialize `"plugin"`, `"config"`), `src/sources.cpp` (quét plugin lúc khởi động, tạo `PluginSource`), `src/sensor.cpp` (menu "+ Add device" có thêm một mục cho mỗi plugin; card Plugin có ô config), `src/main.cpp` (`SensorSelfTest`)
- Modify: `src/CMakeLists.txt`. `src/` là project CMake riêng, không thấy target của CMake gốc, nên tự build một bản `${REPO}/tests/plugins/fake_scan_plugin.c` thành target `SHARED` riêng, xuất thẳng vào `$<TARGET_FILE_DIR:mikmap>/plugins/sensors/`. Bọc trong `option(MIKMAP_TEST_PLUGINS ... ON)`, và workflow release truyền `-DMIKMAP_TEST_PLUGINS=OFF` để bản phát hành không mang plugin giả theo.
- Modify: `.github/workflows/release.yml:70` (`cmake -S src -B src/build -DCMAKE_BUILD_TYPE=Release -DMIKMAP_TEST_PLUGINS=OFF`)

**Interfaces:**
- Consumes: `scanPluginDir`, `SensorPlugin`, `PluginSource` (Task 9); khung nhìn lidar theo `ScanSource` (Task 8).
- Produces:
  ```cpp
  std::string SensorPluginDir();                      // <thư mục chứa mikmap(.exe)>/plugins/sensors
  const std::vector<mikmap::SensorPlugin>& SensorPlugins();   // quét một lần lúc khởi động
  ```
- `Device::plugin` là **tên file** (vd `"mikmap_ydlidar.dll"`), không phải đường dẫn tuyệt đối, để dự án mở được trên máy diễn có cài ở chỗ khác. `Device::config` là chuỗi JSON. Thiết bị mới thì lấy `defaultConfig` của plugin; người dùng sửa trong một `ui::TextField` trên card. Config đổi thì chữ ký `kind|plugin|config` đổi, và nguồn được tạo lại (cơ chế của Task 2). `// shortcut: config là JSON thô; khi có plugin thứ hai thì thêm mikmap_sensor_params() để mô tả từng trường, giống setupParameters của TD`.
- Mở dự án mà không thấy file plugin: thiết bị vẫn nằm trong danh sách (không mất cấu hình), card màu red, `error = "plugin not found: <tên file> in <SensorPluginDir()>"`. Plugin nạp lỗi lúc quét: `A.notify` một lần cho mỗi lỗi.
- Card Plugin dùng lại khung nhìn lidar, ROI mm, slider blob và capture wizard (Task 8), không có nhánh UI riêng.

- [ ] **Step 1: Mở rộng `SensorSelfTest` cho thất bại:**
  - Thêm thiết bị `DevKind::Plugin`, `plugin = <tên file plugin giả>`, `config = "{\"fail_after\":0}"`, `enabled = true` → trong 2 s: `DeviceScan(d) != nullptr`, `d.touches.size() == 1`.
  - Lưu → nạp → `plugin` và `config` còn nguyên.
  - Đổi `plugin = "no_such_plugin.dll"` → tick → `status == (int)SourceStatus::Error` và `error` chứa `"plugin not found"`.
- [ ] **Step 2: Chạy, xác nhận thất bại.** Run: build app + `ctest --test-dir src/build -C Release --output-on-failure`. Expected: `project_roundtrip` FAIL.
- [ ] **Step 3: Cài** theo Interfaces.
- [ ] **Step 4: Chạy lại, xác nhận đạt;** chụp `--shot plugin.png --screen 2 --frames 60`. Expected: menu "+ Add device" có mục "Fake scan"; card Plugin hiện scan và một blob, như Hokuyo.
- [ ] **Step 5: Commit** `git add src CMakeLists.txt && git commit -m "sensor: devices backed by plugin DLLs; a missing plugin keeps the device and says where it looked"`

### Task 11: Plugin YDLIDAR bọc `ydlidar_sdk.dll`

> **Đang chờ input:** `YDLidarCHOP.cpp` của người dùng. Các giá trị `setlidaropt` (loại lidar, baudrate, cổng mạng, tần số quét, sample rate, `LidarPropSingleChannel`, `LidarPropIntenstiy`, khoảng góc/khoảng cách…) phải **chép đúng** từ file đó cho từng model. Không đoán. Chưa có file thì dừng ở Step 3.

Đã biết từ việc đọc vỏ DLL (2026-10-27): `YDLidar.dll` (TouchDesigner) gọi đúng 6 hàm của `CYdLidar` (`setlidaropt`, `initialize`, `turnOn`, `doProcessSimple(LaserScan&)`, `turnOff`, `disconnecting`). Hai model có trong menu: `G4 (Triangle Model)` qua `COM (Serial)`, và `TEA (ToF Model)` qua `NET (TCP)`. `ydlidar_sdk.dll` được build bằng MSVC (`MSVCP140`), nên plugin **phải** build bằng MSVC để ABI C++ của `CYdLidar` khớp. Đây đúng là lý do phần ranh giới với MikMap phải là C.

**Files:**
- Create: `plugins/ydlidar/CMakeLists.txt` (project CMake riêng, như `src/`; build `mikmap_ydlidar` dạng `SHARED`; biến `YDLIDAR_SDK_DIR` trỏ tới bản cài SDK có `include/CYdLidar.h`, `lib/ydlidar_sdk.lib`, `bin/ydlidar_sdk.dll`; sau khi build, chép cả hai DLL vào `plugins/ydlidar/build/out/`)
- Create: `plugins/ydlidar/ydlidar_plugin.cpp` (cài 9 hàm ABI quanh một `CYdLidar`)
- Create: `plugins/ydlidar/scan_convert.h` (hàm thuần, test được không cần phần cứng)
- Create: `plugins/ydlidar/test_scan_convert.cpp` (exe nhỏ dùng `tests/TestHarness.h`, `add_test` trong CMake của plugin)
- Create: `plugins/ydlidar/README.md` (build, chép hai DLL vào `plugins/sensors/` cạnh `mikmap.exe`, bảng config)

**Interfaces:**
- Consumes: `engine/io/plugin/mikmap_sensor_plugin.h` (Task 9); `engine/core/util/Json.{h,cpp}` biên dịch thẳng vào plugin để đọc config (chỉ STL, không kéo gì thêm).
- Config JSON — khoá đặt theo đúng tham số của CHOP để người dùng TouchDesigner nhận ra:
  ```json
  {"model": "TEA", "comm": "net", "address": "192.168.0.11", "port": 0, "baudrate": 0}
  ```
  `model ∈ {"G4","TEA"}`, `comm ∈ {"serial","net"}`. `port` (net) / `baudrate` (serial) và mọi `setlidaropt` theo từng model lấy từ `YDLidarCHOP.cpp`. `mikmap_sensor_default_config()` trả cấu hình TEA mặc định của CHOP.
- `int ConvertScan(const LaserScan& in, float* angleRad, float* rangeMm, int maxPoints)` (`scan_convert.h`): `rangeMm = point.range × 1000` (SDK trả mét); `range ≤ 0` thì ra 0 (không hợp lệ); chuẩn hoá góc về (−π, π]; quá `maxPoints` thì cắt bớt. Chiều quay của SDK có thể ngược quy ước của ABI. Không lật ở đây: `H_s` xử lý được lật, còn ROI chỉ cần nhất quán. Ghi một dòng chú thích nói rõ điều này.
- `mikmap_sensor_poll`: gọi `doProcessSimple(scan)`. Thành công thì trả `ConvertScan(...)`. Thất bại thì trả −1 kèm `DescribeError()`. SDK tự chặn chờ scan, nên phải đặt timeout của SDK ≤ 1000 ms, đúng hợp đồng ABI.
- `mikmap_sensor_stop` → `turnOff()` rồi `disconnecting()`. `mikmap_sensor_create` bắt **mọi** exception C++ và trả NULL: exception không bao giờ được vượt qua biên C.

- [ ] **Step 1: Viết test thất bại** (`test_scan_convert.cpp`): dựng tay một `LaserScan` gồm 3 điểm `{angle 0, range 1.0}`, `{angle π/2, range 0}`, `{angle 3π/2, range 2.5}`:
  - → trả 3; `rangeMm[0] == 1000`; `rangeMm[1] == 0`; `rangeMm[2] == 2500`; `angleRad[2]` gần `−π/2` (±1e-5).
  - `maxPoints = 2` → trả 2.
- [ ] **Step 2: Chạy, xác nhận thất bại.** Run: `cmake -S plugins/ydlidar -B plugins/ydlidar/build -DYDLIDAR_SDK_DIR=<đường dẫn SDK> && cmake --build plugins/ydlidar/build --config Release && ctest --test-dir plugins/ydlidar/build -C Release --output-on-failure`. Expected: lỗi biên dịch `scan_convert.h`.
- [ ] **Step 3: Cài `scan_convert.h` + `ydlidar_plugin.cpp`;** chép `setlidaropt` từ `YDLidarCHOP.cpp`.
- [ ] **Step 4: Chạy lại, xác nhận đạt.** Rồi nạp thử bằng loader của MikMap không cần phần cứng: `test_scan_convert` gọi thêm `loadSensorPlugin("<build>/out/mikmap_ydlidar.dll")` → true, `name == "YDLIDAR"`. Việc này kiểm được cả việc tìm thấy `ydlidar_sdk.dll` nằm cạnh plugin.
- [ ] **Step 5: Kiểm với máy thật** (TEA qua mạng, và G4 qua COM nếu có): chép hai DLL vào `plugins/sensors/` cạnh `mikmap.exe` → "+ Add device > YDLIDAR" → Running; scan trên radar khớp với phòng thật; đi vào ROI thấy blob; wizard 4 điểm. Rút cáp mạng → card Error rồi tự nối lại, không có touch nào kẹt. Không có máy thì ghi **chưa kiểm phần cứng**.
- [ ] **Step 6: Tài liệu.** `features.md`: thêm `G22 Plugin cảm biến DLL (ABI C)` `[x]` và `G23 YDLIDAR (plugin, bọc YDLidar-SDK)` `[x]`/`[~]` theo kết quả Step 5. Ghi chú kiểm tra: plugin chạy chung tiến trình, nên plugin sập thì app sập theo.
- [ ] **Step 7: Commit** `git add plugins/ydlidar features.md && git commit -m "plugins: YDLIDAR via YDLidar-SDK behind the C ABI — the MSVC C++ ABI stays inside the plugin"`

---

## Kiểm cuối (sau Task 11)

- `ctest --test-dir build --output-on-failure` và `ctest --test-dir src/build -C Release --output-on-failure` đều 0 đỏ. `python -m unittest discover -s tools/pose_bridge` OK. `ctest --test-dir plugins/ydlidar/build -C Release` OK (máy có YDLidar-SDK).
- Agent `reviewer` của repo duyệt toàn nhánh (layering, render thread không lock/alloc, `/W4`).
- Nói rõ trong báo cáo những gì chỉ build/test mà **chưa chạy trên phần cứng thật** (webcam, Hokuyo, YDLIDAR).

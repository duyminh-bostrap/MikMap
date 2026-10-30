# A: Khung camera + Camera 2D + Body — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Một sensor `Camera 2D` bắt webcam trong App, có thuộc tính Mode (Off · Body), chạy nhận dạng thân người nhiều người (RTMO qua ONNX Runtime + CUDA nạp từ gói P), giữ id người ổn định, và đưa điểm khớp vào đường sensor hiện có (calibration, warp, gộp, OSC/UDP).

**Architecture:**
- `engine/io/camera` khai báo thiết bị theo luồng (`ICameraDevice`, `CameraCaps`) và cài Camera 2D bằng Media Foundation.
- `engine/io/ai` nạp ONNX Runtime lúc chạy, chạy RTMO, giải mã đầu ra, theo dõi người.
- `engine/io/sources/CameraSource` ghép chúng trong hai thread và xuất `SensorFrame` thô + khung xem trước qua `TripleBuffer`.
- `src/` thêm `DevKind::Camera2D`/`Camera3D`, `Device::camMode`, Properties và stage INPUT hiện ảnh camera cùng khung xương.

**Tech Stack:** C++20, Media Foundation (Windows), ONNX Runtime C API (header MIT vendored, DLL từ gói P), `tests/TestHarness.h`, Dear ImGui (`src/`).

**Spec:** `docs/superpowers/specs/2026-10-30-ai-body-tracking-a-design.md`

**Nền:** nhánh `feat/sensor-skeleton-hokuyo`. Plan P (gói sensor) đã có `PacksDir()`, `PackAvailableForDevice`, `PackStateFor`, `Device::needsPack`, thẻ thiếu gói trong popup.

## Global Constraints

- `engine/core` chỉ STL; `engine/io` chỉ STL + `engine/core` + API hệ điều hành (Media Foundation, `LoadLibraryExW`); không biết Screen, Slice, ImGui.
- Render thread không lock, không cấp phát trong vòng lặp nóng, không I/O. Camera và suy luận ở thread riêng; giao tiếp chỉ qua `TripleBuffer` (khung mới nhất thắng).
- Sensor thread gửi **toạ độ thô** (ảnh, 0–1); calibration/warp/gộp/chuẩn hoá screen ở render thread như sensor khác.
- Đánh số khớp theo **MediaPipe** (0..32): id điểm = `slot · 100 + chỉ số MediaPipe`; khớp COCO-17 ánh xạ sang chỉ số MediaPipe; `kCalibJoint = 16` (cổ tay phải) dùng được ngay.
- `z` của điểm khớp Camera 2D = **độ tin cậy 0–1**.
- `kMaxTouchPoints` = **512**; mọi đếm điểm dùng kiểu đủ rộng (không còn `uint8_t`).
- Gửi OSC/UDP: mỗi datagram **≤ 1200 byte**; một khung có thể thành nhiều datagram.
- ONNX Runtime **không link cứng**: nạp từ thư mục gói `onnx-runtime-cuda`; App build và chạy khi chưa có gói. Header C API vendored ở `third_party/onnxruntime/` (giấy phép MIT, phiên bản ghim, SHA-256 ghi trong README của thư mục đó).
- Mô hình RTMO (Apache-2.0) chỉ đến từ gói `pose-rtmo`; repo không chứa tệp mô hình hay DLL.
- Không có CUDA hoặc CUDA lỗi: Auto ⇒ chạy CPU kèm cảnh báo "Running on CPU — slow"; "CUDA only" ⇒ lỗi rõ, không chạy.
- `Device::needsPack` suy ra từ Mode: `Body` → `"pose-rtmo"`, `Off` → `""`.
- Linux/macOS: camera và ONNX Runtime trả lỗi "unsupported", mã vẫn biên dịch được.
- Dùng `std::filesystem::path(std::u8string(...))` cho đường dẫn UTF-8 (không dùng `u8path`).
- MSVC `/W4 /permissive- /fp:precise /utf-8`, 0 cảnh báo mới. Dùng helper `ui::` có sẵn.
- Build/test:
  ```
  cmake --build build --config Release --target mikmap_tests -j && ctest --test-dir build -C Release --output-on-failure
  cmake --build src/build --config Release -j && ctest --test-dir src/build -C Release --output-on-failure
  ```
- Commit không có dòng attribution / Co-Authored-By.

## Review Focus

1. **Camera bị rút giữa buổi hoặc đang bị app khác giữ:** sensor báo lỗi kèm tên camera, thử lại mỗi 2 s, gửi `frame` 0 người, id người xoá sau 0,5 s; sensor khác và App không ảnh hưởng. Test ở Task 3, 7.
2. **DLL ONNX Runtime sai phiên bản, thiếu tệp, hoặc CUDA không khởi tạo được:** Auto lùi về CPU với cảnh báo; CUDA only báo lỗi; không bao giờ sập App vì nạp lỗi. Test ở Task 4, 7.
3. **Suy luận chậm hơn camera:** chỉ xử lý khung mới nhất, hàng đợi không phình, độ trễ đo từ `tCaptureNs`. Test ở Task 7.
4. **Đông người** (nhiều hơn `maxPeople`, hoặc 30 người × 17 khớp gần 512): giữ người tin cậy nhất, không bao giờ vượt sức chứa; gửi ra chia gói đúng và bên nhận ghép lại đủ. Test ở Task 1, 6.
5. **Đổi Mode Body ↔ Off hoặc gỡ gói khi sensor đang chạy:** nguồn khởi động lại sạch, `needsPack` theo Mode, tick không làm bẩn dự án. Test ở Task 8.

---

### Task 1: Sức chứa 512 điểm và chia gói gửi ra

**Files:**
- Modify: `engine/io/SensorFrame.h:29,46-58` (`kMaxTouchPoints = 512`; `count` thành `uint16_t`; giữ POD)
- Modify: `engine/io/net/TouchSender.h:28-42`, `engine/io/net/TouchSender.cpp` (`OutPacket.count` `uint16_t`; mã hoá thành nhiều datagram)
- Modify: mọi chỗ còn giả định `count` 8 bit hoặc mảng cố định 64 (grep `kMaxTouchPoints`, `uint8_t count`: `LidarBlobs.cpp:79-85`, `MockSource`, `OscSource.cpp:367`, `src/`)
- Test: `tests/test_touchsender.cpp`, `tests/test_io.cpp` (hoặc tệp test đang chứa `SensorFrame`)

**Interfaces:**
- Produces:
  ```cpp
  inline constexpr int kMaxTouchPoints = 512;          // SensorFrame.h
  inline constexpr size_t kMaxDatagram = 1200;         // TouchSender.h
  // Encode points [first, n) of p into one datagram (OSC bundle or text lines), never above cap.
  // Returns bytes written (0 if not even one point fits) and sets next = index of the first point not written.
  size_t encodeOscChunk(const OutPacket& p, int first, uint8_t* buf, size_t cap, int& next);
  size_t encodeTextChunk(const OutPacket& p, int first, uint8_t* buf, size_t cap, int& next);
  ```
  The first chunk of a frame starts with `/mikmap/<dev>/frame i:frame i:count`; following chunks only carry `/touch` messages. The sender thread loops chunks until `next == count`. `encodeOsc`/`encodeText` stay as the single-chunk wrappers used by existing tests.

- [ ] **Step 1: Failing tests:** `sizeof(SensorFrame)` is a compile-time constant and `std::is_trivially_copyable_v<SensorFrame>`; `addPoint` accepts 512 points and refuses the 513th; a `TripleBuffer<SensorFrame>` round-trips 512 points; an `OutPacket` with 510 points → every chunk from `encodeOscChunk` ≤ 1200 bytes, the chunks fed to `OscSource::feedPacket` (prefix `/mikmap/dev1/touch`) yield all 510 ids and x/y (±1e-6); same for `encodeTextChunk` (lines parse back); an empty frame still sends one chunk with `frame … 0`; the existing single-chunk tests still pass.
- [ ] **Step 2-4:** RED → implement → GREEN. Run both suites (the app uses these types).
- [ ] **Step 5:** Commit `io: 512 touch points per frame and send-out split into datagrams of at most 1200 bytes`.

### Task 2: Giao diện camera và bảng khả năng

**Files:**
- Create: `engine/io/camera/CameraDevice.h`, `engine/io/camera/CameraDevice.cpp`
- Test: `tests/test_camera.cpp`

**Interfaces:**
- Produces (namespace `mikmap`):
  ```cpp
  struct StreamInfo { int width = 0, height = 0; float fps = 0; };
  struct CameraCaps { bool hasColor = false, hasDepth = false, hasIR = false; StreamInfo color, depth, ir; };
  struct CameraFrame {                          // owned buffers, reused by the producer (no per-frame allocation after warm-up)
    int width = 0, height = 0; std::vector<uint8_t> rgb;     // RGB24, tightly packed
    int depthW = 0, depthH = 0; std::vector<uint16_t> depthMm;   // empty for Camera 2D
    int64_t tCaptureNs = 0; uint64_t index = 0; };
  struct CameraInfo { std::string id, name; };   // id is stable across runs (symbolic link / device path), name is for display
  class ICameraDevice { public: virtual ~ICameraDevice() = default;
    virtual bool open(const std::string& id, int width, int height, std::string& err) = 0;
    virtual void close() = 0;
    virtual CameraCaps caps() const = 0;
    virtual bool read(CameraFrame& out, int timeoutMs, std::string& err) = 0;   // blocking, on the capture thread only
  };
  enum class CamMode { Off = 0, Body = 1 };      // B adds Hand/Face/Full/Gesture
  bool modeSupported(CamMode m, const CameraCaps& c, std::string& why);   // Off: always; Body: needs hasColor
  class FakeCamera : public ICameraDevice { /* scripted frames for tests; can simulate unplug after N frames */ };
  ```

- [ ] **Step 1: Failing tests:** `modeSupported(Off, {})` true; `Body` with `hasColor` false → false and `why` mentions "Color"; `Body` with a Color+Depth caps → true; `FakeCamera` returns scripted frames in order, honours `timeoutMs`, and after the configured unplug returns false with an error.
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** Commit `io/camera: device interface by stream, capability table, fake camera`.

### Task 3: Camera 2D bằng Media Foundation

**Files:**
- Create: `engine/io/camera/MfCamera.h`, `engine/io/camera/MfCamera.cpp`
- Modify: root `CMakeLists.txt` (`if(WIN32)` link `mfplat mfreadwrite mf mfuuid ole32` to `mikmap_io`)
- Test: `tests/test_mfcamera.cpp`

**Interfaces:**
- Produces:
  ```cpp
  std::vector<CameraInfo> listCameras(std::string& err);          // empty + err off Windows
  class MfCamera : public ICameraDevice { /* IMFSourceReader, MFVideoFormat_RGB24 or NV12/YUY2 converted to RGB24 */ };
  ```
  `open` picks the closest supported size ≥ requested (else the largest), sets `tCaptureNs` from `std::chrono::steady_clock` when the sample is read, and returns a clear error when the device is busy ("in use by another application") or gone. `MFStartup`/`MFShutdown` and COM init are reference-counted per thread that uses them.

- [ ] **Step 1: Failing tests:** `listCameras` never throws (returns a list or an error); opening an unknown id fails with a message containing the id; when the environment variable `MIKMAP_TEST_CAMERA` is set to a camera id/index, open 640×480, read 10 frames: sizes match `caps().color`, `rgb.size() == w*h*3`, `tCaptureNs` strictly increasing, `index` increasing; otherwise print "camera test skipped (set MIKMAP_TEST_CAMERA)" and pass. Off Windows: `listCameras` returns empty with "unsupported".
- [ ] **Step 2-4:** RED → implement → GREEN. Run once with the user's webcam if one is attached (`MIKMAP_TEST_CAMERA=0`) and report the result; say plainly if no camera was available.
- [ ] **Step 5:** Commit `io/camera: Camera 2D through Media Foundation`.

### Task 4: Nạp ONNX Runtime lúc chạy

**Files:**
- Create: `third_party/onnxruntime/onnxruntime_c_api.h`, `third_party/onnxruntime/README.md` (version, source URL, SHA-256, licence MIT), `engine/io/ai/OrtRuntime.h`, `engine/io/ai/OrtRuntime.cpp`
- Modify: root `CMakeLists.txt` (include dir for `third_party/onnxruntime` on `mikmap_io`, no link)
- Test: `tests/test_ortruntime.cpp`

**Interfaces:**
- Produces:
  ```cpp
  enum class OrtDevice { Auto, CudaOnly, Cpu };
  struct OrtTensorInfo { std::string name; std::vector<int64_t> shape; };
  class OrtRuntime {
  public:
    bool load(const std::string& runtimeDirUtf8, std::string& err);          // LoadLibraryExW(<dir>/onnxruntime.dll, LOAD_WITH_ALTERED_SEARCH_PATH) + GetProcAddress("OrtGetApiBase"); refuses an API version below the vendored header's ORT_API_VERSION
    bool createSession(const std::string& modelPathUtf8, OrtDevice dev, std::string& err, bool* usedCuda = nullptr);   // Auto: try CUDA EP, fall back to CPU and set *usedCuda = false
    std::vector<OrtTensorInfo> inputs() const, outputs() const;
    bool run(const float* input, const std::vector<int64_t>& shape, std::vector<std::vector<float>>& outputs, std::vector<std::vector<int64_t>>& outShapes, std::string& err);   // reuses output buffers
    ~OrtRuntime();                                                            // releases session/env; never FreeLibrary (consistent with PluginLoader)
  };
  ```
  Vendored header: ONNX Runtime **1.20.1** `include/onnxruntime_c_api.h` from the official GitHub release; record its SHA-256 in the README. If the implementer cannot download it (no network), report BLOCKED rather than retyping it.

- [ ] **Step 1: Failing tests:** `load` of a missing dir → false with a message naming the path; `load` of a dir containing a non-DLL file named `onnxruntime.dll` → false, no crash; `createSession` before `load` → false; when `MIKMAP_TEST_ORT_DIR` and `MIKMAP_TEST_POSE_MODEL` are set: load, create a session with `Auto` (report `usedCuda`), inputs/outputs non-empty, `run` on a zero tensor of the input shape succeeds; with `Cpu` also succeeds; otherwise print the skip line and pass.
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** Commit `io/ai: load ONNX Runtime from a pack at run time (CUDA with CPU fallback)`.

### Task 5: RTMO — tiền xử lý, giải mã, ánh xạ khớp

**Files:**
- Create: `engine/io/ai/PoseModel.h`, `engine/io/ai/PoseModel.cpp`
- Test: `tests/test_posemodel.cpp`

**Interfaces:**
- Consumes: `OrtRuntime` (Task 4), `CameraFrame` (Task 2).
- Produces:
  ```cpp
  struct Keypoint { float x = 0, y = 0, conf = 0; };          // image space 0..1
  struct PersonPose { float box[4] = {}; float score = 0; Keypoint kp[17]; };
  struct Letterbox { float scale = 1, padX = 0, padY = 0; int inW = 640, inH = 640; };
  Letterbox letterboxRgbToChw(const CameraFrame& f, int inW, int inH, std::vector<float>& outChw);   // match rtmlib's RTMO preprocessing: channel order BGR, values 0..255 (no mean/std), pad value 114; if the exported model's metadata says otherwise, follow the model and record it in the report; no per-call allocation after the first
  // RTMO end-to-end export (rtmlib style): outputs "dets" [1,N,5] (x1,y1,x2,y2,score) and "keypoints" [1,N,17,3] (x,y,score) in network input pixels
  int decodeRtmo(const float* dets, const float* kps, int n, const Letterbox& lb, int imgW, int imgH,
                 float personThr, float jointThr, PersonPose* out, int maxOut);   // undo letterbox, normalise to 0..1, drop people below personThr and joints below jointThr (conf 0), sort by score desc
  extern const int kCocoToMediaPipe[17];   // 0 nose->0, 1 l-eye->2, 2 r-eye->5, 3 l-ear->7, 4 r-ear->8, 5 l-shoulder->11, 6 r-shoulder->12, 7 l-elbow->13, 8 r-elbow->14, 9 l-wrist->15, 10 r-wrist->16, 11 l-hip->23, 12 r-hip->24, 13 l-knee->25, 14 r-knee->26, 15 l-ankle->27, 16 r-ankle->28
  class PoseModel { public: bool init(OrtRuntime& rt, std::string& err);   // reads input size from the session
                    int infer(const CameraFrame& f, float personThr, float jointThr, PersonPose* out, int maxOut, std::string& err); };
  ```

- [ ] **Step 1: Failing tests (no model needed):** `kCocoToMediaPipe` equals the table above and `kCocoToMediaPipe[10] == 16 == kCalibJoint`-equivalent (right wrist); `letterboxRgbToChw` of a 1280×720 frame into 640×640 gives `scale == 0.5`, `padY == 140`, `padX == 0`; `decodeRtmo` on hand-made tensors (3 people, one below `personThr`, one joint below `jointThr`) returns 2 people sorted by score, coordinates mapped back to 0..1 within 1e-4, the low joint with `conf == 0`; `maxOut` caps the count; `n == 0` returns 0. With `MIKMAP_TEST_ORT_DIR`/`MIKMAP_TEST_POSE_MODEL` and the sample image `tests/data/pose_three_people.jpg` **only if the implementer can obtain a licence-clean image** (otherwise a synthetic check that `infer` runs on a gray frame and returns 0 people): `infer` finds 3 people.
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** Commit `io/ai: RTMO pre/post-processing and COCO-to-MediaPipe joint map`.

### Task 6: Theo dõi người

**Files:**
- Create: `engine/io/ai/PersonTracker.h`, `engine/io/ai/PersonTracker.cpp`
- Test: `tests/test_persontracker.cpp`

**Interfaces:**
- Consumes: `PersonPose` (Task 5).
- Produces:
  ```cpp
  struct TrackerParams { int maxPeople = 10; double keepSec = 0.5; float matchIou = 0.3f; int smoothing = 0 /*0 off,1 light,2 medium*/; };
  struct TrackedPerson { int slot = 0; PersonPose pose; double lastSeen = 0; };
  class PersonTracker { public:
    void setParams(const TrackerParams&);
    int update(const PersonPose* people, int n, double tSec, TrackedPerson* out, int maxOut);   // returns active people; slot = stable id 0..; never more than maxPeople; keeps the most confident when over
    void reset(); };
  ```
  Matching: Hungarian or greedy by IoU of keypoint bounding boxes, then hip-centre distance as tie-break; a person unseen for `keepSec` frees its slot; the lowest free slot is reused.

- [ ] **Step 1: Failing tests:** two people walking past each other (crossing paths over 10 frames) keep their slots; one disappears for 0.4 s and returns → same slot; for 0.6 s → slot freed and later reused as the lowest free; 15 detections with `maxPeople = 10` → 10 returned, the 5 lowest scores dropped; slot ids are always in `0 .. maxPeople-1`; lowering `maxPeople` at run time drops the highest slots first; `smoothing` 0 returns raw coordinates exactly.
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** Commit `io/ai: person tracker with stable slots and a people limit`.

### Task 7: CameraSource (capture + suy luận, hai thread)

**Files:**
- Create: `engine/io/sources/CameraSource.h`, `engine/io/sources/CameraSource.cpp`
- Test: `tests/test_camerasource.cpp`, `tests/data/replay_two_people.mikpose`

**Interfaces:**
- Consumes: Tasks 2–6.
- Produces:
  ```cpp
  struct CameraSourceConfig { std::string cameraId; int width = 640, height = 480; bool flip = false;
    CamMode mode = CamMode::Off; OrtDevice device = OrtDevice::Auto; std::string runtimeDir, modelPath;
    float personThr = 0.5f, jointThr = 0.3f; TrackerParams tracker; std::string replayPath; };   // replayPath non-empty = no camera, no model
  struct CameraStats { float camFps = 0, inferFps = 0, inferMs = 0, latencyMs = 0; int people = 0; bool onCpu = false; };
  class CameraSource : public ISensorSource {      // same contract as other sources (start/stop/status/lastError/readSlot)
  public:
    explicit CameraSource(CameraSourceConfig, std::unique_ptr<ICameraDevice> dev = nullptr);   // nullptr → MfCamera; tests inject FakeCamera
    bool previewLatest(CameraFrame& out);          // copies the latest preview (downscaled to ≤ 640 wide) if newer; render thread
    CameraStats stats() const;                     // atomics
  };
  ```
  Threads: capture (reads the device, publishes the newest frame to a `TripleBuffer<CameraFrame>`, retries opening every 2 s on error) and infer (takes only the newest frame, runs `PoseModel` + `PersonTracker` when mode is Body, publishes `SensorFrame` with id `slot*100 + kCocoToMediaPipe[j]`, x/y image 0..1 (flipped if `flip`), z = conf, `tCaptureNs` from the camera frame; on camera loss publishes an empty frame after `keepSec`). Replay file `.mikpose`: text lines `t_sec slot joint x y conf` (MediaPipe joint index), a blank line ends a frame.

- [ ] **Step 1: Failing tests (FakeCamera / replay, no GPU):** replay of `replay_two_people.mikpose` yields frames whose ids are `0*100+16`, `1*100+16`, … and z equals the file's conf; `mode = Off` with a FakeCamera publishes previews and **no** points and never touches ONNX; `mode = Body` with no runtime dir → `status()` error "Needs pack" style message, no crash; FakeCamera unplug after 5 frames → error status mentioning the camera, an empty points frame within 0.6 s, and recovery (FakeCamera "replugged") within 2.5 s (**Review Focus 1**); a slow fake inference (inject a delay hook 100 ms) with a 30 fps fake camera → infer processes newest frames only, the published `tCaptureNs` is never older than 150 ms behind the newest capture, memory/queues bounded (**Review Focus 3**); runtime load failure path → `status` error, process alive (**Review Focus 2**); `stop()` joins both threads within 1 s even while the camera read is blocking (use `timeoutMs`).
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** Commit `io/sources: CameraSource — capture and inference threads, newest-frame-wins, replay mode`.

### Task 8: App — Camera 2D/3D, Mode, Properties, INPUT preview

**Files:**
- Modify: `src/app.h` (`DevKind { …, Camera2D = 4, Camera3D = 5 }`, `DevKindName`, `Device::camMode`, `camId`, `camW`, `camH`, `camFlip`, `camDevice`, `maxPeople`, `personThr`, `jointThr`, `smoothing`), `src/project.cpp` (schema `"cam":{"id","w","h","flip","mode","device","maxPeople","personThr","jointThr","smooth"}`, absent = defaults), `src/sources.cpp` (`Start()` ≈97: `Camera2D` → `CameraSource` with `runtimeDir = PacksDir()/onnx-runtime-cuda/<ver>`, `modelPath` from `pose-rtmo`; `Camera3D` → "No driver for this sensor yet"; `SensorPropSections`), `src/sensor.cpp` (popup card "Camera 2D" — Mode Off needs no pack, choosing Body sets `needsPack`; Properties per spec §5; INPUT shows the preview as a GL texture with `SkeletonView`-style bones on top; status line with fps/ms/CPU warning), `src/main.cpp` (`--replay <file>` test flag), PerfPanel line for camera sources
- Test: `SensorSelfTest` in `src/sources.cpp`

**Interfaces:**
- Consumes: `CameraSource`, `listCameras`, `modeSupported` (engine), `PacksDir()`, `PackAvailableForDevice`, `PackStateFor` (P).
- Produces:
  ```cpp
  std::string NeedsPackForMode(DevKind k, int camMode);   // Camera2D/3D: Body → "pose-rtmo", Off → ""; other kinds unchanged
  void SetCamMode(Device& d, int mode);                    // sets camMode and needsPack together, restarts the source on the next tick
  ```
  The preview texture is allocated once per size on the render thread and updated with `glTexSubImage2D` only when `previewLatest` returns a newer frame.

- [ ] **Step 1: Failing self-tests:** `Camera2D` with Mode and settings round-trips; an old file has no camera devices and loads unchanged; `SetCamMode(d, Body)` → `needsPack == "pose-rtmo"`, `Off` → `""`; with the pack missing and Mode Body the device does not start a source and `d.error` is "Needs pack: …"; with `--replay` (or the self-test's replay path) a Camera2D device with Mode Body runs through the real `SensorsTick` path: calibrated with 4 synthetic points and Send out on to a loopback `OscSource`, the receiver gets ids `16`, `116` with y flipped per `originBottom`; switching Mode Body→Off while running stops sending points within 1 s, restarts cleanly, and two ticks do not make `ProjectDirty()` true (**Review Focus 5**); `Camera3D` device shows "No driver for this sensor yet" and is not offered in the popup.
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** `--shot` of INPUT with `--replay` (bones over a gray preview), Properties (Mode Off and Body, pack missing), and the popup card; LOOK at them (Read) and report. Real webcam + GPU run: if the user's machine has the packs installed, record fps/ms for 1, 5, 10 people from the status line; otherwise say they were not measured.
- [ ] **Step 6:** Commit `sensor: Camera 2D with a Mode (Off / Body), body tracking in the app, camera preview on INPUT`.

### Task 9: Gói mẫu và tài liệu

**Files:**
- Create: `tools/packs/examples/onnx-runtime-cuda/pack.json`, `tools/packs/examples/pose-rtmo/pack.json`, `tools/packs/examples/README.md` (how to fill files/hashes with `mikmap_packtool`, where to obtain ONNX Runtime 1.20.1 GPU and RTMO weights, licences: MIT / Apache-2.0, CUDA/cuDNN version that matches)
- Modify: `features.md`, `README.md`, `.claude/CLAUDE.md`, `.claude/rules/design.md`, `tools/pose_bridge/README.md` (mention Camera 2D + Body as the in-app alternative and the crash trade-off), OSC protocol notes (z = confidence for Camera 2D; frames split into datagrams ≤ 1200 bytes)

- [ ] **Step 1:** Grep `src/*.cpp` and `engine/io/**` to confirm `CameraSource`, `MfCamera`, `OrtRuntime`, `PoseModel`, `PersonTracker`, `SetCamMode` are called on the app path.
- [ ] **Step 2:** Docs, with honest "Ghi chú kiểm tra": what was run with a real webcam/GPU and what was not; measured fps/ms or "not measured"; the in-process crash trade-off (ONNX Runtime crash takes the app down — keep `pose_bridge` for shows that must not stop); `kMaxTouchPoints` 512 and its P99 cost if measured.
- [ ] **Step 3:** Commit `docs: Camera 2D, body tracking, pack examples`.

---

## Kiểm cuối

- Hai bộ test xanh, chạy mỗi bộ hai lần; 0 cảnh báo mới `/W4`.
- P99 khung hình với 4 sensor (Hokuyo giả, Sim, Skeleton OSC, Camera 2D replay) đo trước/sau Task 1 bằng PerfPanel; ghi số.
- Một vòng thật nếu có webcam + gói: Camera 2D Mode Body nhận nhiều người, gửi OSC tới TouchDesigner hoặc `OscSource` loopback.
- Agent `reviewer` của repo duyệt toàn nhánh.

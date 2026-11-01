# B: Chế độ Hand, Face, Full — Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Camera có thêm Mode Hand, Face, Full chạy trên một đường ống top-down dùng chung; mỗi điểm mang nhóm (thân, tay trái, tay phải, mặt, bàn chân); gửi OSC theo địa chỉ nhóm mà không làm đổi gì cho người đang dùng Body.

**Architecture:**
- `TouchPoint` dùng một byte đệm có sẵn làm `group`, nên kích thước khung không đổi.
- Đối tượng pose được tổng quát hoá: tối đa 133 điểm, kèm số điểm thật và bên tay. `PersonTracker` của A theo dõi người, bàn tay hay khuôn mặt như nhau.
- `TopDownPose`: phát hiện → cắt affine → mô hình SimCC → giải mã. Một bảng `ModeModels` nói Mode nào dùng gói nào, bao nhiêu điểm, nhóm nào, điểm calibrate nào.
- `CameraSource` chọn đường ống theo Mode; App thêm Mode, lọc nhóm ở Full, vẽ theo nhóm.

**Tech Stack:** C++20, ONNX Runtime (nạp lúc chạy, `OrtRuntime` của A), `tests/TestHarness.h`, Dear ImGui.

**Spec:** `docs/superpowers/specs/2026-11-01-camera-modes-b-design.md`

**Nền:** plan A đã có trong code (`engine/io/camera`, `engine/io/ai/{OrtRuntime,PoseModel,PersonTracker}`, `engine/io/sources/CameraSource`, `kMaxTouchPoints = 512`, `OutPacket.hasZ`, `CamMode {Off, Body}`, `SetCamMode`, `NeedsPackForMode`).

## Global Constraints

- Mode = `Off · Body · Hand · Face · Full`; một Mode mỗi lúc trên một camera; "Pose" = Body.
- Mô hình (Apache-2.0): Hand = bộ phát hiện tay + RTMPose-hand (21 điểm); Face = bộ phát hiện mặt + RTMPose-face (106 điểm); Full = bộ phát hiện người + RTMW (133 điểm: 0–16 thân, 17–22 bàn chân, 23–90 mặt 68 điểm, 91–111 tay trái, 112–132 tay phải).
- Gói: `hand-rtm`, `face-rtm`, `wholebody-rtmw` (mỗi gói gồm bộ phát hiện + mô hình điểm), đều cần `onnx-runtime-cuda`. `NeedsPackForMode`: Body → `pose-rtmo`, Hand → `hand-rtm`, Face → `face-rtm`, Full → `wholebody-rtmw`, Off → `""`.
- Nhóm: `0 body, 1 hand-left, 2 hand-right, 3 face, 4 feet`.
- OSC: `/mikmap/<dev>/touch` (thân, **không đổi so với A**), `/mikmap/<dev>/hand`, `/mikmap/<dev>/face`, `/mikmap/<dev>/feet`; tin nhắn `i:id f:x f:y [f:z]`.
- Id: thân `slot·100 + chỉ số MediaPipe` (như A); bàn chân `slot·100 + i` (`i` 0–5); tay trái `slot·100 + i` (0–20), tay phải `slot·100 + 50 + i` (50–70); **mặt `slot·1000 + i`** (Face 0–105, Full 0–67).
- Kẹp số đối tượng: `maxObjects ≤ ⌊512 / số điểm mỗi đối tượng⌋` → Body 30 người, Hand 24 tay, Face 4 mặt, Full 3 người.
- Điểm calibrate: Body và Full = id `16` (cổ tay phải, nhóm thân); Hand = id `58` (đầu ngón trỏ phải); Face = chóp mũi (chỉ số lấy từ metadata của mô hình, ghi trong báo cáo).
- `z` = độ tin cậy 0–1.
- Ba thread của A giữ nguyên; render thread không lock, không cấp phát trong vòng lặp nóng; toạ độ thô qua biên thread.
- Gửi ra khi chỉ dùng Body phải **giống hệt từng byte** với trước plan B (OSC và text).
- Dùng `std::filesystem::path(std::u8string(...))`; MSVC `/W4 /permissive- /fp:precise /utf-8`, 0 cảnh báo mới; helper `ui::`.
- Build/test:
  ```
  cmake --build build --config Release --target mikmap_tests -j && ctest --test-dir build -C Release --output-on-failure
  cmake --build src/build --config Release -j && ctest --test-dir src/build -C Release --output-on-failure
  ```
- Commit không có dòng attribution / Co-Authored-By.

## Review Focus

1. **Bên nhận đang dùng Body (A):** sau B, datagram OSC và dòng text của Mode Body phải giống từng byte như trước. Test ở Task 1.
2. **Đổi Mode giữa buổi** (Body → Hand → Full), kể cả khi gói của Mode mới chưa cài: gửi một khung rỗng trước khi chuyển, slot reset, hiện dải "Cần gói" và không tự quay lại Mode cũ. Test ở Task 5, 6.
3. **Đông đối tượng** (6 người ở Full, 8 mặt ở Face): kẹp theo bảng, giữ đối tượng tin cậy nhất, Properties hiện số đã kẹp, khung không bao giờ tràn. Test ở Task 4, 5.
4. **Hộp phát hiện ở mép ảnh, rất nhỏ hoặc suy biến** (rộng 0, NaN từ mô hình): vùng cắt bị kẹp, không có NaN hay vô cực trong điểm ra, đối tượng hỏng bị bỏ. Test ở Task 3.
5. **Tay trái/phải** ở Mode Hand khi lật ảnh (Flip): bên tay theo ảnh đã lật, ổn định qua các khung (không đổi bên theo từng khung). Test ở Task 4.

---

### Task 1: Nhóm điểm trong khung và trong gửi ra

**Files:**
- Modify: `engine/io/SensorFrame.h:31-39` (`uint8_t group = 0;` thay `_pad[0]`; giữ `sizeof(TouchPoint)` và POD)
- Modify: `engine/io/net/TouchSender.h`, `engine/io/net/TouchSender.cpp` (`OutPacket` thêm `uint8_t groups[kMaxTouchPoints]`; `encodeOscChunk`/`encodeTextChunk` theo nhóm)
- Modify: `src/sources.cpp` (chỗ dựng `OutPacket` trong `SendOut`: chép `group`)
- Test: `tests/test_touchsender.cpp`, test đang chứa `SensorFrame`

**Interfaces:**
- Produces:
  ```cpp
  enum PointGroup : uint8_t { kGroupBody = 0, kGroupHandL = 1, kGroupHandR = 2, kGroupFace = 3, kGroupFeet = 4 };   // SensorFrame.h
  const char* groupAddressSuffix(uint8_t g);   // "touch", "hand", "hand", "face", "feet"; unknown → "touch"
  ```
  OSC: each chunk still begins with `/mikmap/<dev>/frame i:frame i:count`; each point uses `/mikmap/<dev>/<suffix>`. Text: body lines keep the exact old format `<device> <id> <x> <y>[ <z>]`; other groups `<device> <suffix> <id> <x> <y>[ <z>]`.

- [ ] **Step 1: Failing tests:** `sizeof(TouchPoint)` equals its pre-B value (capture the number in the test with a comment) and stays trivially copyable; a frame of only body points encodes to **byte-identical** OSC and text output versus a golden captured from the current code before the change (write the golden first, from HEAD); a mixed frame (body + hand-left + hand-right + face + feet) → each point's address suffix is right, chunks ≤ 1200 bytes, all ids round-trip through a raw OSC parse; text lines for non-body groups carry the suffix word; unknown group 9 encodes as `touch`.
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** Commit `io: point groups in frames and send-out (body output unchanged byte for byte)`.

### Task 2: Đối tượng pose tổng quát và bộ theo dõi dùng chung

**Files:**
- Modify: `engine/io/ai/PoseModel.h/.cpp`, `engine/io/ai/PersonTracker.h/.cpp`, `engine/io/sources/CameraSource.h/.cpp`, and every `src/` use of `PersonPose` (grep: `src/sources.cpp`, `src/sensor.cpp`)
- Test: existing `tests/test_posemodel.cpp`, `tests/test_persontracker.cpp`, `tests/test_camerasource.cpp` (must stay green), plus new cases

**Interfaces:**
- Produces:
  ```cpp
  inline constexpr int kMaxKeypoints = 133;
  struct PoseObject { float box[4] = {}; float score = 0; int nkp = 0; int8_t side = -1 /* -1 none, 0 left, 1 right */; Keypoint kp[kMaxKeypoints]; };
  using PersonPose = PoseObject;                 // keep the A name compiling; RTMO fills nkp = 17
  // PersonTracker unchanged in name and API but tracks PoseObject of any nkp; objects with different nkp never match each other;
  // for hands, a track keeps its side once assigned (majority over the last 5 frames) — see Task 4
  ```
  `CameraSource::Inference` keeps its signature with `PoseObject*`.

- [ ] **Step 1: Failing tests:** all A tests still pass unchanged; tracker on 21-keypoint objects keeps slots across 10 frames of crossing hands; an object with `nkp = 21` never matches a track with `nkp = 17`; `side` survives in the tracked object; `sizeof(PoseObject)` is bounded (assert ≤ 2 KiB) and the tracker holds 30 tracks without heap allocation per update.
- [ ] **Step 2-4:** RED → implement → GREEN (both suites).
- [ ] **Step 5:** Commit `io/ai: pose objects of any keypoint count, one tracker for people, hands and faces`.

### Task 3: TopDownPose (phát hiện, cắt affine, SimCC)

**Files:**
- Create: `engine/io/ai/TopDownPose.h`, `engine/io/ai/TopDownPose.cpp`
- Test: `tests/test_topdownpose.cpp`

**Interfaces:**
- Consumes: `OrtRuntime` (A), `CameraFrame`, `PoseObject` (Task 2).
- Produces:
  ```cpp
  struct CropXf { float a[6] = {}; float inv[6] = {}; int outW = 0, outH = 0; };     // 2x3 affine image->crop and its inverse
  bool makeCrop(const float box[4], int imgW, int imgH, int outW, int outH, float padding, CropXf& out);   // RTMPose convention: centre of box, scale = box size * padding (1.25), aspect fixed to outW:outH; false for degenerate/NaN boxes
  void warpCropToChw(const CameraFrame& f, const CropXf& xf, std::vector<float>& outChw);   // bilinear, BGR, mean/std of RTMPose (123.675,116.28,103.53 / 58.395,57.12,57.375) — confirm against the model card and record
  // SimCC: simcc_x [K, outW*split], simcc_y [K, outH*split], split = 2.0
  void decodeSimcc(const float* sx, const float* sy, int K, int outW, int outH, float split, const CropXf& xf, int imgW, int imgH, Keypoint* out);   // argmax per axis, conf = min(max_x, max_y) clamped to 0..1, map back through inv to image 0..1
  struct DetBox { float box[4]; float score; };
  int decodeDetector(const float* dets, int n, const Letterbox& lb, int imgW, int imgH, float thr, DetBox* out, int maxOut);   // RTMDet end-to-end export: dets [1,N,5]; same letterbox as A
  class TopDownPose { public:
    bool init(OrtRuntime& detector, OrtRuntime& keypoints, int numKeypoints, std::string& err);   // reads input sizes from the sessions
    int infer(const CameraFrame& f, float detThr, float kpThr, int maxObjects, PoseObject* out, std::string& err);   // runs the keypoint model once per kept box, highest scores first
  };
  ```

- [ ] **Step 1: Failing tests (no model needed):** `makeCrop` of a 100×200 box with padding 1.25 into 192×256 gives the expected centre/scale and `inv(a(p)) == p` (±1e-4); box at the image edge → crop still valid, partly outside the image is allowed (pixels outside sample black); zero-width box, NaN or inf coordinates → `false` (**Review Focus 4**); `decodeSimcc` on hand-made tensors: a peak at `(x=96, y=128)` in crop pixels maps back to the box centre, conf equals the smaller peak value; an all-zero tensor gives conf 0 and finite coordinates; `decodeDetector` drops boxes below `thr`, undoes the letterbox and keeps at most `maxOut`. With env `MIKMAP_TEST_ORT_DIR` + `MIKMAP_TEST_HAND_MODELS` (two paths): `infer` runs on a gray frame without error and returns 0 objects; otherwise print the skip line.
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** Commit `io/ai: shared top-down pipeline — detector, affine crop, SimCC decoding`.

### Task 4: Bảng ModeModels, nhóm điểm, bên tay, kẹp số đối tượng

**Files:**
- Modify: `engine/io/camera/CameraDevice.h/.cpp` (`CamMode { Off = 0, Body = 1, Hand = 2, Face = 3, Full = 4 }`; `modeSupported`: Hand/Face/Full need Color)
- Create: `engine/io/ai/ModeModels.h`, `engine/io/ai/ModeModels.cpp`
- Test: `tests/test_modemodels.cpp`, `tests/test_camera.cpp`

**Interfaces:**
- Produces:
  ```cpp
  struct ModeSpec { CamMode mode; const char* pack; const char* detectorFile; const char* keypointFile; int keypointsPerObject; int objectsCap; };
  const ModeSpec& modeSpec(CamMode m);                                      // file names inside the pack: "detector.onnx", "keypoints.onnx" (Body: "rtmo.onnx", no detector)
  int clampObjects(CamMode m, int requested);                               // min(requested, 512 / pointsPerObject): Body 30, Hand 24, Face 4, Full 3; Off 0
  // Map keypoint k of an object to (group, id) — id per Global Constraints
  void pointGroupAndId(CamMode m, int slot, int8_t side, int k, uint8_t& group, uint32_t& id, bool& emit);   // emit=false for keypoints that have no output (none in B; kept for filtering)
  uint32_t calibPointId(CamMode m);                                          // Body/Full 16, Hand 58, Face slot0 nose tip index (record index)
  int8_t handSide(const PoseObject& hand, bool flipped);                    // 21-pt hand: sign of cross(wrist->index_mcp(5), wrist->pinky_mcp(17)) assuming palm toward camera; flipped inverts; returns 0 left / 1 right
  ```
  RTMW index ranges (Global Constraints): `0–16` body via `kCocoToMediaPipe` (group body), `17–22` feet (`i = k-17`), `23–90` face (`i = k-23`, id `slot*1000+i`), `91–111` hand-left (`i = k-91`), `112–132` hand-right (`i = k-112`, id `slot*100+50+i`).

- [ ] **Step 1: Failing tests:** `clampObjects` values for every Mode (Body 30, Hand 24, Face 4, Full 3, requested lower is kept); `pointGroupAndId` for Full: k=10 → body id `slot*100+16`, k=17 → feet `slot*100+0`, k=23 → face `slot*1000+0`, k=90 → face `slot*1000+67`, k=91 → hand-left `slot*100+0`, k=120 → hand-right `slot*100+50+8 == slot*100+58`; for Hand mode with side right k=8 → `58` (calibPointId(Hand) == 58); Face k=105 → `slot*1000+105`; `modeSupported(Hand, color-only caps)` true, with no Color false; `handSide` on a synthetic right hand → 1, mirrored → 0, `flipped` inverts (**Review Focus 5**); tracker keeps a hand's side stable when one frame's `handSide` disagrees (majority of 5, uses Task 2's tracker).
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** Commit `io/ai: mode table — packs, point groups and ids, calibration points, object caps, hand side`.

### Task 5: CameraSource theo Mode

**Files:**
- Modify: `engine/io/sources/CameraSource.h/.cpp`
- Test: `tests/test_camerasource.cpp`, `tests/data/replay_modes.mikpose`

**Interfaces:**
- Consumes: Tasks 1–4.
- Produces:
  - `CameraSourceConfig` gains `uint8_t fullGroups = 0x1F` (bit per group: body, hand-left, hand-right, face, feet) and `int maxObjects` (replaces `tracker.maxPeople` as the user value; the source applies `clampObjects`).
  - `CameraStats` gains `int objectsCap` (the clamped value, for the UI).
  - Replay `.mikpose` lines accept an optional 7th column `group` (default 0); A files still load.

- [ ] **Step 1: Failing tests (injected inference / replay, no GPU):** Hand mode with an injected inference returning 2 hands → points tagged hand-left/right with ids per Task 4, `z` = conf; Face mode with 8 faces requested `maxObjects = 10` → 4 kept, the 4 most confident (**Review Focus 3**), `stats().objectsCap == 4`; Full with `fullGroups` excluding face → no face points emitted, body/hands present; mode change while running (via restart) emits one empty frame before the first frame of the new mode and resets slots (**Review Focus 2**); replay file with group column reproduces groups; an A replay file (6 columns) still works.
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** Commit `io/sources: camera modes Hand, Face, Full on the shared pipeline, group filter and object caps`.

### Task 6: App — Mode, gói, nhóm ở Full, vẽ theo nhóm

**Files:**
- Modify: `src/app.h` (`Device` gains `uint8_t camGroups = 0x1F`; `NeedsPackForMode`, `SetCamMode` for new modes), `src/project.cpp` (schema `"cam":{…,"groups"}`, absent = 0x1F; `mode` values 2–4), `src/sources.cpp` (`Start()` builds the pipeline per `modeSpec`, `runtimeDir`/model paths from the pack; calibration capture uses `calibPointId(mode)` instead of the fixed `kCalibJoint`), `src/sensor.cpp` (Mode list Off/Body/Hand/Face/Full with pack banners; Full group toggles; "Max objects" field showing `objectsCap`; INPUT draws body bones, hand bones (21-pt connections), face as small dots, feet; one colour per group from existing palette tokens; calibration hint text per Mode)
- Test: `SensorSelfTest` in `src/sources.cpp`

**Interfaces:**
- Consumes: Tasks 1–5; `PackAvailableForDevice`, `PackStateFor` (P).
- Produces: `const char* CalibHintForMode(int camMode)` ("Hold your right wrist…", "Point your right index finger…", "Put the tip of your nose…").

- [ ] **Step 1: Failing self-tests:** every Mode round-trips with `camGroups`; `NeedsPackForMode` for each Mode per Global Constraints; switching to Hand with `hand-rtm` missing → no source, "Needs pack" error, Mode stays Hand (**Review Focus 2**); replay with hands and face through `SensorsTick` → loopback receiver gets `/hand` and `/face` addresses with ids per the table, and a Body-only replay yields the exact pre-B bytes (reuse Task 1 golden); calibration capture in Hand mode uses id 58; Full with face unticked sends no `/face`; two ticks do not dirty the project.
- [ ] **Step 2-4:** RED → implement → GREEN.
- [ ] **Step 5:** `--shot` of INPUT for each Mode using replay files (body, hands, face, full), Properties for Full (group toggles, "Max objects 3"), and the Mode list with a missing-pack banner; LOOK at them and report. If packs and GPU are available on the user's machine, record fps/ms for Hand (1, 4 hands), Face (1, 4), Full (1, 3 people); otherwise say "not measured".
- [ ] **Step 6:** Commit `sensor: camera modes Hand, Face, Full — packs per mode, group toggles, per-group drawing`.

### Task 7: Gói mẫu và tài liệu giao thức

**Files:**
- Create: `tools/packs/examples/hand-rtm/pack.json`, `tools/packs/examples/face-rtm/pack.json`, `tools/packs/examples/wholebody-rtmw/pack.json` (no binaries; file names `detector.onnx`, `keypoints.onnx`; licence Apache-2.0; `requires onnx-runtime-cuda`)
- Modify: `tools/packs/examples/README.md`, `features.md`, `README.md`, `.claude/CLAUDE.md`, `.claude/rules/design.md`, the OSC protocol notes added by plan A (group addresses, id table with the face `slot·1000` exception, Body unchanged)

- [ ] **Step 1:** Grep `src/*.cpp` and `engine/io/**` to confirm `TopDownPose`, `modeSpec`, `clampObjects`, `pointGroupAndId`, `calibPointId`, `handSide` are called on the app path.
- [ ] **Step 2:** Docs with honest "Ghi chú kiểm tra": which Modes ran on a real camera/GPU, measured fps/ms or "not measured", hand-side heuristic limits, the face id exception.
- [ ] **Step 3:** Commit `docs: camera modes Hand, Face, Full and the grouped OSC protocol`.

---

## Kiểm cuối

- Hai bộ test xanh, chạy mỗi bộ hai lần; 0 cảnh báo mới.
- Golden Body (Task 1) vẫn giống từng byte sau mọi task.
- Agent `reviewer` của repo duyệt toàn nhánh; ghi rõ Mode nào chưa chạy với mô hình thật.

# A: Khung camera (2D/3D) và nhận dạng thân người nhiều người ngay trong MikMap

Ngày: 2026-10-30 · Trạng thái: chờ người dùng duyệt · Phụ thuộc: spec P (`2026-10-30-sensor-packs-design.md`) · Nhánh nền: `feat/sensor-skeleton-hokuyo`

## 1. Mục tiêu và bối cảnh

Hiện nay webcam đi qua sidecar Python `tools/pose_bridge` (MediaPipe, 1 người mặc định, gửi OSC vào cổng của sensor `Skeleton`). Người dùng muốn chạy nhận dạng **ngay trong App** (2026-10-30, chọn "cách 2": ONNX Runtime trong `engine/io`, chạy như một sensor thread).

Yêu cầu đã nêu:
- Máy diễn có GPU NVIDIA (chạy CUDA).
- Theo dõi **càng nhiều người càng tốt**, có **giới hạn số người** chỉnh được.
- Sau này có chế độ Face, Body, Pose, Hand, Gesture và cử chỉ tuỳ chỉnh (B, C). **Không** thuộc spec này.
- Gói nặng (ONNX Runtime GPU, mô hình) cài theo nhu cầu qua P.

Người dùng chốt thêm (2026-10-30): camera chỉ phân **hai loại, Camera 2D và Camera 3D**; camera 3D vẫn phải xuất được hình như camera 2D.

Spec này lo **A**:
- **Khung camera chung:** thiết bị khai báo luồng nó có (Color, Depth, IR); chế độ xử lý chạy trên luồng chứ không trên loại thiết bị.
- **Camera 2D:** webcam hoặc capture card qua Media Foundation.
- **Chế độ Body:** mô hình thân người nhiều người qua ONNX Runtime và CUDA, giữ id người qua các khung hình, đưa điểm khớp vào đường sensor hiện có (calibration, warp, gộp, gửi OSC/UDP), hiện hình camera trên stage INPUT.

Camera 3D dùng **chính khung này** và làm ở spec sau (§8): nó có luồng Color nên mọi chế độ 2D chạy y hệt, cộng thêm các chế độ cần Depth.

Thước đo thành công:
- Thêm "Camera 2D" bằng popup, chọn Mode = Body; nếu thiếu gói thì cài từ đó. Sau đó thấy khung xương của nhiều người trên hình camera ở INPUT, căn chỉnh như sensor khác, và bên nhận OSC nhận điểm khớp với id ổn định.
- Thiếu gói, thiếu GPU hay mất camera thì App **không sập** và báo rõ lý do; các sensor khác vẫn chạy.
- Render thread không bị chặn; độ trễ được **đo** (`tCaptureNs` → lúc gửi) và hiện ra.

## 2. Quyết định (mình đề xuất, người dùng xác nhận khi duyệt spec)

| # | Quyết định | Lý do |
|---|---|---|
| A1 | Hai loại sensor mới **`DevKind::Camera2D`** và **`DevKind::Camera3D`** (A chỉ làm `Camera2D`; `Camera3D` được khai báo nhưng chưa thêm được từ popup). Chế độ xử lý là thuộc tính **`Device::camMode`** (`Off`, `Body`; sau này `Hand`, `Face`, `Full`, `Gesture` ở B và C), không phải loại sensor. `Skeleton` (OSC sidecar `pose_bridge`) giữ nguyên làm phương án dự phòng, ngoài hai loại camera | Người dùng chỉ muốn hai loại camera; camera 3D chạy được mọi chế độ của 2D nhờ luồng Color |
| A1b | Mỗi chế độ khai báo luồng nó cần (`Body` cần Color). Chế độ cần luồng mà thiết bị không có thì bị làm mờ trong danh sách, kèm lý do | Một bảng khả năng, không rẽ nhánh theo loại thiết bị |
| A1c | Mode = `Off`: chỉ hiện hình và xuất hình, không chạy mô hình, **không cần gói** | Camera dùng để xem hoặc làm nguồn hình không phải tải gói AI |
| A2 | Mô hình mặc định **RTMO** (một tầng, nhiều người, 17 khớp COCO, giấy phép **Apache-2.0**), cỡ `s` mặc định, `m` là tuỳ chọn | Chi phí gần như không đổi theo số người (khác với kiểu "phát hiện người rồi chạy pose từng người"); giấy phép cho phép đóng gói. **Không** dùng YOLO-pose (AGPL-3.0) |
| A3 | Đánh số khớp theo **MediaPipe** (0..32): COCO-17 ánh xạ vào đúng chỉ số MediaPipe tương ứng; khớp MediaPipe không có trong COCO thì không gửi | Giữ nguyên giao thức `pose_bridge` (`id = slot·100 + landmark`), `SkeletonView`, `kCalibJoint = 16` (cổ tay phải) và các patch TouchDesigner đã có |
| A4 | Nâng sức chứa khung sensor `kMaxTouchPoints` từ **64 lên 512** (≈30 người × 17 khớp) | 64 điểm chỉ đủ 3–4 người; khung vẫn là POD kích thước cố định để qua `TripleBuffer` không cấp phát |
| A5 | Gửi ra: chia một khung thành **nhiều gói UDP, mỗi gói ≤ 1200 byte**, theo từng người (một bundle OSC mỗi người, kèm `/mikmap/<dev>/frame i:frame i:people`) | Gói UDP lớn bị phân mảnh hoặc rơi trên mạng thật; bên nhận vẫn ghép được theo `frame` |
| A6 | Bắt hình bằng **Media Foundation** (Windows, có sẵn trong hệ điều hành) trên thread riêng | Không thêm thư viện; Linux/macOS chưa hỗ trợ (báo "unsupported") |
| A7 | ONNX Runtime nạp **lúc chạy** từ gói P (`onnx-runtime-cuda`) bằng `LoadLibraryExW` + `OrtGetApiBase`; repo chỉ thêm **một header C API** của ONNX Runtime (giấy phép MIT) vào `third_party/onnxruntime/` | App build và chạy được khi chưa có gói; không link cứng thư viện lớn |
| A8 | Không có CUDA hoặc khởi tạo CUDA lỗi → **chạy CPU** kèm cảnh báo vàng "Running on CPU — slow"; người dùng có thể tắt tự lùi về CPU | Buổi diễn vẫn chạy khi driver lỗi, nhưng người vận hành biết |
| A9 | Theo dõi id người bằng bộ ghép theo IoU khung xương + khoảng cách hông, giữ id tối đa 0,5 s khi mất dấu (kiểu ByteTrack rút gọn) | `pose_bridge` hiện nhảy id khi hai người cắt nhau; cần id ổn định hơn cho hiệu ứng |
| A10 | Gói dùng: `onnx-runtime-cuda` (runtime) và `pose-rtmo` (mô hình, cần runtime). `Device::needsPack` **suy ra từ Mode**: `Body` → `"pose-rtmo"`, `Off` → rỗng; đổi Mode thì cập nhật theo | Theo mô hình gói của P; camera chỉ để xem không cần gói |
| A11 | Trường `z` của điểm khớp: Camera 2D = **độ tin cậy 0–1** (như `pose_bridge`); Camera 3D (spec sau) = **khoảng cách theo mét** khi bật "Body + depth". Ghi rõ trong tài liệu giao thức | Bên nhận OSC biết cách đọc `z` theo loại camera |

## 3. Bất biến kiến trúc

- `engine/core` chỉ STL. Mã camera, ONNX Runtime và bộ theo dõi nằm ở `engine/io`, không biết Screen, Slice hay ImGui.
- Ba luồng, không mutex qua biên:
  1. **Capture** (Media Foundation) đẩy khung ảnh vào `TripleBuffer`.
  2. **Infer** (ONNX Runtime) lấy khung mới nhất, chạy mô hình và bộ theo dõi, xuất `SensorFrame` (điểm thô, toạ độ ảnh 0–1) qua `TripleBuffer`, kèm khung ảnh xem trước (đã thu nhỏ) qua một `TripleBuffer` riêng.
  3. **Render** đọc frame mới nhất, áp `H_s`/warp như các sensor khác, và tải ảnh xem trước lên texture GL đã cấp phát sẵn.
- Sensor thread gửi **toạ độ thô**; ngữ nghĩa (calibration, gộp, chuẩn hoá screen) áp ở render thread như hiện tại.
- Độ trễ đo bằng `tCaptureNs` đóng dấu lúc nhận khung từ camera; PerfPanel hiện thời gian suy luận (ms) và fps của nguồn.
- Chạy được không cần phần cứng (nguyên tắc #7, như `MockSource`): có chế độ "replay" đọc một file khung xương ghi sẵn, để chạy test không cần camera hay GPU (nguyên tắc #7).

## 4. Thành phần

| Thành phần | Vị trí | Việc |
|---|---|---|
| `ICameraDevice` | `engine/io/camera/` | Giao diện chung: `caps()` trả `{hasColor, hasDepth, hasIR}` và kích thước từng luồng; `open/close`; mỗi khung là `CameraFrame` gồm ảnh Color (RGB24), Depth (uint16 mm, có thể rỗng), thông số căn khớp Color↔Depth (có thể rỗng), `tCaptureNs` |
| `MfCamera` (Camera 2D) | `engine/io/camera/` | Cài `ICameraDevice` bằng Media Foundation: liệt kê, mở theo chỉ số với kích thước yêu cầu (mặc định 640×480 @30), chỉ có Color |
| Camera 3D | (spec sau) | Cài `ICameraDevice` bằng SDK của hãng trong gói P; có Color + Depth |
| `OrtRuntime` | `engine/io/ai/` | Nạp DLL ONNX Runtime từ thư mục gói, tạo session với CUDA EP (lùi về CPU), báo lỗi rõ |
| `PoseModel` (RTMO) | `engine/io/ai/` | Tiền xử lý (letterbox về kích thước đầu vào của mô hình), chạy, hậu xử lý (giải mã đầu ra, ngưỡng tin cậy, NMS), trả danh sách người × 17 khớp + độ tin cậy |
| `PersonTracker` | `engine/io/ai/` | Gán id ổn định, giới hạn số người (`maxPeople`, giữ những người tin cậy nhất), làm mượt nhẹ (tuỳ chọn) |
| `CameraSource` | `engine/io/sources/` | Ghép một `ICameraDevice` với chế độ đang chọn trong hai thread; chế độ `Off` bỏ qua suy luận; xuất `SensorFrame` (id = slot·100 + chỉ số MediaPipe, x/y ảnh 0–1, z theo A11) và khung xem trước. Dùng chung cho Camera 2D và 3D |
| App | `src/` | `DevKind::Camera2D` (và khai báo `Camera3D`), thẻ "Camera 2D" trong popup, ô chọn Mode trong Properties (gói chỉ cần khi Mode cần), INPUT hiện ảnh camera + khung xương (dùng lại `SkeletonView`), PerfPanel |

## 5. Thiết lập trong Properties (Camera 2D)

- **INPUT:**
  - Camera: chọn thiết bị, độ phân giải (640×480, 1280×720), lật ngang.
  - **Mode:** Off · Body (Hand, Face, Full, Gesture hiện mờ, ghi "sắp có"). Chọn Body khi chưa có gói thì hiện dải "Cần gói" như P.
  - Các mục dưới chỉ hiện khi Mode khác Off:
  - Model: s / m.
  - Thiết bị chạy: Auto (CUDA, lùi về CPU), CUDA only, CPU.
  - **Max people** (1–30, mặc định 10).
  - Ngưỡng tin cậy khớp (mặc định 0,3) và người (mặc định 0,5).
  - Làm mượt (tắt/nhẹ/vừa).
- **OUTPUT:** như các sensor khác (Calibration 4 điểm dùng `kCalibJoint`, Send out, Triggers).
- **Trạng thái** (khối luôn hiện): fps camera, fps suy luận, ms suy luận, độ trễ, GPU/CPU, số người.

## 6. Lỗi và giới hạn phải thấy được

| Tình huống | Hành vi |
|---|---|
| Thiếu gói | Chip chấm 1 cam, "Needs pack: Body tracking", nút Cài đặt (theo P); không chạy gì |
| Không có camera hoặc camera đang bị app khác giữ | Trạng thái lỗi kèm tên camera; thử lại mỗi 2 s; sensor khác không ảnh hưởng |
| Mất camera giữa buổi | Như trên; id người được xoá sau 0,5 s; gửi `frame` với 0 người để bên nhận biết |
| CUDA không khởi tạo được | Theo A8 (CPU kèm cảnh báo, hoặc lỗi nếu chọn "CUDA only") |
| Suy luận chậm hơn camera | Bỏ khung cũ (chỉ xử lý khung mới nhất); fps suy luận hiện riêng |
| Số người vượt `maxPeople` | Giữ những người tin cậy nhất; không bao giờ vượt `kMaxTouchPoints` |
| ONNX Runtime sập trong tiến trình | **Sập cả App**: đây là giới hạn đã biết của cách 2, ghi rõ trong tài liệu; khuyến nghị giữ `pose_bridge` cho buổi diễn không được phép dừng |

## 7. Kiểm thử

- **Engine (`tests/`, không cần GPU hay camera):**
  - Hậu xử lý RTMO trên tensor đầu ra cố định (lấy mẫu từ chạy thật, lưu thành tệp nhị phân nhỏ trong `tests/data/`): số người, toạ độ, ngưỡng, NMS.
  - Bảng ánh xạ COCO-17 → MediaPipe.
  - `PersonTracker`: hai người cắt nhau thì giữ id; một người biến mất 0,4 s thì giữ id, 0,6 s thì bỏ id; `maxPeople` cắt đúng người kém tin cậy nhất.
  - `kMaxTouchPoints = 512`: `SensorFrame` vẫn là POD, `TripleBuffer` khứ hồi đủ 512 điểm.
  - Bộ chia gói OSC: một khung 30 người được chia thành các gói ≤ 1200 byte, và `OscSource` ghép lại đúng toàn bộ điểm và id.
- **Engine, chỉ chạy khi có gói và GPU** (bỏ qua có ghi chú khi thiếu): `OrtRuntime` nạp DLL từ thư mục gói, tạo session CUDA, chạy một ảnh mẫu có 3 người, ra 3 người với ±2 px quanh kết quả mong đợi; ép CUDA lỗi thì lùi về CPU.
- **Bảng khả năng:** chế độ cần luồng thiết bị không có thì bị từ chối (test với một `ICameraDevice` giả có Color, và một giả có Color + Depth).
- **App (`--roundtrip`):** `DevKind::Camera2D` và `camMode` khứ hồi trong file; `needsPack` đổi theo Mode (Off → rỗng, Body → `pose-rtmo`); thiếu gói thì không chạy và không bẩn dự án; chế độ replay đưa khung xương ghi sẵn qua đúng đường sensor (calibration, gửi OSC) mà không cần camera.
- **Ảnh `--shot`:** INPUT với ảnh replay và khung xương nhiều người; Properties.
- **Đo:** fps và ms suy luận với 1, 5, 10 người trên máy có GPU của người dùng. **Phải ghi số đo thật** trong tài liệu, không ước lượng.

## 8. Ngoài phạm vi (spec sau)

- **Camera 3D:** cài `ICameraDevice` bằng SDK của hãng (gói P riêng cho từng hãng: RealSense, Orbbec, Kinect, ZED); chọn luồng Color hoặc Depth tô màu để xem và xuất hình; chế độ "Body + depth" (khớp có z theo mét từ Depth đã căn khớp); chế độ chạm hoặc blob từ Depth trên tường hay sàn (giống lidar); đặt camera theo mét so với screen.

- **B:** tay (21 điểm), mặt, chọn chế độ Face/Body/Pose/Hand/Full, và bố cục id cho các chế độ đó.
- **C:** cử chỉ có sẵn và cử chỉ tuỳ chỉnh.
- Camera 2D cắt khung (Straighten), NDI/Spout xuất hình, nhiều camera ghép.
- Linux/macOS (Media Foundation, CUDA trên Windows trước).
- Cách ly tiến trình cho ONNX Runtime (chạy mô hình trong tiến trình phụ của chính MikMap): cân nhắc sau nếu sự cố sập trong tiến trình xảy ra thật.

## 9. Rủi ro

- **Dung lượng gói GPU:** ONNX Runtime GPU cộng cuDNN có thể gần 1 GB; nếu máy đã có CUDA/cuDNN đúng phiên bản thì dùng của hệ thống (P chốt cách đóng gói).
- **Phiên bản CUDA/cuDNN phải khớp với ONNX Runtime:** sai phiên bản thì CUDA EP không khởi tạo được; A8 cho App tiếp tục chạy bằng CPU và báo rõ.
- **Độ chính xác của 17 khớp COCO** thấp hơn 33 khớp MediaPipe ở bàn tay và bàn chân; người cần chi tiết tay chờ B.
- **Nâng `kMaxTouchPoints` lên 512** làm mỗi `SensorFrame` lớn khoảng 8 lần (sao chép qua `TripleBuffer` ở mọi sensor). Cần đo lại P99 khung hình với 4 sensor chạy cùng lúc; nếu tốn, chỉ khung của sensor camera dùng sức chứa lớn.
- **Giấy phép:** chỉ đóng gói mô hình có giấy phép cho phép phân phối (RTMO Apache-2.0); ghi giấy phép vào `pack.json` và phần "Giới thiệu" của App.

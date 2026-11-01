# B: Chế độ Hand, Face, Full cho camera

Ngày: 2026-11-01 · Trạng thái: chờ người dùng duyệt · Phụ thuộc: P (gói sensor), A (`2026-10-30-ai-body-tracking-a-design.md`: khung camera, `CamMode`, `CameraSource`, ONNX Runtime) · Nhánh nền: `feat/sensor-skeleton-hokuyo`

## 1. Mục tiêu

Người dùng (2026-10-30) muốn camera có thể "chọn mode chỉ detect 1 loại: full, face, body, pose, hand…", theo dõi nhiều người với giới hạn chỉnh được. A đã làm Off và Body. B thêm:

- **Hand:** điểm tay, mỗi bàn tay 21 điểm, nhiều bàn tay.
- **Face:** điểm mặt, nhiều khuôn mặt.
- **Full:** thân + tay + mặt của cùng một người trong một lần chạy.
- Bố cục id và địa chỉ OSC cho các nhóm điểm, để bên nhận phân biệt thân, tay, mặt.

Cử chỉ (Gesture) và cử chỉ tuỳ chỉnh là **C**, không thuộc spec này; C dùng điểm tay và thân mà B tạo ra.

Thước đo thành công:
- Đổi Mode giữa Body, Hand, Face, Full trong Properties; mỗi Mode chỉ chạy đúng mô hình của nó (không chạy song song các mô hình khác), cần gói nào thì cài gói đó.
- Bên nhận OSC nhận các nhóm điểm theo địa chỉ riêng, id ổn định theo người, và các patch đã viết cho Body (A) không phải sửa.
- Camera 3D (spec sau) chạy được cả bốn Mode qua luồng Color mà không viết lại gì.

## 2. Quyết định (mình đề xuất, người dùng xác nhận khi duyệt spec)

| # | Quyết định | Lý do |
|---|---|---|
| B1 | Mode = `Off · Body · Hand · Face · Full`. "Pose" trong yêu cầu được hiểu là **Body** (cùng nghĩa), không phải Mode riêng | Tránh hai lựa chọn trùng nhau trong menu |
| B2 | Mỗi lúc chỉ chạy **một** Mode trên một camera (loại trừ nhau). Cần thân và tay cùng lúc thì dùng **Full** | Đúng ý "chỉ detect 1 loại"; tiết kiệm GPU; dễ đoán tải |
| B3 | Mô hình (đều Apache-2.0, chạy bằng ONNX Runtime của A): **Hand** = bộ phát hiện tay + RTMPose-hand (21 điểm); **Face** = bộ phát hiện mặt + RTMPose-face (**106 điểm**); **Full** = bộ phát hiện người + **RTMW** toàn thân (133 điểm: 17 thân, 6 bàn chân, 68 mặt, 2×21 tay) | Cùng họ mô hình, cùng giấy phép cho phép đóng gói, cùng cách hậu xử lý top-down |
| B4 | Mặt **không** dùng lưới dày 468 điểm (MediaPipe Face Mesh) | 468 điểm mỗi người vượt sức chứa khung 512 điểm ngay với 2 người; 106 điểm đủ cho mắt, miệng, viền mặt |
| B5 | Mỗi Mode là **một gói** riêng: `hand-rtm`, `face-rtm`, `wholebody-rtmw` (đều cần `onnx-runtime-cuda`). `needsPack` suy ra từ Mode như A | Người dùng chỉ tải mô hình của Mode mình dùng |
| B6 | Thêm trường **`group`** vào `TouchPoint` (POD): `0 body, 1 hand-left, 2 hand-right, 3 face, 4 feet` | Calibration, warp, gộp và gửi ra cần biết điểm thuộc nhóm nào mà không đoán từ id |
| B7 | Địa chỉ OSC theo nhóm: `/mikmap/<dev>/touch` (thân, **giữ nguyên A**), `/mikmap/<dev>/hand`, `/mikmap/<dev>/face`, `/mikmap/<dev>/feet`; mỗi tin nhắn `i:id f:x f:y f:z` | Patch Body cũ chạy tiếp; bên nhận lọc theo địa chỉ |
| B8 | Id trong mỗi nhóm: `slot · 100 + i` cho thân (chỉ số MediaPipe như A), bàn chân (`i = 0..5`) và tay (`i = 0..20` tay trái, `50..70` tay phải; `slot` là của người ở Full, của chính bàn tay ở Mode Hand). **Mặt** có 106 điểm, không vừa 100, nên dùng `slot · 1000 + i` (`i = 0..105`) | Giữ quy ước `slot·100` ở mọi nhóm vừa 100; mặt là ngoại lệ duy nhất, ghi rõ trong tài liệu giao thức |
| B9 | Giới hạn số đối tượng tự kẹp theo sức chứa: `maxPeople ≤ ⌊512 / số điểm mỗi người⌋` (Body 17 → 30, Hand 21 mỗi tay → 24 tay, Face 106 → 4 mặt, Full 133 → 3 người). Properties hiện con số kẹp này cạnh ô nhập | Người vận hành biết vì sao đặt 10 mà chỉ thấy 3 |
| B10 | Ở Mode **Full** có thể tắt từng nhóm gửi ra (thân, tay, mặt, bàn chân) để giảm băng thông; mô hình vẫn chạy một lần | Nhiều patch chỉ cần thân + tay |
| B11 | Điểm calibrate theo Mode: Body và Full = cổ tay phải (`kCalibJoint = 16`); Hand = đầu ngón trỏ tay phải (`id 58`, chỉ số 8 của tay phải); Face = chóp mũi | Mỗi Mode có một điểm dễ đặt lên dấu thập |
| B12 | `z` = độ tin cậy 0–1 như A (Camera 3D đổi thành mét ở spec sau) | Nhất quán với A |

## 3. Bất biến kiến trúc

Giữ nguyên các bất biến của A:
- Ba luồng (capture, infer, render), chỉ giao tiếp qua `TripleBuffer`, khung mới nhất thắng.
- Toạ độ thô qua biên thread.
- `engine/io` không biết UI.
- ONNX Runtime nạp lúc chạy từ gói.

Thêm:
- **Một đường ống top-down dùng chung** cho Hand, Face, Full: bộ phát hiện → cắt vùng (affine) → mô hình điểm (SimCC) → giải mã → ánh xạ về ảnh. Chỉ khác mô hình, số điểm và nhóm. Không viết ba đường riêng.
- Bộ theo dõi của A (`PersonTracker`) dùng lại cho người (Full), bàn tay (Hand) và khuôn mặt (Face): nó theo dõi "đối tượng" có hộp bao và điểm; chỉ đổi tham số.
- Ở Mode Hand không có thân người nên tay không gắn với người: mỗi bàn tay là một đối tượng có `slot` riêng. Trái/phải lấy từ mô hình (handedness) hoặc, nếu mô hình không có, từ vị trí so với ảnh đã lật; ghi rõ trong tài liệu là có thể nhầm.

## 4. Thành phần

| Thành phần | Vị trí | Việc |
|---|---|---|
| `TopDownPose` | `engine/io/ai/` | Bộ phát hiện (RTMDet: người, tay hoặc mặt) + cắt vùng affine + mô hình điểm SimCC + giải mã về ảnh 0–1; tham số: số điểm, kích thước vào, ngưỡng |
| `ModeModels` | `engine/io/ai/` | Bảng Mode → gói, tệp mô hình, số điểm, nhóm của từng điểm, điểm calibrate, sức chứa mỗi đối tượng |
| `CameraSource` (A) | `engine/io/sources/` | Chọn đường ống theo Mode; gắn `group` cho từng điểm; kẹp số đối tượng theo B9; Full lọc nhóm theo B10 |
| `TouchPoint.group` | `engine/io/SensorFrame.h` | Trường mới (B6); mặc định 0 (thân), nên mọi sensor cũ không đổi |
| Gửi ra | `engine/io/net/TouchSender` | Địa chỉ theo nhóm (B7); chia datagram như A |
| App | `src/` | Ô Mode có thêm Hand, Face, Full; tuỳ chọn nhóm gửi ra ở Full; hiện số đối tượng tối đa sau khi kẹp; INPUT vẽ khung xương thân, tay (21 điểm, nối ngón) và mặt (điểm mảnh) theo nhóm, mỗi nhóm một màu |

## 5. Lỗi và giới hạn phải thấy được

| Tình huống | Hành vi |
|---|---|
| Chọn Mode mà chưa cài gói của Mode đó | Như P: chip cam, dải "Cần gói", nút Cài đặt; Mode cũ không tự quay lại |
| Đổi Mode khi đang chạy | Nguồn khởi động lại với mô hình mới; slot đối tượng reset; gửi `frame` 0 điểm trước khi chuyển |
| Số đối tượng vượt sức chứa | Kẹp theo B9, giữ đối tượng tin cậy nhất; không bao giờ tràn khung |
| Full với nhiều người | Chi phí tăng theo số người (top-down chạy mô hình điểm một lần mỗi người); fps suy luận hiện riêng; khuyến nghị Body nếu chỉ cần thân |
| Nhầm tay trái/phải ở Mode Hand | Ghi trong tài liệu; Full lấy trái/phải theo cấu trúc toàn thân nên đáng tin hơn |

## 6. Kiểm thử

- **Engine (không cần GPU hay mô hình):**
  - Cắt vùng affine và phép ngược về ảnh: điểm (0,5; 0,5) của vùng cắt về đúng tâm hộp.
  - Giải mã SimCC trên tensor tự dựng: argmax đúng ô, độ tin cậy là giá trị lớn nhất.
  - Bảng `ModeModels`: số điểm, nhóm, điểm calibrate, sức chứa mỗi Mode.
  - Kẹp số đối tượng (B9) với cả bốn Mode.
  - `TouchPoint.group` vẫn POD và mặc định 0.
  - Mã hoá OSC theo nhóm (B7): một khung Full 3 người cho ra đúng địa chỉ cho từng nhóm, id theo B8, mặt dùng `slot·1000`; `OscSource` ghép lại đủ.
  - Ánh xạ 133 điểm RTMW sang nhóm (17 thân → id MediaPipe như A, 6 bàn chân, 68 mặt, 2×21 tay).
- **Chỉ chạy khi có gói và GPU** (bỏ qua có ghi chú khi thiếu): mỗi Mode chạy trên một ảnh mẫu có giấy phép sạch, ra đúng số đối tượng.
- **App (`--roundtrip`):** Mode khứ hồi trong file; `needsPack` đổi theo Mode; Full lọc nhóm gửi ra; phát lại (replay) một tệp có nhóm tay và mặt đi qua calibration và Send out, bên nhận loopback thấy đúng địa chỉ.
- **Ảnh `--shot`:** INPUT ở mỗi Mode bằng tệp replay; Properties với số đối tượng sau khi kẹp.
- **Đo trên máy có GPU:** fps và ms suy luận cho Hand (1, 4 tay), Face (1, 4 mặt), Full (1, 3 người). Ghi số đo thật.

## 7. Ngoài phạm vi

- **C:** cử chỉ có sẵn và cử chỉ tuỳ chỉnh (dùng nhóm tay và thân của B).
- Biểu cảm khuôn mặt (blendshape), hướng nhìn, lưới mặt dày.
- Chạy nhiều Mode cùng lúc trên một camera (ngoài Full).
- Camera 3D (spec riêng; B chạy trên luồng Color của nó).

## 8. Rủi ro

- **Full tốn GPU theo số người:** với 3 người, mô hình điểm chạy 3 lần mỗi khung. Phải đo trên máy thật; nếu không đạt, giảm `maxPeople` mặc định của Full.
- **Phát hiện tay ở xa:** bàn tay nhỏ trong ảnh 640×480 dễ trượt; có thể cần ảnh 1280×720 cho Hand. Ghi trong tài liệu, cho chọn độ phân giải như A.
- **Id mặt khác quy ước** (`slot·1000`): bên nhận phải đọc tài liệu giao thức; giữ trong một bảng duy nhất ở tài liệu OSC.
- **Thêm trường `group`** làm `TouchPoint` lớn thêm một byte (có thể bị đệm thành 4); phải đo lại kích thước khung sau A.

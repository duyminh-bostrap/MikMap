# C: Nhận dạng cử chỉ, gồm cử chỉ tuỳ chỉnh

Ngày: 2026-11-01 · Trạng thái: chờ người dùng duyệt · Phụ thuộc: A (khung camera, `CameraSource`, `PersonTracker`), B (Mode Hand/Full, nhóm điểm tay) · Nhánh nền: `feat/sensor-skeleton-hokuyo`

## 1. Mục tiêu

Người dùng (2026-10-30): "gesture detection… nếu custom được cử chỉ thì tốt". C biến điểm thân và tay (A, B) thành **sự kiện cử chỉ** dùng được ngay trong buổi diễn:

- **Bộ cử chỉ có sẵn**: tư thế bàn tay tĩnh, cử chỉ tay động, tư thế thân.
- **Cử chỉ tuỳ chỉnh**: người vận hành tự ghi vài mẫu rồi đặt tên, không cần huấn luyện mô hình hay GPU.
- **Đầu ra**: gửi sự kiện qua OSC/UDP kèm vị trí trên screen, và làm **nguồn trigger** trong MikMap (bắn clip, cột…) qua mục Triggers đã có.

Thước đo thành công:
- Giơ tay mở lòng bàn tay trước camera thì bên nhận OSC nhận đúng một sự kiện `start`, đúng một `end` khi hạ tay, không nhấp nháy giữa chừng.
- Ghi một cử chỉ mới bằng 5 mẫu trong chưa tới một phút, rồi thấy nó được nhận ra trên người khác.
- Không sự kiện nào bị rơi (kênh sự kiện, không phải kênh trạng thái), và việc nhận dạng không làm chậm render thread.

## 2. Quyết định (mình đề xuất, người dùng xác nhận khi duyệt spec)

| # | Quyết định | Lý do |
|---|---|---|
| C1 | Nhận dạng **bằng hình học và mẫu**, không dùng mạng nơ-ron: tư thế tĩnh = so vector đặc trưng đã chuẩn hoá với mẫu (láng giềng gần nhất có ngưỡng); cử chỉ động = so quỹ đạo đã lấy mẫu lại bằng **DTW** | Chạy CPU vài micro giây, tất định, ghi mẫu mới là dùng được ngay, không cần gói hay GPU, dễ test |
| C2 | Đặc trưng tay: 21 điểm, dời gốc về cổ tay, chia theo khoảng cách cổ tay–gốc ngón giữa, xoay về trục cổ tay → ngón giữa, lật tay trái về tay phải. Đặc trưng thân: 17 khớp chuẩn hoá theo vai và hông | Không phụ thuộc vị trí, cỡ người, khoảng cách tới camera, tay trái hay phải |
| C3 | Bộ có sẵn — **tay tĩnh**: Open palm, Fist, Point, Thumbs up, Victory (V), OK, Pinch. **Tay động**: Swipe left/right/up/down, Wave. **Thân**: Both arms up, T-pose, Hands together (vỗ tay) | Các cử chỉ phổ biến nhất trong sắp đặt tương tác; mở rộng được bằng cử chỉ tuỳ chỉnh |
| C4 | Pha sự kiện: `start` (giữ đủ `holdMs`, mặc định 150 ms với tĩnh), `end` (mất đủ `releaseMs`, mặc định 200 ms); cử chỉ động chỉ có `fire` một lần; `cooldownMs` mặc định 500 ms giữa hai lần `fire` | Chống nhấp nháy, tránh bắn trùng |
| C5 | Nhận dạng chạy **trong thread suy luận** của `CameraSource` (thấy mọi khung), sự kiện đi sang render thread qua **`SpscRingBuffer`** (kênh sự kiện, không được rơi) | Đúng quy tắc kiến trúc: trạng thái qua `TripleBuffer`, sự kiện qua ring; render thread không làm việc nặng |
| C6 | Sự kiện mang **điểm neo thô** (tâm bàn tay hoặc giữa hai cổ tay, toạ độ ảnh 0–1); render thread áp calibration (`H_s`/warp) để ra vị trí trên screen, giống điểm thường | Hiệu ứng nổ đúng chỗ người làm cử chỉ; toạ độ thô qua biên thread |
| C7 | OSC: `/mikmap/<dev>/gesture s:name s:phase i:slot s:hand f:x f:y f:score` (`hand` = `left`, `right`, `both` hoặc `body`; x, y theo gốc toạ độ của sensor, D17) | Một địa chỉ cho mọi cử chỉ, lọc theo tên ở bên nhận |
| C8 | Trigger: mỗi cử chỉ đang bật là một **nguồn** trong Triggers (`Route.source = "gesture:<deviceId>:<gestureId>"`, với `gestureId` là id ổn định của cử chỉ, không phải tên hiển thị); pha `start`/`fire` bắn route | Dùng lại cơ chế patch cord đã có, không làm hệ thống trigger thứ hai |
| C9 | Cử chỉ tuỳ chỉnh lưu **trong dự án** (`.mikmap`, mục `gestures`), vì nó thuộc buổi diễn; xuất/nhập được qua tệp `.mikgesture` để dùng lại giữa các dự án | Mô tả buổi diễn tách khỏi mô tả máy |
| C10 | Ghi mẫu: tĩnh = 5 mẫu (mỗi mẫu là trung bình 10 khung khi người giữ yên); động = 5 lần làm, mỗi lần là quỹ đạo 0,3–2 s tự cắt theo chuyển động. Có chế độ **Thử** hiện điểm khớp trực tiếp với từng cử chỉ | Đủ để nhận ra trên người khác; người vận hành tự kiểm trước khi diễn |
| C11 | Mỗi cử chỉ có **độ nhạy** (ngưỡng), tay áp dụng (trái, phải, cả hai), bật/tắt riêng; mặc định tắt mọi cử chỉ có sẵn trừ Open palm và Swipe left/right | Ít sự kiện thừa khi chưa cấu hình |
| C12 | Cử chỉ tay cần Mode **Hand hoặc Full** (B); cử chỉ thân cần **Body hoặc Full**. Cử chỉ không chạy được với Mode hiện tại thì hiện mờ, kèm lý do | Cùng kiểu "bảng khả năng" như A1b |

## 3. Bất biến kiến trúc

- `engine/io` chứa toàn bộ phần nhận dạng (đặc trưng, mẫu, DTW, máy trạng thái pha) và không biết UI hay `Route`. Lưu trữ mẫu là JSON thuần; `src/` đọc/ghi vào dự án.
- Sự kiện **không bao giờ bị rơi**: `SpscRingBuffer<GestureEvent>` có sức chứa đủ (ví dụ 256); khi đầy thì đếm và báo lỗi rõ chứ không ghi đè im lặng.
- `GestureEvent` là POD: tên cử chỉ là id số (`uint16_t`) tra theo bảng, không phải chuỗi.
- Thư viện mẫu đổi trên render thread (ghi mẫu, đổi ngưỡng) được chuyển sang thread suy luận bằng cách đổi cả khối qua `TripleBuffer` (sao chép một lần khi đổi), không dùng khoá.

## 4. Thành phần

| Thành phần | Vị trí | Việc |
|---|---|---|
| `GestureFeatures` | `engine/io/gesture/` | Chuẩn hoá tay và thân thành vector đặc trưng (C2); lấy mẫu lại quỹ đạo |
| `GestureLibrary` | `engine/io/gesture/` | Định nghĩa cử chỉ (có sẵn + tuỳ chỉnh): loại tĩnh/động, tay áp dụng, ngưỡng, mẫu; đọc/ghi JSON |
| `GestureRecognizer` | `engine/io/gesture/` | Mỗi khung: đặc trưng theo từng đối tượng (slot, tay), so với thư viện, máy trạng thái pha theo C4, phát `GestureEvent` |
| `GestureRecorder` | `engine/io/gesture/` | Thu mẫu theo C10, trả về mẫu mới hoặc lỗi ("giữ yên chưa đủ lâu", "không thấy tay") |
| Nối vào `CameraSource` | `engine/io/sources/` | Chạy recognizer sau tracker trong thread suy luận; ring sự kiện; nhận thư viện mới qua `TripleBuffer` |
| App | `src/` | Properties › **Gestures** (tab OUTPUT): danh sách cử chỉ, đèn sáng khi đang nhận, độ nhạy, tay áp dụng; nút **Record gesture** (đếm ngược 3 s, ghi mẫu, đặt tên), **Test**, xuất/nhập; gửi OSC (C7); nguồn Triggers (C8); INPUT vẽ tên cử chỉ cạnh tay đang làm |

## 5. Lỗi và giới hạn phải thấy được

| Tình huống | Hành vi |
|---|---|
| Hai cử chỉ giống nhau quá (khoảng cách mẫu nhỏ hơn ngưỡng) | Khi ghi mẫu thì cảnh báo "Gần giống <tên>"; vẫn cho lưu |
| Mẫu ghi được kém (tay bị khuất, quá ít khung) | Bỏ mẫu đó, nói lý do, cho ghi lại |
| Tay trái/phải bị nhầm (Mode Hand) | Đặc trưng đã lật về một phía (C2) nên cử chỉ tĩnh không ảnh hưởng; swipe trái/phải dùng hướng trên ảnh đã lật theo cài đặt Flip |
| Ring sự kiện đầy | Đếm số sự kiện bị bỏ, hiện lỗi đỏ ở Properties; không ghi đè im lặng |
| Đổi Mode làm mất nhóm điểm cần thiết | Cử chỉ phụ thuộc hiện mờ (C12); mọi cử chỉ đang `start` phát `end` ngay |
| Người rời khung hình khi đang giữ cử chỉ | Phát `end` khi slot bị bỏ (theo `keepSec` của tracker) |

## 6. Kiểm thử

- **Engine (không cần camera, GPU hay mô hình):**
  - Đặc trưng: cùng một tay dời, phóng to, xoay, lật trái/phải cho ra cùng vector (±1e-4).
  - Mỗi cử chỉ có sẵn: một bộ điểm tay/thân dựng tay được nhận đúng tên; các tư thế khác bị từ chối.
  - DTW: swipe trái khớp mẫu swipe trái kể cả khi nhanh/chậm gấp đôi; swipe phải không khớp.
  - Máy trạng thái pha: dữ liệu nhấp nháy (khớp/không khớp xen kẽ ngắn hơn `holdMs`) không phát `start`; giữ đủ thì đúng một `start`, thả đủ `releaseMs` thì đúng một `end`; `cooldownMs` chặn `fire` trùng.
  - Recorder: 5 mẫu tĩnh tạo được cử chỉ nhận ra chính nó; mẫu thiếu tay bị từ chối kèm lý do; cảnh báo "Gần giống".
  - Thư viện JSON khứ hồi; tệp hỏng không làm sập.
  - Ring đầy: đếm bỏ, không ghi đè.
- **App (`--roundtrip`):** cử chỉ tuỳ chỉnh lưu trong `.mikmap` và khứ hồi; xuất/nhập `.mikgesture`; một tệp replay (A) chứa chuỗi tay mở → nắm → mở đi qua `SensorsTick` cho đúng chuỗi sự kiện tới `OscSource` loopback, kèm vị trí đã calibrate; một Route `gesture:<dev>:<id của Open palm>` bắn đúng target, và vẫn bắn sau khi đổi tên cử chỉ; tick không làm bẩn dự án.
- **Ảnh `--shot`:** Properties › Gestures (danh sách, đèn đang sáng, mờ theo Mode), màn hình ghi mẫu, INPUT có nhãn cử chỉ.
- **Trên camera thật:** độ chính xác từng cử chỉ có sẵn với vài người khác nhau; ghi số liệu thật, không ước lượng.

## 7. Ngoài phạm vi

- Nhận dạng cử chỉ bằng mạng nơ-ron hay huấn luyện mô hình.
- Ngôn ngữ ký hiệu, cử chỉ hai người phối hợp.
- Cử chỉ khuôn mặt (nháy mắt, há miệng): cần biểu cảm mặt, nằm ngoài B.
- Cử chỉ 3D từ chiều sâu (để cho spec Camera 3D).

## 8. Rủi ro

- **Nhận dạng hình học kém hơn mô hình học máy** với tư thế khó (tay nghiêng mạnh, ngón che nhau). Cần đo độ chính xác thật; nếu không đạt, có thể thêm một bộ phân loại nhỏ sau mà không đổi giao diện sự kiện.
- **Tay ở xa** cho điểm nhiễu (rủi ro "phát hiện tay ở xa" của B): cử chỉ tĩnh dễ nhấp nháy; `holdMs` và độ nhạy là cách điều chỉnh, ghi trong tài liệu.
- **Swipe phụ thuộc hướng camera và Flip:** tài liệu phải nói swipe trái là trái **trên ảnh hiển thị** (sau khi lật).
- **Tên cử chỉ đổi sau khi đã nối Triggers:** Route tham chiếu theo id ổn định của cử chỉ, không theo tên hiển thị, để đổi tên không làm đứt trigger.

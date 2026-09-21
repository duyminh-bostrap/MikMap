---
name: shop-amazon
description: Hỗ trợ tìm/so sánh linh kiện phần cứng AV-tương tác (sensor, board điều khiển, cáp/switcher) trên Amazon để phục vụ MikMap — không phải mua sắm chung chung. Dùng khi cần chọn thiết bị cho một nguồn sensor mới (Serial/OSC/TUIO/Kinect/LiDAR) hoặc phần cứng trình chiếu (cáp HDMI/DP, mount máy chiếu).
---

# Mua linh kiện AV/sensor cho MikMap trên Amazon

Ghi chú: môi trường này không có công cụ duyệt/mua hàng trực tiếp trên
Amazon. Skill này định hướng **tiêu chí chọn hàng** khớp với kiến trúc sensor
của MikMap (`architecture.md` §"calib", `newui/SKILL.md` mục "Sensor (nhóm
G)") — khi có quyền truy cập web/agent duyệt web, dùng skill này làm checklist
trước khi chốt đơn.

## Ánh xạ nhu cầu phần cứng → module code tương ứng

| Nhu cầu phần cứng | Module MikMap đọc dữ liệu | Lưu ý khi chọn |
|---|---|---|
| Board Arduino/vi điều khiển gửi cảm biến qua Serial | `io/` — **G3 Serial Arduino** | Chọn board có driver USB-Serial ổn định (CH340/FTDI chính hãng) — hàng nhái CH340 hay rớt cổng COM khi cắm nóng giữa buổi diễn. |
| Cảm biến chạm/khoảng cách rời rạc | `io/sources` qua Serial hoặc OSC | Ưu tiên loại ra tín hiệu số/analog ổn định, có datasheet — tránh loại "không rõ nguồn gốc" không đo được jitter. |
| Máy tính bảng/app điều khiển gửi TUIO | **G14 TUIO** | Không cần mua phần cứng riêng — kiểm ứng dụng nguồn TUIO có chuẩn tương thích trước. |
| Depth camera (Kinect/LiDAR) | `ofxAzureKinect` (tuỳ chọn, "chốt sau khi mua phần cứng" theo `architecture.md`) | Kiểm SDK còn được hỗ trợ trên Windows 10/11 hiện tại trước khi mua — dòng Kinect cũ có SDK ngừng cập nhật. |
| Cáp/switcher HDMI-DP ra nhiều máy chiếu | Không qua code, chỉ ảnh hưởng `Screen`/output | Ưu tiên cáp có chip chuyển tín hiệu chủ động (active) cho khoảng cách >5–10m và độ phân giải 4K@60 — cáp thụ động giá rẻ hay rớt tín hiệu giữa buổi diễn, đúng dạng lỗi mà `architecture.md` C6 (đo độ trễ, không đoán) muốn tránh ở tầng phần mềm thì phần cứng cũng phải đạt. |

## Checklist trước khi chốt đơn

1. **Độ trễ/tần suất đo được, không suy đoán từ mô tả sản phẩm** — cùng
   nguyên tắc C6 trong `architecture.md`: nếu seller không ghi rõ baud rate/
   sample rate, tìm sản phẩm khác thay vì đoán.
2. **Có driver/SDK còn duy trì** cho Windows 10/11 (môi trường build chính
   của MikMap) — kiểm ngày cập nhật SDK trước khi mua, không chỉ theo giá.
3. **Đủ số lượng để test thật với `MockSource`/`--demo` trước khi mua nhiều**
   — mua 1 cái để xác nhận tích hợp OSC/Serial chạy đúng bằng `SensorMapper`
   thật rồi mới đặt số lượng lớn cho buổi diễn.
4. **Tránh hàng nhái driver USB-Serial** (CH340/CL2102 giả) — nguồn phổ biến
   nhất của lỗi "cổng COM rớt giữa chừng" trong hệ thống dùng Serial liên tục
   nhiều giờ như một buổi trình diễn.
5. Ghi lại model/nguồn mua vào `.claude/CLAUDE.local.md` (không vào git) —
   đây là thông tin *máy/người*, giống lý do `bin/data/settings.json` cũng
   đứng ngoài git.

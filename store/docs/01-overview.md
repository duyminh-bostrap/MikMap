---
title: Tổng quan
slug: overview
---
MikMap là phần mềm projection mapping. Một dự án có ba trang, chuyển bằng ba tab trên cùng.

| Trang | Dùng để |
|---|---|
| Composition | Dựng hình: lưới layer và cột, hai khung xem, Browser, Properties |
| Advanced Mapping | Chọn vùng canvas cho từng slice, uốn hình, thêm mặt nạ, bố trí đầu ra |
| Sensor I/O | Thiết bị cảm biến, hiệu chỉnh (calibration), vùng chạm, dây định tuyến tham số |

Bố cục cố định, không có cửa sổ nổi. Ngoài ba trang còn có cửa sổ **Cài đặt** và cửa sổ **output** trên màn hình máy chiếu.

## Đường đi của hình

Composition tạo ra một canvas (mặc định 1920×1080). Mỗi slice trong Advanced Mapping lấy một vùng của canvas, uốn nó theo phối cảnh và lưới điểm, cắt bằng mặt nạ, rồi vẽ vào cửa sổ output của screen chứa nó.

## Tình trạng tính năng

Bản hiện tại là bản đang phát triển. Tóm tắt những gì chạy thật:

- Chạy thật: lưới clip, phát clip và cột, hai khung xem, ảnh tĩnh làm nguồn, slice với phối cảnh và lưới warp, mặt nạ nhiều hình dạng, cửa sổ output, lưu và mở dự án, Undo và Redo.
- Giả lập: thiết bị cảm biến và điểm chạm ở trang Sensor I/O. Hiệu chỉnh tính ma trận thật, nhưng điểm chạm đến từ việc bấm lên radar.
- Chưa có: video, âm thanh, NDI, Spout, nhận OSC hay TUIO từ cảm biến thật. Các mục Sources, Generators và Effects trong Browser là dữ liệu mẫu.

Chi tiết từng trang: [Composition](/store/mikmap/docs/composition), [Advanced Mapping](/store/mikmap/docs/advanced-mapping), [Sensor I/O](/store/mikmap/docs/sensor-io).

## Quyền sử dụng

Quy định: bản không có license có watermark và không gửi được tín hiệu sensor hay NDI. Bản hiện tại chưa thực thi quy định này. Xem [Phiên bản và license](/store/mikmap/docs/editions-and-licensing).

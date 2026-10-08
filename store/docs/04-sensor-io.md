---
title: Trang Sensor I/O
slug: sensor-io
---
Ba cột: **Device Manager** và hiệu chỉnh ở trái, **Radar View** ở giữa, **Parameter Routing** ở phải.

> Trang này hiện chạy với dữ liệu giả lập. MikMap chưa nhận tín hiệu từ cảm biến thật. Phần tính toán hiệu chỉnh là thật.

## Thiết bị

Mỗi thẻ thiết bị có nút **CONNECT** hoặc **DISCONNECT**. Nút chỉ đổi cờ trạng thái, các con số FPS, LATENCY và PACKETS là mẫu.

## Hiệu chỉnh (calibration)

1. Bấm **Start wizard**.
2. Bấm vào radar để đặt từng điểm theo thứ tự các góc. Có thể bấm **Cancel** giữa chừng.
3. Khi đủ điểm, ma trận biến đổi 3×3 và sai số RMS hiện ra. Ma trận tính bằng DLT.

Có thể lưu và nạp hồ sơ hiệu chỉnh bằng **Save profile** và **Load profile**.

## Radar

Bấm lên radar để giả lập một điểm chạm. Điểm đó được chiếu qua hiệu chỉnh lên sân khấu Mapping (trang Output, screen đầu tiên). **Output overlay** vẽ điểm chạm lên cửa sổ output, mặc định tắt. **Edit ROI** cho kéo bốn điểm giới hạn vùng chạm.

## Parameter Routing

Mỗi dây nối một sự kiện với một đích. Dây `touch.down` đang bật sẽ phát clip đích khi bạn bấm radar. Các dây khác mới là hiển thị, chưa tác động. Hai thanh **Noise threshold** và **Min blob size** chỉ lưu giá trị.

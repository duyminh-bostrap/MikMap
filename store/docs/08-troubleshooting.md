---
title: Xử lý sự cố
slug: troubleshooting
---
## Ứng dụng không mở được, cửa sổ không hiện

- **Nguyên nhân:** máy không tạo được ngữ cảnh OpenGL cần thiết. Hay gặp khi chạy trong máy ảo, qua điều khiển từ xa hoặc khi thiếu trình điều khiển đồ hoạ.
- **Cách xử lý:** cập nhật trình điều khiển đồ hoạ. Trong máy ảo, bật tăng tốc 3D. Trên Linux, cài thư viện OpenGL của hệ thống.

## Windows chặn hoặc cảnh báo khi chạy

- **Nguyên nhân:** gói phát hành chưa được ký số.
- **Cách xử lý:** ở cửa sổ SmartScreen chọn **More info**, rồi **Run anyway**. Có thể kiểm tra tệp trước bằng `SHA256SUMS.txt` đi kèm bản phát hành.

## macOS không cho mở

- **Nguyên nhân:** gói chưa được ký và công chứng.
- **Cách xử lý:** vào **System Settings**, **Privacy & Security**, chọn **Open Anyway** cho MikMap.

## Linux báo thiếu thư viện

- **Nguyên nhân:** thiếu GLFW hoặc OpenGL của hệ thống.
- **Cách xử lý:** cài các gói tương ứng, ví dụ `libglfw3` và `libgl1` trên Debian hoặc Ubuntu.

## Ảnh không hiện trong thư mục Media

- **Nguyên nhân:** tệp không nằm trong `Documents/MikMap/media`, hoặc định dạng không phải PNG, JPG, BMP, TGA, hoặc bạn chép ảnh sau khi Browser đã quét.
- **Cách xử lý:** chép ảnh đúng thư mục rồi bấm vào thư mục **Media** để quét lại.

## Hình ra máy chiếu không khớp bề mặt

- **Nguyên nhân thường gặp:** slice chưa được chọn đúng screen, hoặc đang ở trang Input thay vì Output.
- **Cách xử lý:** mở **Advanced Mapping**, chọn tab **Output routing**, bật **Edit Points** và kéo các điểm. Bật **SHOW TESTCARD** để thấy rõ các góc.

## Cửa sổ output mở nhầm màn hình

Chọn lại thiết bị đầu ra của screen trong Advanced Mapping. Nếu cửa sổ đang mở, nó tự chuyển sang màn hình mới.

## Khung Timeline và nút transport biến mất

Chiều cao thanh timeline đang là 0. Vào Cài đặt, tab **Bố cục**, tăng **Cao thanh timeline**.

## Mở dự án báo cảnh báo hoặc thiếu dữ liệu

Tham chiếu hỏng được đưa về trạng thái mặc định kèm cảnh báo, không bị bỏ qua im lặng. Đọc thông báo hiện ở thanh trạng thái.

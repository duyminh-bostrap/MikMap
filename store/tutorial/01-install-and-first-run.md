---
title: Cài đặt và chạy lần đầu
slug: install-first-run
---
Bài này đưa MikMap lên máy bạn và mở được dự án mẫu.

## Tải về

1. Mở trang Download của MikMap trên cửa hàng, hoặc vào kho phát hành `duyminh-bostrap/MikMap-releases` trên GitHub.
2. Tải gói đúng hệ điều hành: tệp `.zip` cho Windows, tệp `.tar.gz` cho macOS và Linux.
3. Giải nén vào một thư mục bạn chọn. Giữ thư mục `assets` nằm cạnh tệp chạy, ứng dụng đọc font và logo từ đó.

> Các gói hiện tại là bản nén, chưa có trình cài đặt, và chưa được ký số. Xem [Yêu cầu hệ thống](/store/mikmap/docs/system-requirements) và [Ghi chú phát hành](/store/mikmap/docs/release-notes).

## Mở ứng dụng

1. Windows: chạy `mikmap.exe`. Nếu SmartScreen hiện cảnh báo "Windows protected your PC", chọn **More info**, rồi **Run anyway**.
2. macOS: chạy `mikmap`. Nếu hệ thống chặn vì chưa ký, mở **System Settings**, vào **Privacy & Security**, chọn **Open Anyway** cho MikMap.
3. Linux: cài thư viện GLFW và OpenGL của hệ thống rồi chạy `./mikmap`.

Bạn sẽ thấy cửa sổ MikMap với ba tab ở trên cùng: **Composition**, **Advanced Mapping** và **Sensor I/O**, cùng một dự án mẫu tên *MikMap Stage 01*.

![Trang Composition khi mở lần đầu](TODO-upload:first-run-composition.png)

## Chọn ngôn ngữ

1. Bấm biểu tượng bánh răng ở góc phải thanh trên cùng để mở **Cài đặt**.
2. Ở tab **Ngôn ngữ**, chọn ngôn ngữ giao diện.
3. Bấm **XONG** hoặc nhấn `Esc`.

Thay đổi có hiệu lực ngay và được nhớ cho lần mở sau.

![Cài đặt, tab Ngôn ngữ](TODO-upload:settings-language.png)

Bước tiếp theo: [phát clip trên lưới](/store/mikmap/tutorial/trigger-clips).

---
title: Yêu cầu hệ thống
slug: system-requirements
---
## Hệ điều hành

| Hệ điều hành | Đã kiểm chứng |
|---|---|
| Windows | Build bằng MSVC (Visual Studio 2022) và MinGW-w64, chạy được |
| macOS | Apple Silicon, Xcode SDK 15.0 |
| Linux | Build và chạy dưới Xvfb |

TODO(owner): phiên bản tối thiểu của Windows, macOS và Linux mà bạn muốn hỗ trợ chính thức.

## Đồ hoạ

MikMap cần OpenGL. Trên macOS ứng dụng xin ngữ cảnh OpenGL 3.2 core; trên hệ khác, thử 3.2 core rồi lùi về 3.0. Không tạo được cửa sổ OpenGL thì ứng dụng không mở. Máy chiếu là màn hình hoặc cổng ra thông thường, hệ điều hành thấy như một màn hình thứ hai.

TODO(owner): yêu cầu RAM, dung lượng đĩa và GPU khuyến nghị, chưa có số đo nào trong repo.

## Gói cài

- Windows: tệp thực thi chỉ cần các DLL có sẵn của Windows.
- macOS: không cần cài thêm gì đã biết. TODO(owner): xác nhận gói chạy được trên máy chưa cài Homebrew GLFW.
- Linux: cần GLFW và thư viện OpenGL của hệ thống, ví dụ gói `libglfw3` và `libgl1` trên Debian hoặc Ubuntu. TODO(owner): xác nhận tên gói trên các bản phân phối bạn muốn hỗ trợ.

## Thư mục dữ liệu

Cần quyền ghi vào `Documents/MikMap` (dự án, media) và thư mục cấu hình của hệ điều hành (cài đặt). Xem [Dữ liệu và quyền riêng tư](/store/mikmap/docs/data-and-privacy).

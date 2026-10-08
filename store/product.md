---
title: MikMap
slug: mikmap
summary: Phần mềm projection mapping cho biểu diễn trực tiếp: dựng hình theo layer và cột, uốn hình lên bề mặt thật, che mặt nạ và xuất ra máy chiếu.
tech_stack: [C++20, Dear ImGui, GLFW, OpenGL, CMake]
releases_repo: duyminh-bostrap/MikMap-releases
licensed: true
status: draft
---
MikMap là phần mềm projection mapping chạy trên Windows, macOS và Linux. Bạn dựng hình trên một lưới layer và cột (mỗi ô là một clip), bấm vào ô hoặc vào đầu cột để phát, rồi xem đồng thời ở hai khung: Preview Cue để chuẩn bị và Live Output để xem đúng thứ đang chiếu.

Trang Advanced Mapping dành cho phần uốn hình. Mỗi màn hình đầu ra chứa nhiều slice. Bạn chọn vùng của canvas mà slice lấy, kéo bốn góc phối cảnh và lưới điểm warp để hình khớp với bề mặt thật, thêm mặt nạ nhiều hình dạng với viền mềm, rồi gửi kết quả sang cửa sổ máy chiếu.

Dự án lưu thành tệp .mikmap trong thư mục Documents/MikMap, có Undo và Redo cho toàn bộ ứng dụng. Cài đặt cho chọn ngôn ngữ giao diện (tiếng Việt, Anh, Nhật, Hàn, Trung), font chữ, màu nhấn và cỡ chữ.

Đây là bản đang phát triển. Nguồn hình thật hiện là ảnh tĩnh PNG, JPG, BMP và TGA; trang Sensor I/O đang chạy với thiết bị và điểm chạm giả lập, chưa nhận tín hiệu từ cảm biến thật. Trang Docs ghi rõ từng tính năng nào đã chạy thật và tính năng nào chưa.

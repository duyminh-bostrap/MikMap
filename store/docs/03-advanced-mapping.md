---
title: Trang Advanced Mapping
slug: advanced-mapping
---
Ba vùng: cây **Screen / Slice / Mask** ở trái, sân khấu (stage) ở giữa và **Properties** ở phải. Cây có thể thu gọn thành một dải dọc.

## Cây

- Bấm một dòng để chọn. Giữ `Ctrl` hoặc `Shift` rồi bấm để chọn nhiều phần tử cùng loại (không trộn screen, slice, mask).
- Biểu tượng mắt ẩn hoặc hiện screen, slice và mask. Mask ẩn vẫn nằm trong slice nhưng không cắt hình.
- Chuột phải mở menu: thêm, nhân đôi, xoá, Solo cho slice, Reset warp, Whole area và các lệnh khác.
- Ba nút ở chân cây thêm **Screen**, **Slice** và **Mask**.

## Hai trang trên sân khấu

**Input selection**: chọn vùng của canvas mà slice lấy. Nền là nội dung đang phát của nguồn mà slice nhận (Composition, Layer hoặc Group, chọn ở **Input source**). Khung input kéo, co giãn và xoay như khung biến đổi ở Preview Cue.

**Output routing**: đặt hình dạng của slice trên đầu ra. Mọi slice luôn có bốn điểm phối cảnh lớn ở góc và một lưới điểm warp bên trong. **Subdivisions** quyết định số điểm của lưới.

## Công cụ

- **Edit Points**: kéo điểm. Kéo thả vùng trống để chọn nhiều điểm bằng khung chọn.
- **Transform**: di chuyển, co giãn, xoay cả slice.
- **Bàn tay**: kéo chuột trái để cuộn sân khấu.
- **Magnet**: hít điểm vào điểm và cạnh gần nhất. Cạnh đã hít sáng trắng.
- Lăn chuột để zoom theo con trỏ; `Shift` và lăn để cuộn dọc; kéo chuột phải để pan.

## Slice Properties

Tên slice, **Input source**, **Input rectangle** (X, Y, Left, Top, Width, Height, Rotation), **Soft edge**, **INPUT MASK** và các nhóm Picture, Soft edge, Black level.

> **Soft edge** hiện chỉ là công tắc lưu theo slice. Cửa sổ output chưa làm mờ viền.

## Mask

Sáu hình có sẵn: tim, vuông, tròn, tam giác, lục giác và bút. Mask nằm trong không gian canvas, sửa ở trang Input và cắt hình gửi ra output thật. Panel Mask có **Invert (cut hole)**, **Feather** và **Mask rectangle**. Chế độ điểm **LINEAR** hoặc **BEZIER** cho đường biên thẳng hoặc cong mượt.

## Screen và thiết bị đầu ra

Mỗi screen có thiết bị đầu ra và độ phân giải. Màn hình vật lý hiện tên và độ phân giải thật. Nút **MỞ OUTPUT (F11)** mở cửa sổ máy chiếu cho mọi screen đã gán màn hình.

## Phím tắt của trang

`Ctrl+C`, `Ctrl+X`, `Ctrl+V`, `Ctrl+D` copy, cắt, dán, nhân đôi phần tử đang chọn; `Delete` xoá; phím mũi tên dịch 1 px, `Shift` và mũi tên dịch 10 px. Trên macOS thay `Ctrl` bằng `Cmd`.

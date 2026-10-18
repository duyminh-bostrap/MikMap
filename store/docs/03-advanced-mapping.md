---
title: Trang Advanced Mapping
slug: advanced-mapping
---
Ba vùng: cây **Screen / Slice / Mask** ở trái, sân khấu (stage) ở giữa và **Properties** ở phải. Cây có thể thu gọn thành một dải dọc.

## Cây

- Bấm một dòng để chọn. Giữ `Ctrl` hoặc `Shift` rồi bấm để chọn nhiều phần tử cùng loại (không trộn screen, slice, mask).
- Biểu tượng mắt ẩn hoặc hiện screen, slice và mask. Mask ẩn vẫn nằm trong slice nhưng không cắt hình.
- Chuột phải mở menu: thêm, nhân đôi, xoá, Solo cho slice, Reset warp, Whole area và các lệnh khác.
- Hai nút ở chân cây thêm **Screen** và **Slice**. Nút **Slice** mở menu chọn **Add slice** hoặc **Add polygon slice**.

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

Tên slice, **Input source**, **Input rectangle** (X, Y, Left, Top, Width, Height, Rotation), **Soft edge**, **INPUT MASK** và các nhóm Picture, Soft edge, Black level compensation.

**Soft edge** ghép mép giữa các máy chiếu. Bật nó ở hai slice có input rect chồng lên nhau (trên cùng screen hay khác screen đều được): mỗi slice tự làm mờ dải chồng ở cửa sổ output và trên sân khấu. Nhóm Soft edge chỉnh đường mờ bằng **Gamma red**, **Gamma green**, **Gamma blue**, **Gamma**, **Luminance** và **Power**. Bật mà không chồng với slice nào thì nhóm hiện `NO OVERLAP`.

**Black level compensation** (**Red**, **Green**, **Blue**) nâng màu đen ở phần nằm ngoài dải chồng, tối đa xám 20 %, để phần chỉ có một máy chiếu khớp với màu đen gấp đôi ở chỗ chồng.

> Input rect có xoay được tính theo khung bao. Công tắc **Edge blending** của screen chưa có tác dụng. Soft edge chưa được thử trên máy chiếu thật.

## Nhập toạ độ điểm

Ở trang **Output routing** với công cụ **Edit Points**, chọn đúng một điểm (góc phối cảnh hoặc điểm warp): đầu khối **WARPING** hiện tên điểm và hai ô **X**, **Y** (px đầu ra). Gõ số để đặt chính xác điểm đó. Chọn nhiều điểm hoặc không chọn điểm nào thì hai ô này ẩn.

## Slice đa giác

Slice đa giác có hình bất kỳ từ 3 đến 64 điểm, thay cho bốn góc và lưới warp. Thêm bằng **Add polygon slice** ở menu chuột phải của screen hoặc nút **Slice** ở chân cây. Slice mới là một ngũ giác.

- Trang **Output routing**, công cụ **Edit Points**: kéo điểm để đặt hình trên đầu ra. Trang **Input selection**: kéo điểm để chọn phần canvas mà slice lấy; kéo bên trong hình để dời cả hình.
- Bấm đúp lên một cạnh để thêm điểm. Chuột phải một điểm rồi chọn **Delete point**, hoặc chọn điểm rồi bấm `Delete`.
- Thao tác làm hình tự cắt chính nó bị từ chối, điểm ở lại chỗ cũ và thanh trạng thái báo `Shape would cross itself`.
- **Transform** di chuyển, co giãn, xoay cả hình. **Magnet** hít vào điểm và cạnh của hình.
- Chọn đúng một điểm: khối **OUTPUT POINTS** (hoặc **INPUT POINTS**) hiện **Point N** với hai ô **X**, **Y**.
- **Flip** lật hình ảnh bên trong, giữ nguyên hình dạng. Soft edge, Black level compensation, màu, gamma và mask dùng được như slice thường. Slice đa giác được lưu trong dự án.

> **Match Input Shape**, **Match Output Shape** và **Swap Input Output Shape** chép điểm theo thứ tự, một đổi một. Phím mũi tên dời cả slice đa giác không kiểm tra tự cắt. Phần mask nằm ngoài hình đa giác được ánh xạ gần đúng. Slice đa giác chưa được thử trên máy chiếu thật.

## Mask

Sáu hình có sẵn: tim, vuông, tròn, tam giác, lục giác và bút. Mask nằm trong không gian canvas, sửa ở trang Input và cắt hình gửi ra output thật. Panel Mask có **Invert (cut hole)**, **Feather** và **Mask rectangle**. Chế độ điểm **LINEAR** hoặc **BEZIER** cho đường biên thẳng hoặc cong mượt.

## Screen và thiết bị đầu ra

Mỗi screen có thiết bị đầu ra và độ phân giải. Màn hình vật lý hiện tên và độ phân giải thật. Nút **MỞ OUTPUT (F11)** mở cửa sổ máy chiếu cho mọi screen đã gán màn hình.

## Phím tắt của trang

`Ctrl+C`, `Ctrl+X`, `Ctrl+V`, `Ctrl+D` copy, cắt, dán, nhân đôi phần tử đang chọn; `Delete` xoá; phím mũi tên dịch 1 px, `Shift` và mũi tên dịch 10 px. Trên macOS thay `Ctrl` bằng `Cmd`.

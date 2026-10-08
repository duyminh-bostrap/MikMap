---
title: Uốn hình lên bề mặt bằng slice
slug: map-a-slice
---
Bạn sẽ thêm một slice, chọn vùng canvas nó lấy, rồi kéo góc để hình khớp bề mặt thật.

## Khái niệm

Một **screen** là một đầu ra, ví dụ một máy chiếu. Mỗi screen chứa các **slice**. Mỗi slice lấy một vùng của canvas làm đầu vào (**Input**) và đặt vùng đó ở một vị trí, một hình dạng trên đầu ra (**Output**).

## Thêm slice

1. Bấm tab **Advanced Mapping** ở trên cùng.
2. Ở cây bên trái, bấm một dòng screen để chọn nó.
3. Bấm nút **+ Slice** ở chân cây.
4. Slice mới xuất hiện dưới screen và đang được chọn.

![Slice mới trong cây và trên sân khấu](TODO-upload:mapping-add-slice.png)

## Chọn vùng lấy từ canvas

1. Ở đầu vùng giữa, chọn tab **Input selection**.
2. Kéo khung trong canvas để di chuyển, kéo ô vuông ở góc hoặc dọc cạnh để co giãn, kéo vòng tròn quanh góc để xoay.
3. Giữ `Shift` khi xoay để nhảy theo bước 15 độ.

Bạn có thể gõ số trong **Slice Properties** ở bên phải: ô **Input rectangle** có X, Y, Left, Top, Width, Height và Rotation.

## Uốn hình ở đầu ra

1. Chọn tab **Output routing**.
2. Bật công cụ **Edit Points**.
3. Kéo bốn điểm lớn ở góc để tạo phối cảnh. Kéo các điểm nhỏ trong lưới để uốn từng phần.

Hình trong slice thay đổi theo từng điểm bạn kéo.

![Kéo một góc để hình méo theo bề mặt](TODO-upload:mapping-edit-points.png)

Để thêm điểm, đổi **Subdivisions** trong Slice Properties. Muốn làm lại, mở menu **Reset** và chọn **Reset warp points** hoặc **Reset all warping**.

## Che bớt bằng mặt nạ

1. Chuyển sang tab **Input selection**.
2. Trong mục **INPUT MASK** của Slice Properties, bấm một trong sáu hình: tim, vuông, tròn, tam giác, lục giác hoặc bút.
3. Kéo khung mặt nạ như khung input. Bật **Invert (cut hole)** nếu muốn khoét lỗ thay vì giữ phần bên trong.

![Mặt nạ hình tròn trên trang Input](TODO-upload:mapping-input-mask.png)

## Làm nhanh hơn

- Giữ `Ctrl` hoặc `Shift` rồi bấm để chọn nhiều slice.
- Công cụ **Magnet** hít các điểm vào điểm và cạnh gần nhất.
- `Ctrl+Z` hoàn tác.

Bước tiếp theo: [xuất ra máy chiếu](/store/mikmap/tutorial/output-to-projector).

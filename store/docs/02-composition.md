---
title: Trang Composition
slug: composition
---
## Lưới

Mỗi hàng là một layer, mỗi cột là một cảnh. Các layer có thể gộp vào nhóm; bấm tiêu đề nhóm để thu gọn hoặc mở.

| Thao tác | Kết quả |
|---|---|
| Bấm bar của ô clip | Chọn và đưa lên Preview Cue, không phát |
| Bấm body của ô clip | Phát ngay ở Preview Cue và Live Output |
| Bấm ô trống | Dừng đúng layer đó |
| Bấm tiêu đề cột | Phát mọi clip không rỗng trong cột, dừng layer nào có ô trống |
| Kéo bar sang ô khác | Di chuyển clip |
| Kéo tiêu đề cột | Đổi thứ tự cột |
| Chuột phải bar | Menu clip: Trigger, Cue to Preview, Loop, Rename, Clear Slot và sáu màu clip |
| Chuột phải tiêu đề cột | Chèn, dịch, đổi tên, xoá cột, đặt cột tự phát khi mở dự án |

Màu của ô clip chỉ đánh dấu trong lưới, không đổi hình được chiếu.

## Hai khung xem và thanh timeline

**Preview Cue** hiện clip đã chọn. **Live Output** hiện toàn bộ composite đang chiếu. Dưới hai khung có đồng hồ hệ thống (**SYSTEM TIME**), menu deck, năm nút transport (lùi, phát, tạm dừng, dừng, tiến) và bộ đếm **TIMELINE**.

Có thể phóng to, thu nhỏ, kéo và chỉnh khung biến đổi của clip ngay trong Preview Cue.

## Deck và chế độ Timeline

Một dự án có thể có nhiều deck, chọn ở menu deck. Nút **GRID** và **TIMELINE** chuyển giữa lưới và chế độ timeline với mỗi layer một làn.

## Browser

Cây Browser có thư mục **Media**, nơi liệt kê ảnh PNG, JPG, BMP và TGA trong `Documents/MikMap/media`. Các thư mục **Sources**, **Generators**, **Effects** và **Composition** là dữ liệu mẫu cố định, chưa phải nguồn thật. Kéo ảnh vào ô để tạo clip.

## Properties

Ba tab:

- **COMP**: độ phân giải canvas, Master, Speed, âm lượng, độ mờ. Nhiều mục chỉ lưu giá trị, chưa tác động (ví dụ âm thanh và crossfader).
- **LAYER**: độ mờ, âm lượng, tám chế độ hoà trộn (blend mode), Solo, Mute và Bypass.
- **CLIP**: ảnh xem trước, PLAYHEAD, PLAY MODE (LOOP, BOUN, HOLD, ONCE), SPEED, TRANSFORM (X, Y, Scale, Rotation, Opacity, lật ngang dọc) và FX CHAIN.

Trong FX CHAIN, hiện chỉ **Strobe**, **Hue Shift** và **Mirror** làm đổi hình. Các hiệu ứng khác chỉnh được tham số nhưng chưa vẽ.

## Đổi kích thước các khối

Kéo thanh ngang giữa nửa trên và lưới để đổi chiều cao. Kéo hai khe dọc hai bên cụm hai khung xem để đổi bề rộng Browser (140 đến 320 px) và Properties (180 đến 360 px). Hai giá trị này cũng nằm ở [Cài đặt](/store/mikmap/docs/settings), tab **Bố cục**.

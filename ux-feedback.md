# Góp ý UX — ghi ý bạn ở đây, Claude sẽ sửa theo

> Cách dùng: thêm một mục dưới "Đang chờ sửa". Viết tự nhiên cũng được, càng
> rõ **màn hình nào** và **muốn hành vi/hình thức ra sao** càng tốt. Khi Claude
> sửa xong sẽ chuyển mục sang "Đã xong" kèm tên commit.
>
> Bản đồ file (để bạn biết ý mình nằm ở đâu, không bắt buộc ghi):
> Composition → `src/deck.cpp` · Advanced Mapping → `src/mapping.cpp` ·
> Sensor I/O → `src/sensor.cpp` · Cài đặt → `src/settings.cpp` ·
> thanh trên/dưới, menu Project, phím tắt → `src/main.cpp` ·
> nút/badge/màu dùng chung → `src/ui.h` + `src/ui.cpp`

## Đang chờ sửa

<!-- Mẫu:
### [Composition] Bấm ô clip
- Hiện tại: bấm một lần chỉ chọn, bấm đúp mới phát.
- Muốn: bấm một lần phát luôn; giữ Shift + bấm mới chỉ chọn.
- Ghi chú: (ảnh chụp, ví dụ Resolume làm thế nào, mức ưu tiên...)
-->

## Đã xong

### [Composition] Tách vùng bấm ô clip: bar vs body
- Yêu cầu (2026-09-22): bar (phần tên, trên) — bấm chọn không đổi clip của layer,
  chỉ hiển thị trên preview; kéo thả đổi sang ô mới; chuột phải mở popover.
  body (phần gradient, dưới) — bấm phát clip cả preview lẫn output, chọn clip
  của layer đó; không kéo thả, không chuột phải ở đây.
- Đã sửa: `ClipCell`/`CellOut`, `src/deck.cpp` — commit "Split clip cell into
  independent bar (cue) and body (trigger) hit-zones". Cũng cập nhật
  `.claude/rules/design.md`, `ux-current.md` §2.1, dòng gợi ý ở thanh trạng thái.
- Đã xác nhận bằng ảnh chụp headless: bấm bar → cue (không phát, Live Output
  không đổi); bấm body → phát ngay (Live Output đổi ngay từ 1 click); kéo từ
  bar → di chuyển clip như cũ; kéo từ body → không di chuyển, chỉ phát; chuột
  phải body → không mở popover.

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

### [Composition] Kéo thanh giữa các khối để đổi bề rộng/chiều cao
- Yêu cầu (2026-09-24): ở giữa các khối bố cục trang Comp, kéo được thanh ở giữa
  để chỉnh width/height.
- Trước đó chỉ có thanh ngang (dải trên ↔ Deck) kéo được; bề rộng Browser và
  Properties chỉ chỉnh được bằng slider trong Cài đặt → Layout.
- Đã sửa: hai khe dọc 4px hai bên cụm monitor giờ là thanh kéo —
  `ColumnSplittersInput`/`ColumnSplittersDraw`, `src/deck.cpp`. Ghi thẳng vào
  `prefs.browserW`/`prefs.inspectorW` (cùng giới hạn 140–320 / 180–360px như
  slider, tự lưu `settings.json`), cụm monitor không hẹp quá 560px. Commit
  "Composition: drag the gaps beside the monitors to resize Browser/Properties".
- Đã xác nhận bằng ảnh chụp headless (`--drag`): kéo khe trái 202→300 ra Browser
  ~298px, khe phải 1202→1100 ra Properties ~338px; trên cửa sổ 1100px kéo cả hai
  quá tay thì Browser dừng ở 296px, Properties giữ 236px — monitor còn đúng 560px,
  tiêu đề Preview Cue không đè nhau.

### [Toàn app] Cỡ chữ mặc định lớn hơn 20%
- Yêu cầu (2026-09-24): toàn bộ cỡ chữ mặc định lớn hơn hiện tại 20%.
- Đã sửa: hệ số `ui::kTextScale = 1.2` áp ở `ui::TextW`/`ui::Text` (mọi chữ vẽ
  qua đây) và 4 chỗ `PushFont` của ô nhập liệu (`ui::TextPx`). Chỉ chữ to lên —
  panel, hàng, icon giữ nguyên kích thước; khác Cài đặt → Cỡ chữ (phóng cả giao
  diện). Commit "Draw all UI text 20% larger".
- Sửa kèm: tên nhóm trên dải ghim vẽ đè fader khi chữ to hơn → nay cắt bằng
  "…" trước fader; chữ giãn cách vẽ từng ký tự bị lệch khoảng ("AD D") → làm
  tròn vị trí glyph.
- Đã soát bằng ảnh chụp cả 3 trang + cửa sổ Cài đặt ở 1440×900. **Hệ quả cần
  biết:** cột layer cố định 178px nên tên nhóm dài bị cắt nhiều hơn trước
  ("STAGE …", "AUDIO …").

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

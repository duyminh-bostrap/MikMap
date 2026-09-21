# Quy ước giao diện (ImGui)

Nguồn sự thật cho giao diện là bản thiết kế tham khảo `MikMap_Web`
(React/Vite/Tailwind, xem `README.md`/`UI_UPDATE_PROGRESS.md`) và `sampleUI/`
— **không đoán từ lớp Tailwind, đối chiếu bằng số đo DOM thật**:

```js
// console tại localhost:3000 của MikMap_Web
el.getBoundingClientRect()   // vị trí + kích thước px thật
getComputedStyle(el)         // font-size, padding, border, color thật
```

## Bố cục

Ba trang cố định qua tab trên cùng — **không phải cửa sổ nổi**: trong phòng
tối giữa buổi diễn không ai có thời gian sắp lại bàn làm việc, và một bảng
trôi ra ngoài màn hình là chuyện xảy ra thật.

```
MIKMAP  Dự án │ COMPOSITION │ ADVANCED MAPPING │ SENSOR I/O    FPS 60.0  ...
```

Riêng **Cài đặt** là popup (ngôn ngữ, màn hình output mặc định, vsync) —
đây là mô tả *máy/người*, tách khỏi file project (`.hexmap`) mô tả *một buổi
diễn*. Trộn hai thứ nghĩa là mở project đồng nghiệp sẽ đổi ngôn ngữ giao diện
của bạn. Vì vậy `bin/data/settings.json` cố ý không vào git.

## Helper dùng chung (`Theme.h`/`Theme.cpp`)

Luôn dùng lại, đừng viết widget riêng lẻ trùng chức năng:

- `theme::beginPanel`/`endPanel` — mọi panel con. Cần
  `ImGuiChildFlags_AlwaysUseWindowPadding` nếu không lề trong bị ImGui bỏ qua.
- `theme::panelHeader` — dải 40px nền `#181818`, chữ 10px in hoa giãn 1px
  (`textTracked`/`trackedWidth` — ImGui không có letter-spacing, phải tự vẽ
  từng ký tự UTF-8 rồi cộng bước nhảy).
- `theme::fieldLabel` (nhãn ô nhập 10px đậm #888), `theme::sectionDivider`
  (vạch ngăn mục 1px #222), `theme::segButton` (bộ chọn phân đoạn, ô không
  chọn vẫn thấy khung — khác `tabButton`), `theme::tabButton` (active nhuộm
  accent), `theme::opacityBar`, `theme::treeRow`.
- `FramePadding (10,9)`, `FrameRounding 4px` — khớp ô nhập cao 30px của bản
  thiết kế, đừng đổi cho panel riêng lẻ.

## Font & thang cỡ chữ

- Inter 400/600/700 — chữ giao diện. Cắt từ `Inter.ttf` (variable, trục
  `wght`) bằng `fontTools`, không tải thêm file.
- RobotoMono — **riêng cho số liệu** (toạ độ, FPS, độ phân giải). Số liệu đổi
  60 lần/giây với font tỉ lệ sẽ nhảy bề rộng, khó liếc nhanh lúc cần xem fps
  có tụt không.
- Lucide (`lucide.ttf`, đúng bản `lucide-react`) — icon. Nút chỉ có icon
  **luôn kèm tooltip**.
- Thang cỡ chữ cố định: **9 / 10 / 11 / 12 / 14 / 18 px** — lấy đúng từ bản
  thiết kế, đừng thêm cỡ mới tuỳ tiện. Sự chênh lệch giữa các cỡ tạo thứ bậc
  thị giác (tiêu đề bảng / nhãn mục / giá trị).

## Thao tác chuẩn (đừng đổi mà không hỏi)

| Thao tác | Kết quả |
|---|---|
| Bấm ô clip | **Chọn** (không phát) — để chỉnh clip sắp dùng mà không làm gián đoạn clip đang chiếu |
| Bấm đúp ô clip | Phát clip |
| Bấm ô trống | Mở hộp thoại chọn file |
| Kéo điểm ở ĐƯỜNG RA | Keystone / mesh warp |
| Kéo khung ở VÙNG LẤY | Đổi phần canvas mà slice lấy |
| Tick "Chỉnh mặt nạ" | Chuyển sang kéo nút mặt nạ bezier (F12) |

## Sai phải thấy được

Mask/TriggerZone luôn ở contentUV/canvas, không ở pixel máy chiếu — chỉnh lại
keystone không phải vẽ lại. Khi một tham chiếu bị hỏng (ví dụ index layer bị
xoá), **lùi về trạng thái mặc định + cảnh báo màu vàng** thay vì màn đen im
lặng — xem cách `F22` xử lý layer bị xoá trong `newui/SKILL.md`.

## Trước khi tick một mục UI là "đã xong"

Grep source để xác nhận widget/hàm thật tồn tại và chạy được — repo đã từng
phải đính chính 6 mục tick sai trong `features.md`. Cập nhật
`UI_UPDATE_PROGRESS.md` khi xong **một mảng lớn**, không phải mỗi lần chỉnh
pixel nhỏ.

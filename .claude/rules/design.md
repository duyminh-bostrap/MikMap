# Quy ước giao diện (Dear ImGui + GLFW, `src/`)

> **Cập nhật sau khi đổi tên `newui/` → `src/`:** helper vẽ UI của app hiện
> tại nằm trong namespace `ui::` (`src/ui.h`/`ui.cpp`), **khác hoàn toàn**
> API `theme::*` (`beginPanel`, `panelHeader`, `fieldLabel`...) của bản UI cũ
> — bản đó đã lưu trữ ở nhánh git `legacy-oF-ui`, không còn trong working tree
> này. Đừng tìm/gọi `theme::` trong `src/` nữa.

Nguồn sự thật cho giao diện là bản thiết kế tham khảo `MikMap Workspace.dc.html`
của Claude Design (xem `src/CMakeLists.txt` comment) và `MikMap_Web`/`sampleUI/`
— đối chiếu bằng số đo DOM thật khi có bản web để so, đừng đoán từ mắt.

**Đã đối chiếu trực tiếp với file `MikMap Workspace.dc.html` thật (2026-09-22)**
cho riêng phần Deck (Composition), gồm cả phần JS state phía sau (`tlLayout`,
`syncTimeline`, `switchDeck`...), không chỉ nhìn layout HTML. Token màu
(`colors.css`) và spacing (`spacing.css`) đã khớp sẵn với `pal::`/kích thước
hiện có trước khi đối chiếu — chỉ 3 chỗ lệch thật: màu 3 trạng thái ô clip
(đã sửa), Multi-deck (đã thêm), Timeline run mode (đã thêm). Xem "Ghi chú
kiểm tra" cuối `features.md` để biết chi tiết.

## Bố cục

Hai trang cố định qua tab trên cùng — **không phải cửa sổ nổi**: trong phòng
tối giữa buổi diễn không ai có thời gian sắp lại bàn làm việc, và một bảng
trôi ra ngoài màn hình là chuyện xảy ra thật. Trước đây là ba trang
(Composition · Advanced Mapping · Sensor I/O); hai trang sau đã gộp thành
**Mapping** (`src/main.cpp`: mảng tab chỉ có Composition và Mapping):

```
Composition · Mapping    (+ cửa sổ Cài đặt riêng)
```

Trang Mapping chứa cả phần máy chiếu (screen, slice, mask) lẫn sensor: cây bên
trái có cả hai, chọn sensor trong cây thì tab INPUT/OUTPUT và panel Properties
chuyển sang sensor đó. Trang **Effect** là kế hoạch sau, chưa có trong `src/`.
Thêm sensor là popup trên mọi trang, không phải một trang riêng.

**Cài đặt** cho phép chọn cả ngôn ngữ lẫn **font giao diện** (xem dưới) — đây
là mô tả *máy/người*, không phải *một buổi diễn*, cùng lý do
`engine/i18n/Localization` tách khỏi model project.

## Helper vẽ dùng chung (`namespace ui::`, `src/ui.h`)

Luôn dùng lại, đừng viết widget riêng lẻ trùng chức năng — đọc `ui.h` trước
khi thêm helper mới, danh sách dưới đây chỉ là các nhóm chính đã xác nhận
trong source, không phải toàn bộ API:

- `ui::Text`/`TextR`/`TextC`/`TextEll` — vẽ chữ căn trái/phải/giữa/cắt bớt,
  nhận `FontId` + cỡ chữ + màu tường minh (không dựa vào `ImGui::PushFont`
  ngầm).
- `ui::Fill`/`Border`/`Box`/`Glow`/`HLine`/`VLine`/`GradDiag`/`RadialFan`/`Dot`/
  `DashedPoly` — vẽ hình học cơ bản qua `ImDrawList`, không qua widget ImGui
  chuẩn.
- `ui::Icon` — icon vẽ tay kiểu Lucide trên lưới 24 đơn vị (không nạp từ file
  `.ttf` icon).
- `ui::PanelHeader`, `ui::Badge`, `ui::PropertyRow` — mảnh UI ghép sẵn cho
  header panel, nhãn trạng thái, hàng thuộc tính.
- `ui::Hit` — struct kết quả tương tác chuột dùng chung (`hover/click/dbl/
  rclick/down/release`) thay vì gọi rời `ImGui::IsItemClicked()` nhiều chỗ.

`FrameRounding` dùng **3.f** ở các nơi đã kiểm (không phải 4px như bản cũ).
`FramePadding` **không cố định một giá trị cho toàn app** — mỗi nhóm widget tự
`PushStyleVar` theo ngữ cảnh (vd `(8, cao_động)` cho input, `(0,1)` cho ô nhỏ
trong deck) — kiểm trực tiếp trong `ui.cpp`/`deck.cpp` trước khi giả định một
con số chung.

## Font — người dùng chọn được, không cố định

**Khác bản cũ:** đây không phải một bộ font cố định (Inter+RobotoMono+Lucide)
mà là **lựa chọn trong Cài đặt** (`src/settings.cpp`):

| Vai trò | Các lựa chọn |
|---|---|
| Interface face | Archivo · Barlow Condensed · IBM Plex Sans (variable) · Space Grotesk (variable) |
| Technical face (số liệu) | JetBrains Mono · IBM Plex Mono · Roboto Mono (variable) |

Font nằm ở `src/assets/fonts/` (đóng gói cùng OFL.txt). Nguyên tắc **số liệu
dùng font mono riêng** vẫn giữ nguyên tinh thần từ bản cũ — lý do: chữ số của
font tỉ lệ có bề rộng khác nhau, giá trị đổi liên tục (FPS, toạ độ) sẽ nhảy
qua nhảy lại nếu dùng font tỉ lệ, khó liếc nhanh.

`FontId` trong `ui::` có các hằng `UI_R/UI_S/UI_B/UI_X` (interface, 4 độ đậm)
và `MONO_R/MONO_M/MONO_B` (mono, 3 độ đậm) — dùng đúng hằng này khi vẽ chữ
mới, đừng hard-code tên file font.

**Cỡ chữ = số px của bản thiết kế × `ui::kTextScale` (1.2, theo yêu cầu người
dùng 2026-09-24: chữ to hơn 20%).** Trong code vẫn ghi đúng số của bản thiết kế
(`Text(..., UI_B, 11, ...)`); `ui::Text`/`TextW` tự nhân. Đừng tự nhân thêm 1.2
ở chỗ gọi, và khi đối chiếu ảnh chụp với bản thiết kế thì chữ to hơn 20% là cố ý,
không phải lệch. Chỗ nào đưa cỡ chữ thẳng cho ImGui (`ImGui::PushFont`) phải bọc
`ui::TextPx(sz)`. Bố cục (khung, hàng, icon) KHÔNG nhân — ô chật phải dùng
`TextEll` để cắt "…" chứ đừng để chữ tràn.

## Thao tác chuẩn (đối chiếu lại nếu nghi ngờ — đây là kỳ vọng thiết kế, không phải đã verify từng dòng)

| Thao tác | Kết quả |
|---|---|
| Bấm **bar** ô clip (dải tên, phía trên) | **Chọn/cue** (không phát) — để chỉnh clip sắp dùng mà không làm gián đoạn clip đang chiếu |
| Kéo **bar** ô clip | Di chuyển clip sang ô khác |
| Chuột phải **bar** ô clip | Popover (Trigger/Cue/Loop/Rename/Clear/màu) |
| Bấm **body** ô clip (vùng gradient, phía dưới) | Phát ngay clip đó (cue + trigger) — không cần bấm đúp; không kéo-thả, không chuột phải ở vùng này |
| Bấm **header cột** | Chọn cột + bắn ngay mọi clip không rỗng trong cột (mỗi layer 1 clip) — không cần bấm đúp |
| Output > **Edit Points**: kéo ô vuông lớn / nhỏ | Góc phối cảnh (keystone) / điểm warp của lưới (cả 4 góc lưới) |
| Output > **Transform**: kéo hộp bao slice | Di chuyển / co giãn / xoay cả slice, warp giữ nguyên hình (Shift: khoá trục, giữ tỉ lệ, 45°) |
| Kéo khung ở VÙNG LẤY | Đổi phần canvas mà slice lấy |
| Cây Mapping: bấm tiêu đề **Slices (n)** / **Sensors (n)** dưới một Screen | Gập / mở phần đó (chỉ runtime, không lưu) |
| Cây Mapping: kéo dòng **slice** (rê > 4 px) sang dòng Screen / tiêu đề Slices / dòng trong phần Slices của screen KHÁC | Chuyển slice sang screen đó (giữ id, hình học, mask); screen nguồn còn 1 slice thì từ chối + toast. Thả lên slice CÙNG screen = đổi thứ tự z (chèn trước dòng đó). `Esc` huỷ, thả ra ngoài không làm gì |
| Cây Mapping: kéo dòng **sensor** sang dòng Screen / tiêu đề Sensors / dòng sensor của screen khác | Chuyển sensor (như menu "Move to screen"); lidar đã đặt chỗ tự tính lại, calib tay giữ nguyên + toast. Chỉ kéo một dòng dù đang chọn nhiều |
| Mapping > sensor đang chọn > **INPUT**: kéo cảm biến lidar trên sân khấu | Dời vị trí (làm tròn 10 mm; Shift: một trục) |
| Mapping > sensor đang chọn > **INPUT**: kéo chấm xoay | Xoay hướng lidar (Shift: bước 15°) |
| Mapping > sensor đang chọn > tab **OUTPUT**: Edit Points / Transform | Kéo góc quad nơi cảm biến đổ vào screen / dời-co-xoay cả quad |
| Bấm một dòng sensor trong cây Mapping | Chọn sensor (`SelectDevice`): sân khấu hiện hình máy chiếu bên dưới; sensor chọn lấy màu của trang (INPUT cyan, OUTPUT coral), sensor khác cùng screen xám và không bấm được |
| Bấm chip 4 chấm ở dòng sensor | Nhảy tới bước kế tiếp chưa xong (1 Connection · 2 Placement · 3 Calibration · 4 Send out); đủ 4 chấm thì chỉ chọn sensor. Chấm 4 = gói đã giao cho bộ gửi, KHÔNG chứng minh bên nhận đã nhận |
| Sensor Properties › Send out › **Origin** (Bottom-left / Top-left) | Gốc y của toạ độ gửi ra và ô Y (mm) ở Placement ("Y (mm, from bottom)" khi dưới-trái). Mặc định dưới-trái cho sensor mới; project cũ nạp thành trên-trái. Sân khấu INPUT không đổi |
| Bấm nút **Sensor** ở chân cây | Mở popup thêm sensor giữa cửa sổ, trên trang đang mở; trang phía sau và menu của nó không nhận chuột |
| **Esc** khi popup thêm sensor đang mở | Đóng popup (menu đang mở thì Esc đóng menu trước; không đóng khi đang gõ chữ) |

> **Đã đổi khỏi bản tham khảo (2026-09-22):** bản gốc dùng bấm-đơn=chọn/bấm-đúp=phát
> cho toàn bộ ô clip, và bấm-đơn=chọn/bấm-đúp=bắn cho header cột. Theo yêu cầu người
> dùng, cả hai đều bỏ double-click: ô clip tách hai vùng bấm độc lập (`ClipCell`,
> struct `CellOut`), header cột bấm đơn là bắn luôn (`ColumnHeader`) — tất cả trong
> `src/deck.cpp`.

Đây là nguyên tắc thiết kế chung kế thừa từ bản tham khảo — **grep
`src/deck.cpp`/`mapping.cpp` để xác nhận hành vi thật** trước khi khẳng
định với người dùng, vì theo `features.md` nhiều hành vi ở đây mới là `[~]`
(một phần), chưa chắc khớp 100% mô tả trên.

## Sai phải thấy được

Mask/TriggerZone luôn ở contentUV/canvas, không ở pixel máy chiếu — chỉnh lại
keystone không phải vẽ lại (nguyên tắc #8 trong `.claude/CLAUDE.md`, từ
`architecture.md` §10). Khi một tham chiếu bị hỏng, lùi về trạng thái mặc
định + cảnh báo thay vì im lặng.

## Trước khi tick một mục UI là "đã xong"

Grep `src/*.cpp` để xác nhận widget/hàm thật tồn tại và chạy được —
`features.md` chấm điểm nghiêm khắc, `[~]` nghĩa là còn giả một phần, không
phải "gần xong". Xem mục "Ghi chú kiểm tra" cuối `features.md` trước khi đổi
trạng thái một dòng.

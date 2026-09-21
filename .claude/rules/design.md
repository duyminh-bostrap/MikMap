# Quy ước giao diện (Dear ImGui + GLFW, `src/`)

> **Cập nhật sau khi đổi tên `newui/` → `src/`:** helper vẽ UI của app hiện
> tại nằm trong namespace `ui::` (`src/ui.h`/`ui.cpp`), **khác hoàn toàn**
> API `theme::*` (`beginPanel`, `panelHeader`, `fieldLabel`...) của bản UI cũ
> — bản đó đã lưu trữ ở nhánh git `legacy-oF-ui`, không còn trong working tree
> này. Đừng tìm/gọi `theme::` trong `src/` nữa.

Nguồn sự thật cho giao diện là bản thiết kế tham khảo `MikMap Workspace.dc.html`
của Claude Design (xem `src/CMakeLists.txt` comment) và `MikMap_Web`/`sampleUI/`
— đối chiếu bằng số đo DOM thật khi có bản web để so, đừng đoán từ mắt.

## Bố cục

Ba trang cố định qua tab trên cùng — **không phải cửa sổ nổi**: trong phòng
tối giữa buổi diễn không ai có thời gian sắp lại bàn làm việc, và một bảng
trôi ra ngoài màn hình là chuyện xảy ra thật. `src/README` cũ (nay gộp vào
`README.md` gốc) xác nhận đúng 3 màn này đã có trong `src/`:

```
Composition · Advanced Mapping · Sensor I/O    (+ cửa sổ Cài đặt riêng)
```

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

## Thao tác chuẩn (đối chiếu lại nếu nghi ngờ — đây là kỳ vọng thiết kế, không phải đã verify từng dòng)

| Thao tác | Kết quả |
|---|---|
| Bấm ô clip | **Chọn** (không phát) — để chỉnh clip sắp dùng mà không làm gián đoạn clip đang chiếu |
| Bấm đúp ô clip | Phát clip |
| Kéo điểm ở ĐƯỜNG RA | Keystone / mesh warp |
| Kéo khung ở VÙNG LẤY | Đổi phần canvas mà slice lấy |

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

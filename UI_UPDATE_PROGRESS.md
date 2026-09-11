# Tiến độ cập nhật UI theo tham khảo MikMap_Web

Theo dõi việc đưa giao diện ImGui thật (`src/ui/`) khớp với bản thiết kế
tham khảo `D:\2026\Mike\MikMap_Web` (React/Vite/Tailwind). Cập nhật file
này mỗi khi có một mảng lớn hoàn tất hoặc đổi hướng — không phải mỗi
commit.

## Đã xong

### Thanh điều hướng trên cùng (topbar)
- [x] Khối thương hiệu MIKMAP: icon 24×24 gradient cam→vàng có quầng
      sáng mềm, chữ 14px (`text-sm`) kèm mũi tên xuống, dòng phụ tên dự
      án + chấm trạng thái, bọc trong khung viền bo tròn.
- [x] Thanh công cụ nhanh (Show Mode / Test Grid / Auto Calib /
      Fullscreen), khối lưu/mở dự án, công tắc ngôn ngữ.
- [x] Thẻ số liệu FPS / LATENCY / độ phân giải gộp vào MỘT khung có
      gạch ngăn, thay vì ba mảnh rời rạc.
- [x] Lề trái/phải thanh top bar chỉnh về 16px (đo từ DOM thật, không
      đoán từ lớp Tailwind).

### Trang Advanced Mapping (Ánh xạ chi tiết)
- [x] Bố cục 3 cột: cây SCREEN SETUP (240px) / vùng làm việc / SLICE
      SETTINGS (280px) — đúng độ rộng đo từ bản thiết kế.
- [x] Thanh công cụ mapping xếp lại đúng thứ tự tham khảo: công tắc
      Input/Output trước tiên, dải chọn màn chiếu, dòng breadcrumb, cụm
      công cụ bên phải, Reset Warp ngoài cùng.
- [x] Nút đóng/mở cây màn hình chuyển ra ngoài cùng bên trái thanh công
      cụ (theo yêu cầu 2026-09-11).
- [x] Bỏ "Test Card" và "Apply" khỏi thanh công cụ mapping — trùng chức
      năng với topbar, làm cụm nút bên phải tràn cột.
- [x] Tiêu đề 3 phần (MAPPING TREE / thanh công cụ / SLICE PROPERTIES)
      nằm cùng một hàng, không phải thanh riêng nằm trên 3 cột.
- [x] Hàng trong cây SCREEN SETUP: thẻ bo góc có viền theo từng cấp
      (34/29/23px, chữ 12/11/10px), nhãn độ phân giải căn phải ở hàng
      screen, nút mắt bật/tắt không còn bị chặn hover bởi hàng bên dưới
      (`ImGui::SetNextItemAllowOverlap` trong `theme::treeRow`).
- [x] SLICE SETTINGS: bỏ danh sách slice trùng lặp, đổi "Slice đang
      chọn: X" thành ô đổi tên trực tiếp, loại warp chuyển từ hộp xổ
      sang bộ chọn phân đoạn 3 ô luôn hiện.
- [x] Corner-pin: tay cầm to hơn (r=11), có nhãn toạ độ TL/TR/BR/BL.
- [x] Chức năng "Thêm màn hình" (trước đây thiếu hoàn toàn trong app
      thật) — `UiActions::addScreen`.

### Trang Composition
- [x] Mọi bảng con (LIBRARY / hai màn hình PREVIEW+ACTIVE / INSPECTOR /
      dải LAYER×COLUMN) chuyển sang `theme::beginPanel/endPanel` — sửa
      lỗi lề trong bị ImGui bỏ qua vì thiếu cờ
      `ImGuiChildFlags_AlwaysUseWindowPadding`.
- [x] Sửa nhãn "PREVIEW"/"ACTIVE COMPOSITION" bị khung hình đen của
      canvas vẽ đè lên (do canh theo FramePadding sai ngữ cảnh).

### Hạ tầng dùng chung (Theme.h / Theme.cpp)
- [x] Bảng màu phụ đo từ DOM thật: `BgHeader/BgDock/BgInput/BgButton/
      BorderSoft/BorderField`, `TextMuted`.
- [x] `theme::panelHeader` vẽ đúng dải 40px nền `#181818`, chữ 10px in
      hoa giãn 1px (`theme::textTracked`/`trackedWidth` — ImGui không có
      letter-spacing, phải tự vẽ từng ký tự UTF-8 rồi cộng bước nhảy).
- [x] `theme::fieldLabel` (nhãn ô nhập 10px đậm #888) và
      `theme::sectionDivider` (vạch ngăn mục 1px #222) dùng lại được ở
      mọi bảng.
- [x] `theme::segButton` — bộ chọn phân đoạn (khác `tabButton` ở chỗ ô
      KHÔNG chọn vẫn thấy khung).
- [x] `theme::tabButton` khi active nhuộm theo màu nhấn (accent/15 nền +
      accent/40 viền) thay vì nền xám trung tính.
- [x] `FramePadding` tăng lên (10,9) khớp ô nhập cao 30px của bản thiết
      kế; `FrameRounding` 4px.

### Nguồn hình sinh bằng shader (bonus, không có trong yêu cầu gốc — user
xin thêm "copy particle mẫu thành GLSL")
- [x] `render/GeneratorBank` + 3 shader GLSL chuyển thẳng công thức từ
      `LiveCanvas.tsx` của bản thiết kế: Plasma Waves, Geometric
      Wireframe, Particle Vortex.
- [x] `MediaType::Generator` chạy qua `MediaCache` như Video/Image; danh
      mục nằm ở `core/model/Generators.h` để `ui/` đọc được mà không vi
      phạm quy tắc `ui/` không phụ thuộc `render/`.
- [x] Xuất hiện trong trình duyệt media (Composition) ở trên cùng danh
      sách file, gán được vào ô clip bằng một cú bấm — không cần file.
- [x] Test thủ công: cả 3 generator chạy đúng, hiển thị trong preview
      lẫn ACTIVE COMPOSITION.

## Đang làm / kiểm tra lại

- [ ] Có một vệt nhiễu (static/noise) xuất hiện ở vùng vàng của khung
      xem trước ACTIVE COMPOSITION (trang Composition) sau khi gán
      nhiều generator liên tiếp vào Layer 3 — CHƯA rõ là artefact chụp
      màn hình lúc chuyển clip hay lỗi thật. Vùng đó là pattern
      "Test Card" có sẵn từ trước (đã thấy y hệt, không nhiễu, ở trang
      Advanced Mapping) — cần xem lại bằng mắt thật trong app, không
      chỉ qua ảnh chụp tự động.

## Chưa làm

- [ ] Trang SENSOR I/O (`drawSensorView`, dòng ~2224 `ControlPanel.cpp`)
      — CHƯA đối chiếu với `SensorIOView.tsx` của bản tham khảo. Đây là
      trang duy nhất trong 3 trang chính chưa được rà theo DOM thật của
      MikMap_Web.
- [ ] Modal Settings/Help/About và menu dự án đầy đủ (New/Open/Recent/
      Save/Export) thấy trong `Header.tsx` bản cập nhật — ĐÃ CỐ Ý bỏ
      ngoài phạm vi các yêu cầu trước đây vì vượt quá việc "sửa kích
      thước/độ dày/glow" được hỏi. Cân nhắc làm nếu người dùng xác nhận
      muốn cả hệ thống menu này.
- [ ] Panel LIBRARY (trình duyệt media) của Composition chưa đối chiếu
      pixel-by-pixel với bản tham khảo (chỉ mới sửa lề trong + thêm mục
      generator) — hàng media, badge định dạng, icon còn theo bản cũ.
- [ ] Panel INSPECTOR (tab CLIP/LAYER) của Composition chưa đối chiếu —
      chỉ mới sửa lề trong.
- [ ] Dải LAYER×COLUMN (deck) — mới sửa lề trái, thumbnail/blend-dropdown
      từng ô chưa đối chiếu kích thước với bản tham khảo.

## Cách kiểm tra nhanh

```powershell
# Build
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" HexMapping.vcxproj /p:Configuration=Debug /p:Platform=x64 /m /nologo /v:minimal

# Test
cmake --build build --target hexmap_tests --config Debug
.\build\Debug\hexmap_tests.exe

# Xem giao diện tham khảo trực tiếp (đã cài sẵn deps)
cd D:\2026\Mike\MikMap_Web && npm run dev   # http://localhost:3000
```

So khớp bằng số đo DOM thật (đáng tin hơn đọc lớp Tailwind — lớp có thể
ghi đè bằng biến CSS, DOM thì không nói dối):

```js
// chạy trong console tại localhost:3000
el.getBoundingClientRect()   // vị trí + kích thước px thật
getComputedStyle(el)         // font-size, padding, border, color thật
```

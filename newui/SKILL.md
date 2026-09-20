---
name: mikmap-mapping
description: Tổng hợp app projection mapping MikMap (mã nguồn nội bộ HexMapping, C++20 + openFrameworks + ImGui) — kiến trúc, tính năng đã làm, quy ước UI, cách build/test và các bẫy đã gặp. Dùng khi làm việc trong repo này (sửa mapping/warp/slice, sensor, giao diện ImGui, build MSBuild/CMake) hoặc cần biết một tính năng đã có chưa.
---

# MikMap / HexMapping — tổng hợp công việc

Projection mapping engine kiểu Resolume + chuỗi calibration sensor: **chạm vào vật thể thật, hiệu ứng nổ đúng chỗ đó**.
Repo tên `MikMap`, code tên `HexMapping` (namespace `hexmap`, `HexMapping.sln`, `bin/HexMapping.exe`) — cùng một thứ.
Stack: C++20 · openFrameworks 0.12.x · OpenGL · Dear ImGui · Windows/MSVC 2022. Tài liệu gốc: `README.md`, `features.md` (backlog 135 mục + nhật ký), `architecture.md`, `UI_UPDATE_PROGRESS.md`.

## Mô hình dữ liệu
Deck · Layer · Column · Clip → **Composition** (canvas ảo) → **Screen** (1 máy chiếu/output) chứa nhiều **Slice** (vùng lấy `inputOrigin/inputSize` + warp + mask + màu) → máy chiếu.
Công thức sensor: `p_content = H_w⁻¹ · H_s · p_sensor` (`H_s` calibration sensor, `H_w` warp của slice — hai ma trận tách rời).

## Kiến trúc (quy tắc phụ thuộc bất khả xâm phạm)
```
core   -> không ai          (C++20 thuần: math, model, calib, filter, util; KHÔNG GL/oF/ImGui)
io     -> core              (thread sensor; không biết Slice/Layer)
render -> core              (GL + oF; chỉ đọc model)
ui     -> core              (ImGui; đọc model + phát lệnh; KHÔNG phụ thuộc render/)
app    -> tất cả            (AppController: nơi duy nhất 4 tầng gặp nhau)
```
Kiểm tự động: `tools/check_layering.ps1`. Vì vậy danh mục shader generator nằm ở `core/model/Generators.h` để `ui/` đọc được.
Render thread: không lock/alloc/I/O. Video **phải là HAP** (`ffmpeg -c:v hap -format hap_q -chunks 8`), không H.264.

## Tính năng đã xong (theo nhóm)
**Nền core:** Vec2/Mat3/LinearSolver, Homography (DLT+Hartley+RANSAC), BilinearInverse, BlendMode (11 mode), Transform2D, Transport, Clip/Layer/Deck/Composition, Json tự viết, ProjectIO `.hexmap` (ghi qua file tạm; nạp file hỏng không sập).
**Warp/Slice (nhóm F):** F1–F8 (screen, output display, slice, input rect, corner pin, kéo chuột, đa slice, preset), F9/F11 mesh N×M, **F10 Bezier 4×4** (nghịch đảo = lưới 8×8 + Newton; round-trip 3721/3721), F12 mặt nạ bezier (contentUV, mép mờ, đảo), F13–F17, F19 màu riêng từng slice, F20 soft-edge (gamma+luminance), **F21 snapping** (ngưỡng theo pixel màn hình), **F22 slice lấy từ Layer riêng** (`[~]`: nửa Group chờ A13; chỉ nướng FBO cho layer có slice dùng; index layer đã xoá lùi về Composition kèm cảnh báo vàng).
**Chưa làm:** F18 polygon slice, F23 Spout/NDI, F24 Art-Net/sACN, F25 SDI, F22-Group.
**Sensor (nhóm G):** SPSC ring/TripleBuffer wait-free, MockSource, OSC, **G3 Serial Arduino**, **G14 TUIO**, G11/G12 OneEuro + ID bền, G16 ghi/phát lại log, G17 vùng cảm ứng (TriggerZone), **G18 mỗi cảm biến một profile hiệu chỉnh riêng**, SensorMapper (sensor → slice → contentUV → canvas), `Screen::hitTest` bỏ qua vùng bị mask.
**Kết quả gần nhất:** 313→345+ unit test xanh (CMake, không cần GPU/oF), 60 fps, p99 ~17 ms, HAP 4K 1–5 luồng đủ tốc độ.
**App:** Cài đặt + đa ngôn ngữ Việt/Anh (`Localization.cpp`, tra theo khoá; test chặn thiếu khoá/lệch `%d`/`%s`); `settings.json` cố ý KHÔNG vào git (mô tả máy/người, không phải buổi diễn); `Thêm màn hình` (`UiActions::addScreen`).

## Giao diện (bám bản thiết kế `MikMap_Web`, `sampleUI/`)
Bố cục **cố định**, 3 trang qua tab: COMPOSITION · ADVANCED MAPPING · SENSOR I/O (không cửa sổ nổi; riêng Cài đặt là popup). Nút `Dự án` = Lưu/Mở/Mới.
- Topbar: khối MIKMAP (icon gradient cam→vàng có glow), quick toolbar (Show Mode/Test Grid/Auto Calib/Fullscreen), thẻ FPS/LATENCY/độ phân giải trong một khung.
- Advanced Mapping: 3 cột — cây SCREEN SETUP 240px / vùng làm việc / SLICE SETTINGS 280px; toolbar theo đúng thứ tự thiết kế (Input/Output, dải chọn màn, breadcrumb, cụm công cụ, Reset Warp); nút mắt bật/tắt Screen+Slice; corner-pin r=11 có nhãn toạ độ; ô đổi tên slice trực tiếp; loại warp là segmented 3 ô (ẩn với warp không cần phân đoạn).
- Composition: mọi panel dùng `theme::beginPanel/endPanel`; LIBRARY liệt kê `bin/data/media` (file không phải `.mov` hiện vàng); clip: bấm = chọn, bấm đúp = phát, ô màu (thumbnail) chỉ phát, ô đen (nhãn) mới kéo-thả/menu.
- Theme helper (`Theme.h/.cpp`): `panelHeader` (40px #181818, chữ 10px hoa giãn 1px qua `textTracked`), `fieldLabel`, `sectionDivider`, `segButton`, `tabButton` (active nhuộm accent), `opacityBar` (vẽ bằng ImDrawList, nhãn dòng riêng + núm nổi), `treeRow`. FramePadding (10,9), FrameRounding 4.
- Font: Inter 400/600/700 (cắt từ variable font), RobotoMono cho số liệu, Lucide icon; thang 9/10/11/12/14/18px. Nút chỉ icon luôn có tooltip.
- **Nguồn hình sinh bằng shader** `render/GeneratorBank` + `MediaType::Generator` (Plasma Waves, Geometric Wireframe, Particle Vortex), gán vào ô clip không cần file.
- Còn lại: trang SENSOR I/O chưa đối chiếu `SensorIOView.tsx`; modal Settings/Help/About + menu dự án đầy đủ; LIBRARY/INSPECTOR/deck strip chưa so pixel. Cập nhật `UI_UPDATE_PROGRESS.md` khi xong một mảng lớn.
- Đối chiếu bằng số đo DOM thật (`getBoundingClientRect`/`getComputedStyle` tại `localhost:3000` của MikMap_Web), không đoán từ lớp Tailwind.

## Build & test
```powershell
# Unit test (CMake thuần, không cần oF)
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --target hexmap_tests --config Debug
.\build\Debug\hexmap_tests.exe
# App chính
& "C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" HexMapping.sln /p:Configuration=Release /p:Platform=x64 /m
.\bin\HexMapping.exe [--demo]
```
- openFrameworks + `tools/` phải là thư mục **anh em** của repo (`../openFrameworks`, `../tools/ffmpeg`).
- Sau **mỗi lần** chạy oF Project Generator phải chạy lại `tools/fix_project.ps1` (bỏ 2 stub, thêm `src` vào include path). File `.ps1` viết không dấu vì PS 5.1 đọc ANSI.
- File `.cpp/.h` mới phải thêm vào `HexMapping.vcxproj` (MSBuild) — quên thì link lỗi.
- Phím tắt: Space Show Mode · G test grid · M/O mock/OSC · A auto-calibrate · C dấu thập · P chấm sensor · F11 fullscreen.

## Bẫy đã mắc (đừng lặp lại)
1. `near` (và `far`) là macro `windows.h` — đừng đặt tên biến/lambda vậy; lỗi báo rất khó hiểu.
2. ImGui **nới rộng** vùng nội dung child khi một dòng quá rộng → widget `-FLT_MIN` rộng theo → cả bảng bị cắt. `GetContentRegionAvail()` trong ô table không phải bề rộng ô.
3. `SameLine()` không căn giữa theo chiều dọc khi trộn cỡ chữ → mỗi phần tử tự đặt `CursorPosY` quanh điểm giữa cố định.
4. Child panel cần `ImGuiChildFlags_AlwaysUseWindowPadding` nếu không lề trong bị bỏ qua; muốn nút mắt trên hàng cây nhận hover cần `ImGui::SetNextItemAllowOverlap`.
5. Nướng texture mask **trước** `m_sliceShader.begin()` — sau đó `ofPath::draw()` vẽ bằng slice shader → mask đen, cả màn đen.
6. `Slice` copy phải sao chép đủ trường: dùng `WarpPtr` (tự `clone()`) + `= default`, đừng viết copy ctor tay (mask từng lặng lẽ biến mất).
7. HAP Q là YCoCg — phải bind `ofxHapPlayer::getShader()` nếu khác null, nếu không màu sai.
8. `pointInQuad` cần ngưỡng theo tỉ lệ diện tích, không dung sai 0 → tránh "vệt chạm chết" chéo ô lưới.
9. Test phải so đúng đại lượng (từng điểm trên đường cong, không so diện tích đa giác sau `flatten()`).
10. Vệt nhiễu ở khung ACTIVE COMPOSITION khi chụp màn hình lúc chuyển clip là artefact chụp ảnh, không phải lỗi thật.
11. Đừng tick `[x]` cho mục chưa có code — từng phải đính chính 6 mục F sai; kiểm bằng grep source trước khi tick.

## Nguyên tắc thiết kế cần giữ
- Mask/TriggerZone ở **contentUV/canvas**, không ở pixel máy chiếu → chỉnh lại keystone không phải vẽ lại.
- Sai phải **thấy được** (lùi về composition + cảnh báo) hơn là màn đen im lặng.
- Bấm ô clip chỉ chọn (không phát) để không làm gián đoạn buổi diễn; Lưu/Mở project luôn phải có lối vào.
- Mọi tính năng mới: thêm test trong `tests/`, giữ 0 cảnh báo `/W4`, kiểm fps không tụt.

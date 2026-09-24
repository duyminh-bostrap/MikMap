# Quy ước kỹ thuật mặc định

> **Cập nhật cấu trúc:** `core/`/`io/` nay ở `engine/core`,`engine/io` (gốc
> repo, dùng chung). App hiện tại là `src/` (đổi tên từ `newui/`, GLFW+ImGui).
> Bản oF+MSBuild cũ đã lưu trữ ở nhánh git `legacy-oF-ui` — mục nào dưới đây
> chỉ áp dụng cho bản đó được ghi rõ.

## Ngôn ngữ & cảnh báo

- C++20, không dùng extension ngoài chuẩn (`CMAKE_CXX_EXTENSIONS OFF`).
- MSVC: `/W4 /permissive- /fp:precise /utf-8`. **`/fp:precise` là bắt buộc**
  cho `engine/core/math` — toán homography/calibration cần độ chính xác,
  không được đổi sang `/fp:fast`.
- Không-MSVC (Linux/macOS, dùng để build `engine/`+test nhanh):
  `-Wall -Wextra -Wpedantic`. Mục tiêu: **0 cảnh báo** trên cả hai bộ cờ.
- `near`/`far` là macro của `windows.h` — không đặt tên biến/lambda trùng,
  lỗi báo ra sẽ rất khó hiểu (đã từng mắc, áp dụng cho code Windows nói chung).

## Build

Hai hệ build tách biệt, đọc chung `engine/`, đừng trộn:

```bash
# engine/ (core+io+i18n) + unit test — CMake thuần, không cần GPU/GLFW
cmake -S . -B build
cmake --build build --target mikmap_tests -j
ctest --test-dir build --output-on-failure
```

```powershell
# app (src/) — Windows: MinGW/GLFW/.tools trong src/.tools/ (không vào git)
cmake -S src -B src/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build src/build
```

```bash
# app (src/) — Linux: apt install libglfw3-dev libgl-dev trước
# app (src/) — macOS: brew install glfw trước
# ImGui v1.92.9b tự tải qua FetchContent lúc configure, không vào git.
cmake -S src -B src/build && cmake --build src/build
```

- `src/CMakeLists.txt` tự `file(GLOB_RECURSE ...)` `../engine/core` +
  `../engine/io` — KHÔNG cần thêm file mới vào đâu thủ công (khác bản cũ với
  `.vcxproj`). Tắt bằng `-DMIKMAP_WITH_ENGINE=OFF` nếu chỉ muốn dựng riêng
  phần giao diện.
- Đã build+chạy thật kiểm chứng trên Linux (build sạch, chạy dưới Xvfb, giao
  diện render đúng) — không chỉ suy đoán từ việc compile được. macOS
  (Apple Silicon, SDK 15.0, `brew install glfw`) cũng đã build sạch và mở
  được cửa sổ, chạy ổn định. Khác biệt riêng của macOS trong code: `main.cpp`
  xin context GL 3.2 core + forward-compat và GLSL `#version 150` (context
  3.0 thường bị từ chối, app thoát mã 2); `clipart.cpp` tự định nghĩa
  `APIENTRY` rỗng. Output máy chiếu (F2/I1) đã mở và vẽ đúng trên macOS (chụp bằng
  `--outshot`); chưa thử nhiều màn hình thật. Kiểm tự động lưu/mở/undo:
  `mikmap --roundtrip <file>` (không cần cửa sổ). Debug bằng F5 trong VS Code
  (`.vscode/launch.json`, build vào `src/build-debug/`, đã gitignore).
- Chuỗi hex trong literal (`"\xE1\xBB\x8B"`) mà đứng ngay trước ký tự hex
  (`0-9a-fA-F`) sẽ bị GCC/Clang coi là một escape dài và báo lỗi (MSVC bỏ
  qua) — tách bằng `" "` như `"...\x8B" "ch"`.
- *(Chỉ nhánh `legacy-oF-ui`)* App oF cũ build bằng MSBuild + `MikMap.vcxproj`,
  file mới phải thêm thủ công vào đó, và phải chạy lại `tools/fix_project.ps1`
  sau mỗi lần chạy oF Project Generator. Không áp dụng cho `new_UI`.

## Test

- Test framework tự viết (`tests/TestHarness.h`), 0 phụ thuộc ngoài.
- Mọi tính năng mới trong `engine/core`/`engine/io` phải có test trong `tests/`.
- So sánh phải đúng đại lượng — ví dụ so từng điểm trên đường cong bezier,
  không so diện tích đa giác sau khi `flatten()` (sai số tích luỹ che mất
  lỗi thật).
- `pointInQuad` và các test hình học cần ngưỡng theo tỉ lệ diện tích, không
  dùng dung sai tuyệt đối 0 — tránh "vệt chạm chết" chéo ô lưới.

## Threading & hiệu năng

- Render thread **không lock, không alloc, không I/O**. Mọi cấp phát động
  phải nằm ngoài đường vẽ mỗi frame.
- Mỗi sensor (Serial/OSC/TUIO/Mock/Kinect) chạy thread riêng, **được phép
  chặn** ở `read()`/`recvfrom()`/SDK — nhưng không bao giờ chia sẻ dữ liệu với
  render thread bằng mutex, chỉ qua:
  - `TripleBuffer<SensorFrame>` — kênh **state**, cho phép rơi frame cũ (vd vị
    trí ngón tay đang di chuyển; render chỉ cần frame mới nhất).
  - `SpscRingBuffer<TouchEvent>` — kênh **event**, KHÔNG được rơi (vd
    TOUCH_DOWN/UP; rơi 1 sự kiện = hiệu ứng không kích hoạt hoặc kẹt vĩnh viễn).
- `SensorFrame`/`TouchPoint` (`engine/io/SensorFrame.h`) là POD kích thước cố
  định, memcpy-able — không `std::vector`, không con trỏ, để không cấp phát
  heap trong hot path.
- **Toạ độ thô đi qua biên thread; ngữ nghĩa (`H_w⁻¹·H_s`) áp dụng tại nơi
  dùng (render thread)** — quyết định cố ý, không phải thiếu sót: `H_w` đổi
  bất cứ lúc nào người dùng kéo góc, chia sẻ nó với sensor thread sẽ bắt buộc
  phải lock và phá vỡ ràng buộc render-không-lock ở trên.
- Độ trễ đo bằng `tCaptureNs` (đóng dấu lúc thu thập, `steady_clock`), không
  đoán. Mục tiêu: 60 fps ổn định, frame p99 < ~17ms, độ trễ sensor p99 < ~10ms.
- **Lưu ý về `src/` hiện tại:** đây là mục tiêu kiến trúc của `engine/io`;
  `src/sensor.cpp` hiện là radar mô phỏng, chưa chắc đã theo đúng mô hình
  thread này — kiểm trước khi giả định.

## Hợp đồng `IWarp` (`engine/core/model/IWarp.h`)

```cpp
virtual Vec2 forward(const Vec2& contentUV) const = 0;
virtual bool inverse(const Vec2& outputPx, Vec2& outUV) const = 0;  // bool, KHÔNG phải Vec2
```

`inverse()` phải trả `bool` + tham số ra vì phép nghịch đảo có thể thất bại
hợp lệ (điểm ngoài vùng warp, ô lưới suy biến) — trả thẳng `Vec2` buộc phải
bịa giá trị (`{0,0}` lại là toạ độ hợp lệ ở góc trên-trái), khiến người gọi
không phân biệt được "chạm góc" với "trượt ra ngoài". Mọi warp mới (kể cả
Bezier) phải theo đúng chữ ký này ngay từ `IWarp`, không thêm muộn.

## `engine/core/` chỉ phụ thuộc STL

Nếu một tính năng trong `engine/core/` (vd RANSAC) muốn dùng OpenCV, đặt sau
macro `MIKMAP_USE_OPENCV` kèm fallback tự viết — để `mikmap_tests` vẫn build
được ở môi trường tối giản (CI, máy không có OpenCV). Đừng thêm dependency
ngoài STL vào `engine/core/` mà không có fallback này.

## Video

**Phải là HAP** khi phát video thật, không phải H.264/MP4 — nguyên nhân số
một khiến fps tụt từ 60 xuống ~12 (xem `architecture.md` §4, băng thông đo
thật). `src/` hiện tại **chưa phát video thật** (`B1` trong `features.md` là
`[ ]`) — mục này là ràng buộc cho khi nào `render/`+HAP được ghép vào, không
phải hành vi hiện có để test ngay bây giờ.

*(Chỉ nhánh `legacy-oF-ui`)* Encode: `ffmpeg -c:v hap -format hap_q -chunks 8`;
HAP Q là YCoCg, phải bind `ofxHapPlayer::getShader()` nếu khác null nếu không
màu ra sai — chi tiết ofxHapPlayer chỉ tồn tại ở bản đó.

## Serialize / IO

- `Json` tự viết (0 phụ thuộc), escape `\` cho đường dẫn Windows —
  `engine/core/util/Json.*`.
- `ProjectIO` (`.mikmap`, `engine/core/model/ProjectIO.*`) ghi qua file tạm
  rồi rename — nạp file hỏng không được làm app sập. `src/` hiện tự viết lưu/mở
  riêng trong `settings.cpp`/`app.h`, **chưa gọi** `ProjectIO` thật (I2/F8/G8
  trong `features.md`).
- Nguyên tắc chung (áp dụng cho mọi UI): file mô tả *máy/người* (ngôn ngữ, màn
  hình output, cache) tách hoàn toàn khỏi file mô tả *một buổi diễn* — không
  bao giờ gộp hai khái niệm này lại, kể cả khi "tiện".

## Đa ngôn ngữ

`engine/i18n/Localization.{h,cpp}` — thêm ngôn ngữ mới: chép một bảng, dịch
giá trị, thêm vào `enum class Language`. `tests/test_localization.cpp` báo
ngay nếu thiếu khoá hoặc lệch định dạng `%d`/`%s` giữa các bảng — chạy lại
test này sau khi sửa bất kỳ bảng ngôn ngữ nào. Target build là `mikmap_i18n`
trong `CMakeLists.txt` gốc, tách khỏi mọi UI cụ thể (chỉ std + `mikmap_core`).

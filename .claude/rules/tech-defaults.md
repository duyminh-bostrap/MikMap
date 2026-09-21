# Quy ước kỹ thuật mặc định

## Ngôn ngữ & cảnh báo

- C++20, không dùng extension ngoài chuẩn (`CMAKE_CXX_EXTENSIONS OFF`).
- MSVC: `/W4 /permissive- /fp:precise /utf-8`. **`/fp:precise` là bắt buộc**
  cho `core/math` — toán homography/calibration cần độ chính xác, không được
  đổi sang `/fp:fast`.
- Không-MSVC (Linux/macOS, dùng để build `core/`+`io/`+test nhanh):
  `-Wall -Wextra -Wpedantic`. Mục tiêu: **0 cảnh báo** trên cả hai bộ cờ.
- `near`/`far` là macro của `windows.h` — không đặt tên biến/lambda trùng,
  lỗi báo ra sẽ rất khó hiểu (đã từng mắc).

## Build

Hai hệ build tách biệt, đừng trộn:

```bash
# core/ + io/ + unit test — CMake thuần, không cần openFrameworks/GPU
cmake -S . -B build
cmake --build build --target hexmap_tests -j
ctest --test-dir build --output-on-failure
```

```powershell
# App chính — chỉ Windows/MSVC, cần openFrameworks là thư mục anh em (../openFrameworks)
& "C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe" `
  HexMapping.sln /p:Configuration=Release /p:Platform=x64 /m
```

- File `.cpp/.h` mới trong `src/` phải được thêm vào `HexMapping.vcxproj`
  (MSBuild không tự glob) — quên bước này thì lỗi link, không phải lỗi biên
  dịch, nên dễ đoán sai nguyên nhân.
- Sau **mỗi lần** chạy oF Project Generator, chạy lại `tools/fix_project.ps1`
  (PG chèn 2 stub không tồn tại và bỏ sót `src` khỏi include path).
- `newui/` có `CMakeLists.txt` riêng, tự gom `../src/core` + `../src/io`, tắt
  bằng `-DMIKMAP_WITH_ENGINE=OFF` nếu chỉ muốn dựng phần giao diện.

## Test

- Test framework tự viết (`tests/TestHarness.h`), 0 phụ thuộc ngoài.
- Mọi tính năng mới trong `core/`/`io/` phải có test trong `tests/`.
- So sánh phải đúng đại lượng — ví dụ so từng điểm trên đường cong bezier,
  không so diện tích đa giác sau khi `flatten()` (sai số tích luỹ che mất
  lỗi thật).
- `pointInQuad` và các test hình học cần ngưỡng theo tỉ lệ diện tích, không
  dùng dung sai tuyệt đối 0 — tránh "vệt chạm chết" chéo ô lưới.

## Threading & hiệu năng

- Render thread (`render/`, vòng lặp `ofApp::draw`) **không lock, không
  alloc, không I/O**. Mọi cấp phát động phải nằm ngoài đường vẽ mỗi frame.
- Mỗi sensor (Serial/OSC/TUIO/Mock/Kinect) chạy thread riêng, giao tiếp qua
  buffer wait-free (SPSC ring / TripleBuffer) — không mutex trên đường nóng.
- Mục tiêu: 60 fps ổn định, frame p99 < ~17ms, độ trễ sensor p99 < ~10ms.

## Video

**Phải là HAP**, không phải H.264/MP4 — nguyên nhân số một khiến fps tụt từ
60 xuống ~12:

```powershell
../tools/ffmpeg/bin/ffmpeg.exe -i input.mp4 -c:v hap -format hap_q -chunks 8 output.mov
```

HAP Q là YCoCg — phải bind `ofxHapPlayer::getShader()` nếu khác null, nếu
không màu ra sai (đã từng mắc).

## Serialize / IO

- `Json` tự viết (0 phụ thuộc), escape `\` cho đường dẫn Windows.
- `ProjectIO` (`.hexmap`) ghi qua file tạm rồi rename — nạp file hỏng không
  được làm app sập.
- File mô tả *máy/người* (`bin/data/settings.json`) tách hoàn toàn khỏi file
  project (`.hexmap`) mô tả *một buổi diễn* — không bao giờ gộp hai khái
  niệm này lại, kể cả khi "tiện".

## Đa ngôn ngữ

Thêm ngôn ngữ mới: chép một bảng trong `src/ui/Localization.cpp`, dịch giá
trị, thêm vào `enum class Language`. `tests/test_localization.cpp` báo ngay
nếu thiếu khoá hoặc lệch định dạng `%d`/`%s` giữa các bảng — chạy lại test
này sau khi sửa bất kỳ bảng ngôn ngữ nào.

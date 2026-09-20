# newui — giao diện mới (Dear ImGui + GLFW)

Bản dựng lại giao diện theo `MikMap Workspace.dc.html` của Claude Design, **dùng chung engine
với repo** thay vì viết lại: `CMakeLists.txt` ở đây tự gom `../src/core/**` và `../src/io/**`
(đều là C++20 thuần, không dính openFrameworks) vào cùng một target.

Nhờ vậy nhánh này **không sửa một file nào đang có** — không đụng `CMakeLists.txt` gốc,
`HexMapping.vcxproj` hay `src/ui/` — nên không thể conflict với `main`.

## Build

```powershell
cmake -S newui -B newui/build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build newui/build
.\newui\build\mikmap.exe
```

Bộ công cụ (MinGW GCC, CMake, Ninja, Dear ImGui, GLFW) nằm trong `newui/.tools/`, không vào git.
Xem `newui/build.ps1` để build nhanh.

Tắt engine để dựng riêng phần giao diện: `-DMIKMAP_WITH_ENGINE=OFF`.

## Trạng thái ghép

| | |
|---|---|
| Giao diện | 3 màn Composition · Advanced Mapping · Sensor I/O + cửa sổ Cài đặt, bám bản thiết kế |
| Output | Cửa sổ máy chiếu riêng, không viền (`F11`), chỉ vẽ slice đã warp |
| Engine của repo | `core/` + `io/` đã biên dịch và link vào (31 đối tượng), **chưa được giao diện gọi tới** |
| Mô hình dữ liệu | Giao diện vẫn chạy trên struct riêng trong `src/app.h` |

## Việc còn lại để ghép trọn

Giao diện hiện giữ mô hình riêng; bước tiếp theo là chuyển sang mô hình thật của repo, làm từng
mảng để luôn build được:

1. `core/model/ProjectIO` → lưu/mở `.hexmap` (các mục P0 còn thiếu: I2, F8, G8).
2. `core/calib/*` → thay phép tính homography và SensorMapper tự viết trong `src/calib.cpp` (G5, G7).
3. `core/model/Slice` + `WarpCornerPin` / `WarpMesh` / `WarpBezier` → thay phép warp trong `src/mapping.cpp`,
   qua đó có luôn Bezier (F10) và mặt nạ bezier (F12).
4. `io/*` → nguồn sensor thật (G1–G4, G14) thay cho radar mô phỏng.
5. `core/model/Composition` · `Layer` · `Clip` · `Transport` → thay mô hình deck trong `src/app.h`.

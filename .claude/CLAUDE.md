# MikMap / HexMapping — bộ não dự án

> File này Claude Code tự nạp mỗi phiên làm việc trong repo. Chi tiết theo từng
> mảng nằm ở `.claude/rules/`. Tài liệu gốc của dự án — đọc trước khi hỏi lại
> người dùng những gì đã có sẵn — là `README.md`, `architecture.md`,
> `features.md`, `UI_UPDATE_PROGRESS.md`, `newui/SKILL.md`.

## Dự án là gì

Projection mapping engine kiểu Resolume (deck · layer · column) → composition
canvas ảo → slice có keystone/mesh warp → máy chiếu, kèm chuỗi ánh xạ ngược từ
sensor về toạ độ nội dung: **chạm vào vật thể thật, hiệu ứng nổ đúng chỗ đó**.

Repo tên `MikMap`, mã nguồn dùng tên nội bộ `HexMapping` (namespace `hexmap`,
`HexMapping.sln`, `bin/HexMapping.exe`) — cùng một thứ, không phải hai dự án
khác nhau.

```
C++20 · openFrameworks 0.12.x · OpenGL · Dear ImGui · Windows / MSVC 2022
core/ + io/ build thuần bằng CMake — chạy được trên Linux/macOS/Windows,
không cần openFrameworks, không cần GPU.
```

Công thức trung tâm của toàn hệ thống sensor:

```
p_content = H_w⁻¹ · H_s · p_sensor
```

`H_s` (calibration sensor) và `H_w` (keystone/warp của slice) là **hai ma trận
tách rời** — chỉnh lại cái này không được làm hỏng cái kia.

## Quy tắc phụ thuộc BẤT KHẢ XÂM PHẠM

```
core   -> không phụ thuộc ai      (C++20 thuần: math, model, calib, filter, util)
io     -> core                    (thread sensor; KHÔNG biết Slice/Layer tồn tại)
render -> core                    (OpenGL + oF; chỉ đọc model)
ui     -> core                    (Dear ImGui; đọc model + phát lệnh)
app    -> core, io, render, ui    (AppController — nơi DUY NHẤT 4 tầng gặp nhau)
```

`core/` không bao giờ `#include` `ofMain.h`, `<GL/...>`, `imgui.h`. `io/` không
bao giờ biết `Slice`/`Layer`/ma trận mapping tồn tại. `render/` không bao giờ
gọi hàm của `ui/`. Đây là quy tắc dễ vi phạm nhất và cũng nghiêm trọng nhất
trong repo — **luôn kiểm trước khi thêm include mới vào `core/` hoặc `io/`**
(kiểm tự động: `tools/check_layering.ps1`, và hook `pre-push` ở dưới cũng chạy
một bản kiểm nhẹ bằng grep).

Có hai nhánh song song:

- `src/` — app chính, build bằng oF Project Generator + MSBuild
  (`HexMapping.sln` / `HexMapping.vcxproj`). File `.cpp/.h` mới **phải** được
  thêm vào `.vcxproj` nếu không sẽ lỗi link (chỉ áp dụng khi build MSBuild
  thật — không áp dụng cho `core/`/`io/` build bằng CMake thuần).
- `newui/` — dựng lại giao diện bằng GLFW, tự gom `../src/core` + `../src/io`
  qua `newui/CMakeLists.txt` riêng, **không đụng** file nào của `src/` hay
  `.vcxproj` gốc nên không thể conflict với nhánh chính. Đọc `newui/SKILL.md`
  trước khi sửa trong thư mục này.

## Build & test nhanh

```bash
# core + unit test — build được ngay trên máy này (Linux), không cần oF/GPU
cmake -S . -B build && cmake --build build --target hexmap_tests -j
ctest --test-dir build --output-on-failure
```

App chính (`HexMapping.exe`) và `newui/mikmap.exe` chỉ build đầy đủ trên
Windows/MSVC — lệnh cụ thể ở `README.md` và `newui/README.md`. Quy ước
build/test/style mặc định (bao gồm cả cách xử lý khi không có MSVC để test app
chính) nằm ở `.claude/rules/tech-defaults.md`.

## Đọc thêm theo việc đang làm

| Đang làm gì | Đọc |
|---|---|
| Quy trình làm việc, commit, nhánh, PR | `.claude/rules/workflow.md` |
| Sửa UI ImGui, đối chiếu bản thiết kế `MikMap_Web`/`sampleUI/` | `.claude/rules/design.md` |
| Quy ước code/test/build C++ mặc định | `.claude/rules/tech-defaults.md` |
| Bẫy đã mắc (đừng lặp lại), nguyên tắc thiết kế cốt lõi | mục cuối `newui/SKILL.md` |
| Tính năng nào đã/chưa làm (backlog 135 mục) | `features.md` |
| Vì sao kiến trúc chia tầng thế này | `architecture.md` |

## Việc KHÔNG được làm

- Không thêm include phá quy tắc phụ thuộc core/io/render/ui ở trên.
- Không đưa file mô tả *máy này và người này* vào git — ví dụ
  `bin/data/settings.json`, `bin/data/perf.log`, `.vs/`, `imgui.ini`. Đã bị
  chặn ở `.gitignore` gốc; đừng `git add -f` đè lên.
- Không tick `[x]` cho một mục feature chưa có code thật trong `features.md`
  hay `UI_UPDATE_PROGRESS.md` — luôn grep source để xác nhận trước. Repo này
  đã từng phải đính chính 6 mục tick sai.
- Không dùng `/fp:fast` cho toán homography/calibration — cần `/fp:precise`
  (đã set trong `CMakeLists.txt`, đừng đổi).
- Render thread (`render/`, vòng lặp `ofApp::draw`) không được lock, alloc,
  hay I/O — mọi phân bổ bộ nhớ động phải nằm ngoài đường vẽ mỗi frame.
- Video test/demo phải là **HAP**, không phải H.264/MP4 — xem
  `.claude/rules/tech-defaults.md`.

## Sub-agent & hook riêng của repo

- `.claude/agents/researcher.md` — điều tra kiến trúc/tính năng trước khi sửa.
- `.claude/agents/reviewer.md` — review diff theo checklist riêng của dự án.
- `.claude/hooks/pre-commit.sh` / `pre-push.sh` — chạy tự động qua
  `.claude/settings.json` trước khi Claude (hoặc bạn) chạy `git commit` /
  `git push`; có thể chặn nếu phát hiện vấn đề. Xem `.claude/rules/workflow.md`.

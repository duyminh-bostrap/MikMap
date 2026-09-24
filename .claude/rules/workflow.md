# Quy trình làm việc

## Nhánh & commit

- Nhánh chính hiện tại: `new_UI`. Kiểm `git branch -a`/`git status` trước khi
  bắt đầu — đừng giả định.
- Nhánh `legacy-oF-ui` giữ nguyên vẹn bản engine oF+MSBuild cũ (trước khi
  `newui/` được đổi tên thành `src/` và `core/`/`io/` chuyển sang `engine/`) —
  chỉ đọc/tham khảo/khôi phục từ đó, không phát triển tiếp trên nhánh này trừ
  khi được yêu cầu tường minh.
- Commit message ngắn gọn, mô tả **vì sao** hơn là liệt kê file đã đổi. Nếu
  đổi một quy tắc kiến trúc (layering, HAP-only, `/fp:precise`...), nói rõ lý
  do trong commit để người đọc sau không tưởng đó là tai nạn.
- Không tạo commit rỗng, không `--amend` commit đã push, không `--force` trừ
  khi được yêu cầu tường minh.
- Trước khi `git add`, chạy `git status` xem có gì lạ không (file build,
  `.vs/`, media thử nghiệm) — `.gitignore` gốc đã chặn phần lớn nhưng
  `git add -f` vẫn có thể đè lên nếu gõ nhầm.

## Hook tự động (`.claude/hooks/`)

`.claude/settings.json` gắn hai hook vào Bash tool:

- `git commit*` → chạy `pre-commit.sh`: chặn commit nếu có file media/binary
  lớn hoặc file "riêng máy" (`bin/data/settings.json`, `.vs/`, `imgui.ini`...)
  lỡ bị stage.
- `git push*` → chạy `pre-push.sh`: build `mikmap_core` + `mikmap_tests` bằng
  CMake và chạy `ctest`; chặn push nếu build lỗi hoặc test đỏ. Cũng chạy một
  bản kiểm layering nhẹ (grep include cấm trong `engine/core/`/`engine/io/`).

Hook chặn bằng cách thoát mã khác 0 — nếu bị chặn, đọc thông báo lỗi, sửa, rồi
thử lại. Đừng bỏ qua bằng `--no-verify` (đây là hook của Claude Code, không
phải git hook, nên `--no-verify` không có tác dụng — nếu hook sai, sửa
`.claude/hooks/*.sh`, đừng tìm cách lách).

## Trước khi báo "xong"

1. Nếu sửa `engine/core`/`engine/io`: chạy `cmake --build build --target
   mikmap_tests && ctest --test-dir build --output-on-failure` — 0 test đỏ,
   0 cảnh báo mới.
2. Nếu sửa `src/`: chạy thêm `ctest --test-dir src/build` (test `project_roundtrip`: lưu/nạp/undo, không cần cửa sổ)
   và build được cả trên Linux/macOS (cần `libglfw3-dev`
   +`libgl-dev` trên Linux, hoặc `brew install glfw` trên macOS — xem
   `README.md`), không chỉ Windows như trước — chạy
   `cmake -DSRC=src -DBUILD=src/build -P cmake/configure.cmake && cmake --build src/build` để xác nhận build
   sạch trước khi báo "đã sửa xong", đừng chỉ đọc code. Nếu máy không có
   GLFW/OpenGL để thử chạy thật (không chỉ build), nói rõ điều đó thay vì báo
   "đã test" khi chỉ build được nhị phân.
3. Nếu tick tính năng trong `features.md`: grep `src/*.cpp` để xác nhận
   code thật tồn tại và được GỌI tới (không chỉ vì `engine/core` đã hỗ trợ
   tính năng đó), đừng tin theo trí nhớ hay theo yêu cầu. `[~]` nghĩa là chỉ
   có UI/một phần hành vi, không phải "gần xong" — đọc "Ghi chú kiểm tra" cuối
   file trước khi đổi trạng thái một dòng. Repo chỉ còn MỘT `features.md`
   đang hoạt động (ở gốc, theo dõi `src/`); bản cũ theo dõi engine oF nằm
   trong lịch sử/nhánh `legacy-oF-ui`, không sửa nó trên `new_UI`.

## Sub-agent riêng của repo

- Dùng `researcher` (`.claude/agents/researcher.md`) khi cần trả lời "tính
  năng X đã có chưa", "hàm Y nằm ở đâu", trước khi bắt tay sửa để tránh làm
  lại việc đã có hoặc phá quy tắc kiến trúc.
- Dùng `reviewer` (`.claude/agents/reviewer.md`) sau khi sửa C++ đáng kể,
  trước khi commit — checklist riêng cho layering, `/W4`, HAP, render thread.

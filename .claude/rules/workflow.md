# Quy trình làm việc

## Nhánh & commit

- Nhánh chính hiện tại: `new_UI`. Kiểm `git branch -a`/`git status` trước khi
  bắt đầu — đừng giả định.
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
- `git push*` → chạy `pre-push.sh`: build `hexmap_core` + `hexmap_tests` bằng
  CMake và chạy `ctest`; chặn push nếu build lỗi hoặc test đỏ. Cũng chạy một
  bản kiểm layering nhẹ (grep include cấm trong `core/`/`io/`).

Hook chặn bằng cách thoát mã khác 0 — nếu bị chặn, đọc thông báo lỗi, sửa, rồi
thử lại. Đừng bỏ qua bằng `--no-verify` (đây là hook của Claude Code, không
phải git hook, nên `--no-verify` không có tác dụng — nếu hook sai, sửa
`.claude/hooks/*.sh`, đừng tìm cách lách).

## Trước khi báo "xong"

1. Nếu sửa `core/`/`io/`: chạy `cmake --build build --target hexmap_tests &&
   ctest --test-dir build --output-on-failure` — 0 test đỏ, 0 cảnh báo mới.
2. Nếu sửa `src/ui/` hoặc `newui/`: không build được app đầy đủ trên máy này
   (cần Windows/MSVC/openFrameworks) — nói rõ điều đó thay vì báo "đã test"
   khi chỉ đọc code. Nếu có máy Windows thật, làm theo `README.md`.
3. Nếu tick tính năng trong `features.md`/`UI_UPDATE_PROGRESS.md`: grep source
   để xác nhận code thật tồn tại, đừng tin theo trí nhớ hay theo yêu cầu.
4. Cập nhật `UI_UPDATE_PROGRESS.md` khi hoàn thành **một mảng lớn** — không
   phải mỗi commit nhỏ.

## Sub-agent riêng của repo

- Dùng `researcher` (`.claude/agents/researcher.md`) khi cần trả lời "tính
  năng X đã có chưa", "hàm Y nằm ở đâu", trước khi bắt tay sửa để tránh làm
  lại việc đã có hoặc phá quy tắc kiến trúc.
- Dùng `reviewer` (`.claude/agents/reviewer.md`) sau khi sửa C++ đáng kể,
  trước khi commit — checklist riêng cho layering, `/W4`, HAP, render thread.

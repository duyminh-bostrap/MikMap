#!/usr/bin/env bash
# Chạy tự động bởi Claude Code (PostToolUse trên Edit/Write/MultiEdit, xem .claude/settings.json)
# ngay SAU khi một file được sửa. Mục đích: bắt vi phạm quy tắc phụ thuộc ngay lúc viết,
# thay vì đợi tới `git push` (pre-push.sh chạy cùng kiểm tra này nhưng muộn và chậm hơn).
#
# Chỉ xét file nằm dưới engine/core/ hoặc engine/io/ — mọi file khác thoát ngay (vài ms).
# Thoát 2 = phản hồi stderr cho Claude để tự sửa; hook này KHÔNG hoàn tác file đã ghi.
#
# Quy tắc (xem .claude/CLAUDE.md và architecture.md §1):
#   engine/core  → không include GL / GLFW / ImGui / oF, và không include engine/io hay src/
#   engine/io    → không include GL / GLFW / ImGui / oF, và không biết model (Slice/Layer/Clip...)

set -uo pipefail

# Claude Code gửi JSON trên stdin: {"tool_input":{"file_path":"..."},...}
payload="$(cat 2>/dev/null || true)"
file="$(printf '%s' "$payload" | sed -n 's/.*"file_path"[[:space:]]*:[[:space:]]*"\(\([^"\\]\|\\.\)*\)".*/\1/p' | head -n1)"
[ -n "$file" ] || exit 0

# JSON escape \\ -> \ , rồi đổi \ thành / để so khớp cùng một dạng trên Windows lẫn macOS/Linux.
file="${file//\\\\/\\}"
file="${file//\\//}"

case "$file" in
  */engine/core/*|engine/core/*) layer=core ;;
  */engine/io/*|engine/io/*)     layer=io ;;
  *) exit 0 ;;
esac

# Chỉ kiểm file nguồn C++.
case "$file" in
  *.h|*.hpp|*.cpp|*.cc|*.inl) ;;
  *) exit 0 ;;
esac
[ -f "$file" ] || exit 0

problems=""
add() { problems="${problems}  - $1"$'\n'; }

# GL / GLFW / ImGui / openFrameworks — cấm ở cả hai lớp (cùng mẫu với pre-push.sh, thêm GLFW).
hits="$(grep -nE '#[[:space:]]*include[[:space:]]*[<"](ofMain\.h|GL/|GLFW/|OpenGL/|imgui[a-z_]*\.h)' "$file" 2>/dev/null || true)"
[ -n "$hits" ] && add "include GL/GLFW/ImGui/oF (cấm trong engine/${layer}):"$'\n'"$(printf '%s\n' "$hits" | sed 's/^/      /')"

if [ "$layer" = core ]; then
  # core không phụ thuộc ai: không được kéo io/ hay UI vào.
  hits="$(grep -nE '#[[:space:]]*include[[:space:]]*[<"](io/|\.\./io/|\.\./\.\./io/|ui/|app\.h|ui\.h)' "$file" 2>/dev/null || true)"
  [ -n "$hits" ] && add "core include io/ hoặc UI (core không phụ thuộc ai):"$'\n'"$(printf '%s\n' "$hits" | sed 's/^/      /')"
else
  # io không biết model: không include core/model/ (Slice, Layer, Clip, Warp...). core/math, core/util thì được.
  hits="$(grep -nE '#[[:space:]]*include[[:space:]]*[<"](core/model/|\.\./core/model/|\.\./\.\./core/model/)' "$file" 2>/dev/null || true)"
  [ -n "$hits" ] && add "io include core/model/ (io không được biết Slice/Layer/Clip tồn tại):"$'\n'"$(printf '%s\n' "$hits" | sed 's/^/      /')"
fi

if [ -n "$problems" ]; then
  {
    printf '[post-edit-layering] %s vi phạm quy tắc phụ thuộc (architecture.md §1):\n' "$file"
    printf '%s' "$problems"
    printf 'Sửa ngay: chuyển phần cần GL/ImGui sang src/, hoặc truyền dữ liệu qua tham số thay vì include.\n'
  } >&2
  exit 2
fi
exit 0

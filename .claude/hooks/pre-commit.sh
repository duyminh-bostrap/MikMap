#!/usr/bin/env bash
# Chạy tự động bởi Claude Code (PreToolUse trên Bash, xem .claude/settings.json)
# ngay trước khi lệnh "git commit*" thực sự chạy. Thoát khác 0 = chặn commit.
#
# Kiểm những thứ rẻ, nhanh (không build) trước khi cho phép commit:
#   1. Không có file "riêng máy/người" bị stage (settings.json, .vs/, imgui.ini...).
#   2. Không có file media/binary lớn bị stage (đáng lẽ .gitignore đã chặn).
#   3. Không có secret/credential rõ ràng trong nội dung đang được stage.

set -uo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || exit 0
cd "$REPO_ROOT"

staged="$(git diff --cached --name-only --diff-filter=ACM 2>/dev/null || true)"
if [ -z "$staged" ]; then
    exit 0  # không có gì stage (vd chỉ git commit --amend --no-edit rỗng) — để git tự xử lý
fi

issues=0
fail() { printf '[pre-commit] %s\n' "$1" >&2; issues=$((issues + 1)); }

# ── 1. File riêng máy/người ────────────────────────────────────────────
PRIVATE_PATTERNS='bin/data/settings\.json$|bin/data/perf\.log$|imgui\.ini$|(^|/)\.vs/|CLAUDE\.local\.md$|settings\.local\.json$'
private_hits="$(echo "$staged" | grep -E "$PRIVATE_PATTERNS" || true)"
if [ -n "$private_hits" ]; then
    fail "Đang stage file mô tả máy/người (đáng lẽ không vào git):"
    echo "$private_hits" | sed 's/^/    /' >&2
fi

# ── 2. Media/binary lớn ─────────────────────────────────────────────────
while IFS= read -r f; do
    [ -z "$f" ] && continue
    [ -f "$f" ] || continue
    size=$(wc -c < "$f" 2>/dev/null || echo 0)
    if [ "$size" -gt 5242880 ]; then
        fail "File > 5MB đang stage: $f ($((size / 1048576))MB) — kiểm .gitignore trước khi commit thật."
    fi
done <<< "$staged"

# ── 3. Secret/credential rõ ràng trong nội dung đang stage ──────────────
SECRET_PATTERN='(AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----|(api|secret)[_-]?key["'"'"']?\s*[:=]\s*["'"'"'][A-Za-z0-9/+_-]{16,})'
if git diff --cached -U0 2>/dev/null | grep -E '^\+' | grep -vE '^\+\+\+' | grep -qE "$SECRET_PATTERN"; then
    fail "Nội dung đang stage có chuỗi giống secret/credential (AWS key, private key, api_key=...)."
fi

if [ "$issues" -gt 0 ]; then
    echo "[pre-commit] Chặn commit — sửa xong rồi git add lại trước khi thử tiếp." >&2
    exit 2
fi
exit 0

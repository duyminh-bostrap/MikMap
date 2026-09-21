#!/usr/bin/env bash
# Quét bảo mật nhẹ cho repo MikMap: secret rò rỉ, file riêng máy/người lỡ
# vào git, và (nếu có cppcheck) static analysis cơ bản cho C++.
# Dùng: .claude/skills/run-security-scan.sh   (chạy từ bất kỳ đâu trong repo)
# Thoát 0 nếu sạch, khác 0 nếu phát hiện vấn đề (in báo cáo ra stderr).

set -uo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null || pwd)"
cd "$REPO_ROOT"

issues=0
note() { printf '  - %s\n' "$1"; }
fail() { printf '[FAIL] %s\n' "$1" >&2; issues=$((issues + 1)); }
ok()   { printf '[ OK ] %s\n' "$1"; }

echo "== Quét bảo mật MikMap =="
echo "Repo: $REPO_ROOT"
echo

# ── 1. Secret / credential rò rỉ trong nội dung được track bởi git ────────
echo "-- 1. Tìm secret/credential trong file đã track --"
SECRET_PATTERN='AKIA[0-9A-Z]{16}|-----BEGIN [A-Z ]*PRIVATE KEY-----|(api|secret)[_-]?key["'"'"']?[[:space:]]*[:=][[:space:]]*["'"'"'][A-Za-z0-9/+_-]{16,}|password[[:space:]]*[:=][[:space:]]*["'"'"'][^"'"'"'[:space:]]{6,}'
secret_hits="$(git grep -IlE "$SECRET_PATTERN" -- . 2>/dev/null || true)"
if [ -n "$secret_hits" ]; then
    fail "Có chuỗi giống secret/credential trong file đã track:"
    echo "$secret_hits" | sed 's/^/        /' >&2
else
    ok "Không thấy pattern secret rõ ràng trong file đã track."
fi

# ── 2. File "riêng máy/người" cố ý đứng ngoài git nhưng lỡ bị track ───────
echo
echo "-- 2. File riêng máy/người lỡ bị track bởi git --"
PRIVATE_PATTERNS='bin/data/settings\.json$|bin/data/perf\.log$|imgui\.ini$|\.vs/|CLAUDE\.local\.md$|settings\.local\.json$'
tracked_private="$(git ls-files 2>/dev/null | grep -E "$PRIVATE_PATTERNS" || true)"
if [ -n "$tracked_private" ]; then
    fail "File đáng lẽ chỉ ở máy này lại đang được git track:"
    echo "$tracked_private" | sed 's/^/        /' >&2
else
    ok "Không có file riêng máy/người nào bị track."
fi

# ── 3. Media/binary lớn lỡ commit (đáng lẽ .gitignore đã chặn) ────────────
echo
echo "-- 3. File media/binary lớn đã track --"
big_media=""
while IFS= read -r -d '' f; do
    [ -f "$f" ] || continue
    size=$(wc -c < "$f" 2>/dev/null || echo 0)
    if [ "$size" -gt 5242880 ]; then
        big_media="$big_media$f ($((size / 1048576))MB)"$'\n'
    fi
done < <(git ls-files -z 2>/dev/null)
if [ -n "$big_media" ]; then
    fail "File > 5MB đang được track (kiểm xem có nên vào .gitignore không):"
    printf '%s' "$big_media" | sed 's/^/        /' >&2
else
    ok "Không có file > 5MB nào bị track."
fi

# ── 4. Static analysis C++ (tuỳ chọn, chỉ khi có cppcheck) ────────────────
echo
echo "-- 4. cppcheck cho core/ + io/ (bỏ qua nếu chưa cài) --"
if command -v cppcheck >/dev/null 2>&1; then
    cppcheck --quiet --enable=warning,portability --std=c++20 \
        --error-exitcode=0 src/core src/io 2>/tmp/mikmap_cppcheck.$$ || true
    if [ -s /tmp/mikmap_cppcheck.$$ ]; then
        echo "  cppcheck có ghi chú (không tự chặn build, xem lại thủ công):"
        sed 's/^/        /' /tmp/mikmap_cppcheck.$$
    else
        ok "cppcheck không có ghi chú."
    fi
    rm -f /tmp/mikmap_cppcheck.$$
else
    note "cppcheck chưa cài — bỏ qua bước static analysis (không tính là lỗi)."
fi

echo
if [ "$issues" -gt 0 ]; then
    echo "== Kết quả: $issues vấn đề cần xem lại ==" >&2
    exit 1
fi
echo "== Kết quả: sạch =="
exit 0

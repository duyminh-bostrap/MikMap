#!/usr/bin/env bash
# Chạy tự động bởi Claude Code (PreToolUse trên Bash, xem .claude/settings.json)
# ngay trước khi lệnh "git push*" thực sự chạy. Thoát khác 0 = chặn push.
#
# Kiểm nặng hơn pre-commit vì push ảnh hưởng người khác:
#   1. Kiểm layering nhẹ (grep include cấm) — core/ và io/ không được đụng GL/oF/ImGui.
#   2. Build hexmap_core + hexmap_tests bằng CMake (không cần openFrameworks/GPU).
#   3. Chạy ctest — chặn push nếu có test đỏ.
#
# Bỏ qua an toàn (không chặn, chỉ cảnh báo) nếu máy này không có cmake/ctest —
# ví dụ môi trường chỉ chỉnh tài liệu, không có toolchain C++.

set -uo pipefail

REPO_ROOT="$(git rev-parse --show-toplevel 2>/dev/null)" || exit 0
cd "$REPO_ROOT"

issues=0
fail() { printf '[pre-push] %s\n' "$1" >&2; issues=$((issues + 1)); }
info() { printf '[pre-push] %s\n' "$1"; }

# ── 1. Kiểm layering: engine/core/ và engine/io/ không được include GL/oF/ImGui ──
info "Kiểm quy tắc phụ thuộc engine/core, engine/io (xem architecture.md §1)..."
forbidden_includes="$(grep -rIlE '#include\s*[<"](ofMain\.h|GL/|imgui\.h|imgui_internal\.h)' engine/core engine/io 2>/dev/null || true)"
if [ -n "$forbidden_includes" ]; then
    fail "engine/core/ hoặc engine/io/ include thứ bị cấm (ofMain.h/GL/imgui.h) — phá quy tắc phụ thuộc bất khả xâm phạm:"
    echo "$forbidden_includes" | sed 's/^/    /' >&2
fi

# ── 2 & 3. Build + test core/io bằng CMake (bỏ qua nếu thiếu toolchain) ──
if ! command -v cmake >/dev/null 2>&1; then
    info "Không có cmake trên PATH — bỏ qua build/test, chỉ chặn theo mục 1 ở trên."
elif [ "$issues" -gt 0 ]; then
    info "Đã có lỗi layering ở mục 1 — bỏ qua build/test, sửa layering trước."
else
    BUILD_DIR="build"
    info "Build hexmap_tests (cmake -S . -B $BUILD_DIR)..."
    if ! cmake -S . -B "$BUILD_DIR" >/tmp/mikmap_prepush_configure.log 2>&1; then
        fail "cmake configure thất bại — xem /tmp/mikmap_prepush_configure.log"
    elif ! cmake --build "$BUILD_DIR" --target hexmap_tests -j >/tmp/mikmap_prepush_build.log 2>&1; then
        fail "Build hexmap_tests thất bại:"
        tail -40 /tmp/mikmap_prepush_build.log | sed 's/^/    /' >&2
    else
        info "Build OK — chạy ctest..."
        if ! ctest --test-dir "$BUILD_DIR" --output-on-failure >/tmp/mikmap_prepush_test.log 2>&1; then
            fail "Có unit test đỏ:"
            tail -60 /tmp/mikmap_prepush_test.log | sed 's/^/    /' >&2
        else
            info "ctest: tất cả test xanh."
        fi
    fi
fi

if [ "$issues" -gt 0 ]; then
    echo "[pre-push] Chặn push — sửa lỗi ở trên rồi thử lại." >&2
    exit 2
fi
exit 0

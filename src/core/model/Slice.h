// ════════════════════════════════════════════════════════════════════════
//  core/model/Slice.h — đối tượng mapping cơ bản (F3 F4 F16)
//
//  Một slice trả lời hai câu hỏi:
//    1. LẤY vùng nào của Composition Canvas?   → inputOrigin + inputSize
//    2. ĐẶT nó ở đâu trên máy chiếu?           → warp (corner pin / mesh)
//
//      ┌──────── COMPOSITION CANVAS ────────┐
//      │        ┌──────────┐                │
//      │        │ inputRect│ ← F4           │
//      │        └──────────┘                │
//      └────────────┬───────────────────────┘
//                   │ warp.forward()
//                   ▼
//         ╔═══ SCREEN (máy chiếu) ═══╗
//         ║      ╱‾‾‾‾‾‾‾╲           ║
//         ║     ╱ đã warp  ╲         ║
//         ║    ╲___________╱         ║
//         ╚══════════════════════════╝
//
//  ── Ghi chú về sở hữu ────────────────────────────────────────────────
//  Slice SỞ HỮU warp qua unique_ptr (đa hình). Vì vậy phải tự viết
//  copy constructor dùng clone() — nếu quên, sao chép slice sẽ mất warp
//  và mọi căn chỉnh biến mất.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"
#include "core/model/IWarp.h"

#include <memory>
#include <string>

namespace hexmap {

class Slice {
public:
    Slice();
    Slice(const Vec2& inputOrigin, const Vec2& inputSize,
          const Vec2& outputTopLeft, const Vec2& outputSize);

    // Sao chép sâu — warp được clone(), không chia sẻ con trỏ.
    Slice(const Slice& other);
    Slice& operator=(const Slice& other);
    Slice(Slice&&) noexcept = default;
    Slice& operator=(Slice&&) noexcept = default;
    ~Slice() = default;

    std::string name = "Slice";

    /// F16 — tắt slice mà không xoá.
    bool enabled = true;

    /// F16 — chỉ hiện slice này. Screen quyết định, không phải slice.
    bool solo = false;

    // ── F4: vùng lấy trên Composition Canvas ───────────────────────────
    Vec2 inputOrigin{0.0, 0.0};
    Vec2 inputSize{1920.0, 1080.0};

    // ── Warp ───────────────────────────────────────────────────────────
    void setWarp(std::unique_ptr<IWarp> w);
    IWarp*       warp()       { return m_warp.get(); }
    const IWarp* warp() const { return m_warp.get(); }

    /// Thay bằng loại warp khác, giữ nguyên vùng output hiện tại.
    /// Dùng khi người dùng đổi từ corner pin sang mesh giữa chừng.
    void convertWarp(WarpType type, int meshCols = 4, int meshRows = 4);

    // ── Chuyển đổi không gian ──────────────────────────────────────────
    /// contentUV [0,1]² → toạ độ Composition Canvas.
    Vec2 contentToCanvas(const Vec2& contentUV) const {
        return {inputOrigin.x + contentUV.x * inputSize.x,
                inputOrigin.y + contentUV.y * inputSize.y};
    }

    /// Canvas → contentUV. Không kẹp — giá trị ngoài [0,1] nghĩa là điểm
    /// nằm ngoài vùng lấy của slice này.
    Vec2 canvasToContent(const Vec2& canvasPx) const;

    /// ★ output px → contentUV. Đây là bước sensor cần.
    /// @return false nếu điểm nằm ngoài slice.
    bool outputToContent(const Vec2& outputPx, Vec2& contentUV) const;

    /// ★ output px → toạ độ Composition Canvas.
    bool outputToCanvas(const Vec2& outputPx, Vec2& canvasPx) const;

    /// contentUV → output px.
    Vec2 contentToOutput(const Vec2& contentUV) const;

    /// Có sẵn sàng dùng không: đang bật, có warp, và warp nghịch đảo được.
    bool isUsable() const {
        return enabled && m_warp && m_warp->isInvertible();
    }

private:
    std::unique_ptr<IWarp> m_warp;
};

} // namespace hexmap

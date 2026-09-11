// ════════════════════════════════════════════════════════════════════════
//  core/model/Screen.h — một thiết bị output (F1 F7 F17)
//
//  Screen = một máy chiếu (hoặc màn hình, hoặc output ảo Spout/NDI).
//  Nó sở hữu N slice, mỗi slice lấy một vùng của Composition Canvas rồi
//  đặt lên một chỗ trên screen này.
//
//  ── Thứ tự z và quy tắc "ai thắng khi chồng nhau" ────────────────────
//  slices[0] vẽ TRƯỚC (dưới cùng), slices[n-1] vẽ SAU (trên cùng) —
//  giống thứ tự vẽ thông thường.
//
//  Nhưng khi TRA CỨU điểm chạm thì duyệt NGƯỢC LẠI: slice trên cùng
//  thắng. Đây là điều người dùng mong đợi (giống cách xử lý click chuột)
//  và nếu làm ngược thì chạm vào slice nhỏ nằm trên sẽ kích hoạt nhầm
//  slice nền phía dưới.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/model/Slice.h"

#include <string>
#include <vector>

namespace hexmap {

/// Loại thiết bị output. P0 chỉ làm Display; các loại khác là P2.
enum class ScreenOutputType {
    Display = 0,   ///< màn hình / máy chiếu qua cổng video
    Virtual,       ///< không xuất ra đâu — để xem thử
    Spout,         ///< F23 (P2)
    NDI,           ///< F23 (P2)
};

class Screen {
public:
    Screen() = default;
    Screen(int screenId, std::string screenName, const Vec2& res);

    int         id = 0;
    std::string name = "Screen";
    Vec2        resolution{1920.0, 1080.0};
    bool        enabled = true;

    ScreenOutputType outputType = ScreenOutputType::Display;

    /// Chỉ số màn hình vật lý khi outputType == Display. −1 = chưa gán.
    int displayIndex = -1;

    // ── Slice ──────────────────────────────────────────────────────────
    /// Thứ tự vẽ: [0] dưới cùng → [n−1] trên cùng.
    std::vector<Slice> slices;

    int sliceCount() const { return static_cast<int>(slices.size()); }

    /// Thêm slice chiếm toàn màn hình, lấy toàn bộ canvas.
    /// Đây là mặc định hợp lý khi tạo screen mới.
    Slice& addFullScreenSlice(const Vec2& canvasSize);

    void removeSlice(int index);

    /// Đổi thứ tự z. Trả về false nếu chỉ số không hợp lệ.
    bool moveSlice(int from, int to);

    // ── F16: solo ──────────────────────────────────────────────────────
    bool anySliceSolo() const;

    /// Slice có được VẼ không — đã xét enabled và solo của cả nhóm.
    bool isSliceVisible(int index) const;

    /// Chỉ số các slice cần vẽ, theo đúng thứ tự vẽ (dưới → trên).
    std::vector<int> visibleSlices() const;

    // ── ★ Tra cứu điểm (dùng cho sensor) ───────────────────────────────
    /// Slice nào nhận điểm output này? Duyệt từ TRÊN xuống.
    /// @return −1 nếu không slice nào chứa điểm.
    int hitTest(const Vec2& outputPx) const;

    /// Tra cứu đầy đủ: output px → slice + contentUV + toạ độ canvas.
    bool outputToCanvas(const Vec2& outputPx,
                        int& outSliceIndex,
                        Vec2& outContentUV,
                        Vec2& outCanvasPx) const;
};

/// F22 — những layer đang được ít nhất một slice lấy làm nguồn riêng.
///
/// ★ RenderEngine phải nướng MỘT FBO riêng cho mỗi layer như vậy, và ở
///   4K thì mỗi FBO là ~32 MB VRAM cộng một lần clear + vẽ mỗi frame.
///   Nướng cho MỌI layer là cách chắc chắn làm tụt fps của một tính năng
///   mà đa số project không dùng tới. Danh sách này cho render biết đúng
///   phần tối thiểu cần làm.
///
/// Chỉ trả về chỉ số HỢP LỆ (đã lọc qua `Slice::effectiveSourceLayer`),
/// đã sắp xếp và không trùng lặp.
std::vector<int> layersUsedAsSource(const std::vector<Screen>& screens,
                                    int layerCount);

} // namespace hexmap

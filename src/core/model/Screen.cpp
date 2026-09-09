#include "core/model/Screen.h"

#include <algorithm>

namespace hexmap {
namespace {
/// Nới biên khi lọc bằng hộp bao.
///
/// Hộp bao chỉ là bộ lọc SƠ BỘ; phép kiểm tra thật là warp->inverse().
/// Nếu lọc chặt hơn phép kiểm tra thật, ta loại nhầm điểm hợp lệ —
/// cụ thể là khi người dùng chạm đúng VIỀN slice, sai số ~1e-9 của
/// homography đủ để đẩy điểm ra ngoài hộp bao.
constexpr double kBoundsEpsilon = 1.0;   // px, dưới ngưỡng nhìn thấy
} // namespace

Screen::Screen(int screenId, std::string screenName, const Vec2& res)
    : id(screenId), name(std::move(screenName)), resolution(res) {}

Slice& Screen::addFullScreenSlice(const Vec2& canvasSize) {
    slices.emplace_back(Vec2{0.0, 0.0}, canvasSize,
                        Vec2{0.0, 0.0}, resolution);
    slices.back().name = "Slice " + std::to_string(slices.size());
    return slices.back();
}

void Screen::removeSlice(int index) {
    if (index < 0 || index >= sliceCount()) return;
    slices.erase(slices.begin() + index);
}

bool Screen::moveSlice(int from, int to) {
    if (from < 0 || from >= sliceCount()) return false;
    if (to   < 0 || to   >= sliceCount()) return false;
    if (from == to) return true;

    Slice tmp = std::move(slices[static_cast<size_t>(from)]);
    slices.erase(slices.begin() + from);
    slices.insert(slices.begin() + to, std::move(tmp));
    return true;
}

bool Screen::anySliceSolo() const {
    return std::any_of(slices.begin(), slices.end(),
                       [](const Slice& s) { return s.solo; });
}

bool Screen::isSliceVisible(int index) const {
    if (index < 0 || index >= sliceCount()) return false;
    const Slice& s = slices[static_cast<size_t>(index)];
    if (!s.enabled) return false;
    if (anySliceSolo() && !s.solo) return false;
    return true;
}

std::vector<int> Screen::visibleSlices() const {
    std::vector<int> out;
    out.reserve(slices.size());
    for (int i = 0; i < sliceCount(); ++i) {
        if (isSliceVisible(i)) out.push_back(i);
    }
    return out;
}

int Screen::hitTest(const Vec2& outputPx) const {
    // ★ Duyệt NGƯỢC: slice trên cùng thắng.
    for (int i = sliceCount() - 1; i >= 0; --i) {
        if (!isSliceVisible(i)) continue;

        const Slice& s = slices[static_cast<size_t>(i)];
        if (!s.isUsable()) continue;

        // Lọc nhanh bằng hộp bao trước khi gọi inverse() — với mesh,
        // inverse() phải quét từng ô lưới nên đắt hơn nhiều.
        Vec2 lo, hi;
        s.warp()->boundingBox(lo, hi);
        if (outputPx.x < lo.x - kBoundsEpsilon || outputPx.x > hi.x + kBoundsEpsilon) continue;
        if (outputPx.y < lo.y - kBoundsEpsilon || outputPx.y > hi.y + kBoundsEpsilon) continue;

        Vec2 uv;
        if (s.outputToContent(outputPx, uv)) return i;
    }
    return -1;
}

bool Screen::outputToCanvas(const Vec2& outputPx,
                            int& outSliceIndex,
                            Vec2& outContentUV,
                            Vec2& outCanvasPx) const {
    const int idx = hitTest(outputPx);
    if (idx < 0) return false;

    const Slice& s = slices[static_cast<size_t>(idx)];
    if (!s.outputToContent(outputPx, outContentUV)) return false;

    outSliceIndex = idx;
    outCanvasPx = s.contentToCanvas(outContentUV);
    return true;
}

} // namespace hexmap

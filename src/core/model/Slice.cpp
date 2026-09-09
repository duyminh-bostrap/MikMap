#include "core/model/Slice.h"

#include "core/model/WarpCornerPin.h"
#include "core/model/WarpMesh.h"

#include <cmath>

namespace hexmap {

Slice::Slice()
    : m_warp(std::make_unique<WarpCornerPin>()) {}

Slice::Slice(const Vec2& inOrigin, const Vec2& inSize,
             const Vec2& outTopLeft, const Vec2& outSize)
    : inputOrigin(inOrigin), inputSize(inSize) {
    auto w = std::make_unique<WarpCornerPin>();
    w->resetToRect(outTopLeft, outSize);
    m_warp = std::move(w);
}

Slice::Slice(const Slice& other)
    : name(other.name),
      enabled(other.enabled),
      solo(other.solo),
      color(other.color),
      inputOrigin(other.inputOrigin),
      inputSize(other.inputSize),
      // ★ clone() chứ không copy con trỏ. Quên bước này thì sao chép
      //   slice sẽ chia sẻ warp — kéo góc slice này làm đổi luôn slice kia.
      m_warp(other.m_warp ? other.m_warp->clone() : nullptr) {}

Slice& Slice::operator=(const Slice& other) {
    if (this == &other) return *this;
    name        = other.name;
    enabled     = other.enabled;
    solo        = other.solo;
    color       = other.color;
    inputOrigin = other.inputOrigin;
    inputSize   = other.inputSize;
    m_warp      = other.m_warp ? other.m_warp->clone() : nullptr;
    return *this;
}

void Slice::setWarp(std::unique_ptr<IWarp> w) {
    m_warp = std::move(w);
}

void Slice::convertWarp(WarpType type, int meshCols, int meshRows) {
    if (m_warp && m_warp->type() == type) return;

    // Giữ nguyên vùng output hiện tại — người dùng đổi loại warp là để
    // tinh chỉnh THÊM, không phải để mất công căn chỉnh đã làm.
    Vec2 lo{0.0, 0.0}, hi{1920.0, 1080.0};
    if (m_warp) m_warp->boundingBox(lo, hi);
    const Vec2 size{hi.x - lo.x, hi.y - lo.y};

    switch (type) {
    case WarpType::CornerPin: {
        auto w = std::make_unique<WarpCornerPin>();
        w->resetToRect(lo, size);
        m_warp = std::move(w);
        break;
    }
    case WarpType::Mesh: {
        m_warp = std::make_unique<WarpMesh>(meshCols, meshRows, lo, size);
        break;
    }
    case WarpType::Bezier:
    default:
        // Chưa cài đặt (F10 — P1). Giữ nguyên warp hiện tại thay vì
        // đặt nullptr, để slice không rơi vào trạng thái hỏng.
        break;
    }
}

Vec2 Slice::canvasToContent(const Vec2& canvasPx) const {
    // inputSize = 0 theo một trục thì không chia được. Xảy ra khi người
    // dùng kéo vùng lấy thành một đường thẳng.
    const double sx = (std::abs(inputSize.x) > 1e-12) ? inputSize.x : 1.0;
    const double sy = (std::abs(inputSize.y) > 1e-12) ? inputSize.y : 1.0;
    return {(canvasPx.x - inputOrigin.x) / sx,
            (canvasPx.y - inputOrigin.y) / sy};
}

bool Slice::outputToContent(const Vec2& outputPx, Vec2& contentUV) const {
    if (!isUsable()) return false;
    return m_warp->inverse(outputPx, contentUV);
}

bool Slice::outputToCanvas(const Vec2& outputPx, Vec2& canvasPx) const {
    Vec2 uv;
    if (!outputToContent(outputPx, uv)) return false;
    canvasPx = contentToCanvas(uv);
    return true;
}

Vec2 Slice::contentToOutput(const Vec2& contentUV) const {
    if (!m_warp) return contentUV;
    return m_warp->forward(contentUV);
}

} // namespace hexmap

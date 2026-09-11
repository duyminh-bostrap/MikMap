#include "WarpCornerPin.h"

namespace HexMap::Core::Model {

WarpCornerPin::WarpCornerPin() {
    m_srcQuad = { Math::Vec2(0, 0), Math::Vec2(1920, 0), Math::Vec2(1920, 1080), Math::Vec2(0, 1080) };
    m_dstQuad = { Math::Vec2(0, 0), Math::Vec2(1920, 0), Math::Vec2(1920, 1080), Math::Vec2(0, 1080) };
    recomputeMatrices();
}

WarpCornerPin::WarpCornerPin(const std::array<Math::Vec2, 4>& srcQuad,
                             const std::array<Math::Vec2, 4>& dstQuad)
    : m_srcQuad(srcQuad), m_dstQuad(dstQuad) {
    recomputeMatrices();
}

void WarpCornerPin::setCorners(const std::array<Math::Vec2, 4>& srcQuad,
                               const std::array<Math::Vec2, 4>& dstQuad) {
    m_srcQuad = srcQuad;
    m_dstQuad = dstQuad;
    recomputeMatrices();
}

void WarpCornerPin::recomputeMatrices() {
    auto maybeH = Math::Homography::find4Point(m_srcQuad, m_dstQuad);
    if (maybeH) {
        m_H = *maybeH;
        m_H_inv = m_H.inverse();
    } else {
        m_H = Math::Mat3::identity();
        m_H_inv = Math::Mat3::identity();
    }
}

Math::Vec2 WarpCornerPin::forward(const Math::Vec2& srcPoint) const {
    return m_H.transformPoint(srcPoint);
}

std::optional<Math::Vec2> WarpCornerPin::inverse(const Math::Vec2& dstPoint) const {
    if (m_H_inv) {
        return m_H_inv->transformPoint(dstPoint);
    }
    return std::nullopt;
}

std::unique_ptr<IWarp> WarpCornerPin::clone() const {
    return std::make_unique<WarpCornerPin>(*this);
}

} // namespace HexMap::Core::Model

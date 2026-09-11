#pragma once

#include "IWarp.h"
#include "../math/Homography.h"
#include "../math/Mat3.h"
#include <array>

namespace HexMap::Core::Model {

class WarpCornerPin : public IWarp {
public:
    WarpCornerPin();
    WarpCornerPin(const std::array<Math::Vec2, 4>& srcQuad,
                  const std::array<Math::Vec2, 4>& dstQuad);

    void setCorners(const std::array<Math::Vec2, 4>& srcQuad,
                    const std::array<Math::Vec2, 4>& dstQuad);

    Math::Vec2 forward(const Math::Vec2& srcPoint) const override;
    std::optional<Math::Vec2> inverse(const Math::Vec2& dstPoint) const override;

    std::unique_ptr<IWarp> clone() const override;

    const std::array<Math::Vec2, 4>& getDstQuad() const { return m_dstQuad; }
    const Math::Mat3& getForwardMatrix() const { return m_H; }

private:
    void recomputeMatrices();

    std::array<Math::Vec2, 4> m_srcQuad;
    std::array<Math::Vec2, 4> m_dstQuad;
    Math::Mat3 m_H;
    std::optional<Math::Mat3> m_H_inv;
};

} // namespace HexMap::Core::Model

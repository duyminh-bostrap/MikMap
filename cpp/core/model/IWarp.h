#pragma once

#include "../math/Vec2.h"
#include <memory>
#include <optional>

namespace HexMap::Core::Model {

class IWarp {
public:
    virtual ~IWarp() = default;

    // Biến đổi từ không gian nguồn (Source/Content UV) sang không gian máy chiếu (Output)
    virtual Math::Vec2 forward(const Math::Vec2& srcPoint) const = 0;

    // Nghịch đảo: từ không gian máy chiếu (Output) trở về không gian nguồn (Source/Content UV)
    // Cực kỳ quan trọng cho Sensor Calibration (Homography H_s và H_w inverse)
    virtual std::optional<Math::Vec2> inverse(const Math::Vec2& dstPoint) const = 0;

    // Clone đa hình an toàn
    virtual std::unique_ptr<IWarp> clone() const = 0;
};

} // namespace HexMap::Core::Model

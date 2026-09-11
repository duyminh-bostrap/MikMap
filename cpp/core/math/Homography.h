#pragma once

#include "Mat3.h"
#include "Vec2.h"
#include <array>
#include <optional>

namespace HexMap::Core::Math {

class Homography {
public:
    // Tính ma trận Homography biến đổi 4 điểm nguồn (src) sang 4 điểm đích (dst) bằng DLT
    static std::optional<Mat3> find4Point(const std::array<Vec2, 4>& src,
                                          const std::array<Vec2, 4>& dst);
};

} // namespace HexMap::Core::Math

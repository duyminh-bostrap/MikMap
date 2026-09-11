#pragma once

#include "Vec2.h"
#include <array>
#include <optional>

namespace HexMap::Core::Math {

// Ma trận thuần nhất 3x3 cho phép biến đổi affine & projective (Homography)
struct Mat3 {
    std::array<double, 9> m = {
        1, 0, 0,
        0, 1, 0,
        0, 0, 1
    };

    static constexpr Mat3 identity() {
        return Mat3{ { 1, 0, 0, 0, 1, 0, 0, 0, 1 } };
    }

    double at(int row, int col) const { return m[row * 3 + col]; }
    double& at(int row, int col) { return m[row * 3 + col]; }

    Mat3 operator*(const Mat3& o) const;
    Vec2 transformPoint(const Vec2& p) const;

    double determinant() const;
    std::optional<Mat3> inverse() const;
};

} // namespace HexMap::Core::Math

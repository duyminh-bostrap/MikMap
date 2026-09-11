#include "Mat3.h"
#include <cmath>

namespace HexMap::Core::Math {

Mat3 Mat3::operator*(const Mat3& o) const {
    Mat3 res;
    for (int r = 0; r < 3; ++r) {
        for (int c = 0; c < 3; ++c) {
            res.at(r, c) = at(r, 0) * o.at(0, c) +
                           at(r, 1) * o.at(1, c) +
                           at(r, 2) * o.at(2, c);
        }
    }
    return res;
}

Vec2 Mat3::transformPoint(const Vec2& p) const {
    double x = at(0, 0) * p.x + at(0, 1) * p.y + at(0, 2);
    double y = at(1, 0) * p.x + at(1, 1) * p.y + at(1, 2);
    double w = at(2, 0) * p.x + at(2, 1) * p.y + at(2, 2);

    if (std::abs(w) > 1e-9) {
        return Vec2(x / w, y / w);
    }
    return Vec2(x, y);
}

double Mat3::determinant() const {
    return at(0, 0) * (at(1, 1) * at(2, 2) - at(1, 2) * at(2, 1)) -
           at(0, 1) * (at(1, 0) * at(2, 2) - at(1, 2) * at(2, 0)) +
           at(0, 2) * (at(1, 0) * at(2, 1) - at(1, 1) * at(2, 0));
}

std::optional<Mat3> Mat3::inverse() const {
    double det = determinant();
    if (std::abs(det) < 1e-9) {
        return std::nullopt;
    }

    double invDet = 1.0 / det;
    Mat3 res;
    res.at(0, 0) =  (at(1, 1) * at(2, 2) - at(1, 2) * at(2, 1)) * invDet;
    res.at(0, 1) = -(at(0, 1) * at(2, 2) - at(0, 2) * at(2, 1)) * invDet;
    res.at(0, 2) =  (at(0, 1) * at(1, 2) - at(0, 2) * at(1, 1)) * invDet;

    res.at(1, 0) = -(at(1, 0) * at(2, 2) - at(1, 2) * at(2, 0)) * invDet;
    res.at(1, 1) =  (at(0, 0) * at(2, 2) - at(0, 2) * at(2, 0)) * invDet;
    res.at(1, 2) = -(at(0, 0) * at(1, 2) - at(0, 2) * at(1, 0)) * invDet;

    res.at(2, 0) =  (at(1, 0) * at(2, 1) - at(1, 1) * at(2, 0)) * invDet;
    res.at(2, 1) = -(at(0, 0) * at(2, 1) - at(0, 1) * at(2, 0)) * invDet;
    res.at(2, 2) =  (at(0, 0) * at(1, 1) - at(0, 1) * at(1, 0)) * invDet;

    return res;
}

} // namespace HexMap::Core::Math

#include "core/math/Mat3.h"

#include <cmath>

namespace hexmap {

Mat3 Mat3::rotation(double radians) {
    const double c = std::cos(radians);
    const double s = std::sin(radians);
    return Mat3{c, -s, 0,
                s,  c, 0,
                0,  0, 1};
}

Mat3 Mat3::rotationAround(double radians, const Vec2& anchor) {
    // T(anchor) · R(θ) · T(-anchor)
    return translation(anchor.x, anchor.y)
         * rotation(radians)
         * translation(-anchor.x, -anchor.y);
}

Mat3 Mat3::operator*(const Mat3& o) const {
    Mat3 r;
    for (int i = 0; i < 3; ++i) {
        for (int j = 0; j < 3; ++j) {
            r.at(i, j) = at(i, 0) * o.at(0, j)
                       + at(i, 1) * o.at(1, j)
                       + at(i, 2) * o.at(2, j);
        }
    }
    return r;
}

Mat3 Mat3::operator*(double s) const {
    Mat3 r;
    for (size_t i = 0; i < 9; ++i) r.m[i] = m[i] * s;
    return r;
}

Mat3 Mat3::operator+(const Mat3& o) const {
    Mat3 r;
    for (size_t i = 0; i < 9; ++i) r.m[i] = m[i] + o.m[i];
    return r;
}

Vec2 Mat3::transformPoint(const Vec2& p) const {
    const double x = at(0, 0) * p.x + at(0, 1) * p.y + at(0, 2);
    const double y = at(1, 0) * p.x + at(1, 1) * p.y + at(1, 2);
    const double w = at(2, 0) * p.x + at(2, 1) * p.y + at(2, 2);

    // w == 0 nghĩa là điểm bị ánh xạ ra vô cực — xảy ra khi điểm nằm
    // đúng trên "đường chân trời" của phép biến đổi phối cảnh.
    // Trả về điểm gốc thay vì sinh ra inf/NaN lan ra toàn hệ thống.
    if (std::abs(w) < 1e-12) return p;

    return {x / w, y / w};
}

Vec2 Mat3::transformDirection(const Vec2& v) const {
    return {at(0, 0) * v.x + at(0, 1) * v.y,
            at(1, 0) * v.x + at(1, 1) * v.y};
}

double Mat3::determinant() const {
    return at(0, 0) * (at(1, 1) * at(2, 2) - at(1, 2) * at(2, 1))
         - at(0, 1) * (at(1, 0) * at(2, 2) - at(1, 2) * at(2, 0))
         + at(0, 2) * (at(1, 0) * at(2, 1) - at(1, 1) * at(2, 0));
}

bool Mat3::invert(Mat3& out) const {
    const double det = determinant();
    if (std::abs(det) < 1e-12) return false;

    const double invDet = 1.0 / det;

    // Nghịch đảo = adj(M) / det, với adj = chuyển vị của ma trận phần bù đại số.
    out.at(0, 0) =  (at(1, 1) * at(2, 2) - at(1, 2) * at(2, 1)) * invDet;
    out.at(0, 1) = -(at(0, 1) * at(2, 2) - at(0, 2) * at(2, 1)) * invDet;
    out.at(0, 2) =  (at(0, 1) * at(1, 2) - at(0, 2) * at(1, 1)) * invDet;

    out.at(1, 0) = -(at(1, 0) * at(2, 2) - at(1, 2) * at(2, 0)) * invDet;
    out.at(1, 1) =  (at(0, 0) * at(2, 2) - at(0, 2) * at(2, 0)) * invDet;
    out.at(1, 2) = -(at(0, 0) * at(1, 2) - at(0, 2) * at(1, 0)) * invDet;

    out.at(2, 0) =  (at(1, 0) * at(2, 1) - at(1, 1) * at(2, 0)) * invDet;
    out.at(2, 1) = -(at(0, 0) * at(2, 1) - at(0, 1) * at(2, 0)) * invDet;
    out.at(2, 2) =  (at(0, 0) * at(1, 1) - at(0, 1) * at(1, 0)) * invDet;

    return true;
}

Mat3 Mat3::inverted() const {
    Mat3 out;
    if (!invert(out)) return Mat3::identity();
    return out;
}

bool Mat3::isInvertible(double eps) const {
    return std::abs(determinant()) > eps;
}

Mat3 Mat3::transposed() const {
    return Mat3{at(0, 0), at(1, 0), at(2, 0),
                at(0, 1), at(1, 1), at(2, 1),
                at(0, 2), at(1, 2), at(2, 2)};
}

Mat3 Mat3::normalized() const {
    const double s = at(2, 2);
    if (std::abs(s) < 1e-12) return *this;
    return (*this) * (1.0 / s);
}

bool Mat3::nearlyEquals(const Mat3& o, double eps) const {
    for (size_t i = 0; i < 9; ++i) {
        if (std::abs(m[i] - o.m[i]) > eps) return false;
    }
    return true;
}

bool Mat3::isFinite() const {
    for (size_t i = 0; i < 9; ++i) {
        if (!std::isfinite(m[i])) return false;
    }
    return true;
}

} // namespace hexmap

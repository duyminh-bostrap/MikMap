// ════════════════════════════════════════════════════════════════════════
//  core/math/Vec2.h — điểm/vector 2D
//
//  POD, constexpr, không cấp phát. Dùng double (không phải float) vì
//  chuỗi biến đổi toạ độ có tới 3 phép nhân ma trận nối tiếp
//  (H_s → slice.inverse → inputRect); sai số float tích luỹ đủ để
//  lệch vài pixel trên máy chiếu 4K.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <cmath>

namespace mikmap {

struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    constexpr Vec2() = default;
    constexpr Vec2(double x_, double y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(const Vec2& o) const { return {x + o.x, y + o.y}; }
    constexpr Vec2 operator-(const Vec2& o) const { return {x - o.x, y - o.y}; }
    constexpr Vec2 operator*(double s)      const { return {x * s, y * s}; }
    constexpr Vec2 operator/(double s)      const { return {x / s, y / s}; }
    constexpr Vec2 operator-()              const { return {-x, -y}; }

    constexpr Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    constexpr Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    constexpr Vec2& operator*=(double s)      { x *= s;   y *= s;   return *this; }

    constexpr double dot(const Vec2& o)   const { return x * o.x + y * o.y; }
    constexpr double cross(const Vec2& o) const { return x * o.y - y * o.x; }

    double length()  const { return std::sqrt(x * x + y * y); }
    constexpr double lengthSq() const { return x * x + y * y; }

    double distanceTo(const Vec2& o) const { return (*this - o).length(); }
    constexpr double distanceSqTo(const Vec2& o) const { return (*this - o).lengthSq(); }

    Vec2 normalized() const {
        const double len = length();
        return (len > 1e-12) ? Vec2{x / len, y / len} : Vec2{0.0, 0.0};
    }

    /// So sánh gần đúng — dùng trong test và kiểm tra hội tụ.
    bool nearlyEquals(const Vec2& o, double eps = 1e-9) const {
        return std::abs(x - o.x) <= eps && std::abs(y - o.y) <= eps;
    }

    bool isFinite() const {
        return std::isfinite(x) && std::isfinite(y);
    }
};

constexpr Vec2 operator*(double s, const Vec2& v) { return {v.x * s, v.y * s}; }

/// Nội suy tuyến tính.
constexpr Vec2 lerp(const Vec2& a, const Vec2& b, double t) {
    return {a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t};
}

/// Nội suy song tuyến tính trên một tứ giác.
/// Thứ tự đỉnh: p00 = (u=0,v=0), p10 = (1,0), p11 = (1,1), p01 = (0,1)
/// — cùng chiều kim đồng hồ từ góc trên-trái, khớp với thứ tự corner
/// pin của Resolume và của WarpMesh.
constexpr Vec2 bilerp(const Vec2& p00, const Vec2& p10,
                      const Vec2& p11, const Vec2& p01,
                      double u, double v) {
    const Vec2 top    = lerp(p00, p10, u);
    const Vec2 bottom = lerp(p01, p11, u);
    return lerp(top, bottom, v);
}

} // namespace mikmap

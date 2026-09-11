#pragma once

#include <cmath>

namespace HexMap::Core::Math {

struct Vec2 {
    double x = 0.0;
    double y = 0.0;

    constexpr Vec2() = default;
    constexpr Vec2(double x_, double y_) : x(x_), y(y_) {}

    constexpr Vec2 operator+(const Vec2& o) const { return { x + o.x, y + o.y }; }
    constexpr Vec2 operator-(const Vec2& o) const { return { x - o.x, y - o.y }; }
    constexpr Vec2 operator*(double s) const { return { x * s, y * s }; }
    constexpr Vec2 operator/(double s) const { return { x / s, y / s }; }

    Vec2& operator+=(const Vec2& o) { x += o.x; y += o.y; return *this; }
    Vec2& operator-=(const Vec2& o) { x -= o.x; y -= o.y; return *this; }
    Vec2& operator*=(double s) { x *= s; y *= s; return *this; }

    constexpr bool operator==(const Vec2& o) const {
        return std::abs(x - o.x) < 1e-9 && std::abs(y - o.y) < 1e-9;
    }

    double length() const { return std::hypot(x, y); }
    double lengthSquared() const { return x * x + y * y; }

    Vec2 normalized() const {
        double len = length();
        return (len > 1e-9) ? (*this / len) : Vec2(0, 0);
    }
};

} // namespace HexMap::Core::Math

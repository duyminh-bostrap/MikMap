#include "core/calib/LidarPlacement.h"

#include <cmath>

namespace mikmap {

namespace {
constexpr double kPi = 3.14159265358979323846;
struct Axes { double fx, fy, lx, ly; };
// F = phía trước, L = phía trái của sensor (đổi dấu khi mirror).
Axes axes(const LidarPose& p) {
    const double a = p.angleDeg * kPi / 180.0, c = std::cos(a), s = std::sin(a);
    return p.mirror ? Axes{c, s, -s, c} : Axes{c, s, s, -c};
}
}  // namespace

Vec2 lidarToSurface(const LidarPose& p, const Vec2& v) {
    const Axes k = axes(p);
    return Vec2{p.x + v.x * k.fx + v.y * k.lx, p.y + v.x * k.fy + v.y * k.ly};
}

Vec2 surfaceToLidar(const LidarPose& p, const Vec2& s) {
    const Axes k = axes(p);
    const double dx = s.x - p.x, dy = s.y - p.y;
    return Vec2{dx * k.fx + dy * k.fy, dx * k.lx + dy * k.ly};
}

}  // namespace mikmap

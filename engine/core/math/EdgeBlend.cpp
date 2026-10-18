#include "core/math/EdgeBlend.h"

#include <algorithm>
#include <cmath>

namespace mikmap::edgeblend {

bool overlapBand(const Rect& self, const Rect& other, Band& out) {
    const double x0 = std::max(self.x, other.x), x1 = std::min(self.x + self.w, other.x + other.w);
    const double y0 = std::max(self.y, other.y), y1 = std::min(self.y + self.h, other.y + other.h);
    const Rect ov{x0, y0, x1 - x0, y1 - y0};
    if (ov.w <= 0 || ov.h <= 0) return false;

    // Dốc theo trục mà vùng chồng chỉ phủ MỘT PHẦN cả hai rect; cả hai trục đều một phần → trục ngắn hơn.
    const bool partX = ov.w < self.w && ov.w < other.w;
    const bool partY = ov.h < self.h && ov.h < other.h;
    if (!partX && !partY) return false;
    if (partX && (!partY || ov.w <= ov.h)) {  // dốc theo x
        out.side = (ov.x + ov.w / 2 > self.x + self.w / 2) ? Side::Right : Side::Left;
        out.start = x0; out.end = x1;
    } else {             // dốc theo y
        out.side = (ov.y + ov.h / 2 > self.y + self.h / 2) ? Side::Bottom : Side::Top;
        out.start = y0; out.end = y1;
    }
    out.area = ov;
    return true;
}

double bandT(const Band& b, double x, double y) {
    const double len = b.end - b.start;
    double t = 0;
    switch (b.side) {
        case Side::Left:   t = (x - b.start) / len; break;
        case Side::Right:  t = (b.end - x) / len;   break;
        case Side::Top:    t = (y - b.start) / len; break;
        case Side::Bottom: t = (b.end - y) / len;   break;
    }
    return std::clamp(t, 0.0, 1.0);
}

double ramp(double t, double luminance, double power) {
    t = std::clamp(t, 0.0, 1.0);
    const double a = std::clamp(luminance, 0.0, 1.0);
    const double p = std::max(power, 0.01);
    return t < 0.5 ? a * std::pow(2 * t, p) : 1 - (1 - a) * std::pow(2 * (1 - t), p);
}

double gammaRamp(double t, double luminance, double power, double gamma) {
    if (!std::isfinite(gamma) || gamma <= 0) gamma = 1;
    return std::pow(ramp(t, luminance, power), 1 / gamma);
}

} // namespace mikmap::edgeblend

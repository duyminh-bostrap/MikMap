#include "core/math/Snapping.h"

#include <cmath>

namespace hexmap {
namespace {

/// Đường gióng gần `v` nhất trong ngưỡng.
/// @return false nếu không có đường nào đủ gần.
bool nearestGuide(double v, const std::vector<double>& guides,
                  double threshold, double& out) {
    bool found = false;
    double bestDist = threshold;

    for (const double g : guides) {
        // Bỏ qua giá trị vô nghĩa: một NaN lọt vào danh sách sẽ làm mọi
        // so sánh trả về false và người dùng thấy hút "thỉnh thoảng chết"
        // mà không có cách nào lần ra.
        if (!std::isfinite(g)) continue;

        const double d = std::abs(v - g);

        // `<=` chứ không phải `<`: khi hai đường cách đều, lấy đường đứng
        // SAU trong danh sách là tuỳ tiện — nhưng ít nhất phải nhất quán,
        // và trường hợp thường gặp là hai mốc TRÙNG NHAU (mép slice nằm
        // đúng mép màn), lúc đó chọn cái nào cũng ra cùng một kết quả.
        if (d <= bestDist) {
            bestDist = d;
            out = g;
            found = true;
        }
    }
    return found;
}

} // namespace

SnapResult snapPoint(const Vec2& p,
                     const std::vector<double>& guidesX,
                     const std::vector<double>& guidesY,
                     double threshold) {
    SnapResult r;
    r.position = p;

    if (!(threshold > 0.0) || !std::isfinite(p.x) || !std::isfinite(p.y)) {
        return r;
    }

    // ★ Hai trục xét ĐỘC LẬP. Chỉ hút khi cả hai cùng khớp thì tính năng
    //   gần như không bao giờ kích hoạt — người dùng hay muốn "thẳng cột
    //   với góc kia" mà chiều còn lại thì tuỳ ý.
    double g = 0.0;
    if (nearestGuide(p.x, guidesX, threshold, g)) {
        r.position.x = g;
        r.guideX     = g;
        r.snappedX   = true;
    }
    if (nearestGuide(p.y, guidesY, threshold, g)) {
        r.position.y = g;
        r.guideY     = g;
        r.snappedY   = true;
    }
    return r;
}

double snapThresholdFor(double pixels, double zoom) {
    if (!(zoom > 0.0) || !std::isfinite(zoom) || !std::isfinite(pixels)) return 0.0;
    if (!(pixels > 0.0)) return 0.0;
    return pixels / zoom;
}

} // namespace hexmap

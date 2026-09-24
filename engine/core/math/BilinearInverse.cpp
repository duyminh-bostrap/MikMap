#include "core/math/BilinearInverse.h"

#include <algorithm>
#include <cmath>

namespace mikmap {
namespace {

/// Tính u từ v đã biết, dùng thành phần có mẫu số lớn hơn để ổn định số học.
///
///   Hx = u·Ex + v·Fx + u·v·Gx   ⇒   u = (Hx − v·Fx) / (Ex + v·Gx)
///
/// Nếu mẫu số theo x gần 0 (cạnh gần như thẳng đứng), dùng y thay thế.
bool solveUFromV(const Vec2& E, const Vec2& F, const Vec2& G, const Vec2& H,
                 double v, double& outU) {
    const double denomX = E.x + v * G.x;
    const double denomY = E.y + v * G.y;

    if (std::abs(denomX) >= std::abs(denomY)) {
        if (std::abs(denomX) < 1e-12) return false;
        outU = (H.x - v * F.x) / denomX;
    } else {
        if (std::abs(denomY) < 1e-12) return false;
        outU = (H.y - v * F.y) / denomY;
    }
    return true;
}

bool inRange(double t, double tol) {
    return t >= -tol && t <= 1.0 + tol;
}

/// Diện tích có dấu ×2 của tam giác.
double cross3(const Vec2& a, const Vec2& b, const Vec2& c) {
    return (b - a).cross(c - a);
}

} // namespace

bool invertBilinear(const Vec2& p00, const Vec2& p10,
                    const Vec2& p11, const Vec2& p01,
                    const Vec2& P,
                    Vec2& outUV,
                    double tolerance) {
    const Vec2 E = p10 - p00;                    // hướng u
    const Vec2 F = p01 - p00;                    // hướng v
    const Vec2 G = p00 - p10 + p11 - p01;        // độ xoắn của ô
    const Vec2 H = P - p00;

    const double k2 = G.cross(F);
    const double k1 = E.cross(F) + H.cross(G);
    const double k0 = H.cross(E);

    // ── Trường hợp suy biến: ô là hình bình hành (G ≈ 0) ────────────────
    // k₂ = 0 ⇒ phương trình bậc nhất. Đây KHÔNG phải trường hợp hiếm:
    // một mesh chưa bị kéo méo thì mọi ô đều là hình chữ nhật.
    if (std::abs(k2) < 1e-12) {
        if (std::abs(k1) < 1e-12) return false;   // ô suy biến hoàn toàn

        const double v = -k0 / k1;
        if (!inRange(v, tolerance)) return false;

        double u = 0.0;
        if (!solveUFromV(E, F, G, H, v, u)) return false;
        if (!inRange(u, tolerance)) return false;

        outUV = {std::clamp(u, 0.0, 1.0), std::clamp(v, 0.0, 1.0)};
        return true;
    }

    // ── Trường hợp chung: phương trình bậc hai ─────────────────────────
    const double disc = k1 * k1 - 4.0 * k2 * k0;
    if (disc < 0.0) return false;                 // không có ảnh ngược thực

    const double sqrtDisc = std::sqrt(disc);

    // Dạng ổn định số học của công thức nghiệm: tránh triệt tiêu khi
    // k1 và sqrtDisc gần bằng nhau (mất chữ số có nghĩa).
    const double q  = -0.5 * (k1 + (k1 >= 0.0 ? sqrtDisc : -sqrtDisc));
    double v1 = (std::abs(k2) > 1e-300) ? q / k2 : 0.0;
    double v2 = (std::abs(q)  > 1e-300) ? k0 / q : v1;

    // Thử cả hai nghiệm; giữ nghiệm cho (u,v) đều nằm trong [0,1].
    // Một tứ giác cong có thể có hai ảnh ngược về mặt toán học, nhưng
    // chỉ một nằm trong ô.
    for (double v : {v1, v2}) {
        if (!std::isfinite(v) || !inRange(v, tolerance)) continue;

        double u = 0.0;
        if (!solveUFromV(E, F, G, H, v, u)) continue;
        if (!inRange(u, tolerance)) continue;

        outUV = {std::clamp(u, 0.0, 1.0), std::clamp(v, 0.0, 1.0)};
        return true;
    }

    return false;
}

bool pointInQuad(const Vec2& p00, const Vec2& p10,
                 const Vec2& p11, const Vec2& p01,
                 const Vec2& P) {
    // Chia tứ giác thành 2 tam giác theo đường chéo p00–p11.
    // Cách này đúng cả với tứ giác lõm, khác với kiểm tra nửa mặt phẳng.
    //
    // ★ Phép thử dấu PHẢI có dung sai, và dung sai phải theo TỈ LỆ.
    //
    //   Điểm nằm đúng trên đường chéo p00–p11 cho tích có hướng bằng 0 về
    //   mặt toán học, nhưng dấu phẩy động trả về một số nhiễu cỡ 1e-13 —
    //   có thể ÂM trong khi hai số kia dương. So dấu chặt với 0 thì cả hai
    //   tam giác đều nói "điểm nằm ngoài", và tứ giác từ chối một điểm
    //   nằm chính giữa nó.
    //
    //   Đây không phải chuyện lý thuyết: tâm của một ô hình bình hành nằm
    //   ĐÚNG trên đường chéo, mà lưới warp phẳng hoặc uốn đều thì mọi ô
    //   đều là hình bình hành. Hậu quả trong buổi diễn là một VỆT CHẠM
    //   CHẾT chạy chéo qua từng ô lưới: chạm vào đúng đó thì không có
    //   phản ứng gì, còn lệch một pixel lại chạy bình thường.
    //
    //   Ngưỡng lấy theo độ lớn của chính tam giác vì tích có hướng có đơn
    //   vị DIỆN TÍCH: một hằng số tuyệt đối sẽ quá chặt với lưới toạ độ
    //   lớn và quá lỏng với lưới nhỏ.
    auto inTriangle = [&](const Vec2& a, const Vec2& b, const Vec2& c) {
        const double d1 = cross3(a, b, P);
        const double d2 = cross3(b, c, P);
        const double d3 = cross3(c, a, P);

        const double scale = std::max({std::abs(d1), std::abs(d2), std::abs(d3)});
        if (!(scale > 0.0)) return false;   // tam giác suy biến

        const double eps = scale * 1e-9;
        const bool hasNeg = (d1 < -eps) || (d2 < -eps) || (d3 < -eps);
        const bool hasPos = (d1 >  eps) || (d2 >  eps) || (d3 >  eps);
        return !(hasNeg && hasPos);   // cùng dấu ⇒ nằm trong
    };

    return inTriangle(p00, p10, p11) || inTriangle(p00, p11, p01);
}

} // namespace mikmap

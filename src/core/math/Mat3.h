// ════════════════════════════════════════════════════════════════════════
//  core/math/Mat3.h — ma trận thuần nhất 3×3
//
//  Đây là kiểu dữ liệu trung tâm của toàn bộ hệ thống mapping:
//    · H_w — corner pin của slice (content → output)
//    · H_s — calibration của sensor (sensor → output)
//    · Công thức lõi:  p_content = H_w⁻¹ · H_s · p_sensor
//
//  Lưu trữ ROW-MAJOR:
//        ┌ m[0]  m[1]  m[2] ┐
//        │ m[3]  m[4]  m[5] │        m[r*3 + c]
//        └ m[6]  m[7]  m[8] ┘
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"

#include <array>

namespace hexmap {

class Mat3 {
public:
    std::array<double, 9> m{};

    // ── Khởi tạo ───────────────────────────────────────────────────────
    constexpr Mat3() : m{1, 0, 0,
                         0, 1, 0,
                         0, 0, 1} {}

    constexpr Mat3(double m00, double m01, double m02,
                   double m10, double m11, double m12,
                   double m20, double m21, double m22)
        : m{m00, m01, m02, m10, m11, m12, m20, m21, m22} {}

    static constexpr Mat3 identity() { return Mat3{}; }
    static constexpr Mat3 zero() {
        return Mat3{0, 0, 0, 0, 0, 0, 0, 0, 0};
    }

    static constexpr Mat3 translation(double tx, double ty) {
        return Mat3{1, 0, tx,
                    0, 1, ty,
                    0, 0, 1};
    }

    static constexpr Mat3 scaling(double sx, double sy) {
        return Mat3{sx, 0,  0,
                    0,  sy, 0,
                    0,  0,  1};
    }

    static Mat3 rotation(double radians);

    /// Xoay quanh một điểm tuỳ ý (anchor).
    static Mat3 rotationAround(double radians, const Vec2& anchor);

    // ── Truy cập ───────────────────────────────────────────────────────
    constexpr double  at(int r, int c) const { return m[static_cast<size_t>(r) * 3 + static_cast<size_t>(c)]; }
    constexpr double& at(int r, int c)       { return m[static_cast<size_t>(r) * 3 + static_cast<size_t>(c)]; }

    // ── Phép toán ──────────────────────────────────────────────────────
    Mat3 operator*(const Mat3& o) const;
    Mat3 operator*(double s) const;
    Mat3 operator+(const Mat3& o) const;

    /// Biến đổi một điểm, có chia đồng nhất (perspective divide).
    /// Đây là điểm khác biệt cốt lõi giữa affine và homography:
    /// w ≠ 1 chính là thứ tạo ra hiệu ứng phối cảnh của keystone.
    Vec2 transformPoint(const Vec2& p) const;

    /// Biến đổi vector chỉ phương — bỏ qua phần tịnh tiến.
    Vec2 transformDirection(const Vec2& v) const;

    double determinant() const;

    /// Nghịch đảo. Trả về false nếu ma trận suy biến (det ≈ 0).
    /// ⚠️ Luôn kiểm tra giá trị trả về: một corner pin bị kéo thành
    ///    hình suy biến (3 điểm thẳng hàng) sẽ không nghịch đảo được,
    ///    và khi đó calibration không thể hoạt động.
    bool invert(Mat3& out) const;

    /// Nghịch đảo, trả về identity nếu suy biến. Tiện cho hot path
    /// nhưng che giấu lỗi — chỉ dùng khi đã kiểm tra isInvertible().
    Mat3 inverted() const;

    bool isInvertible(double eps = 1e-12) const;

    Mat3 transposed() const;

    /// Chuẩn hoá sao cho m[8] == 1. Homography được định nghĩa sai khác
    /// một hằng số tỉ lệ, nên bước này giúp so sánh hai ma trận với nhau.
    Mat3 normalized() const;

    bool nearlyEquals(const Mat3& o, double eps = 1e-9) const;
    bool isFinite() const;
};

} // namespace hexmap

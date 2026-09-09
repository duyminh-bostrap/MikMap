#include "core/model/Transform2D.h"

#include <cmath>

namespace hexmap {

Mat3 Transform2D::toMatrix(const Vec2& contentSize) const {
    // Anchor theo tỉ lệ → px, để phép xoay/co giãn quay quanh đúng điểm
    // ngay cả khi độ phân giải nội dung thay đổi.
    const Vec2 anchorPx{anchor.x * contentSize.x, anchor.y * contentSize.y};

    const double sx = scale.x * (flipH ? -1.0 : 1.0);
    const double sy = scale.y * (flipV ? -1.0 : 1.0);

    return Mat3::translation(position.x, position.y)
         * Mat3::translation(anchorPx.x, anchorPx.y)
         * Mat3::rotation(rotation)
         * Mat3::scaling(sx, sy)
         * Mat3::translation(-anchorPx.x, -anchorPx.y);
}

bool Transform2D::toInverseMatrix(const Vec2& contentSize, Mat3& out) const {
    // Scale = 0 làm ma trận suy biến. Đây KHÔNG phải trường hợp hiếm:
    // người dùng kéo slider scale về 0 là chuyện thường. Phải báo lỗi
    // thay vì trả về ma trận rác.
    if (std::abs(scale.x) < 1e-12 || std::abs(scale.y) < 1e-12) return false;

    return toMatrix(contentSize).invert(out);
}

bool Transform2D::isIdentity() const {
    constexpr double eps = 1e-12;
    return std::abs(position.x) < eps && std::abs(position.y) < eps
        && std::abs(scale.x - 1.0) < eps && std::abs(scale.y - 1.0) < eps
        && std::abs(rotation) < eps
        && !flipH && !flipV;
}

} // namespace hexmap

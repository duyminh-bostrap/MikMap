// ════════════════════════════════════════════════════════════════════════
//  core/model/Transform2D.h — biến đổi vị trí/tỉ lệ/xoay của clip
//  (D1 D2 D5 D6 D8)
//
//  Áp dụng trong không gian COMPOSITION CANVAS, trước khi slice lấy vùng.
//  Đây là tầng "clip nằm ở đâu trên canvas", tách biệt hoàn toàn với tầng
//  "canvas được chiếu lên đâu trên máy chiếu" (IWarp).
//
//  Thứ tự phép biến đổi (đọc từ phải sang trái khi nhân ma trận):
//      T(position) · T(anchor) · R(rotation) · S(scale·flip) · T(−anchor)
//
//  Anchor tính theo tỉ lệ [0,1] của kích thước nội dung, không phải pixel
//  — nhờ vậy đổi độ phân giải video không làm lệch tâm xoay.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Mat3.h"
#include "core/math/Vec2.h"

namespace mikmap {

struct Transform2D {
    Vec2   position{0.0, 0.0};    ///< px trên canvas
    Vec2   scale{1.0, 1.0};
    double rotation = 0.0;        ///< radian
    Vec2   anchor{0.5, 0.5};      ///< tỉ lệ [0,1], mặc định tâm
    bool   flipH = false;
    bool   flipV = false;

    /// Dựng ma trận biến đổi cho nội dung có kích thước contentSize (px).
    Mat3 toMatrix(const Vec2& contentSize) const;

    /// Ma trận ngược — cần cho sensor: từ toạ độ canvas suy ra pixel nào
    /// của clip đang bị chạm.
    /// @return false nếu scale bằng 0 theo một trục (không nghịch đảo được).
    bool toInverseMatrix(const Vec2& contentSize, Mat3& out) const;

    bool isIdentity() const;
    void reset() { *this = Transform2D{}; }
};

} // namespace mikmap

// ════════════════════════════════════════════════════════════════════════
//  core/model/WarpCornerPin.h — keystone 4 điểm (F5)
//
//  Kiểu warp cơ bản nhất và cũng hay dùng nhất: kéo 4 góc để khớp hình
//  vào một bề mặt phẳng nhìn nghiêng.
//
//  Bên trong là một homography 3×3, nên:
//    · forward / inverse đều có nghiệm dạng đóng — rất nhanh
//    · GPU nội suy phối cảnh CHÍNH XÁC với chỉ 2 tam giác, không cần
//      chia nhỏ lưới (khác hẳn WarpMesh)
//
//  Thứ tự góc:  0 = trên-trái (u0,v0), 1 = trên-phải, 2 = dưới-phải,
//               3 = dưới-trái — theo chiều kim đồng hồ.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Mat3.h"
#include "core/model/IWarp.h"

namespace hexmap {

class WarpCornerPin final : public IWarp {
public:
    WarpCornerPin();
    explicit WarpCornerPin(const Vec2 corners[4]);

    // ── IWarp ──────────────────────────────────────────────────────────
    WarpType type() const override { return WarpType::CornerPin; }

    Vec2 forward(const Vec2& contentUV) const override;
    bool inverse(const Vec2& outputPx, Vec2& outContentUV) const override;
    bool isInvertible() const override { return m_valid; }

    void tessellate(int cols, int rows, WarpGeometry& out) const override;

    /// Corner pin chỉ cần 1 ô: nội suy phối cảnh của GPU đã chính xác
    /// tuyệt đối cho homography. Chia nhỏ thêm chỉ tốn vertex vô ích.
    int defaultSubdivisions() const override { return 1; }

    int  controlPointCount() const override { return 4; }
    Vec2 controlPointAt(int index) const override;
    bool setControlPointAt(int index, const Vec2& p) override;

    std::unique_ptr<IWarp> clone() const override;
    void resetToRect(const Vec2& topLeft, const Vec2& size) override;
    void boundingBox(Vec2& outMin, Vec2& outMax) const override;

    // ── Riêng của corner pin ───────────────────────────────────────────
    const Vec2& corner(int i) const { return m_corners[i]; }

    /// Đặt lại một góc. Tự tính lại ma trận.
    /// @return false nếu vị trí mới làm tứ giác suy biến — KHI ĐÓ GÓC
    ///         KHÔNG BỊ THAY ĐỔI, để UI không bao giờ rơi vào trạng thái hỏng.
    bool setCorner(int i, const Vec2& p);

    void setCorners(const Vec2 corners[4]);

    const Mat3& matrix() const { return m_toOutput; }     ///< content → output
    const Mat3& inverseMatrix() const { return m_toContent; } ///< output → content

private:
    void rebuild();

    Vec2 m_corners[4]{};
    Mat3 m_toOutput;      ///< H_w
    Mat3 m_toContent;     ///< H_w⁻¹ — tính trước, vì sensor dùng mỗi frame
    bool m_valid = false;
};

} // namespace hexmap

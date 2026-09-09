// ════════════════════════════════════════════════════════════════════════
//  core/model/WarpMesh.h — biến dạng lưới N×M (F9)
//
//  Dùng cho bề mặt CONG mà corner pin không khớp được: cột tròn, vòm,
//  tượng, vải rủ. Người dùng kéo từng điểm điều khiển trên lưới.
//
//  ── Vì sao mesh khó hơn corner pin nhiều ─────────────────────────────
//  Corner pin là MỘT homography → nghịch đảo bằng một phép nghịch đảo
//  ma trận. Mesh là (cols×rows) ô ĐỘC LẬP, mỗi ô nội suy song tuyến tính.
//  Nội suy song tuyến tính KHÔNG phải phép biến đổi tuyến tính, nên
//  không có ma trận nghịch đảo nào cả.
//
//  Nghịch đảo phải làm hai bước:
//    1. Tìm ô nào chứa điểm output      (lọc bằng hộp bao + point-in-quad)
//    2. Giải nghịch đảo song tuyến tính trong ô đó  (phương trình bậc 2)
//    3. Ghép (u,v) cục bộ của ô về (u,v) toàn cục của lưới
//
//  Bước 1 là phần tốn kém. Hiện duyệt tuyến tính kèm lọc hộp bao — đủ
//  nhanh cho lưới thường gặp (≤ 16×16 = 256 ô, ~vài µs mỗi điểm chạm).
//  Nếu sau này dùng lưới dày hơn, thay bằng spatial hash: xem ghi chú
//  trong findCell().
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/model/IWarp.h"

#include <vector>

namespace hexmap {

class WarpMesh final : public IWarp {
public:
    WarpMesh();
    WarpMesh(int cols, int rows, const Vec2& topLeft, const Vec2& size);

    // ── IWarp ──────────────────────────────────────────────────────────
    WarpType type() const override { return WarpType::Mesh; }

    Vec2 forward(const Vec2& contentUV) const override;
    bool inverse(const Vec2& outputPx, Vec2& outContentUV) const override;
    bool isInvertible() const override { return m_cols > 0 && m_rows > 0; }

    void tessellate(int cols, int rows, WarpGeometry& out) const override;

    /// Mesh PHẢI chia nhỏ để bề mặt cong trông mượt — mặc định trùng với
    /// mật độ lưới điều khiển.
    int defaultSubdivisions() const override { return m_cols; }

    int  controlPointCount() const override {
        return (m_cols + 1) * (m_rows + 1);
    }
    Vec2 controlPointAt(int index) const override;
    bool setControlPointAt(int index, const Vec2& p) override;

    std::unique_ptr<IWarp> clone() const override;
    void resetToRect(const Vec2& topLeft, const Vec2& size) override;
    void boundingBox(Vec2& outMin, Vec2& outMax) const override;

    // ── Riêng của mesh ─────────────────────────────────────────────────
    int cols() const { return m_cols; }
    int rows() const { return m_rows; }

    /// Điểm điều khiển tại (cx, cy), với cx ∈ [0..cols], cy ∈ [0..rows].
    const Vec2& controlPoint(int cx, int cy) const;
    void setControlPoint(int cx, int cy, const Vec2& p);

    /// Đổi mật độ lưới, GIỮ NGUYÊN hình dạng đã kéo bằng cách lấy mẫu
    /// lại bề mặt hiện tại. Nếu không làm vậy, người dùng tăng subdivision
    /// sẽ mất toàn bộ công căn chỉnh — lỗi UX rất khó chịu.
    void resize(int newCols, int newRows);

private:
    /// Tìm ô lưới chứa điểm output.
    /// @return false nếu điểm nằm ngoài toàn bộ lưới.
    bool findCell(const Vec2& p, int& outCx, int& outCy) const;

    size_t index(int cx, int cy) const {
        return static_cast<size_t>(cy) * static_cast<size_t>(m_cols + 1)
             + static_cast<size_t>(cx);
    }

    int m_cols = 1;
    int m_rows = 1;
    std::vector<Vec2> m_points;   ///< (cols+1) × (rows+1)
};

} // namespace hexmap

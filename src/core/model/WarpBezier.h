// ════════════════════════════════════════════════════════════════════════
//  core/model/WarpBezier.h — mặt Bézier song bậc ba 4×4 (F10)
//
//  Dùng cho bề mặt CONG THẬT: cột tròn, vòm, tượng, vải rủ. Khác với
//  WarpMesh ở chỗ bề mặt LIÊN TỤC chứ không phải ghép từ các ô phẳng.
//
//  ── Vì sao cần cả hai, mesh đã có rồi ────────────────────────────────
//  Mesh nội suy song tuyến tính TỪNG Ô, nên bề mặt bị gãy khúc ở biên ô:
//  đạo hàm không liên tục. Chiếu lên cột tròn thì mắt thấy rõ các vệt gấp
//  chạy dọc theo đường lưới — muốn giấu đi phải tăng mật độ lưới lên rất
//  cao, và khi đó người vận hành phải kéo hàng trăm điểm.
//
//  Mặt Bézier cho đúng thứ ngược lại: 16 điểm điều khiển tạo ra một bề
//  mặt trơn tuyệt đối (liên tục tới đạo hàm mọi cấp). Cong đều thì kéo
//  vài điểm là xong.
//
//  ── Nghịch đảo: vì sao phải làm hai bước ─────────────────────────────
//  Bézier bậc ba theo cả hai chiều KHÔNG có nghịch đảo dạng đóng (giải
//  tích sẽ ra hệ hai phương trình bậc 6). Nên:
//
//    1. Đoán thô  — lấy mẫu bề mặt thành lưới nhỏ rồi nghịch đảo song
//                   tuyến tính trong ô chứa điểm (dùng lại đúng bộ máy
//                   đã có của mesh: pointInQuad + invertBilinear).
//    2. Nắn Newton — lặp trên phương trình S(u,v) − P = 0 với Jacobian
//                   giải tích. Hội tụ bậc hai, vài vòng là tới độ chính
//                   xác của double.
//
//  Bước 2 mới là thứ khiến nghịch đảo này CHÍNH XÁC chứ không xấp xỉ —
//  và độ chính xác ở đây không phải chuyện làm đẹp: nó là đường đi của
//  điểm chạm sensor về toạ độ nội dung (architecture.md §10.3).
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/model/IWarp.h"

#include <array>

namespace hexmap {

class WarpBezier final : public IWarp {
public:
    /// Số điểm điều khiển mỗi chiều. Bậc ba ⇒ 4.
    static constexpr int kDim = 4;
    static constexpr int kPointCount = kDim * kDim;   // 16

    WarpBezier();
    WarpBezier(const Vec2& topLeft, const Vec2& size);

    // ── IWarp ──────────────────────────────────────────────────────────
    WarpType type() const override { return WarpType::Bezier; }

    Vec2 forward(const Vec2& contentUV) const override;
    bool inverse(const Vec2& outputPx, Vec2& outContentUV) const override;
    bool isInvertible() const override;

    void tessellate(int cols, int rows, WarpGeometry& out) const override;

    /// Bề mặt cong cần chia đủ mịn thì mới hết gãy khúc trên màn hình.
    /// 16×16 là mức mắt không còn phân biệt được ở khoảng cách sân khấu.
    int defaultSubdivisions() const override { return 16; }

    int  controlPointCount() const override { return kPointCount; }
    Vec2 controlPointAt(int index) const override;

    /// ★ KHÔNG từ chối như corner pin.
    ///
    ///   Corner pin từ chối được vì nó chỉ có 4 điểm và điều kiện lồi là
    ///   cục bộ, kiểm ngay được. Mặt Bézier thì người dùng thường phải đi
    ///   QUA những trạng thái gấp mép trên đường tới hình mong muốn — chặn
    ///   giữa chừng làm việc căn chỉnh gần như bất khả thi.
    ///
    ///   Giống WarpMesh: nhận mọi vị trí, rồi `isInvertible()` báo trạng
    ///   thái để UI hiện cảnh báo đỏ (đã có sẵn ở bảng thuộc tính slice).
    bool setControlPointAt(int index, const Vec2& p) override;

    std::unique_ptr<IWarp> clone() const override;
    void resetToRect(const Vec2& topLeft, const Vec2& size) override;
    void boundingBox(Vec2& outMin, Vec2& outMax) const override;

    // ── Riêng của Bézier ───────────────────────────────────────────────

    /// Điểm điều khiển tại (cx, cy), cx/cy ∈ [0..3].
    const Vec2& controlPoint(int cx, int cy) const;
    void setControlPoint(int cx, int cy, const Vec2& p);

    /// Đạo hàm riêng của bề mặt — dùng cho Newton và cho kiểm tra gấp mép.
    Vec2 derivativeU(const Vec2& uv) const;
    Vec2 derivativeV(const Vec2& uv) const;

    /// Dựng mặt Bézier khớp với một lưới điều khiển đang có, để đổi từ
    /// mesh sang bezier mà KHÔNG mất công căn chỉnh đã làm.
    ///
    /// Lấy mẫu bề mặt nguồn tại 4×4 điểm rồi đặt làm điểm điều khiển. Đây
    /// là xấp xỉ, không phải khớp chính xác — nhưng giữ được đúng bốn góc
    /// và hình dạng tổng thể, tức là giữ được phần công sức đắt nhất.
    static WarpBezier fromWarp(const IWarp& src);

private:
    static int idx(int cx, int cy) { return cy * kDim + cx; }

    std::array<Vec2, kPointCount> m_points{};
};

} // namespace hexmap

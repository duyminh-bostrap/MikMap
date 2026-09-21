// ════════════════════════════════════════════════════════════════════════
//  core/filter/OneEuroFilter.h — khử nhiễu KHÔNG thêm độ trễ (G11)
//
//  ── Bài toán ─────────────────────────────────────────────────────────
//  Dữ liệu sensor thô luôn rung. Lọc trung bình trượt làm mượt được,
//  nhưng thêm độ trễ tỉ lệ với cửa sổ lọc — và trong hệ tương tác, độ
//  trễ là thứ người dùng cảm nhận rõ nhất. Lọc mạnh thì "trôi theo tay",
//  lọc nhẹ thì con trỏ nhảy loạn khi đứng yên.
//
//  ── Ý tưởng của One Euro ─────────────────────────────────────────────
//  Đánh đổi có ĐIỀU KIỆN theo tốc độ:
//    · Chậm / đứng yên → lọc MẠNH. Rung bị dập, độ trễ không ai thấy
//                        vì có di chuyển đâu mà thấy.
//    · Nhanh           → lọc NHẸ. Bám sát tay, nhiễu bị lu mờ bởi
//                        chính chuyển động.
//
//  Cài đặt là bộ lọc thông thấp bậc một có tần số cắt thay đổi theo
//  vận tốc ước lượng:
//
//      cutoff = minCutoff + beta · |vận tốc|
//
//  ── Chỉnh tham số ────────────────────────────────────────────────────
//    minCutoff ↓  → đứng yên mượt hơn, nhưng bắt đầu chuyển động chậm hơn
//    beta      ↑  → bám tay tốt hơn khi di chuyển nhanh, nhưng rung hơn
//  Cách chỉnh: đặt beta = 0, giảm minCutoff tới khi đứng yên hết rung.
//  Rồi tăng beta tới khi di chuyển nhanh không còn cảm giác trễ.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"

namespace hexmap {

/// Bộ lọc thông thấp bậc một — thành phần của One Euro.
class LowPassFilter {
public:
    /// @param alpha hệ số làm mượt (0,1]. Càng nhỏ càng mượt.
    double filter(double value, double alpha);

    bool   hasValue() const { return m_initialized; }
    double lastRaw() const { return m_lastRaw; }
    double lastFiltered() const { return m_lastFiltered; }
    void   reset() { m_initialized = false; }

private:
    bool   m_initialized = false;
    double m_lastRaw = 0.0;
    double m_lastFiltered = 0.0;
};

struct OneEuroParams {
    /// Tần số cắt khi đứng yên, Hz. Thấp hơn = mượt hơn.
    double minCutoff = 1.0;

    /// Mức nới lỏng bộ lọc theo tốc độ. Cao hơn = bám tay hơn.
    double beta = 0.007;

    /// Tần số cắt cho bộ lọc ước lượng vận tốc. Hiếm khi cần đổi.
    double dCutoff = 1.0;
};

/// Lọc một đại lượng vô hướng.
class OneEuroFilter {
public:
    explicit OneEuroFilter(OneEuroParams params = {}) : m_params(params) {}

    void setParams(const OneEuroParams& p) { m_params = p; }
    const OneEuroParams& params() const { return m_params; }

    /// @param value    giá trị thô
    /// @param timeSec  mốc thời gian đơn điệu, giây
    double filter(double value, double timeSec);

    void reset();

private:
    static double alphaFor(double cutoff, double dtSec);

    OneEuroParams m_params;
    LowPassFilter m_valueFilter;
    LowPassFilter m_speedFilter;
    double m_lastTimeSec = -1.0;
};

/// Lọc điểm 2D — hai bộ lọc độc lập cho x và y.
///
/// Lọc riêng từng trục là đúng ở đây: nhiễu sensor thường độc lập theo
/// trục (khung IR quét ngang/dọc riêng, depth camera nhiễu theo pixel).
class OneEuroFilter2D {
public:
    explicit OneEuroFilter2D(OneEuroParams params = {})
        : m_x(params), m_y(params) {}

    void setParams(const OneEuroParams& p) { m_x.setParams(p); m_y.setParams(p); }

    Vec2 filter(const Vec2& v, double timeSec) {
        return Vec2{m_x.filter(v.x, timeSec), m_y.filter(v.y, timeSec)};
    }

    void reset() { m_x.reset(); m_y.reset(); }

private:
    OneEuroFilter m_x;
    OneEuroFilter m_y;
};

} // namespace hexmap

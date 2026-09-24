#include "core/filter/OneEuroFilter.h"

#include <algorithm>
#include <cmath>

namespace mikmap {
namespace {
constexpr double kPi = 3.14159265358979323846;

/// dt tối thiểu. Hai mẫu đến cùng một mốc thời gian sẽ làm vận tốc
/// bằng vô cực; kẹp lại thay vì sinh ra NaN lan khắp hệ thống.
constexpr double kMinDt = 1e-6;
} // namespace

double LowPassFilter::filter(double value, double alpha) {
    alpha = std::clamp(alpha, 1e-9, 1.0);

    if (!m_initialized) {
        m_lastFiltered = value;
        m_initialized = true;
    } else {
        m_lastFiltered = alpha * value + (1.0 - alpha) * m_lastFiltered;
    }

    m_lastRaw = value;
    return m_lastFiltered;
}

double OneEuroFilter::alphaFor(double cutoff, double dtSec) {
    // tau = 1 / (2π·fc);  alpha = 1 / (1 + tau/dt)
    const double tau = 1.0 / (2.0 * kPi * std::max(1e-9, cutoff));
    return 1.0 / (1.0 + tau / std::max(kMinDt, dtSec));
}

double OneEuroFilter::filter(double value, double timeSec) {
    if (!std::isfinite(value)) return m_valueFilter.lastFiltered();

    // Mẫu đầu tiên: không có dt nên không ước lượng được vận tốc.
    // Trả về nguyên giá trị — bắt đầu bằng một giá trị đã lọc sẵn sẽ
    // tạo ra cú nhảy giả ở khung thứ hai.
    if (m_lastTimeSec < 0.0) {
        m_lastTimeSec = timeSec;
        m_speedFilter.filter(0.0, 1.0);
        return m_valueFilter.filter(value, 1.0);
    }

    const double dt = std::max(kMinDt, timeSec - m_lastTimeSec);
    m_lastTimeSec = timeSec;

    // ── Ước lượng vận tốc, có lọc ─────────────────────────────────────
    const double dValue = (value - m_valueFilter.lastFiltered()) / dt;
    const double edValue = m_speedFilter.filter(dValue, alphaFor(m_params.dCutoff, dt));

    // ── Tần số cắt thích nghi ─────────────────────────────────────────
    // Đây là toàn bộ tinh thần của One Euro: đi nhanh thì nới lỏng.
    const double cutoff = m_params.minCutoff + m_params.beta * std::abs(edValue);

    return m_valueFilter.filter(value, alphaFor(cutoff, dt));
}

void OneEuroFilter::reset() {
    m_valueFilter.reset();
    m_speedFilter.reset();
    m_lastTimeSec = -1.0;
}

} // namespace mikmap

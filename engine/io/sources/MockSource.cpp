#include "io/sources/MockSource.h"

#include "core/util/Clock.h"

#include <algorithm>
#include <chrono>
#include <cmath>

namespace mikmap {
namespace {
constexpr double kPi = 3.14159265358979323846;
} // namespace

MockSource::MockSource(MockConfig cfg)
    : m_cfg(cfg), m_rng(cfg.seed) {}

MockSource::~MockSource() {
    stop();
}

void MockSource::setConfig(const MockConfig& cfg) {
    m_cfg = cfg;
}

bool MockSource::start() {
    if (m_running.load()) return true;

    m_running.store(true);
    m_status.store(SourceStatus::Running);
    m_seq = 0;
    std::fill(std::begin(m_pointActive), std::end(m_pointActive), false);

    m_thread = std::thread(&MockSource::threadLoop, this);
    return true;
}

void MockSource::stop() {
    if (!m_running.exchange(false)) return;
    if (m_thread.joinable()) m_thread.join();
    m_status.store(SourceStatus::Stopped);
}

SensorFrame MockSource::generateFrame(double timeSec) {
    SensorFrame f;
    f.seq = ++m_seq;
    f.tCaptureNs = Clock::nowNs();
    f.sourceId = m_sourceId;

    const double cx = (m_cfg.minX + m_cfg.maxX) * 0.5;
    const double cy = (m_cfg.minY + m_cfg.maxY) * 0.5;
    const double rx = (m_cfg.maxX - m_cfg.minX) * 0.35;
    const double ry = (m_cfg.maxY - m_cfg.minY) * 0.35;

    // ⚠️ std::normal_distribution YÊU CẦU sigma > 0. Truyền 0 là hành vi
    // không xác định: MSVC bản Debug bung hộp thoại assert modal, và khi
    // stdout đã bị chuyển hướng thì tiến trình trông hệt như bị treo.
    // noiseSigma = 0 là giá trị MẶC ĐỊNH, nên bẫy này chắc chắn sẽ dính.
    // Luôn dựng distribution với sigma hợp lệ, rồi mới quyết định có dùng.
    const bool addNoise = (m_cfg.noiseSigma > 0.0);
    std::normal_distribution<double> noise(0.0, addNoise ? m_cfg.noiseSigma : 1.0);
    std::uniform_real_distribution<double> uni(0.0, 1.0);

    const int n = std::clamp(m_cfg.pointCount, 0, kMaxTouchPoints);

    for (int i = 0; i < n; ++i) {
        // Lệch pha giữa các điểm để chúng không chồng lên nhau.
        const double phase = (2.0 * kPi * i) / std::max(1, n);

        double x = cx;
        double y = cy;

        switch (m_cfg.pattern) {
        case MockPattern::Circle:
            x = cx + rx * std::cos(timeSec + phase);
            y = cy + ry * std::sin(timeSec + phase);
            break;

        case MockPattern::Lissajous:
            x = cx + rx * std::sin(3.0 * timeSec + phase);
            y = cy + ry * std::sin(2.0 * timeSec + phase * 1.5);
            break;

        case MockPattern::Static:
            // Rải đều trên lưới — tiện để chạm thử calibration.
            x = m_cfg.minX + (m_cfg.maxX - m_cfg.minX) * ((i % 4) + 0.5) / 4.0;
            y = m_cfg.minY + (m_cfg.maxY - m_cfg.minY) * ((i / 4) + 0.5) / 4.0;
            break;

        case MockPattern::Random:
            x = m_cfg.minX + (m_cfg.maxX - m_cfg.minX) * uni(m_rng);
            y = m_cfg.minY + (m_cfg.maxY - m_cfg.minY) * uni(m_rng);
            break;
        }

        if (addNoise) {
            x += noise(m_rng);
            y += noise(m_rng);
        }

        TouchPoint p;
        p.id = static_cast<uint32_t>(i + 1);
        p.x = static_cast<float>(x);
        p.y = static_cast<float>(y);
        p.z = 0.0f;
        p.confidence = 1.0f;
        p.state = m_pointActive[i] ? TouchState::Move : TouchState::Down;

        f.addPoint(p);
    }

    return f;
}

void MockSource::threadLoop() {
    using namespace std::chrono;

    const auto t0 = steady_clock::now();
    std::uniform_real_distribution<double> uni(0.0, 1.0);

    while (m_running.load(std::memory_order_relaxed)) {
        const double elapsed =
            duration_cast<duration<double>>(steady_clock::now() - t0).count();

        // Mô phỏng mất tín hiệu: bỏ hẳn frame. Render thread PHẢI chịu
        // được điều này mà không giật — nếu không, sensor thật chập chờn
        // sẽ làm hỏng show.
        const bool dropped =
            (m_cfg.dropoutRate > 0.0) && (uni(m_rng) < m_cfg.dropoutRate);

        if (!dropped) {
            const SensorFrame f = generateFrame(elapsed);

            // ── Kênh SỰ KIỆN: phát Down cho điểm mới xuất hiện ─────────
            for (int i = 0; i < f.count; ++i) {
                const TouchPoint& p = f.points[i];
                if (!m_pointActive[i]) {
                    TouchEvent ev;
                    ev.id = p.id;
                    ev.x = p.x;
                    ev.y = p.y;
                    ev.state = TouchState::Down;
                    ev.sourceId = m_sourceId;
                    ev.tCaptureNs = f.tCaptureNs;
                    m_events.push(ev);
                    m_pointActive[i] = true;
                }
            }

            // ── Kênh TRẠNG THÁI ────────────────────────────────────────
            m_frames.write(f);
        }

        // Ngủ tới nhịp kế tiếp. Đây là thread CHẶN — đúng như thiết kế,
        // nó không bao giờ đụng tới render thread bằng khoá.
        const double periodSec = (m_cfg.rateHz > 0.0) ? (1.0 / m_cfg.rateHz) : 0.008;
        std::this_thread::sleep_for(duration<double>(periodSec));
    }

    // Khi dừng: phát Up cho mọi điểm còn hoạt động.
    // KHÔNG được bỏ qua bước này — thiếu một Up là hiệu ứng kẹt vĩnh viễn.
    for (int i = 0; i < kMaxTouchPoints; ++i) {
        if (!m_pointActive[i]) continue;
        TouchEvent ev;
        ev.id = static_cast<uint32_t>(i + 1);
        ev.state = TouchState::Up;
        ev.sourceId = m_sourceId;
        ev.tCaptureNs = Clock::nowNs();
        m_events.push(ev);
        m_pointActive[i] = false;
    }
}

} // namespace mikmap

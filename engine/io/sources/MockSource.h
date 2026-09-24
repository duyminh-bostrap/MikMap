// ════════════════════════════════════════════════════════════════════════
//  io/sources/MockSource.h — nguồn sensor giả lập (G2)
//
//  ★ Đây KHÔNG phải mã dùng một lần.
//  architecture.md §10.7: "Mọi tính năng phải chạy được với MockSource."
//
//  Lý do:
//    · phát triển và test không cần cắm phần cứng
//    · tái lập được — cùng seed cho cùng dữ liệu, nên bug lặp lại được
//    · chạy trên CI, nơi không bao giờ có Kinect
//    · dựng được tình huống khó tạo bằng tay (40 điểm chạm cùng lúc,
//      điểm nhiễu, mất tín hiệu đột ngột)
//
//  Nó chạy THREAD RIÊNG y hệt nguồn thật, nên cũng kiểm chứng luôn
//  đường đi qua TripleBuffer và SpscRingBuffer.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "io/ISensorSource.h"

#include <atomic>
#include <random>
#include <thread>

namespace mikmap {

enum class MockPattern {
    Circle = 0,    ///< các điểm chạy vòng tròn
    Lissajous,     ///< quỹ đạo phức tạp hơn, tốt để mắt thấy được jitter
    Static,        ///< đứng yên — tiện khi test calibration
    Random,        ///< nhảy lung tung — bài kiểm tra khắc nghiệt cho bộ lọc
};

struct MockConfig {
    int    pointCount = 3;
    double rateHz     = 120.0;        ///< tần số phát frame

    /// Phạm vi toạ độ sensor. Mặc định mô phỏng khung 2000×1200 mm —
    /// cố ý KHÁC thang pixel, để lộ ra lỗi nếu ai đó quên calibration.
    double minX = 0.0,    minY = 0.0;
    double maxX = 2000.0, maxY = 1200.0;

    MockPattern pattern = MockPattern::Circle;

    /// Nhiễu Gauss cộng thêm (đơn vị sensor). 0 = sạch tuyệt đối.
    double noiseSigma = 0.0;

    /// Tỉ lệ frame bị bỏ hẳn, mô phỏng mất tín hiệu. [0,1]
    double dropoutRate = 0.0;

    uint32_t seed = 1234;
};

class MockSource final : public ISensorSource {
public:
    explicit MockSource(MockConfig cfg = {});
    ~MockSource() override;

    const char* typeName() const override { return "Mock"; }

    bool start() override;
    void stop() override;
    SourceStatus status() const override { return m_status.load(); }

    const MockConfig& config() const { return m_cfg; }

    /// Đổi cấu hình. An toàn khi đang chạy — chỉ tác dụng ở frame kế tiếp.
    void setConfig(const MockConfig& cfg);

    /// Sinh MỘT frame đồng bộ, không cần thread.
    /// Dùng trong unit test để có kết quả tất định.
    SensorFrame generateFrame(double timeSec);

private:
    void threadLoop();

    MockConfig m_cfg;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<SourceStatus> m_status{SourceStatus::Stopped};

    std::mt19937 m_rng;
    uint64_t m_seq = 0;

    /// Trạng thái trước đó, để phát sinh sự kiện Down/Up đúng lúc.
    bool m_pointActive[kMaxTouchPoints]{};
};

} // namespace mikmap

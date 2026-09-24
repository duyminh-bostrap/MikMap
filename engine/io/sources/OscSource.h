// ════════════════════════════════════════════════════════════════════════
//  io/sources/OscSource.h — nhận dữ liệu sensor qua OSC/UDP (G4)
//
//  OSC là chuẩn công nghiệp cho dữ liệu điều khiển thời gian thực.
//  Nhận được từ TouchDesigner, Max/MSP, Processing, app điện thoại,
//  Arduino có Ethernet, hay bất kỳ cầu nối sensor nào.
//
//  ── Lược đồ địa chỉ mặc định ─────────────────────────────────────────
//      /mikmap/touch      <id:int> <x:float> <y:float> [z:float]
//      /mikmap/touch/down <id:int> <x:float> <y:float>
//      /mikmap/touch/up   <id:int>
//      /mikmap/clear
//
//  Tiền tố đổi được để khớp với thiết bị sẵn có mà không phải sửa code.
//
//  ── Vì sao gom frame theo lô thay vì phát từng điểm ──────────────────
//  Một cú chạm 10 ngón sinh ra 10 message OSC riêng lẻ. Nếu publish
//  SensorFrame sau mỗi message, render thread sẽ thấy các trạng thái
//  nửa vời (3 ngón, rồi 7 ngón, rồi 10 ngón). Vì vậy ta gom lại và chỉ
//  publish khi hết gói UDP — mỗi frame là một ảnh chụp nhất quán.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"
#include "io/ISensorSource.h"
#include "io/proto/OscMessage.h"
#include "io/proto/TuioDecoder.h"

#include <atomic>
#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>

namespace mikmap {

/// G4 / G14 — hai phương ngữ chạy trên cùng một socket OSC.
///
/// ★ Làm thành CHẾ ĐỘ chứ không phải một lớp nguồn riêng, vì phần khó và
///   dễ sai của một nguồn sensor không nằm ở việc đọc message: nó nằm ở
///   socket, thread, vòng sự kiện wait-free, và cơ chế hết hạn điểm khi
///   UDP đánh rơi gói. Chép lại toàn bộ khối đó cho TUIO nghĩa là nhân
///   đôi chỗ để sai, và sửa lỗi ở một bản sẽ quên bản kia.
enum class OscProtocol {
    Mikmap = 0,   ///< phương ngữ riêng: /mikmap/touch[/down|/up|/clear]
    Tuio,         ///< TUIO 1.1 — /tuio/2Dcur set|alive|fseq
};

struct OscConfig {
    uint16_t    port = 9000;
    OscProtocol protocol = OscProtocol::Mikmap;
    std::string addressPrefix = "/mikmap/touch";

    /// Toạ độ đến đã chuẩn hoá [0,1]? Nhiều nguồn (TouchDesigner, app
    /// điện thoại) gửi dạng này. Khi bật, giá trị được nhân với
    /// sensorRange để đưa về đơn vị sensor.
    bool normalizedInput = false;
    Vec2 sensorRange{1920.0, 1080.0};

    /// Điểm không được cập nhật trong khoảng này coi như đã nhấc lên.
    /// Cần thiết vì UDP không đảm bảo: một message "up" bị rớt sẽ làm
    /// điểm kẹt lại vĩnh viễn nếu không có cơ chế hết hạn.
    double pointTimeoutSec = 1.0;
};

class OscSource final : public ISensorSource {
public:
    explicit OscSource(OscConfig cfg = {});
    ~OscSource() override;

    const char* typeName() const override { return "OSC"; }

    bool start() override;
    void stop() override;
    SourceStatus status() const override { return m_status.load(); }
    std::string lastError() const override;

    const OscConfig& config() const { return m_cfg; }

    /// Chỉ đổi được khi đang dừng — cổng UDP đã bind rồi thì không đổi được.
    void setConfig(const OscConfig& cfg);

    /// Nạp trực tiếp một gói OSC, bỏ qua tầng mạng.
    /// Dùng cho unit test và cho chế độ phát lại (replay) file log.
    void feedPacket(const uint8_t* data, size_t size);

    /// Số gói tin đã nhận / số gói dị dạng bị bỏ. Hiện trên PerfPanel.
    uint64_t packetsReceived() const { return m_packets.load(); }
    uint64_t packetsMalformed() const { return m_malformed.load(); }

private:
    void threadLoop();
    void handleMessages(const std::vector<OscMessage>& msgs);

    /// G14 — nhánh TUIO của handleMessages.
    void handleTuio(const std::vector<OscMessage>& msgs);
    void publishFrame();
    void expireStalePoints();
    void setError(const std::string& e);

    struct LivePoint {
        float  x = 0.0f, y = 0.0f, z = 0.0f;
        double lastSeenSec = 0.0;
        bool   isNew = true;
    };

    OscConfig m_cfg;

    std::thread m_thread;
    std::atomic<bool> m_running{false};
    std::atomic<SourceStatus> m_status{SourceStatus::Stopped};
    std::atomic<uint64_t> m_packets{0};
    std::atomic<uint64_t> m_malformed{0};

    mutable std::mutex m_errorMutex;
    std::string m_error;

    // Chỉ thread OSC chạm vào các trường dưới đây (hoặc feedPacket khi
    // đang dừng, trong test). Không cần khoá.
    std::map<uint32_t, LivePoint> m_points;

    /// G14 — chỉ dùng khi protocol == Tuio.
    TuioDecoder m_tuio;
    uint64_t m_seq = 0;
    std::chrono::steady_clock::time_point m_startTime;

    double nowSec() const;
};

} // namespace mikmap

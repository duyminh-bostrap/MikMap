// ════════════════════════════════════════════════════════════════════════
//  io/SensorFrame.h — dữ liệu đi qua biên thread (G1)
//
//  ★ RÀNG BUỘC: PHẢI là POD, kích thước CỐ ĐỊNH.
//    · không con trỏ, không std::string, không std::vector
//    · memcpy-able ⇒ triple buffer chỉ cần gán, không cấp phát
//    · không cấp phát heap trong hot path của sensor thread
//
//  Toạ độ ở đây là THÔ (mm / depth-px / ADC), CHƯA qua biến đổi nào.
//  Xem architecture.md §3.5 để biết vì sao không transform ở sensor thread.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstdint>

namespace hexmap {

/// Trạng thái vòng đời của một điểm chạm.
enum class TouchState : uint8_t {
    None = 0,
    Down,    ///< vừa xuất hiện
    Move,    ///< đang di chuyển
    Up,      ///< vừa biến mất
};

/// Số điểm chạm tối đa trong một frame.
/// 64 dư cho mọi trường hợp thực tế (khung IR thường ≤ 40 điểm, Kinect
/// skeleton ≤ 6 người × ~25 khớp nhưng ta chỉ lấy điểm quan tâm).
inline constexpr int kMaxTouchPoints = 64;

struct TouchPoint {
    uint32_t   id = 0;            ///< ID bền vững qua các frame
    float      x = 0.0f;          ///< ★ TOẠ ĐỘ SENSOR THÔ
    float      y = 0.0f;
    float      z = 0.0f;          ///< độ sâu hoặc áp lực; 0 nếu không có
    float      confidence = 1.0f; ///< [0,1]
    TouchState state = TouchState::None;
    uint8_t    _pad[3] = {0, 0, 0};
};
static_assert(sizeof(TouchPoint) == 24, "TouchPoint phai giu kich thuoc co dinh");

struct SensorFrame {
    uint64_t   seq = 0;           ///< tăng dần — phát hiện frame bị rơi
    int64_t    tCaptureNs = 0;    ///< ★ steady_clock lúc THU THẬP
    uint16_t   sourceId = 0;
    uint8_t    count = 0;         ///< số điểm hợp lệ trong points[]
    uint8_t    _pad = 0;
    TouchPoint points[kMaxTouchPoints]{};

    void clear() {
        seq = 0;
        tCaptureNs = 0;
        sourceId = 0;
        count = 0;
    }

    bool addPoint(const TouchPoint& p) {
        if (count >= kMaxTouchPoints) return false;   // lặng lẽ bỏ, không tràn
        points[count++] = p;
        return true;
    }
};

/// Sự kiện rời rạc — đi qua kênh RING BUFFER, không được phép rơi.
/// Mất một sự kiện Up nghĩa là hiệu ứng kẹt vĩnh viễn trên màn hình.
struct TouchEvent {
    uint32_t   id = 0;
    float      x = 0.0f;
    float      y = 0.0f;
    TouchState state = TouchState::None;
    uint16_t   sourceId = 0;
    uint8_t    _pad = 0;
    int64_t    tCaptureNs = 0;
};

} // namespace hexmap

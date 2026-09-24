// ════════════════════════════════════════════════════════════════════════
//  core/calib/SensorMapper.h — ★ CHUỖI BIẾN ĐỔI TRUNG TÂM (G7)
//
//  Đây là nơi toàn bộ dự án hội tụ: biến một cú chạm vào vật thể thật
//  thành toạ độ pixel trên nội dung đang chiếu.
//
//    p_sensor ──H_s──▶ p_output ──Screen::hitTest──▶ slice
//                                 ──warp.inverse()─▶ contentUV
//                                 ──inputRect──────▶ canvasPx
//
//  ── Hai thất bại HỢP LỆ mà người gọi phải xử lý ──────────────────────
//    1. Chưa calibrate (hoặc calibration hỏng)
//    2. Điểm chạm nằm NGOÀI mọi slice → không có nội dung ở đó
//
//  Cả hai đều bình thường lúc chạy, nên API trả về bool chứ không trả
//  thẳng Vec2 — xem ghi chú ở IWarp.h về lý do.
//
//  ── Vì sao KHÔNG hợp nhất trước ma trận ──────────────────────────────
//  Với corner pin, H_w⁻¹·H_s gộp được thành một ma trận (đã kiểm chứng
//  bằng test "★ Ma tran hop nhat..."). Nhưng mesh warp không gộp được,
//  và việc gộp đòi hỏi theo dõi khi nào cache hỏng — trong khi lợi ích
//  đo được là dưới 1 µs cho 64 điểm. Chọn đơn giản và luôn đúng.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/calib/CalibrationProfile.h"
#include "core/math/Vec2.h"
#include "core/model/Screen.h"

namespace mikmap {

/// Kết quả tra cứu một điểm chạm.
struct MappedPoint {
    bool valid = false;
    int  sliceIndex = -1;   ///< slice nào nhận điểm này
    Vec2 outputPx;          ///< pixel trên máy chiếu
    Vec2 contentUV;         ///< [0,1]² trong slice
    Vec2 canvasPx;          ///< toạ độ trên Composition Canvas
};

/// Vì sao ánh xạ thất bại — hiển thị khi debug.
enum class MapFailure {
    None = 0,
    NotCalibrated,
    NoScreen,
    NoSliceHit,
};

class SensorMapper {
public:
    /// Cả hai con trỏ phải sống lâu hơn mapper.
    void setCalibration(const CalibrationProfile* profile) { m_calib = profile; }
    void setScreen(const Screen* screen) { m_screen = screen; }

    const CalibrationProfile* calibration() const { return m_calib; }
    const Screen*             screen() const { return m_screen; }

    /// Bước 1: sensor → pixel máy chiếu.
    bool sensorToOutput(const Vec2& sensor, Vec2& outputPx) const;

    /// Chuỗi đầy đủ.
    MappedPoint map(const Vec2& sensor) const;

    /// Chiều ngược: muốn hiệu ứng xuất hiện tại contentUV của slice này
    /// thì sensor phải đọc được giá trị nào?
    /// Dùng khi dựng wizard calibration — phần mềm hiện dấu thập, người
    /// vận hành chạm vào đúng chỗ đó.
    bool contentToSensor(int sliceIndex, const Vec2& contentUV, Vec2& outSensor) const;

    MapFailure lastFailure() const { return m_lastFailure; }

private:
    const CalibrationProfile* m_calib = nullptr;
    const Screen*             m_screen = nullptr;
    mutable MapFailure        m_lastFailure = MapFailure::None;
};

} // namespace mikmap

// ════════════════════════════════════════════════════════════════════════
//  io/LaserScan.h — một vòng quét lidar 2D, RAW (mm + radian)
//
//  Dùng chung cho Hokuyo (SCIP 2.0) và mọi lidar 2D sau này (plugin
//  YDLIDAR). KHÔNG chứa toạ độ đã biến đổi: render thread tự áp H_s.
//
//  ★ POD, kích thước cố định, memcpy-able — đi qua TripleBuffer.
//    rangeMm == 0 nghĩa là tia không có hồi âm.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstdint>
#include <type_traits>

namespace mikmap {

/// Đủ cho 360° ở bước 0.125° (2 880 tia).
inline constexpr int kMaxScanPoints = 2880;

struct LaserScan {
    uint64_t seq = 0;
    int64_t  tCaptureNs = 0;
    uint16_t count = 0;
    float    angleRad[kMaxScanPoints]{};
    float    rangeMm[kMaxScanPoints]{};
};

static_assert(std::is_trivially_copyable_v<LaserScan>,
              "LaserScan phai memcpy-able de qua TripleBuffer");

} // namespace mikmap

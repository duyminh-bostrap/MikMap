// ════════════════════════════════════════════════════════════════════════
//  core/util/Clock.h — mốc thời gian đơn điệu
//
//  Dùng steady_clock, KHÔNG dùng system_clock: system_clock có thể nhảy
//  lùi khi máy đồng bộ NTP hoặc đổi múi giờ. Một cú nhảy lùi giữa show
//  sẽ làm mọi phép đo độ trễ ra số âm và autopilot tính sai.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <chrono>
#include <cstdint>

namespace mikmap {

struct Clock {
    static int64_t nowNs() {
        return std::chrono::duration_cast<std::chrono::nanoseconds>(
                   std::chrono::steady_clock::now().time_since_epoch())
            .count();
    }

    static double nsToMs(int64_t ns) { return static_cast<double>(ns) / 1.0e6; }
    static double nsToSec(int64_t ns) { return static_cast<double>(ns) / 1.0e9; }
};

} // namespace mikmap

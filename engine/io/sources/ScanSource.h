// ════════════════════════════════════════════════════════════════════════
//  io/sources/ScanSource.h — phần chung của MỌI lidar 2D
//
//  Hokuyo hôm nay, plugin YDLIDAR ngày mai: cùng một đường ống
//    scan thô → TripleBuffer (radar vẽ)
//             → LidarTouchPublisher (blob + PointTracker → touch)
//
//  src/ chỉ cần biết lớp này (dynamic_cast) để lấy scan và đẩy ROI, không
//  cần biết dữ liệu đến từ SCIP/TCP hay DLL.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "io/ISensorSource.h"
#include "io/LaserScan.h"
#include "io/LidarBlobs.h"

#include <atomic>
#include <cstdint>

namespace mikmap {

class ScanSource : public ISensorSource {
public:
    /// CHỈ gọi từ UI thread. Xuống thread nguồn qua TripleBuffer, không khoá.
    void setParams(const LidarParams& p);

    /// Scan thô cho khung nhìn radar. Render thread consume()/readSlot().
    TripleBuffer<LaserScan>&       scans()       { return m_scans; }
    const TripleBuffer<LaserScan>& scans() const { return m_scans; }

    uint64_t scansReceived()  const { return m_received.load(std::memory_order_relaxed); }
    uint64_t scansMalformed() const { return m_malformed.load(std::memory_order_relaxed); }

protected:
    /// Gọi từ thread của lớp con khi đã có một scan hợp lệ.
    void onScan(LaserScan& s);

    /// Mất kết nối: mọi blob đang sống phát Up.
    void onLost();

    void countMalformed() { m_malformed.fetch_add(1, std::memory_order_relaxed); }

private:
    TripleBuffer<LaserScan>  m_scans;
    TripleBuffer<LidarParams> m_params;
    LidarTouchPublisher      m_pub;
    LidarParams              m_cur;          // bản sao phía thread nguồn
    std::atomic<uint64_t>    m_received{0};
    std::atomic<uint64_t>    m_malformed{0};
    uint64_t                 m_seq = 0;
    uint64_t                 m_t0 = 0;       // mốc giây cho PointTracker
};

} // namespace mikmap

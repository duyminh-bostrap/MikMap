#include "io/sources/ScanSource.h"

#include "core/util/Clock.h"

namespace mikmap {

void ScanSource::setParams(const LidarParams& p) {
    m_params.writeSlot() = p;
    m_params.publish();
}

void ScanSource::onScan(LaserScan& s) {
    s.tCaptureNs = Clock::nowNs();
    s.seq = ++m_seq;
    m_received.fetch_add(1, std::memory_order_relaxed);

    if (m_t0 == 0) m_t0 = static_cast<uint64_t>(s.tCaptureNs);

    LaserScan& slot = m_scans.writeSlot();
    slot = s;
    m_scans.publish();

    if (m_params.consume()) m_cur = m_params.readSlot();

    m_pub.publish(s, m_cur, Clock::nsToSec(s.tCaptureNs - static_cast<int64_t>(m_t0)), *this);
}

void ScanSource::onLost() {
    m_pub.clear(Clock::nsToSec(Clock::nowNs() - static_cast<int64_t>(m_t0)), *this);
}

} // namespace mikmap

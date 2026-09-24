#include "core/filter/PointTracker.h"

#include <algorithm>
#include <cmath>

namespace mikmap {

void PointTracker::update(const std::vector<Vec2>& observations, double nowSec) {
    m_justLost.clear();

    // ── Bước 1: liệt kê mọi cặp (track, quan sát) trong ngưỡng ─────────
    struct Candidate {
        double distSq;
        size_t trackIdx;
        size_t obsIdx;
    };

    const double maxD  = std::max(1e-9, m_params.maxMatchDistance);
    const double maxD2 = maxD * maxD;

    std::vector<Candidate> candidates;
    candidates.reserve(m_tracks.size() * observations.size());

    for (size_t t = 0; t < m_tracks.size(); ++t) {
        for (size_t o = 0; o < observations.size(); ++o) {
            const double d2 = m_tracks[t].position.distanceSqTo(observations[o]);
            if (d2 <= maxD2) candidates.push_back({d2, t, o});
        }
    }

    // ── Bước 2: ghép tham lam theo khoảng cách tăng dần ────────────────
    // Cặp gần nhau nhất được ghép trước. Không tối ưu toàn cục như
    // thuật toán Hungary, nhưng cho kết quả giống hệt trong thực tế vì
    // các điểm chạm cách nhau xa hơn nhiều so với quãng đường chúng đi
    // trong một frame.
    std::sort(candidates.begin(), candidates.end(),
              [](const Candidate& a, const Candidate& b) { return a.distSq < b.distSq; });

    std::vector<bool> trackUsed(m_tracks.size(), false);
    std::vector<bool> obsUsed(observations.size(), false);

    for (const Candidate& c : candidates) {
        if (trackUsed[c.trackIdx] || obsUsed[c.obsIdx]) continue;

        TrackedPoint& tp = m_tracks[c.trackIdx];
        const Vec2 newPos = observations[c.obsIdx];
        const double dt = nowSec - tp.lastSeenSec;

        tp.velocity = (dt > 1e-6) ? (newPos - tp.position) / dt : Vec2{};
        tp.position = newPos;
        tp.lastSeenSec = nowSec;
        tp.isNew = false;
        tp.isCoasting = false;

        trackUsed[c.trackIdx] = true;
        obsUsed[c.obsIdx] = true;
    }

    // ── Bước 3: quan sát chưa ghép được → điểm MỚI ─────────────────────
    for (size_t o = 0; o < observations.size(); ++o) {
        if (obsUsed[o]) continue;

        TrackedPoint tp;
        tp.id = m_nextId++;
        tp.position = observations[o];
        tp.velocity = Vec2{};
        tp.firstSeenSec = nowSec;
        tp.lastSeenSec = nowSec;
        tp.isNew = true;
        tp.isCoasting = false;
        m_tracks.push_back(tp);
    }

    // ── Bước 4: track chưa ghép được → ân hạn, rồi bỏ ──────────────────
    for (size_t t = 0; t < trackUsed.size(); ++t) {
        if (trackUsed[t]) continue;
        m_tracks[t].isCoasting = true;
        m_tracks[t].isNew = false;
    }

    const double grace = std::max(0.0, m_params.graceSec);
    auto expired = std::remove_if(
        m_tracks.begin(), m_tracks.end(),
        [&](const TrackedPoint& tp) {
            return (nowSec - tp.lastSeenSec) > grace;
        });

    for (auto it = expired; it != m_tracks.end(); ++it) {
        m_justLost.push_back(it->id);
    }
    m_tracks.erase(expired, m_tracks.end());
}

std::vector<TrackedPoint> PointTracker::activeTracks() const {
    std::vector<TrackedPoint> out;
    out.reserve(m_tracks.size());
    for (const TrackedPoint& tp : m_tracks) {
        if (!tp.isCoasting) out.push_back(tp);
    }
    return out;
}

void PointTracker::reset() {
    m_tracks.clear();
    m_justLost.clear();
    // KHÔNG đặt lại m_nextId: ID phải là duy nhất trong suốt phiên làm
    // việc. Dùng lại ID cũ sẽ khiến hiệu ứng gắn với ID đó nhận nhầm
    // một điểm chạm hoàn toàn khác.
}

} // namespace mikmap

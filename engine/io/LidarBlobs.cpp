#include "io/LidarBlobs.h"

#include "core/math/BilinearInverse.h"

#include <cmath>

namespace mikmap {
namespace {

bool anyRoiPoint(const Vec2 roi[4]) {
    for (int i = 0; i < 4; ++i)
        if (roi[i].x != 0.0 || roi[i].y != 0.0) return true;
    return false;
}

} // namespace

int extractBlobs(const LaserScan& s, const LidarParams& p, Vec2* out, int maxOut) {
    if (!out || maxOut <= 0 || !p.trackBlobs) return 0;

    const bool useRoi = anyRoiPoint(p.roi);
    int  nOut = 0;
    bool open = false;
    Vec2 first, last, sum;
    int  nPts = 0;

    auto flush = [&] {
        if (!open) return;
        const double width = first.distanceTo(last);
        if (width >= p.minBlobMm && width <= p.maxBlobMm && nOut < maxOut)
            out[nOut++] = sum / static_cast<double>(nPts);
        open = false;
    };

    Vec2 prev;
    bool havePrev = false;

    for (int i = 0; i < static_cast<int>(s.count); ++i) {
        const double r = s.rangeMm[i];
        Vec2 pt;
        bool ok = std::isfinite(r) && r > 0.0 && (p.maxRangeMm <= 0.0 || r <= p.maxRangeMm);

        if (ok) {
            pt = Vec2{r * std::cos(s.angleRad[i]), r * std::sin(s.angleRad[i])};
            if (useRoi && !pointInQuad(p.roi[0], p.roi[1], p.roi[2], p.roi[3], pt))
                ok = false;
        }

        // Tia hỏng/ngoài ROI cắt cụm: hai bên không được nối qua khoảng trống.
        if (!ok) {
            flush();
            havePrev = false;
            continue;
        }

        if (open && (!havePrev || prev.distanceTo(pt) > p.maxGapMm))
            flush();

        if (!open) {
            open = true;
            first = pt;
            sum = pt;
            nPts = 1;
        } else {
            sum += pt;
            ++nPts;
        }

        last = pt;
        prev = pt;
        havePrev = true;
    }

    flush();
    return nOut;
}

LidarTouchPublisher::LidarTouchPublisher(PointTrackerParams tp) : m_tracker(tp) {
    m_obs.reserve(static_cast<size_t>(kMaxTouchPoints));
}

void LidarTouchPublisher::publish(const LaserScan& s, const LidarParams& p,
                                  double nowSec, ISensorSource& dst) {
    Vec2 blobs[kMaxTouchPoints];
    const int n = extractBlobs(s, p, blobs, kMaxTouchPoints);
    m_obs.assign(blobs, blobs + n);
    m_tracker.update(m_obs, nowSec);

    const int64_t  t   = s.tCaptureNs;
    const uint16_t sid = dst.sourceId();

    // Sự kiện đi trước frame: Down/Up không được rơi.
    for (const TrackedPoint& tp : m_tracker.tracks()) {
        if (!tp.isNew) continue;
        TouchEvent ev;
        ev.id = tp.id;
        ev.x = static_cast<float>(tp.position.x);
        ev.y = static_cast<float>(tp.position.y);
        ev.state = TouchState::Down;
        ev.sourceId = sid;
        ev.tCaptureNs = t;
        dst.events().push(ev);
    }

    for (uint32_t id : m_tracker.justLost()) {
        TouchEvent ev;
        ev.id = id;
        ev.x = 0.0f;
        ev.y = 0.0f;
        ev.state = TouchState::Up;
        ev.sourceId = sid;
        ev.tCaptureNs = t;
        dst.events().push(ev);
    }

    SensorFrame& f = dst.frames().writeSlot();
    f.clear();
    f.seq = s.seq;
    f.tCaptureNs = t;
    f.sourceId = sid;
    for (const TrackedPoint& tp : m_tracker.tracks()) {
        if (tp.isCoasting) continue;
        TouchPoint pt;
        pt.id = tp.id;
        pt.x = static_cast<float>(tp.position.x);
        pt.y = static_cast<float>(tp.position.y);
        pt.state = tp.isNew ? TouchState::Down : TouchState::Move;
        f.addPoint(pt);
    }
    dst.frames().publish();
}

void LidarTouchPublisher::clear(double, ISensorSource& dst) {
    const uint16_t sid = dst.sourceId();

    for (const TrackedPoint& tp : m_tracker.tracks()) {
        TouchEvent ev;
        ev.id = tp.id;
        ev.state = TouchState::Up;
        ev.sourceId = sid;
        dst.events().push(ev);
    }
    // reset để blob ở vị trí cũ sau khi nối lại không "thừa hưởng" track đã Up.
    m_tracker.reset();

    SensorFrame& f = dst.frames().writeSlot();
    f.clear();
    f.sourceId = sid;
    dst.frames().publish();
}

} // namespace mikmap

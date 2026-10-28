#include "TestHarness.h"

#include "io/ISensorSource.h"
#include "io/LaserScan.h"
#include "io/LidarBlobs.h"

#include <cmath>
#include <initializer_list>
#include <utility>
#include <vector>

using namespace mikmap;

namespace {

constexpr double kPi = 3.14159265358979323846;
constexpr int    kRays = 1081;   // -135°..+135°, bước 0.25°

/// Quét 270°: tường ở wallMm, các "vật" là cung tròn ở khoảng cách tâm.
/// Vật {tâm, rộngMm} chiếm các tia nằm trong nửa góc atan(rộng/2 / |tâm|).
LaserScan scanWith(float wallMm, std::initializer_list<std::pair<Vec2, float>> objects = {}) {
    LaserScan s;
    s.count = static_cast<uint16_t>(kRays);
    s.tCaptureNs = 123456789;

    for (int i = 0; i < kRays; ++i) {
        const double a = (-135.0 + 0.25 * i) * kPi / 180.0;
        s.angleRad[i] = static_cast<float>(a);

        float r = wallMm;
        for (const auto& obj : objects) {
            const Vec2&  c     = obj.first;
            const double dist  = c.length();
            const double half  = std::atan2(obj.second * 0.5, dist);
            double d = a - std::atan2(c.y, c.x);
            while (d >  kPi) d -= 2.0 * kPi;
            while (d < -kPi) d += 2.0 * kPi;
            if (std::abs(d) <= half) { r = static_cast<float>(dist); break; }
        }
        s.rangeMm[i] = r;
    }
    return s;
}

/// Nguồn rỗng: publisher chỉ cần frames()/events() công khai.
class Sink : public ISensorSource {
public:
    const char* typeName() const override { return "test-sink"; }
    bool start() override { return true; }
    void stop() override {}
    SourceStatus status() const override { return SourceStatus::Running; }
};

std::vector<TouchEvent> drain(ISensorSource& s) {
    std::vector<TouchEvent> out;
    TouchEvent e;
    while (s.events().pop(e)) out.push_back(e);
    return out;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  Cắt cụm
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("Lidar: mot vat 40mm, tuong dai bi loai") {
    const LaserScan s = scanWith(2000.f, {{Vec2{1000, 0}, 40.f}});
    Vec2 out[64];
    const LidarParams p;

    const int n = extractBlobs(s, p, out, 64);
    REQUIRE(n == 1);
    CHECK_NEAR(out[0].x, 1000.0, 25.0);
    CHECK_NEAR(out[0].y, 0.0, 25.0);
}

TEST_CASE("Lidar: ROI loc dung") {
    const LaserScan s = scanWith(2000.f, {{Vec2{1000, 0}, 40.f}});
    Vec2 out[64];

    LidarParams inside;
    inside.roi[0] = {1500, 500};
    inside.roi[1] = {1500, -500};
    inside.roi[2] = {500, -500};
    inside.roi[3] = {500, 500};
    REQUIRE(extractBlobs(s, inside, out, 64) == 1);

    LidarParams outside = inside;
    outside.roi[0] = {3500, 500};
    outside.roi[1] = {3500, -500};
    outside.roi[2] = {2500, -500};
    outside.roi[3] = {2500, 500};
    REQUIRE(extractBlobs(s, outside, out, 64) == 0);
}

TEST_CASE("Lidar: hai vat cho hai blob") {
    const LaserScan s = scanWith(2000.f, {{Vec2{1000, -300}, 40.f}, {Vec2{1000, 300}, 40.f}});
    Vec2 out[64];
    const LidarParams p;

    const int n = extractBlobs(s, p, out, 64);
    REQUIRE(n == 2);
    CHECK_NEAR(out[0].x, 1000.0, 25.0);
    CHECK_NEAR(out[0].y, -300.0, 25.0);
    CHECK_NEAR(out[1].x, 1000.0, 25.0);
    CHECK_NEAR(out[1].y, 300.0, 25.0);
}

TEST_CASE("Lidar: vat 8mm duoi minBlobMm bi loai") {
    const LaserScan s = scanWith(2000.f, {{Vec2{1000, 0}, 8.f}});
    Vec2 out[64];
    const LidarParams p;
    REQUIRE(extractBlobs(s, p, out, 64) == 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  Track + sự kiện
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ Lidar: id ben vung, Down roi Up") {
    Sink dst;
    LidarTouchPublisher pub;
    const LidarParams p;

    pub.publish(scanWith(2000.f, {{Vec2{1000, 0}, 40.f}}), p, 0.0, dst);
    auto ev = drain(dst);
    REQUIRE(ev.size() == 1);
    REQUIRE(ev[0].state == TouchState::Down);
    const uint32_t id = ev[0].id;

    // Cùng người/vật dịch 30 mm: vẫn một id, không Down mới.
    pub.publish(scanWith(2000.f, {{Vec2{1000, 30}, 40.f}}), p, 0.05, dst);
    ev = drain(dst);
    REQUIRE(ev.empty());
    REQUIRE(dst.frames().consume());
    REQUIRE(dst.frames().readSlot().count == 1);
    REQUIRE(dst.frames().readSlot().points[0].id == id);

    // Ba frame không còn blob: ân hạn 0.15 s hết hạn đúng một lần.
    pub.publish(scanWith(2000.f), p, 0.15, dst);
    pub.publish(scanWith(2000.f), p, 0.25, dst);
    pub.publish(scanWith(2000.f), p, 0.35, dst);
    ev = drain(dst);
    REQUIRE(ev.size() == 1);
    REQUIRE(ev[0].state == TouchState::Up);
    REQUIRE(ev[0].id == id);

    REQUIRE(dst.frames().consume());
    REQUIRE(dst.frames().readSlot().count == 0);
}

TEST_CASE("Lidar: clear phat Up va frame rong") {
    Sink dst;
    LidarTouchPublisher pub;
    const LidarParams p;

    pub.publish(scanWith(2000.f, {{Vec2{1000, 0}, 40.f}}), p, 0.0, dst);
    auto ev = drain(dst);
    REQUIRE(ev.size() == 1);
    REQUIRE(ev[0].state == TouchState::Down);
    const uint32_t id = ev[0].id;

    pub.clear(1.0, dst);
    ev = drain(dst);
    REQUIRE(ev.size() == 1);
    REQUIRE(ev[0].state == TouchState::Up);
    REQUIRE(ev[0].id == id);

    REQUIRE(dst.frames().consume());
    REQUIRE(dst.frames().readSlot().count == 0);
}

TEST_CASE("Lidar: maxRangeMm cat vat o xa, 0 = khong gioi han") {
    const LaserScan s = scanWith(2000.f, {{Vec2{1000, 0}, 40.f}});
    Vec2 out[64];
    LidarParams p;
    p.maxRangeMm = 800;
    CHECK(extractBlobs(s, p, out, 64) == 0);
    p.maxRangeMm = 1500;
    CHECK(extractBlobs(s, p, out, 64) == 1);
    p.maxRangeMm = 0;
    CHECK(extractBlobs(s, p, out, 64) == 1);
}

TEST_CASE("Lidar: trackBlobs=false => 0 blob nhung scan van publish") {
    Sink dst;
    LidarTouchPublisher pub;
    LidarParams p;
    p.trackBlobs = false;
    pub.publish(scanWith(2000.f, {{Vec2{1000, 0}, 40.f}}), p, 0.0, dst);
    CHECK(drain(dst).empty());
    REQUIRE(dst.frames().consume());
    CHECK(dst.frames().readSlot().count == 0);
}

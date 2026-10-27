#include "TestHarness.h"

#include "FakeScipServer.h"
#include "io/sources/HokuyoSource.h"

#include <chrono>
#include <cstdint>
#include <thread>

using namespace mikmap;
using mikmap::test::FakeScipServer;

namespace {

template <typename Pred>
bool waitFor(Pred pred, int ms) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return pred();
}

} // namespace

TEST_CASE("Hokuyo: ket noi, nhan scan, ra 1 touch") {
    FakeScipServer srv;
    REQUIRE(srv.error().empty());

    HokuyoSource src({"127.0.0.1", srv.port(), 0.1});
    REQUIRE(src.start());

    REQUIRE(waitFor([&] {
        return src.status() == SourceStatus::Running && src.scansReceived() >= 3;
    }, 2000));

    REQUIRE(src.scans().consume());
    REQUIRE(src.scans().readSlot().count == 1081);

    bool one = false;
    REQUIRE(waitFor([&] {
        if (src.frames().consume()) one = (src.frames().readSlot().count == 1);
        return one;
    }, 1000));
    REQUIRE(one);

    const float x = src.frames().readSlot().points[0].x;
    CHECK(x > 975.0f);
    CHECK(x < 1025.0f);

    src.stop();
}

TEST_CASE("★ Hokuyo: server dong ket noi → Up, het touch, tu noi lai") {
    FakeScipServer srv(5);
    REQUIRE(srv.error().empty());

    HokuyoSource src({"127.0.0.1", srv.port(), 0.1});
    REQUIRE(src.start());

    uint32_t downId = 0;
    bool upSeen = false;
    bool reconnected = false;

    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < end) {
        TouchEvent e;
        while (src.events().pop(e)) {
            if (e.state == TouchState::Down) downId = e.id;
            if (e.state == TouchState::Up && downId != 0 && e.id == downId) upSeen = true;
        }

        if (srv.connections() >= 2 && src.status() == SourceStatus::Running) {
            reconnected = true;
            if (upSeen) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CHECK(downId != 0);
    CHECK(upSeen);
    CHECK(reconnected);

    src.stop();
}

TEST_CASE("Hokuyo: khong co server → Error co lastError") {
    uint16_t port = 0;
    {
        FakeScipServer tmp;
        REQUIRE(tmp.error().empty());
        port = tmp.port();
    }   // destructor đóng listener

    HokuyoSource src({"127.0.0.1", port, 0.2});
    REQUIRE(src.start());

    REQUIRE(waitFor([&] {
        const SourceStatus s = src.status();
        return (s == SourceStatus::Error || s == SourceStatus::Connecting) && !src.lastError().empty();
    }, 2000));

    src.stop();
}

TEST_CASE("Hokuyo: setParams ROI loai vat") {
    FakeScipServer srv;
    REQUIRE(srv.error().empty());

    HokuyoSource src({"127.0.0.1", srv.port(), 0.1});
    REQUIRE(src.start());

    bool one = false;
    REQUIRE(waitFor([&] {
        if (src.frames().consume()) one = (src.frames().readSlot().count == 1);
        return one;
    }, 2000));

    LidarParams p;
    p.roi[0] = {3500, 500};
    p.roi[1] = {3500, -500};
    p.roi[2] = {2500, -500};
    p.roi[3] = {2500, 500};
    src.setParams(p);

    bool zero = false;
    REQUIRE(waitFor([&] {
        if (src.frames().consume()) zero = (src.frames().readSlot().count == 0);
        return zero;
    }, 2000));
    REQUIRE(zero);

    src.stop();
}

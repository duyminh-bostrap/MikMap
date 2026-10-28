#include "TestHarness.h"

#include "io/net/TouchSender.h"
#include "io/sources/OscSource.h"

#include <chrono>
#include <cstring>
#include <string>
#include <thread>

using namespace mikmap;

namespace {

OutPacket twoPoints(const char* host = "127.0.0.1", uint16_t port = 7000, uint8_t proto = 0) {
    OutPacket p;
    std::snprintf(p.host, sizeof p.host, "%s", host);
    p.port = port; p.proto = proto;
    setOutDevice(p, "dev1");
    p.count = 2;
    p.ids[0] = 3;  p.xy[0] = 0.5f;  p.xy[1] = 0.25f;
    p.ids[1] = 12; p.xy[2] = 1.25f; p.xy[3] = -0.0625f;   // not clamped: off-screen stays off-screen
    return p;
}

template <class F> bool waitFor(F ok, double sec) {
    const auto t0 = std::chrono::steady_clock::now();
    while (!ok()) {
        if (std::chrono::steady_clock::now() - t0 > std::chrono::duration<double>(sec)) return false;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return true;
}

} // namespace

TEST_CASE("TouchSender: encodeOsc is real OSC — OscSource reads both points back") {
    const OutPacket p = twoPoints();
    uint8_t buf[8192];
    const size_t n = encodeOsc(p, buf, sizeof buf);
    REQUIRE(n > 0);
    REQUIRE(n % 4 == 0);

    OscConfig cfg; cfg.addressPrefix = "/mikmap/dev1/touch";
    OscSource src(cfg);
    src.feedPacket(buf, n);
    CHECK(src.packetsMalformed() == 0u);
    REQUIRE(src.frames().consume());
    const SensorFrame& f = src.frames().readSlot();
    REQUIRE(f.count == 2);
    CHECK(f.points[0].id == 3u);   // OscSource keeps points ordered by id
    CHECK_NEAR(f.points[0].x, 0.5, 1e-6);
    CHECK_NEAR(f.points[0].y, 0.25, 1e-6);
    CHECK(f.points[1].id == 12u);
    CHECK_NEAR(f.points[1].x, 1.25, 1e-6);
    CHECK_NEAR(f.points[1].y, -0.0625, 1e-6);
}

TEST_CASE("TouchSender: encodeOsc byte-exact golden for one point") {
    OutPacket p; setOutDevice(p, "d"); p.count = 1; p.ids[0] = 7; p.xy[0] = 0.5f; p.xy[1] = 0.25f;
    static const uint8_t golden[84] = {
        '#','b','u','n','d','l','e',0, 0,0,0,0,0,0,0,1,                       // bundle, timetag "immediately"
        0,0,0,24, '/','m','i','k','m','a','p','/','d','/','f','r','a','m','e',0,
        ',','i',0,0, 0,0,0,1,                                                 // count 1
        0,0,0,36, '/','m','i','k','m','a','p','/','d','/','t','o','u','c','h',0,
        ',','i','f','f',0,0,0,0, 0,0,0,7, 0x3f,0,0,0, 0x3e,0x80,0,0 };       // id 7, 0.5f, 0.25f
    uint8_t buf[256];
    REQUIRE(encodeOsc(p, buf, sizeof buf) == sizeof golden);
    CHECK(std::memcmp(buf, golden, sizeof golden) == 0);
    CHECK(encodeOsc(p, buf, 83) == 0u);   // never writes past cap
}

TEST_CASE("TouchSender: encodeText exact string") {
    const OutPacket p = twoPoints();
    uint8_t buf[512];
    const size_t n = encodeText(p, buf, sizeof buf);
    CHECK(std::string(reinterpret_cast<const char*>(buf), n) ==
          "dev1 frame 2\ndev1 3 0.5000 0.2500\ndev1 12 1.2500 -0.0625\n");
    CHECK(encodeText(p, buf, 10) == 0u);
}

TEST_CASE("TouchSender: setOutDevice makes any name OSC-safe and fits 31 bytes") {
    OutPacket p;
    setOutDevice(p, "Hokuyo 10.0.0.5:10940 /#*,?[]{}");
    CHECK(std::string(p.device) == "Hokuyo_10.0.0.5:10940__________");
    setOutDevice(p, "");
    CHECK(std::string(p.device) == "device");
}

TEST_CASE("TouchSender: loopback to a running OscSource arrives within 1 s") {
    OscConfig cfg; cfg.port = 0; cfg.addressPrefix = "/mikmap/dev1/touch";   // 0: the OS picks a free port
    OscSource src(cfg);
    REQUIRE(src.start());
    REQUIRE(src.boundPort() != 0);
    TouchSender s;
    REQUIRE(s.start());
    REQUIRE(s.push(twoPoints("127.0.0.1", src.boundPort())));
    REQUIRE(waitFor([&] { return src.frames().consume(); }, 1.0));
    const SensorFrame& f = src.frames().readSlot();
    REQUIRE(f.count == 2);
    CHECK(f.points[1].id == 12u);
    CHECK_NEAR(f.points[1].x, 1.25, 1e-6);
    CHECK(s.sent() == 1u);
    CHECK(s.lastError().empty());
    s.stop();
    src.stop();
}

TEST_CASE("TouchSender: a 1000-packet flood never blocks push, every packet is sent or dropped") {
    TouchSender s;
    REQUIRE(s.start());
    const OutPacket p = twoPoints("127.0.0.1", 9);   // discard port
    const auto t0 = std::chrono::steady_clock::now();
    for (int i = 0; i < 1000; ++i) s.push(p);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    CHECK_MSG(ms < 200.0, "1000 pushes took " + std::to_string(ms) + " ms");   // generous: a loaded CI box, still far from blocking
    CHECK(waitFor([&] { return s.sent() + s.dropped() == 1000u; }, 3.0));
    CHECK(s.sent() + s.dropped() == 1000u);
    s.stop();
}

TEST_CASE("TouchSender: an unresolvable host sets lastError, never throws") {
    TouchSender s;
    REQUIRE(s.start());
    s.push(twoPoints("no-such-host.invalid", 7000));
    CHECK(waitFor([&] { return s.dropped() == 1u; }, 10.0));   // wait on exactly what is asserted: no ordering between counters
    CHECK(waitFor([&] { return s.lastError() == "Cannot resolve host"; }, 1.0));
    OutPacket bad = twoPoints("127.0.0.1", 0);   // port 0 is rejected by the sender too
    s.push(bad);
    CHECK(waitFor([&] { return s.lastError().find("Port") != std::string::npos; }, 2.0));
    CHECK(waitFor([&] { return s.dropped() == 2u; }, 1.0));
    s.stop();
}

TEST_CASE("TouchSender: a host that cannot be resolved never starves a good one, and is looked up once per backoff") {
    OscConfig cfg; cfg.port = 0; cfg.addressPrefix = "/mikmap/dev1/touch";
    OscSource src(cfg);
    REQUIRE(src.start());
    TouchSender s;
    REQUIRE(s.start());
    s.push(twoPoints("no-such-host.invalid", 7000));
    REQUIRE(waitFor([&] { return s.dropped() == 1u; }, 10.0));   // the one lookup (may be slow on a LAN without DNS)
    for (int i = 0; i < 10; ++i) {   // in backoff now: the bad packets drop at once, the good ones (an IP literal) all go out
        s.push(twoPoints("no-such-host.invalid", 7000));
        s.push(twoPoints("127.0.0.1", src.boundPort()));
        REQUIRE(waitFor([&] { return s.sent() == uint64_t(i + 1) && s.dropped() == uint64_t(i + 2); }, 2.0));
    }
    CHECK(waitFor([&] { return src.frames().consume(); }, 1.0));
    CHECK_MSG(s.lookups() == 1u, "lookups = " + std::to_string(s.lookups()) + " (a bad name in backoff, or an IP literal, must not query DNS)");
    s.stop();
    src.stop();
}

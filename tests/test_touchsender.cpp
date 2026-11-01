#include "TestHarness.h"

#include "io/net/TouchSender.h"
#include "io/sources/OscSource.h"

#include <chrono>
#include <filesystem>
#include <fstream>
#include "io/proto/OscMessage.h"
#include <cstdio>
#include <cstring>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

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

/// n points; x/y la boi so cua 0.125 nen ca float lan chuoi "%.4f" deu chinh xac.
OutPacket manyPoints(int n, uint8_t proto = 0) {
    OutPacket p;
    std::snprintf(p.host, sizeof p.host, "127.0.0.1");
    p.port = 7000; p.proto = proto;
    setOutDevice(p, "dev1");
    p.count = static_cast<uint16_t>(n);
    for (int i = 0; i < n; ++i) {
        p.ids[i] = static_cast<uint32_t>(1000 + i);
        p.xy[2 * i] = static_cast<float>(i) * 0.5f;
        p.xy[2 * i + 1] = static_cast<float>(i) * -0.25f + 0.125f;
    }
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

// ═══════════════════════════════════════════════════════════════════════
//  Chia MOT khung thanh NHIEU datagram (<= kMaxDatagram)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("TouchSender: 510 diem -> OSC chia nhieu datagram <= 1200 byte, OscSource ghep lai du") {
    const OutPacket p = manyPoints(510);
    OscConfig cfg; cfg.addressPrefix = "/mikmap/dev1/touch";
    OscSource src(cfg);

    std::vector<uint8_t> buf(kMaxDatagram);
    int first = 0;
    int chunks = 0;
    while (first < 510) {
        int next = -1;
        const size_t n = encodeOscChunk(p, first, buf.data(), buf.size(), next);
        REQUIRE(n > 0);
        CHECK(n <= kMaxDatagram);
        CHECK(n % 4 == 0);                 // bundle OSC luon can 4 byte
        CHECK(next > first);
        CHECK(next <= 510);
        const std::string chunk(reinterpret_cast<const char*>(buf.data()), n);
        if (chunks == 0) CHECK(chunk.find("/mikmap/dev1/frame") != std::string::npos);
        else             CHECK(chunk.find("/mikmap/dev1/frame") == std::string::npos);
        src.feedPacket(buf.data(), n);
        first = next;
        ++chunks;
    }
    CHECK(chunks >= 2);                    // 510 diem khong lot vao mot datagram 1200 byte
    CHECK(src.packetsMalformed() == 0u);

    REQUIRE(src.frames().consume());
    const SensorFrame& f = src.frames().readSlot();
    REQUIRE(f.count == 510u);
    int bad = 0;
    for (int i = 0; i < 510; ++i) {
        if (f.points[i].id != static_cast<uint32_t>(1000 + i) ||
            std::abs(f.points[i].x - (static_cast<float>(i) * 0.5f)) > 1e-6f ||
            std::abs(f.points[i].y - (static_cast<float>(i) * -0.25f + 0.125f)) > 1e-6f) {
            ++bad;
        }
    }
    CHECK_MSG(bad == 0, std::to_string(bad) + " diem sai khi ghep cac chunk lai");
}

TEST_CASE("TouchSender: 510 diem -> text chia nhieu datagram <= 1200 byte, doc lai du") {
    const OutPacket p = manyPoints(510, 1);

    std::vector<uint8_t> buf(kMaxDatagram);
    std::vector<uint32_t> ids;
    std::vector<double> xs, ys;
    int first = 0, chunks = 0, headerCount = -1;

    while (first < 510) {
        int next = -1;
        const size_t n = encodeTextChunk(p, first, buf.data(), buf.size(), next);
        REQUIRE(n > 0);
        CHECK(n <= kMaxDatagram);
        CHECK(next > first);
        const std::string s(reinterpret_cast<const char*>(buf.data()), n);
        REQUIRE(!s.empty());
        CHECK(s.back() == '\n');           // khong bao gio cat giua mot dong

        size_t pos = 0;
        bool headerPending = (chunks == 0);   // chi dong DAU cua chunk DAU la dong "frame"
        while (pos < s.size()) {
            const size_t eol = s.find('\n', pos);
            REQUIRE(eol != std::string::npos);
            const std::string line = s.substr(pos, eol - pos);
            pos = eol + 1;

            std::istringstream in(line);
            std::string dev, word;
            REQUIRE(static_cast<bool>(in >> dev));
            CHECK(dev == "dev1");
            if (headerPending) {
                headerPending = false;
                int c = -1;
                REQUIRE(static_cast<bool>(in >> word >> c));
                CHECK(word == "frame");
                CHECK(c == 510);
                headerCount = c;
                continue;
            }
            unsigned id = 0;
            double x = 0.0, y = 0.0;
            REQUIRE(static_cast<bool>(in >> id >> x >> y));
            ids.push_back(id); xs.push_back(x); ys.push_back(y);
        }
        first = next;
        ++chunks;
    }

    CHECK(chunks >= 2);
    CHECK(headerCount == 510);
    REQUIRE(ids.size() == 510u);
    int bad = 0;
    for (size_t i = 0; i < ids.size(); ++i) {
        if (ids[i] != 1000u + i ||
            std::abs(xs[i] - (static_cast<double>(i) * 0.5)) > 1e-6 ||
            std::abs(ys[i] - (static_cast<double>(i) * -0.25 + 0.125)) > 1e-6) {
            ++bad;
        }
    }
    CHECK_MSG(bad == 0, std::to_string(bad) + " dong text sai khi doc lai");
}

TEST_CASE("TouchSender: khung rong van gui mot chunk mang 'frame 0'") {
    OutPacket p; setOutDevice(p, "dev1"); p.count = 0;
    std::vector<uint8_t> buf(kMaxDatagram);

    int next = -1;
    const size_t n = encodeOscChunk(p, 0, buf.data(), buf.size(), next);
    REQUIRE(n > 0);
    CHECK(next == 0);
    const std::string s(reinterpret_cast<const char*>(buf.data()), n);
    CHECK(s.find("/mikmap/dev1/frame") != std::string::npos);
    REQUIRE(n >= 4);
    const uint8_t zero[4] = {0, 0, 0, 0};
    CHECK(std::memcmp(buf.data() + n - 4, zero, 4) == 0);   // count = 0

    OscConfig cfg; cfg.addressPrefix = "/mikmap/dev1/touch";
    OscSource src(cfg);
    src.feedPacket(buf.data(), n);
    REQUIRE(src.frames().consume());
    CHECK(src.frames().readSlot().count == 0u);

    int nextT = -1;
    const size_t nt = encodeTextChunk(p, 0, buf.data(), buf.size(), nextT);
    REQUIRE(nt > 0);
    CHECK(nextT == 0);
    CHECK(std::string(reinterpret_cast<const char*>(buf.data()), nt) == "dev1 frame 0\n");
}

TEST_CASE("TouchSender: chunk tra 0 khi khong lot noi mot diem nao") {
    const OutPacket p = manyPoints(2);
    std::vector<uint8_t> buf(kMaxDatagram);

    int next = -1;
    CHECK(encodeOscChunk(p, 0, buf.data(), 40, next) == 0u);   // header 32 byte lot, diem 44 byte khong
    CHECK(next == 0);

    int nextT = -1;
    CHECK(encodeTextChunk(p, 0, buf.data(), 14, nextT) == 0u);  // "dev1 frame 2\n" (13) lot, dong diem khong
    CHECK(nextT == 0);
}

TEST_CASE("TouchSender: loopback 510 diem -> sender thread tu chia datagram") {
    OscConfig cfg; cfg.port = 0; cfg.addressPrefix = "/mikmap/dev1/touch";   // 0: the OS picks a free port
    OscSource src(cfg);
    REQUIRE(src.start());
    REQUIRE(src.boundPort() != 0);

    TouchSender s;
    REQUIRE(s.start());
    OutPacket p = manyPoints(510);
    p.port = src.boundPort();
    REQUIRE(s.push(p));

    REQUIRE(waitFor([&] { return src.frames().consume() && src.frames().readSlot().count == 510u; }, 2.0));
    const SensorFrame& f = src.frames().readSlot();
    CHECK(f.points[0].id == 1000u);
    CHECK(f.points[509].id == 1509u);
    CHECK_NEAR(f.points[509].x, 254.5, 1e-6);
    CHECK(waitFor([&] { return s.sent() == 1u; }, 1.0));   // mot packet (nhieu datagram) = mot lan sent
    CHECK(s.dropped() == 0u);
    s.stop();
    src.stop();
}

TEST_CASE("TouchSender: confidence survives bounded chunks and real loopback") {
    auto p = manyPoints(510); p.hasZ = true;
    for (int i = 0; i < p.count; ++i) p.z[i] = .75f;
    OscConfig cfg; cfg.port = 0; cfg.addressPrefix = "/mikmap/dev1/touch";
    OscSource receiver(cfg); uint8_t buf[kMaxDatagram];
    for (int first = 0; first < p.count;) {
        int next = first; auto n = encodeOscChunk(p, first, buf, sizeof buf, next);
        REQUIRE(n > 0); CHECK(n <= kMaxDatagram); REQUIRE(next > first);
        receiver.feedPacket(buf, n); first = next;
    }
    REQUIRE(receiver.frames().consume());
    const auto& f = receiver.frames().readSlot(); REQUIRE(f.count == 510);
    for (int i = 0; i < f.count; ++i) CHECK_NEAR(f.points[i].z, .75, 1e-6);
    for (int first = 0; first < p.count;) {
        int next = first; auto n = encodeTextChunk(p, first, buf, sizeof buf, next);
        REQUIRE(n > 0); CHECK(n <= kMaxDatagram); REQUIRE(next > first);
        std::istringstream lines(std::string(reinterpret_cast<char*>(buf), n)); std::string line;
        while (std::getline(lines, line)) if (line.find(" frame ") == std::string::npos) {
            std::istringstream row(line); std::string name; unsigned id; float x,y,z;
            REQUIRE(bool(row >> name >> id >> x >> y >> z)); CHECK_NEAR(z, .75, 1e-6);
        }
        first = next;
    }
    OscSource live(cfg); REQUIRE(live.start()); TouchSender sender; REQUIRE(sender.start());
    p = twoPoints("127.0.0.1", live.boundPort()); p.hasZ = true; p.z[0] = .8f; p.z[1] = .6f;
    REQUIRE(sender.push(p));
    REQUIRE(waitFor([&] { return live.frames().consume() && live.frames().readSlot().count == 2; }, 1));
    CHECK_NEAR(live.frames().readSlot().points[0].z, .8, 1e-6);
    CHECK_NEAR(live.frames().readSlot().points[1].z, .6, 1e-6);
}


// Fixtures captured by the unmodified 6d8d079 encoder, including chunk boundaries.
TEST_CASE("TouchSender: Body bytes match pre-B goldens with optional confidence") {
    const auto dir = std::filesystem::path(__FILE__).parent_path() / "data";
    for (bool z : {false, true}) for (int count : {0, 2, 32}) for (bool text : {false, true}) {
        auto p = manyPoints(count); p.hasZ = z;
        for (int i = 0; i < count; ++i) p.z[i] = .75f;
        std::ifstream file(dir / (std::string("body_pre_b_") + (z ? "z_" : "xy_") + std::to_string(count) + (text ? ".text.hex" : ".osc.hex")));
        REQUIRE(file.good());
        int first = 0;
        do {
            uint8_t buf[kMaxDatagram]; int next = -1;
            auto n = text ? encodeTextChunk(p, first, buf, sizeof buf, next) : encodeOscChunk(p, first, buf, sizeof buf, next);
            REQUIRE(n > 0); CHECK(n <= kMaxDatagram);
            std::string expected; REQUIRE(bool(std::getline(file, expected)));
            std::string actual;
            for (size_t i = 0; i < n; ++i) { char h[3]; std::snprintf(h, sizeof h, "%02x", unsigned(buf[i])); actual += h; }
            CHECK(actual == expected); first = next;
        } while (first < count);
        std::string extra; CHECK(!std::getline(file, extra));
    }
}

TEST_CASE("TouchSender: mixed groups preserve all point fields across bounded OSC and text chunks") {
    const uint8_t groups[] = {0, 1, 2, 3, 4, 9};
    const char* suffixes[] = {"touch", "hand", "hand", "face", "feet", "touch"};
    for (bool z : {false, true}) {
        auto p = manyPoints(kMaxTouchPoints); p.hasZ = z;
        for (int i = 0; i < p.count; ++i) { p.groups[i] = groups[i % 6]; p.z[i] = .75f; }
        for (int i = 0; i < 6; ++i) CHECK(std::string(groupAddressSuffix(groups[i])) == suffixes[i]);
        for (bool text : {false, true}) {
            int first = 0, chunks = 0;
            while (first < p.count) {
                uint8_t buf[kMaxDatagram]; int next = -1;
                const auto n = text ? encodeTextChunk(p, first, buf, sizeof buf, next) : encodeOscChunk(p, first, buf, sizeof buf, next);
                REQUIRE(n > 0); CHECK(n <= kMaxDatagram); REQUIRE(next > first);
                int point = first;
                if (text) {
                    std::istringstream lines(std::string(reinterpret_cast<char*>(buf), n));
                    std::string line;
                    if (first == 0) { REQUIRE(bool(std::getline(lines, line))); CHECK(line == "dev1 frame 512"); }
                    while (std::getline(lines, line)) {
                        std::istringstream row(line); std::string dev, suffix; unsigned id; float x, y, confidence;
                        REQUIRE(bool(row >> dev)); CHECK(dev == "dev1");
                        if (point % 6 >= 1 && point % 6 <= 4) { REQUIRE(bool(row >> suffix)); CHECK(suffix == suffixes[point % 6]); }
                        REQUIRE(bool(row >> id >> x >> y));
                        CHECK(id == p.ids[point]); CHECK_NEAR(x, p.xy[2 * point], 1e-6); CHECK_NEAR(y, p.xy[2 * point + 1], 1e-6);
                        if (z) { REQUIRE(bool(row >> confidence)); CHECK_NEAR(confidence, .75, 1e-6); }
                        std::string extra; CHECK(!(row >> extra)); ++point;
                    }
                } else {
                    std::vector<OscMessage> messages; REQUIRE(parseOscPacket(buf, n, messages));
                    size_t m = 0;
                    if (first == 0) { REQUIRE(!messages.empty()); CHECK(messages[0].address == "/mikmap/dev1/frame"); REQUIRE(messages[0].args.size() == 1u); CHECK(messages[0].argInt(0) == 512); ++m; }
                    for (; m < messages.size(); ++m, ++point) {
                        const auto& msg = messages[m];
                        CHECK(msg.address == std::string("/mikmap/dev1/") + suffixes[point % 6]);
                        REQUIRE(msg.args.size() == (z ? 4u : 3u));
                        CHECK(msg.args[0].type == OscArg::Type::Int32);
                        for (size_t a = 1; a < msg.args.size(); ++a) CHECK(msg.args[a].type == OscArg::Type::Float32);
                        CHECK(msg.argInt(0) == int(p.ids[point])); CHECK_NEAR(msg.argDouble(1), p.xy[2 * point], 1e-6); CHECK_NEAR(msg.argDouble(2), p.xy[2 * point + 1], 1e-6);
                        if (z) CHECK_NEAR(msg.argDouble(3), .75, 1e-6);
                    }
                }
                CHECK(point == next); first = next; ++chunks;
            }
            CHECK(chunks > 1);
        }
    }
}

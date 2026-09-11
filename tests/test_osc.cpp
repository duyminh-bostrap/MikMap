#include "TestHarness.h"

#include "io/proto/OscMessage.h"
#include "io/sources/OscSource.h"

#include <cstring>
#include <thread>

// Test cuoi cung tu mo socket de gui goi OSC that qua loopback.
// OscSource.h khong lo header mang ra ngoai (dung la vay — do la chi
// tiet cai dat), nen test phai tu include.
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  pragma comment(lib, "ws2_32.lib")
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif

using namespace hexmap;

namespace {

/// Bo dung goi OSC — de test parser ma khong can mo cong mang.
struct OscBuilder {
    std::vector<uint8_t> data;

    void pad4() {
        while (data.size() % 4 != 0) data.push_back(0);
    }

    void str(const std::string& s) {
        data.insert(data.end(), s.begin(), s.end());
        data.push_back(0);
        pad4();
    }

    void i32(int32_t v) {
        const auto u = static_cast<uint32_t>(v);
        data.push_back(static_cast<uint8_t>((u >> 24) & 0xFF));
        data.push_back(static_cast<uint8_t>((u >> 16) & 0xFF));
        data.push_back(static_cast<uint8_t>((u >> 8)  & 0xFF));
        data.push_back(static_cast<uint8_t>( u        & 0xFF));
    }

    void f32(float v) {
        int32_t bits = 0;
        std::memcpy(&bits, &v, sizeof(bits));
        i32(bits);
    }

    const uint8_t* ptr() const { return data.data(); }
    size_t size() const { return data.size(); }
};

/// Message don: address + typetag + tham so.
OscBuilder makeMessage(const std::string& addr, const std::string& tags) {
    OscBuilder b;
    b.str(addr);
    b.str("," + tags);
    return b;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  Parser OSC
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("OSC: message khong tham so") {
    OscBuilder b;
    b.str("/hexmap/clear");

    std::vector<OscMessage> msgs;
    REQUIRE(parseOscPacket(b.ptr(), b.size(), msgs));
    REQUIRE(msgs.size() == 1u);
    REQUIRE(msgs[0].address == "/hexmap/clear");
    REQUIRE(msgs[0].args.empty());
}

TEST_CASE("★ OSC: doc dung int va float BIG-ENDIAN") {
    OscBuilder b = makeMessage("/hexmap/touch", "iff");
    b.i32(7);
    b.f32(123.5f);
    b.f32(-45.25f);

    std::vector<OscMessage> msgs;
    REQUIRE(parseOscPacket(b.ptr(), b.size(), msgs));
    REQUIRE(msgs.size() == 1u);
    REQUIRE(msgs[0].args.size() == 3u);
    CHECK(msgs[0].argInt(0) == 7);
    CHECK_NEAR(msgs[0].argDouble(1), 123.5, 1e-6);
    CHECK_NEAR(msgs[0].argDouble(2), -45.25, 1e-6);
}

TEST_CASE("OSC: tham so chuoi") {
    OscBuilder b = makeMessage("/test", "s");
    b.str("xin chao");

    std::vector<OscMessage> msgs;
    REQUIRE(parseOscPacket(b.ptr(), b.size(), msgs));
    REQUIRE(msgs[0].args.size() == 1u);
    REQUIRE(msgs[0].args[0].s == "xin chao");
}

TEST_CASE("★ OSC: argDouble doc duoc ca int lan float") {
    // Nhieu thiet bi gui toa do dang int, so khac gui float.
    // Ep nguoi goi phan biet la vo ich.
    OscBuilder b = makeMessage("/t", "if");
    b.i32(100);
    b.f32(200.5f);

    std::vector<OscMessage> msgs;
    REQUIRE(parseOscPacket(b.ptr(), b.size(), msgs));
    CHECK_NEAR(msgs[0].argDouble(0), 100.0, 1e-9);
    CHECK_NEAR(msgs[0].argDouble(1), 200.5, 1e-6);
}

TEST_CASE("OSC: bundle chua nhieu message") {
    OscBuilder m1 = makeMessage("/a", "i"); m1.i32(1);
    OscBuilder m2 = makeMessage("/b", "i"); m2.i32(2);

    OscBuilder bundle;
    bundle.str("#bundle");
    for (int i = 0; i < 8; ++i) bundle.data.push_back(0);   // timetag
    bundle.i32(static_cast<int32_t>(m1.size()));
    bundle.data.insert(bundle.data.end(), m1.data.begin(), m1.data.end());
    bundle.i32(static_cast<int32_t>(m2.size()));
    bundle.data.insert(bundle.data.end(), m2.data.begin(), m2.data.end());

    std::vector<OscMessage> msgs;
    REQUIRE(parseOscPacket(bundle.ptr(), bundle.size(), msgs));
    REQUIRE(msgs.size() == 2u);
    CHECK(msgs[0].address == "/a");
    CHECK(msgs[1].address == "/b");
    CHECK(msgs[1].argInt(0) == 2);
}

TEST_CASE("★ OSC: goi RONG / QUA NGAN bi tu choi, khong doc tran") {
    std::vector<OscMessage> msgs;
    REQUIRE(!parseOscPacket(nullptr, 0, msgs));
    const uint8_t tiny[2] = {0x2F, 0x00};
    REQUIRE(!parseOscPacket(tiny, 2, msgs));
}

TEST_CASE("★ OSC: typetag hua nhieu tham so hon du lieu -> tu choi") {
    // Day la kieu goi tin doc hai co ban nhat: bao co 4 float nhung
    // chi gui 1. Parser phai kiem tra bien, khong duoc doc tran bo dem.
    OscBuilder b = makeMessage("/t", "ffff");
    b.f32(1.0f);   // thieu 3 float

    std::vector<OscMessage> msgs;
    REQUIRE(!parseOscPacket(b.ptr(), b.size(), msgs));
}

TEST_CASE("OSC: address khong bat dau bang '/' bi tu choi") {
    OscBuilder b;
    b.str("khongcodaucheo");
    b.str(",i");
    b.i32(1);

    std::vector<OscMessage> msgs;
    REQUIRE(!parseOscPacket(b.ptr(), b.size(), msgs));
}

TEST_CASE("OSC: typetag khong bat dau bang ',' bi tu choi") {
    OscBuilder b;
    b.str("/t");
    b.str("if");     // thieu dau phay
    b.i32(1);

    std::vector<OscMessage> msgs;
    REQUIRE(!parseOscPacket(b.ptr(), b.size(), msgs));
}

TEST_CASE("OSC: kieu T/F doc thanh 1/0") {
    OscBuilder b = makeMessage("/t", "TF");
    std::vector<OscMessage> msgs;
    REQUIRE(parseOscPacket(b.ptr(), b.size(), msgs));
    REQUIRE(msgs[0].args.size() == 2u);
    CHECK(msgs[0].argInt(0) == 1);
    CHECK(msgs[0].argInt(1) == 0);
}

TEST_CASE("★ OSC: dem chuoi dung boi so 4") {
    // Address dai 4 ky tu can them 4 byte dem (1 null + 3 pad), khong
    // phai 0. Sai o day thi moi tham so phia sau deu lech.
    OscBuilder b = makeMessage("/abc", "i");   // "/abc" = 4 ky tu
    b.i32(999);

    std::vector<OscMessage> msgs;
    REQUIRE(parseOscPacket(b.ptr(), b.size(), msgs));
    REQUIRE(msgs[0].address == "/abc");
    REQUIRE(msgs[0].argInt(0) == 999);
}

// ═══════════════════════════════════════════════════════════════════════
//  OscSource — qua feedPacket, khong can mang
// ═══════════════════════════════════════════════════════════════════════

namespace {
void feedTouch(OscSource& src, int id, float x, float y,
               const std::string& suffix = "") {
    OscBuilder b = makeMessage("/hexmap/touch" + suffix, "iff");
    b.i32(id);
    b.f32(x);
    b.f32(y);
    src.feedPacket(b.ptr(), b.size());
}
} // namespace

TEST_CASE("★ G4: nhan diem cham va publish qua TripleBuffer") {
    OscSource src;
    feedTouch(src, 1, 500.0f, 300.0f);

    REQUIRE(src.frames().consume());
    const SensorFrame& f = src.frames().readSlot();
    REQUIRE(f.count == 1);
    CHECK(f.points[0].id == 1u);
    CHECK_NEAR(f.points[0].x, 500.0, 1e-4);
    CHECK_NEAR(f.points[0].y, 300.0, 1e-4);
}

TEST_CASE("★ G4: nhieu diem gom thanh MOT frame nhat quan") {
    // Neu publish sau moi message, render thread se thay trang thai nua
    // voi (1 diem, roi 2 diem, roi 3 diem) thay vi mot anh chup dung.
    OscBuilder bundle;
    bundle.str("#bundle");
    for (int i = 0; i < 8; ++i) bundle.data.push_back(0);
    for (int id = 1; id <= 3; ++id) {
        OscBuilder m = makeMessage("/hexmap/touch", "iff");
        m.i32(id);
        m.f32(static_cast<float>(id * 100));
        m.f32(static_cast<float>(id * 50));
        bundle.i32(static_cast<int32_t>(m.size()));
        bundle.data.insert(bundle.data.end(), m.data.begin(), m.data.end());
    }

    OscSource src;
    src.feedPacket(bundle.ptr(), bundle.size());

    REQUIRE(src.frames().consume());
    REQUIRE(src.frames().readSlot().count == 3);   // ★ ba diem trong MOT frame
    REQUIRE(src.frames().publishedCount() == 1u);
}

TEST_CASE("★ G4: diem moi xuat hien qua /touch cung phat su kien Down") {
    // Nhieu nguon khong bao gio gui /down — chung chi bat dau gui toa do.
    // Neu chi tin vao /down, hieu ung se khong bao gio kich hoat.
    OscSource src;
    feedTouch(src, 5, 100.0f, 100.0f);

    TouchEvent ev;
    REQUIRE(src.events().pop(ev));
    CHECK(ev.id == 5u);
    CHECK(ev.state == TouchState::Down);
}

TEST_CASE("G4: di chuyen tiep khong phat them Down") {
    OscSource src;
    feedTouch(src, 1, 100.0f, 100.0f);
    feedTouch(src, 1, 150.0f, 120.0f);

    int downs = 0;
    TouchEvent ev;
    while (src.events().pop(ev)) {
        if (ev.state == TouchState::Down) ++downs;
    }
    REQUIRE(downs == 1);
}

TEST_CASE("★ G4: /up xoa diem va phat su kien Up") {
    OscSource src;
    feedTouch(src, 2, 400.0f, 400.0f);

    OscBuilder up = makeMessage("/hexmap/touch/up", "i");
    up.i32(2);
    src.feedPacket(up.ptr(), up.size());

    REQUIRE(src.frames().consume());
    REQUIRE(src.frames().readSlot().count == 0);

    bool sawUp = false;
    TouchEvent ev;
    while (src.events().pop(ev)) {
        if (ev.state == TouchState::Up && ev.id == 2u) sawUp = true;
    }
    REQUIRE(sawUp);
}

TEST_CASE("G4: /clear xoa het moi diem") {
    OscSource src;
    feedTouch(src, 1, 10.0f, 10.0f);
    feedTouch(src, 2, 20.0f, 20.0f);

    OscBuilder c;
    c.str("/hexmap/touch/clear");
    src.feedPacket(c.ptr(), c.size());

    REQUIRE(src.frames().consume());
    REQUIRE(src.frames().readSlot().count == 0);
}

TEST_CASE("★ G4: toa do chuan hoa [0,1] duoc nhan len dai sensor") {
    OscConfig cfg;
    cfg.normalizedInput = true;
    cfg.sensorRange = Vec2{2000.0, 1200.0};
    OscSource src(cfg);

    feedTouch(src, 1, 0.5f, 0.25f);

    REQUIRE(src.frames().consume());
    const SensorFrame& f = src.frames().readSlot();
    CHECK_NEAR(f.points[0].x, 1000.0, 1e-3);
    CHECK_NEAR(f.points[0].y, 300.0, 1e-3);
}

TEST_CASE("G4: doi duoc tien to dia chi") {
    OscConfig cfg;
    cfg.addressPrefix = "/tui/cursor";
    OscSource src(cfg);

    OscBuilder b = makeMessage("/tui/cursor", "iff");
    b.i32(9);
    b.f32(1.0f);
    b.f32(2.0f);
    src.feedPacket(b.ptr(), b.size());

    REQUIRE(src.frames().consume());
    REQUIRE(src.frames().readSlot().count == 1);
    REQUIRE(src.frames().readSlot().points[0].id == 9u);
}

TEST_CASE("G4: dia chi la bi bo qua, khong gay loi") {
    OscSource src;
    OscBuilder b = makeMessage("/khong/lien/quan", "i");
    b.i32(1);
    src.feedPacket(b.ptr(), b.size());

    REQUIRE(src.frames().consume());
    REQUIRE(src.frames().readSlot().count == 0);
}

TEST_CASE("★ G4: goi hong duoc DEM lai de hien tren PerfPanel") {
    OscSource src;
    const uint8_t rac[8] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    src.feedPacket(rac, sizeof(rac));

    REQUIRE(src.packetsReceived() == 1u);
    REQUIRE(src.packetsMalformed() == 1u);
}

TEST_CASE("G4: khong vuot qua kMaxTouchPoints") {
    OscSource src;
    for (int id = 1; id <= kMaxTouchPoints + 20; ++id) {
        feedTouch(src, id, static_cast<float>(id), static_cast<float>(id));
    }
    REQUIRE(src.frames().consume());
    REQUIRE(src.frames().readSlot().count == kMaxTouchPoints);
}

// ═══════════════════════════════════════════════════════════════════════
//  ★★ Kiem chung qua MANG THAT (UDP loopback)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★★ G4: nhan duoc goi OSC that qua UDP loopback") {
    OscConfig cfg;
    cfg.port = 39321;             // cong cao, kho dung do
    OscSource src(cfg);

    if (!src.start()) {
        // Cong bi chiem hoac may cam socket — bao ro thay vi bao that bai
        // mo ho. Day KHONG phai loi cua code dang test.
        std::printf("      [BO QUA] khong bind duoc cong %u: %s\n",
                    cfg.port, src.lastError().c_str());
        return;
    }

    REQUIRE(src.status() == SourceStatus::Running);

    // Gui mot goi OSC that bang socket rieng.
    OscBuilder b = makeMessage("/hexmap/touch", "iff");
    b.i32(42);
    b.f32(777.0f);
    b.f32(555.0f);

#if defined(_WIN32)
    SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    REQUIRE(s != INVALID_SOCKET);
#else
    int s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    REQUIRE(s >= 0);
#endif

    sockaddr_in dst{};
    dst.sin_family = AF_INET;
    dst.sin_port = htons(cfg.port);
    dst.sin_addr.s_addr = htonl(0x7F000001);   // 127.0.0.1

    ::sendto(s, reinterpret_cast<const char*>(b.ptr()),
             static_cast<int>(b.size()), 0,
             reinterpret_cast<sockaddr*>(&dst), sizeof(dst));

    // Cho toi 2 giay — UDP loopback thuong duoi 1ms, nhung CI co the cham.
    bool got = false;
    for (int i = 0; i < 200 && !got; ++i) {
        if (src.frames().consume()) got = true;
        else std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }

#if defined(_WIN32)
    ::closesocket(s);
#else
    ::close(s);
#endif

    REQUIRE(got);
    const SensorFrame& f = src.frames().readSlot();
    REQUIRE(f.count == 1);
    CHECK(f.points[0].id == 42u);
    CHECK_NEAR(f.points[0].x, 777.0, 1e-3);
    CHECK_NEAR(f.points[0].y, 555.0, 1e-3);

    src.stop();
    REQUIRE(src.status() == SourceStatus::Stopped);
}

TEST_CASE("★ G4: bind cong dang bi chiem -> bao loi ro rang, khong treo") {
    OscConfig cfg;
    cfg.port = 39322;

    OscSource a(cfg);
    if (!a.start()) return;      // moi truong khong cho mo socket

    OscSource b(cfg);            // cung cong
    const bool ok = b.start();

    if (!ok) {
        REQUIRE(b.status() != SourceStatus::Running);
        REQUIRE(!b.lastError().empty());
    }
    // Mot so he cho phep bind trung (SO_REUSEADDR mac dinh) — khi do
    // ok == true va cung khong sao. Dieu quan trong la KHONG TREO.

    b.stop();
    a.stop();
}

TEST_CASE("G4: stop khi chua start khong gay loi") {
    OscSource src;
    src.stop();
    src.stop();
    REQUIRE(src.status() == SourceStatus::Stopped);
}

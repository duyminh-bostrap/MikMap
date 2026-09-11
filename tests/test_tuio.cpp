#include "TestHarness.h"

#include "io/proto/TuioDecoder.h"
#include "io/sources/OscSource.h"

#include <cstring>
#include <string>
#include <vector>

using namespace hexmap;

namespace {

// ── Dựng message TUIO ở mức OscMessage ─────────────────────────────────

OscArg str(const std::string& s) {
    OscArg a; a.type = OscArg::Type::String; a.s = s; return a;
}
OscArg i32(int32_t v) {
    OscArg a; a.type = OscArg::Type::Int32; a.i = v; return a;
}
OscArg f32(float v) {
    OscArg a; a.type = OscArg::Type::Float32; a.f = v; return a;
}

OscMessage setMsg(int32_t id, float x, float y) {
    OscMessage m;
    m.address = "/tuio/2Dcur";
    m.args = {str("set"), i32(id), f32(x), f32(y), f32(0.0f), f32(0.0f), f32(0.0f)};
    return m;
}

OscMessage aliveMsg(const std::vector<int32_t>& ids) {
    OscMessage m;
    m.address = "/tuio/2Dcur";
    m.args.push_back(str("alive"));
    for (const int32_t id : ids) m.args.push_back(i32(id));
    return m;
}

OscMessage fseqMsg(int32_t n) {
    OscMessage m;
    m.address = "/tuio/2Dcur";
    m.args = {str("fseq"), i32(n)};
    return m;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  G14 — khung cơ bản
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("G14: mot frame day du -> ra dung diem") {
    TuioDecoder d;
    TuioFrame f;

    REQUIRE(d.feed({setMsg(7, 0.25f, 0.75f), aliveMsg({7}), fseqMsg(1)}, f));
    REQUIRE(f.cursors.size() == 1u);
    CHECK(f.cursors[0].id == 7);
    CHECK_NEAR(f.cursors[0].x, 0.25, 1e-6);
    CHECK_NEAR(f.cursors[0].y, 0.75, 1e-6);
    CHECK(f.ended.empty());
}

TEST_CASE("G14: goi chi co 'set' KHONG dong frame") {
    TuioDecoder d;
    TuioFrame f;
    // Bundle bi chia nho: set toi truoc, alive toi sau.
    CHECK(!d.feed({setMsg(1, 0.5f, 0.5f)}, f));
    CHECK(d.feed({aliveMsg({1}), fseqMsg(1)}, f));
    REQUIRE(f.cursors.size() == 1u);
}

TEST_CASE("G14: bo qua ho so khong phai con tro (2Dobj, 2Dblb)") {
    TuioDecoder d;
    TuioFrame f;

    OscMessage obj;
    obj.address = "/tuio/2Dobj";
    obj.args = {str("set"), i32(99), i32(3), f32(0.1f), f32(0.1f)};

    REQUIRE(d.feed({obj, setMsg(1, 0.5f, 0.5f), aliveMsg({1}), fseqMsg(1)}, f));
    REQUIRE(f.cursors.size() == 1u);
    CHECK(f.cursors[0].id == 1);     // 99 khong duoc lot vao
}

TEST_CASE("G14: nhan ca /tuio/25Dcur (nhieu bo LiDAR gui loai nay)") {
    TuioDecoder d;
    TuioFrame f;

    OscMessage s;
    s.address = "/tuio/25Dcur";
    s.args = {str("set"), i32(4), f32(0.3f), f32(0.6f), f32(0.0f)};

    OscMessage a;
    a.address = "/tuio/25Dcur";
    a.args = {str("alive"), i32(4)};

    REQUIRE(d.feed({s, a}, f));
    REQUIRE(f.cursors.size() == 1u);
    CHECK(f.cursors[0].id == 4);
}

// ═══════════════════════════════════════════════════════════════════════
//  G14 — ★ 'alive' là tín hiệu kết thúc duy nhất
// ═══════════════════════════════════════════════════════════════════════

// ★ TUIO KHONG co message "nhac tay". Cach duy nhat biet mot diem da bien
//   mat la no VANG MAT trong danh sach alive cua frame ke.
//
//   Ai chi xu ly 'set' se co bo theo doi ma diem KHONG BAO GIO CHET: cham
//   roi nhac tay, dau cham van nam do mai mai. Trong tac pham tuong tac
//   thi do la hieu ung ket cung cho toi khi khoi dong lai.
TEST_CASE("★ G14: diem vang mat khoi 'alive' phai duoc bao la da ket thuc") {
    TuioDecoder d;
    TuioFrame f;

    REQUIRE(d.feed({setMsg(1, 0.1f, 0.1f), setMsg(2, 0.2f, 0.2f),
                    aliveMsg({1, 2}), fseqMsg(1)}, f));
    REQUIRE(f.cursors.size() == 2u);

    // Nhac ngon tay thu hai: no chi bien mat khoi alive, khong co message
    // nao khac bao dieu do.
    REQUIRE(d.feed({setMsg(1, 0.15f, 0.1f), aliveMsg({1}), fseqMsg(2)}, f));
    REQUIRE(f.cursors.size() == 1u);
    CHECK(f.cursors[0].id == 1);
    REQUIRE(f.ended.size() == 1u);
    CHECK(f.ended[0] == 2);
}

TEST_CASE("G14: alive rong -> moi diem deu ket thuc") {
    TuioDecoder d;
    TuioFrame f;
    REQUIRE(d.feed({setMsg(1, 0.1f, 0.1f), aliveMsg({1}), fseqMsg(1)}, f));
    REQUIRE(d.feed({aliveMsg({}), fseqMsg(2)}, f));
    CHECK(f.cursors.empty());
    REQUIRE(f.ended.size() == 1u);
    CHECK(f.ended[0] == 1);
}

// ★ Ben gui CHI gui 'set' khi vi tri THAY DOI. Mot ngon tay dat yen chi
//   xuat hien trong 'alive'. Khong nho vi tri cu thi no bien mat roi hien
//   lai moi khi nguoi ta ngung di chuyen — dung luc can no on dinh nhat.
TEST_CASE("★ G14: diem DUNG YEN (chi co trong alive) giu nguyen vi tri") {
    TuioDecoder d;
    TuioFrame f;

    REQUIRE(d.feed({setMsg(3, 0.42f, 0.84f), aliveMsg({3}), fseqMsg(1)}, f));

    // Frame sau: van song nhung khong co 'set' vi khong nhuc nhich.
    REQUIRE(d.feed({aliveMsg({3}), fseqMsg(2)}, f));
    REQUIRE(f.cursors.size() == 1u);
    CHECK_NEAR(f.cursors[0].x, 0.42, 1e-6);
    CHECK_NEAR(f.cursors[0].y, 0.84, 1e-6);
}

// Co trong alive nhung CHUA TUNG thay set: bo qua thay vi bia vi tri
// (0,0). Goc tren-trai la mot toa do HOP LE, nen diem ma o do trong y het
// mot cu cham that — va se kich hoat trigger zone o goc man hinh.
TEST_CASE("★ G14: id la trong alive khong bi bia thanh diem o goc (0,0)") {
    TuioDecoder d;
    TuioFrame f;
    REQUIRE(d.feed({aliveMsg({42}), fseqMsg(1)}, f));
    CHECK(f.cursors.empty());
}

// ═══════════════════════════════════════════════════════════════════════
//  G14 — ★ thứ tự gói (UDP không bảo đảm)
// ═══════════════════════════════════════════════════════════════════════

// ★ Goi den muon mang trang thai CU; ap vao se lam diem cham nhay giat ve
//   sau roi nhay toi. Trieu chung nhin thay la hieu ung "rung" ma doi bo
//   loc bao nhieu cung khong het — vi nguyen nhan khong nam o nhieu.
TEST_CASE("★ G14: frame den muon bi BO, khong lam diem nhay giat") {
    TuioDecoder d;
    TuioFrame f;

    REQUIRE(d.feed({setMsg(1, 0.10f, 0.0f), aliveMsg({1}), fseqMsg(10)}, f));
    REQUIRE(d.feed({setMsg(1, 0.20f, 0.0f), aliveMsg({1}), fseqMsg(11)}, f));
    CHECK_NEAR(f.cursors[0].x, 0.20, 1e-6);

    // Goi cu (fseq 10) toi muon -> phai bi bo hoan toan.
    CHECK(!d.feed({setMsg(1, 0.10f, 0.0f), aliveMsg({1}), fseqMsg(10)}, f));

    // Frame moi tiep theo van dung, va khong bi dinh 'set' cua goi da bo.
    REQUIRE(d.feed({setMsg(1, 0.30f, 0.0f), aliveMsg({1}), fseqMsg(12)}, f));
    CHECK_NEAR(f.cursors[0].x, 0.30, 1e-6);
}

TEST_CASE("G14: fseq = -1 nghia la ben gui khong danh so -> nhan tat") {
    TuioDecoder d;
    TuioFrame f;
    REQUIRE(d.feed({setMsg(1, 0.1f, 0.0f), aliveMsg({1}), fseqMsg(-1)}, f));
    REQUIRE(d.feed({setMsg(1, 0.2f, 0.0f), aliveMsg({1}), fseqMsg(-1)}, f));
    CHECK_NEAR(f.cursors[0].x, 0.2, 1e-6);
}

TEST_CASE("G14: khong co fseq nhung co alive -> van dong frame") {
    TuioDecoder d;
    TuioFrame f;
    REQUIRE(d.feed({setMsg(1, 0.5f, 0.5f), aliveMsg({1})}, f));
    REQUIRE(f.cursors.size() == 1u);
    CHECK(f.fseq == -1);
}

// ═══════════════════════════════════════════════════════════════════════
//  G14 — gói dị dạng không được làm sập
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("G14: 'set' thieu doi so bi bo qua, khong sap") {
    TuioDecoder d;
    TuioFrame f;

    OscMessage bad;
    bad.address = "/tuio/2Dcur";
    bad.args = {str("set"), i32(1)};          // thieu x, y

    REQUIRE(d.feed({bad, aliveMsg({1}), fseqMsg(1)}, f));
    CHECK(f.cursors.empty());                  // khong bia toa do
}

TEST_CASE("G14: message rong / lenh la bi bo qua") {
    TuioDecoder d;
    TuioFrame f;

    OscMessage empty;
    empty.address = "/tuio/2Dcur";

    OscMessage weird;
    weird.address = "/tuio/2Dcur";
    weird.args = {str("khong_phai_lenh"), i32(5)};

    CHECK(!d.feed({empty, weird}, f));         // khong co alive -> chua dong frame
}

TEST_CASE("G14: cung id gui hai lan trong mot goi -> giu ban SAU") {
    TuioDecoder d;
    TuioFrame f;
    REQUIRE(d.feed({setMsg(1, 0.1f, 0.1f), setMsg(1, 0.9f, 0.9f),
                    aliveMsg({1}), fseqMsg(1)}, f));
    REQUIRE(f.cursors.size() == 1u);
    CHECK_NEAR(f.cursors[0].x, 0.9, 1e-6);
}

TEST_CASE("G14: reset() quen sach trang thai") {
    TuioDecoder d;
    TuioFrame f;
    REQUIRE(d.feed({setMsg(1, 0.1f, 0.1f), aliveMsg({1}), fseqMsg(50)}, f));
    d.reset();
    CHECK(d.aliveIds().empty());

    // Sau reset, fseq nho hon van phai duoc nhan (ket noi lai tu dau).
    REQUIRE(d.feed({setMsg(2, 0.4f, 0.4f), aliveMsg({2}), fseqMsg(1)}, f));
    REQUIRE(f.cursors.size() == 1u);
    CHECK(f.cursors[0].id == 2);
    CHECK(f.ended.empty());     // khong bao id cu la "ket thuc" sau reset
}

// ═══════════════════════════════════════════════════════════════════════
//  G14 — ★ chạy qua BYTE THẬT, không chỉ struct dựng sẵn
// ═══════════════════════════════════════════════════════════════════════

namespace {

void pushStr(std::vector<uint8_t>& b, const std::string& s) {
    for (const char c : s) b.push_back(static_cast<uint8_t>(c));
    b.push_back(0);
    while (b.size() % 4 != 0) b.push_back(0);
}
void pushI32(std::vector<uint8_t>& b, int32_t v) {
    const uint32_t u = static_cast<uint32_t>(v);
    b.push_back(static_cast<uint8_t>((u >> 24) & 0xFF));
    b.push_back(static_cast<uint8_t>((u >> 16) & 0xFF));
    b.push_back(static_cast<uint8_t>((u >>  8) & 0xFF));
    b.push_back(static_cast<uint8_t>( u        & 0xFF));
}
void pushF32(std::vector<uint8_t>& b, float v) {
    uint32_t u = 0;
    std::memcpy(&u, &v, 4);
    pushI32(b, static_cast<int32_t>(u));
}

} // namespace

// ★ Test o muc OscMessage khong chung minh duoc no doc duoc goi THAT.
//   Bai nay dung dung byte theo dinh dang OSC 1.0 roi cho chay qua ca
//   parseOscPacket lan TuioDecoder — dung chuoi ma mot bo tracking that
//   se gui toi.
TEST_CASE("★ G14: giai ma duoc goi UDP dung byte, qua ca OSC lan TUIO") {
    std::vector<uint8_t> pkt;
    pushStr(pkt, "/tuio/2Dcur");
    pushStr(pkt, ",siff");
    pushStr(pkt, "set");
    pushI32(pkt, 3);
    pushF32(pkt, 0.5f);
    pushF32(pkt, 0.25f);

    std::vector<OscMessage> msgs;
    REQUIRE(parseOscPacket(pkt.data(), pkt.size(), msgs));
    REQUIRE(msgs.size() == 1u);

    std::vector<uint8_t> pkt2;
    pushStr(pkt2, "/tuio/2Dcur");
    pushStr(pkt2, ",si");
    pushStr(pkt2, "alive");
    pushI32(pkt2, 3);
    REQUIRE(parseOscPacket(pkt2.data(), pkt2.size(), msgs));

    TuioDecoder d;
    TuioFrame f;
    REQUIRE(d.feed(msgs, f));
    REQUIRE(f.cursors.size() == 1u);
    CHECK(f.cursors[0].id == 3);
    CHECK_NEAR(f.cursors[0].x, 0.5,  1e-6);
    CHECK_NEAR(f.cursors[0].y, 0.25, 1e-6);
}

// ═══════════════════════════════════════════════════════════════════════
//  G14 — nguồn TUIO chạy end-to-end: gói UDP → sự kiện chạm
// ═══════════════════════════════════════════════════════════════════════

namespace {

/// Dựng một gói TUIO hoàn chỉnh (set + alive + fseq) trong MỘT bundle,
/// đúng như bộ tracking thật gửi.
std::vector<uint8_t> tuioPacket(const std::vector<TuioCursor>& cur,
                                const std::vector<int32_t>& alive,
                                int32_t fseq) {
    auto msg = [](const std::vector<uint8_t>& body) {
        return body;
    };

    std::vector<std::vector<uint8_t>> elems;

    for (const TuioCursor& c : cur) {
        std::vector<uint8_t> m;
        pushStr(m, "/tuio/2Dcur");
        pushStr(m, ",siff");
        pushStr(m, "set");
        pushI32(m, c.id);
        pushF32(m, c.x);
        pushF32(m, c.y);
        elems.push_back(msg(m));
    }
    {
        std::vector<uint8_t> m;
        pushStr(m, "/tuio/2Dcur");
        std::string tags = ",s";
        for (size_t i = 0; i < alive.size(); ++i) tags += "i";
        pushStr(m, tags);
        pushStr(m, "alive");
        for (const int32_t id : alive) pushI32(m, id);
        elems.push_back(msg(m));
    }
    {
        std::vector<uint8_t> m;
        pushStr(m, "/tuio/2Dcur");
        pushStr(m, ",si");
        pushStr(m, "fseq");
        pushI32(m, fseq);
        elems.push_back(msg(m));
    }

    std::vector<uint8_t> pkt;
    pushStr(pkt, "#bundle");
    for (int i = 0; i < 8; ++i) pkt.push_back(0);   // timetag
    for (const auto& e : elems) {
        pushI32(pkt, static_cast<int32_t>(e.size()));
        pkt.insert(pkt.end(), e.begin(), e.end());
    }
    return pkt;
}

int drainEvents(OscSource& src, std::vector<TouchEvent>& out) {
    TouchEvent ev;
    int n = 0;
    while (src.events().pop(ev)) { out.push_back(ev); ++n; }
    return n;
}

} // namespace

TEST_CASE("★ G14: nguon TUIO — goi that vao, su kien cham ra") {
    OscConfig cfg;
    cfg.protocol = OscProtocol::Tuio;
    cfg.sensorRange = Vec2{1920.0, 1080.0};
    OscSource src(cfg);

    const auto p = tuioPacket({{5, 0.5f, 0.25f}}, {5}, 1);
    src.feedPacket(p.data(), p.size());

    // ★ TUIO gui toa do CHUAN HOA [0,1]; nguon phai nhan voi sensorRange.
    REQUIRE(src.frames().consume());
    const SensorFrame& f = src.frames().readSlot();
    REQUIRE(f.count == 1);
    CHECK(f.points[0].id == 5u);
    CHECK_NEAR(f.points[0].x, 960.0, 1e-3);
    CHECK_NEAR(f.points[0].y, 270.0, 1e-3);

    std::vector<TouchEvent> evs;
    drainEvents(src, evs);
    REQUIRE(evs.size() == 1u);
    CHECK(evs[0].state == TouchState::Down);
}

// ★ Day la bai kiem tra quan trong nhat cua G14 o muc nguon.
//
//   Thieu buoc suy ra tu 'alive' thi cham roi nhac tay se KHONG sinh su
//   kien Up nao, va diem chi bien mat sau khi expireStalePoints don — tuc
//   la hieu ung tre dung MOT GIAY moi lan nhac tay.
TEST_CASE("★ G14: nhac tay -> sinh su kien Up NGAY, khong doi het han") {
    OscConfig cfg;
    cfg.protocol = OscProtocol::Tuio;
    OscSource src(cfg);

    const auto p1 = tuioPacket({{1, 0.2f, 0.2f}, {2, 0.8f, 0.8f}}, {1, 2}, 1);
    src.feedPacket(p1.data(), p1.size());

    std::vector<TouchEvent> evs;
    drainEvents(src, evs);
    REQUIRE(evs.size() == 2u);          // hai Down

    // Nhac ngon thu hai: no chi bien mat khoi 'alive'.
    const auto p2 = tuioPacket({{1, 0.25f, 0.2f}}, {1}, 2);
    src.feedPacket(p2.data(), p2.size());

    evs.clear();
    drainEvents(src, evs);
    REQUIRE(evs.size() == 1u);
    CHECK(evs[0].state == TouchState::Up);
    CHECK(evs[0].id == 2u);

    REQUIRE(src.frames().consume());
    CHECK(src.frames().readSlot().count == 1);
}

TEST_CASE("G14: nguon TUIO khong dinh gi toi phuong ngu Hexmap") {
    // Cung mot lop, hai che do — gui goi Hexmap vao nguon TUIO thi khong
    // duoc sinh diem nao.
    OscConfig cfg;
    cfg.protocol = OscProtocol::Tuio;
    OscSource src(cfg);

    std::vector<uint8_t> m;
    pushStr(m, "/hexmap/touch");
    pushStr(m, ",iff");
    pushI32(m, 1);
    pushF32(m, 100.0f);
    pushF32(m, 100.0f);
    src.feedPacket(m.data(), m.size());

    std::vector<TouchEvent> evs;
    CHECK(drainEvents(src, evs) == 0);
}

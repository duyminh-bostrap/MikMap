#include "TestHarness.h"

#include "core/filter/OneEuroFilter.h"
#include "core/filter/PointTracker.h"

#include <algorithm>
#include <cmath>
#include <random>

using namespace mikmap;

// ═══════════════════════════════════════════════════════════════════════
//  G11 — OneEuroFilter
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("G11: mau dau tien tra ve nguyen gia tri") {
    // Bat dau bang mot gia tri da loc san se tao cu nhay gia o khung 2.
    OneEuroFilter f;
    REQUIRE_NEAR(f.filter(42.0, 0.0), 42.0, 1e-12);
}

TEST_CASE("G11: dau vao khong doi thi dau ra hoi tu ve dung gia tri do") {
    OneEuroFilter f;
    double out = 0.0;
    for (int i = 0; i < 200; ++i) out = f.filter(10.0, i / 60.0);
    REQUIRE_NEAR(out, 10.0, 1e-6);
}

TEST_CASE("★★ G11: DUNG YEN thi khu duoc nhieu") {
    // Kich ban that: ngon tay dat yen, sensor van bao toa do nhay +-2mm.
    OneEuroParams p;
    p.minCutoff = 0.5;      // loc manh khi dung yen
    p.beta = 0.007;
    OneEuroFilter f(p);

    std::mt19937 rng(7);
    std::normal_distribution<double> noise(0.0, 2.0);

    double rawSpread = 0.0, filteredSpread = 0.0;
    double lastRaw = 0.0, lastFiltered = 0.0;
    bool first = true;

    for (int i = 0; i < 300; ++i) {
        const double t = i / 60.0;
        const double raw = 100.0 + noise(rng);
        const double filtered = f.filter(raw, t);

        if (!first && i > 60) {                 // bo qua giai doan on dinh
            rawSpread      += std::abs(raw - lastRaw);
            filteredSpread += std::abs(filtered - lastFiltered);
        }
        lastRaw = raw;
        lastFiltered = filtered;
        first = false;
    }

    // ★ Do rung cua tin hieu da loc phai NHO HON HAN tin hieu tho.
    REQUIRE(filteredSpread < rawSpread * 0.25);
}

TEST_CASE("★★ G11: DI NHANH thi bam sat, khong bi tre") {
    // Day la diem khac biet cua One Euro so voi loc trung binh truot:
    // loc manh luc dung yen NHUNG khong tre khi di chuyen nhanh.
    OneEuroParams p;
    p.minCutoff = 0.5;
    p.beta = 0.5;           // noi long manh theo toc do
    OneEuroFilter f(p);

    // Tin hieu doc tuyen tinh nhanh: 600 don vi/giay.
    double lastErr = 0.0;
    for (int i = 0; i < 120; ++i) {
        const double t = i / 60.0;
        const double raw = 600.0 * t;
        const double filtered = f.filter(raw, t);
        if (i > 30) lastErr = std::abs(raw - filtered);
    }

    // Sai so bam duoi 15 don vi = duoi 25ms tre o toc do nay.
    REQUIRE(lastErr < 15.0);
}

TEST_CASE("★ G11: beta cao bam tot hon beta thap khi di nhanh") {
    auto trackingError = [](double beta) {
        OneEuroParams p;
        p.minCutoff = 0.3;
        p.beta = beta;
        OneEuroFilter f(p);
        double err = 0.0;
        for (int i = 0; i < 120; ++i) {
            const double t = i / 60.0;
            const double raw = 500.0 * t;
            const double out = f.filter(raw, t);
            if (i > 30) err += std::abs(raw - out);
        }
        return err;
    };

    REQUIRE(trackingError(0.5) < trackingError(0.001));
}

TEST_CASE("★ G11: hai mau cung moc thoi gian khong sinh NaN") {
    // dt = 0 lam van toc bang vo cuc. Neu khong kep lai thi NaN se lan
    // ra toan bo he thong va khong the lan nguoc ve nguyen nhan.
    OneEuroFilter f;
    f.filter(10.0, 1.0);
    const double out = f.filter(20.0, 1.0);      // cung moc thoi gian
    REQUIRE(std::isfinite(out));
}

TEST_CASE("G11: gia tri khong huu han bi bo qua, khong lam hong bo loc") {
    OneEuroFilter f;
    f.filter(50.0, 0.0);
    const double out = f.filter(std::nan(""), 0.1);
    REQUIRE(std::isfinite(out));
    REQUIRE_NEAR(f.filter(50.0, 0.2), 50.0, 1.0);
}

TEST_CASE("G11: reset lam bo loc bat dau lai") {
    OneEuroFilter f;
    for (int i = 0; i < 50; ++i) f.filter(100.0, i / 60.0);
    f.reset();
    REQUIRE_NEAR(f.filter(7.0, 0.0), 7.0, 1e-12);
}

TEST_CASE("G11: ban 2D loc hai truc doc lap") {
    OneEuroFilter2D f;
    const Vec2 out = f.filter(Vec2{3.0, 4.0}, 0.0);
    REQUIRE_VEC_NEAR(out, 3.0, 4.0, 1e-12);
}

// ═══════════════════════════════════════════════════════════════════════
//  G12 — PointTracker
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("G12: diem moi duoc cap ID va danh dau isNew") {
    PointTracker t;
    t.update({Vec2{100, 100}, Vec2{500, 500}}, 0.0);

    REQUIRE(t.tracks().size() == 2u);
    CHECK(t.tracks()[0].isNew);
    CHECK(t.tracks()[1].isNew);
    CHECK(t.tracks()[0].id != t.tracks()[1].id);
}

TEST_CASE("★★ G12: diem di chuyen GIU NGUYEN ID qua cac frame") {
    // Khong co ID ben vung thi khong biet dau la "cung mot ngon tay",
    // va OneEuroFilter se loc nham quy dao cua hai nguoi thanh mot.
    PointTracker t;
    t.update({Vec2{100, 100}}, 0.0);
    const uint32_t id = t.tracks()[0].id;

    for (int i = 1; i <= 20; ++i) {
        t.update({Vec2{100.0 + i * 5.0, 100.0}}, i / 60.0);
        REQUIRE(t.tracks().size() == 1u);
        REQUIRE(t.tracks()[0].id == id);      // ★ ID khong doi
        REQUIRE(!t.tracks()[0].isNew);
    }
    REQUIRE_NEAR(t.tracks()[0].position.x, 200.0, 1e-9);
}

TEST_CASE("★★ G12: THU TU dau vao doi khong lam doi ID") {
    // Kinect/LiDAR tra ve danh sach blob voi thu tu tuy thuat toan quet,
    // co the dao moi frame.
    PointTracker t;
    t.update({Vec2{100, 100}, Vec2{800, 800}}, 0.0);

    const uint32_t idA = t.tracks()[0].id;   // gan (100,100)
    const uint32_t idB = t.tracks()[1].id;   // gan (800,800)

    // Frame sau: DAO thu tu, moi diem nhich mot chut.
    t.update({Vec2{805, 805}, Vec2{105, 105}}, 0.016);

    REQUIRE(t.tracks().size() == 2u);
    for (const auto& tp : t.tracks()) {
        if (tp.id == idA) CHECK_NEAR(tp.position.x, 105.0, 1e-6);
        if (tp.id == idB) CHECK_NEAR(tp.position.x, 805.0, 1e-6);
    }
}

TEST_CASE("★ G12: nhay QUA XA thi coi la diem MOI, khong ghep nham") {
    PointTrackerParams p;
    p.maxMatchDistance = 100.0;
    PointTracker t(p);

    t.update({Vec2{0, 0}}, 0.0);
    const uint32_t id = t.tracks()[0].id;

    t.update({Vec2{500, 500}}, 0.016);   // xa hon nguong

    // Diem cu con trong an han, diem moi duoc tao.
    bool foundNew = false;
    for (const auto& tp : t.tracks()) {
        if (tp.id != id && tp.isNew) foundNew = true;
    }
    REQUIRE(foundNew);
}

TEST_CASE("★★ G12: mat dau vai frame KHONG lam mat ID (an han)") {
    // Sensor hay mat dau roi bat lai. Xoa ID ngay se lam hieu ung dang
    // chay bi ngat roi khoi dong lai.
    PointTrackerParams p;
    p.graceSec = 0.15;
    PointTracker t(p);

    t.update({Vec2{100, 100}}, 0.0);
    const uint32_t id = t.tracks()[0].id;

    // Mat dau 5 frame (~83ms), van trong an han.
    for (int i = 1; i <= 5; ++i) t.update({}, i / 60.0);
    REQUIRE(t.tracks().size() == 1u);
    REQUIRE(t.tracks()[0].id == id);
    REQUIRE(t.tracks()[0].isCoasting);
    REQUIRE(t.activeTracks().empty());        // khong "dang thay"

    // Bat lai duoc, VAN dung ID cu.
    t.update({Vec2{110, 100}}, 6 / 60.0);
    REQUIRE(t.tracks()[0].id == id);
    REQUIRE(!t.tracks()[0].isCoasting);
}

TEST_CASE("★ G12: qua an han thi bo han va bao justLost") {
    PointTrackerParams p;
    p.graceSec = 0.1;
    PointTracker t(p);

    t.update({Vec2{100, 100}}, 0.0);
    const uint32_t id = t.tracks()[0].id;

    t.update({}, 0.05);
    REQUIRE(t.tracks().size() == 1u);
    REQUIRE(t.justLost().empty());

    t.update({}, 0.2);                        // vuot an han
    REQUIRE(t.tracks().empty());
    REQUIRE(t.justLost().size() == 1u);
    REQUIRE(t.justLost()[0] == id);
}

TEST_CASE("G12: uoc luong van toc") {
    PointTracker t;
    t.update({Vec2{0, 0}}, 0.0);
    t.update({Vec2{60, 0}}, 0.1);             // 60 don vi trong 0.1s

    REQUIRE_NEAR(t.tracks()[0].velocity.x, 600.0, 1.0);
}

TEST_CASE("★ G12: ID KHONG duoc dung lai sau reset") {
    // Dung lai ID se khien hieu ung gan voi ID do nhan nham mot diem
    // cham hoan toan khac.
    PointTracker t;
    t.update({Vec2{0, 0}}, 0.0);
    const uint32_t first = t.tracks()[0].id;

    t.reset();
    t.update({Vec2{0, 0}}, 1.0);
    REQUIRE(t.tracks()[0].id != first);
}

TEST_CASE("G12: nhieu diem gan nhau ghep dung tung cai") {
    PointTracker t;
    const std::vector<Vec2> frame1 = {
        Vec2{100, 100}, Vec2{200, 100}, Vec2{300, 100}};
    t.update(frame1, 0.0);
    REQUIRE(t.tracks().size() == 3u);

    std::vector<uint32_t> ids;
    for (const auto& tp : t.tracks()) ids.push_back(tp.id);

    // Ca ba nhich sang phai 10 don vi.
    t.update({Vec2{110, 100}, Vec2{210, 100}, Vec2{310, 100}}, 0.016);
    REQUIRE(t.tracks().size() == 3u);
    for (const auto& tp : t.tracks()) {
        CHECK(std::find(ids.begin(), ids.end(), tp.id) != ids.end());
        CHECK(!tp.isNew);
    }
}

TEST_CASE("G12: activeTracks chi tra ve diem dang thay") {
    PointTracker t;
    t.update({Vec2{0, 0}, Vec2{500, 500}}, 0.0);
    t.update({Vec2{5, 5}}, 0.016);            // mat diem thu hai

    REQUIRE(t.tracks().size() == 2u);         // con trong an han
    REQUIRE(t.activeTracks().size() == 1u);   // nhung chi 1 dang thay
}

TEST_CASE("G12: khong co quan sat nao va khong co track thi khong sap") {
    PointTracker t;
    t.update({}, 0.0);
    REQUIRE(t.tracks().empty());
    REQUIRE(t.justLost().empty());
}

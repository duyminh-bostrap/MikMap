#include "TestHarness.h"

#include "core/model/ProjectIO.h"
#include "core/model/TriggerZone.h"

using namespace hexmap;

namespace {

TriggerZone makeZone(double x, double y, double w, double h,
                     const std::string& name = "Z") {
    TriggerZone z;
    z.name = name;
    z.origin = Vec2{x, y};
    z.size = Vec2{w, h};
    z.action = TriggerAction::TriggerClip;
    z.cooldownSec = 0.35;
    return z;
}

const Vec2 kInside{50, 50};
const Vec2 kOutside{500, 500};

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  Hinh hoc
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("G17: contains bao dung trong / ngoai") {
    const TriggerZone z = makeZone(100, 100, 200, 150);
    CHECK(z.contains(Vec2{150, 150}));
    CHECK(z.contains(Vec2{100, 100}));    // goc tren-trai tinh la trong
    CHECK(z.contains(Vec2{300, 250}));    // goc duoi-phai tinh la trong
    CHECK(!z.contains(Vec2{99, 150}));
    CHECK(!z.contains(Vec2{301, 150}));
    CHECK(!z.contains(Vec2{150, 251}));
}

// ═══════════════════════════════════════════════════════════════════════
//  ★★ Kich hoat theo CANH LEN (di vao vung)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★★ G17: diem DI VAO vung thi kich hoat") {
    // Day la kich ban that voi sensor tracking (Kinect/LiDAR/Mock):
    // diem ton tai san, di chuyen vao vung. KHONG co su kien Down nao moi.
    TriggerZoneSet set;
    set.zones.push_back(makeZone(0, 0, 100, 100));

    // Frame 1: diem o ngoai.
    REQUIRE(set.update({kOutside}, 1.0).empty());

    // Frame 2: diem da di vao -> kich hoat.
    const auto hits = set.update({kInside}, 1.02);
    REQUIRE(hits.size() == 1u);
    REQUIRE(hits[0].fired);
    REQUIRE(hits[0].zoneIndex == 0);
}

TEST_CASE("★★ G17: GIU trong vung KHONG kich hoat lien tuc") {
    // Neu khong phat hien canh len ma chi kiem tra "co diem trong vung",
    // giu ngon tay yen se trigger 60 lan/giay.
    TriggerZoneSet set;
    TriggerZone z = makeZone(0, 0, 100, 100);
    z.cooldownSec = 0.0;               // tat chong doi de chi con canh len
    set.zones.push_back(z);

    REQUIRE(set.update({kInside}, 0.0).size() == 1u);   // vao -> kich hoat

    int extra = 0;
    for (int i = 1; i <= 60; ++i) {
        extra += static_cast<int>(set.update({kInside}, i * 0.016).size());
    }
    REQUIRE(extra == 0);               // ★ giu yen thi im lang
}

TEST_CASE("★ G17: ra roi vao lai thi kich hoat lan nua") {
    TriggerZoneSet set;
    TriggerZone z = makeZone(0, 0, 100, 100);
    z.cooldownSec = 0.0;
    set.zones.push_back(z);

    REQUIRE(set.update({kInside}, 0.0).size() == 1u);
    REQUIRE(set.update({kOutside}, 0.1).empty());       // ra
    REQUIRE(set.update({kInside}, 0.2).size() == 1u);   // vao lai
}

TEST_CASE("G17: khong co diem nao thi khong kich hoat") {
    TriggerZoneSet set;
    set.zones.push_back(makeZone(0, 0, 100, 100));
    REQUIRE(set.update({}, 0.0).empty());
}

TEST_CASE("G17: vung bi tat khong bao gio kich hoat") {
    TriggerZoneSet set;
    TriggerZone z = makeZone(0, 0, 100, 100);
    z.enabled = false;
    set.zones.push_back(z);
    REQUIRE(set.update({kInside}, 0.0).empty());
}

TEST_CASE("G17: nhieu vung kich hoat cung frame") {
    TriggerZoneSet set;
    set.zones.push_back(makeZone(0, 0, 100, 100, "A"));
    set.zones.push_back(makeZone(200, 200, 100, 100, "B"));

    const auto hits = set.update({Vec2{50, 50}, Vec2{250, 250}}, 0.0);
    REQUIRE(hits.size() == 2u);
}

// ═══════════════════════════════════════════════════════════════════════
//  ★ Cooldown
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★★ G17: COOLDOWN chan rung o mep vung") {
    // Ngon tay dat ngay mep se tao chuoi vao-ra-vao-ra hang chuc lan
    // moi giay. Khong chong doi thi clip bi trigger lai lien tuc.
    TriggerZoneSet set;
    TriggerZone z = makeZone(0, 0, 100, 100);
    z.cooldownSec = 0.5;
    set.zones.push_back(z);

    REQUIRE(set.update({kInside}, 10.0).size() == 1u);

    // Mo phong rung: vao/ra 20 lan trong 300ms.
    int extra = 0;
    for (int i = 1; i <= 20; ++i) {
        const double t = 10.0 + i * 0.015;
        set.update({kOutside}, t);                 // ra
        extra += static_cast<int>(set.update({kInside}, t + 0.005).size());
    }
    REQUIRE(extra == 0);               // ★ khong mot lan nao lot qua

    REQUIRE(set.update({kOutside}, 10.6).empty());
    REQUIRE(set.update({kInside}, 10.65).size() == 1u);   // het cooldown
}

TEST_CASE("★ G17: cooldown RIENG cho tung vung") {
    TriggerZoneSet set;
    set.zones.push_back(makeZone(0, 0, 100, 100, "A"));
    set.zones.push_back(makeZone(200, 0, 100, 100, "B"));

    REQUIRE(set.update({Vec2{50, 50}}, 5.0).size() == 1u);        // A
    REQUIRE(set.update({Vec2{250, 50}}, 5.01).size() == 1u);      // B ngay sau
}

TEST_CASE("G17: resetRuntimeState xoa cooldown va trang thai") {
    TriggerZoneSet set;
    set.zones.push_back(makeZone(0, 0, 100, 100));

    REQUIRE(set.update({kInside}, 100.0).size() == 1u);
    set.update({kOutside}, 100.05);
    REQUIRE(set.update({kInside}, 100.1).empty());   // dang cooldown

    set.resetRuntimeState();
    REQUIRE(set.update({kInside}, 100.1).size() == 1u);
}

// ═══════════════════════════════════════════════════════════════════════
//  ★ Vung chong nhau
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★★ G17: diem trong vung chong nhau CHI danh thuc vung tren cung") {
    // Neu khong, mot diem roi vao vung nho nam tren cung danh thuc luon
    // vung nen phia duoi -> hai clip cung nhay ra.
    TriggerZoneSet set;
    set.zones.push_back(makeZone(0, 0, 400, 400, "nen"));       // [0] duoi
    set.zones.push_back(makeZone(100, 100, 100, 100, "tren"));  // [1] tren
    set.zones[0].targetColumn = 0;
    set.zones[1].targetColumn = 7;

    const auto hits = set.update({Vec2{150, 150}}, 1.0);
    REQUIRE(hits.size() == 1u);        // ★ CHI mot vung
    REQUIRE(hits[0].zoneIndex == 1);
    REQUIRE(hits[0].column == 7);
}

TEST_CASE("G17: cham ngoai vung tren thi vung duoi van nhan") {
    TriggerZoneSet set;
    set.zones.push_back(makeZone(0, 0, 400, 400, "nen"));
    set.zones.push_back(makeZone(100, 100, 100, 100, "tren"));

    const auto hits = set.update({Vec2{350, 350}}, 1.0);
    REQUIRE(hits.size() == 1u);
    REQUIRE(hits[0].zoneIndex == 0);
}

TEST_CASE("★ G17: vung tren bi TAT thi vung duoi nhan duoc diem") {
    TriggerZoneSet set;
    set.zones.push_back(makeZone(0, 0, 400, 400, "nen"));
    set.zones.push_back(makeZone(100, 100, 100, 100, "tren"));
    set.zones[1].enabled = false;

    const auto hits = set.update({Vec2{150, 150}}, 1.0);
    REQUIRE(hits.size() == 1u);
    REQUIRE(hits[0].zoneIndex == 0);
}

// ═══════════════════════════════════════════════════════════════════════
//  Hanh dong + hien thi
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("G17: hit mang du thong tin de thuc thi") {
    TriggerZoneSet set;
    TriggerZone z = makeZone(0, 0, 100, 100);
    z.action = TriggerAction::TriggerColumn;
    z.targetLayer = 2;
    z.targetColumn = 5;
    set.zones.push_back(z);

    const auto hits = set.update({kInside}, 0.0);
    REQUIRE(hits.size() == 1u);
    CHECK(hits[0].action == TriggerAction::TriggerColumn);
    CHECK(hits[0].layer == 2);
    CHECK(hits[0].column == 5);
}

TEST_CASE("G17: co occupied duoc cap nhat cho UI to sang") {
    TriggerZoneSet set;
    set.zones.push_back(makeZone(0, 0, 100, 100));

    set.update({kInside}, 0.0);
    CHECK(set.zones[0].occupied);

    set.update({}, 0.1);
    CHECK(!set.zones[0].occupied);
}

TEST_CASE("G17: ten hanh dong round-trip") {
    const TriggerAction all[] = {
        TriggerAction::None, TriggerAction::TriggerClip,
        TriggerAction::TriggerColumn, TriggerAction::ClearLayer,
        TriggerAction::ClearAll,
    };
    for (const TriggerAction a : all) {
        CHECK(triggerActionFromName(triggerActionName(a)) == a);
    }
    CHECK(triggerActionFromName("KhongTonTai") == TriggerAction::None);
    CHECK(triggerActionFromName(nullptr) == TriggerAction::None);
}

// ═══════════════════════════════════════════════════════════════════════
//  ProjectIO
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ G17: trigger zone round-trip qua project file") {
    Project a;
    TriggerZone z = makeZone(120, 80, 340, 220, "Nut trai");
    z.action = TriggerAction::TriggerColumn;
    z.targetLayer = 1;
    z.targetColumn = 4;
    z.cooldownSec = 0.75;
    z.enabled = false;
    a.triggerZones.zones.push_back(z);
    a.triggerZones.zones.push_back(makeZone(500, 80, 200, 200, "Nut phai"));

    Project b;
    REQUIRE(projectio::fromJson(projectio::toJson(a), b).ok);
    REQUIRE(b.triggerZones.count() == 2);

    const TriggerZone& r = b.triggerZones.zones[0];
    CHECK(r.name == "Nut trai");
    CHECK(!r.enabled);
    CHECK_NEAR(r.origin.x, 120.0, 1e-9);
    CHECK_NEAR(r.size.y, 220.0, 1e-9);
    CHECK(r.action == TriggerAction::TriggerColumn);
    CHECK(r.targetLayer == 1);
    CHECK(r.targetColumn == 4);
    CHECK_NEAR(r.cooldownSec, 0.75, 1e-9);
}

TEST_CASE("★ G17: KHONG luu trang thai cooldown vao file") {
    // Luu vao thi mo project len vung se "dang trong cooldown" — nguoi
    // dung cham ma khong thay gi xay ra.
    Project a;
    a.triggerZones.zones.push_back(makeZone(0, 0, 100, 100));
    a.triggerZones.update({kInside}, 9999.0);        // dat lastFiredSec

    const std::string text = projectio::toJson(a);
    REQUIRE(text.find("lastFired") == std::string::npos);
    REQUIRE(text.find("occupied") == std::string::npos);

    Project b;
    REQUIRE(projectio::fromJson(text, b).ok);
    REQUIRE(b.triggerZones.update({kInside}, 0.0).size() == 1u);
}

TEST_CASE("G17: project khong co truong triggerZones van nap duoc") {
    Project b;
    REQUIRE(projectio::fromJson("{\"name\":\"cu\"}", b).ok);
    REQUIRE(b.triggerZones.count() == 0);
}

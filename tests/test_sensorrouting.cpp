#include "TestHarness.h"

#include "core/calib/SensorRouting.h"

#include <string>
#include <vector>

using namespace mikmap;

namespace {

/// Hồ sơ đã calibrate xong: bốn cặp điểm là đủ giải homography.
CalibrationProfile makeSolved(const char* name, uint16_t src, int screenId) {
    CalibrationProfile p;
    p.name = name;
    p.sourceId = src;
    p.targetScreenId = screenId;

    p.addPair(Vec2{0.0,    0.0},    Vec2{0.0,    0.0});
    p.addPair(Vec2{1000.0, 0.0},    Vec2{1920.0, 0.0});
    p.addPair(Vec2{1000.0, 800.0},  Vec2{1920.0, 1080.0});
    p.addPair(Vec2{0.0,    800.0},  Vec2{0.0,    1080.0});
    p.solve();
    return p;
}

Screen makeScreen(int id, const char* name) {
    return Screen(id, name, Vec2{1920.0, 1080.0});
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  G18 — định tuyến nhiều sensor
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("G18: mot sensor, mot screen -> mot tuyen") {
    const std::vector<CalibrationProfile> profiles{makeSolved("Sensor 1", 0, 0)};
    const std::vector<Screen> screens{makeScreen(0, "P1")};

    const SensorRoutingTable t = buildSensorRoutes(profiles, screens);
    REQUIRE(t.routes.size() == 1u);
    CHECK(t.warnings.empty());

    const SensorRoute* r = t.find(0);
    REQUIRE(r != nullptr);
    CHECK(r->profileIndex == 0u);
    CHECK(r->screenIndex == 0u);
}

TEST_CASE("★ G18: BA sensor -> BA screen, moi cai di duong rieng") {
    const std::vector<CalibrationProfile> profiles{
        makeSolved("LiDAR san",  0, 10),
        makeSolved("Khung IR",   1, 11),
        makeSolved("Kinect buc", 2, 12),
    };
    const std::vector<Screen> screens{
        makeScreen(10, "San"), makeScreen(11, "Tuong ben"), makeScreen(12, "Buc"),
    };

    const SensorRoutingTable t = buildSensorRoutes(profiles, screens);
    REQUIRE(t.routes.size() == 3u);
    CHECK(t.warnings.empty());

    CHECK(t.find(0)->screenIndex == 0u);
    CHECK(t.find(1)->screenIndex == 1u);
    CHECK(t.find(2)->screenIndex == 2u);
}

// ★ targetScreenId khop theo Screen::id, KHONG theo vi tri trong mang.
//
//   Xoa mot screen o giua se lam moi vi tri sau no dich di mot — va khi
//   do moi cam bien lang le chieu sang MAY CHIEU BEN CANH. Sai theo kieu
//   trong van "co chay", nen rat lau moi bi phat hien.
TEST_CASE("★ G18: khop theo ID chu khong theo vi tri trong mang") {
    const std::vector<CalibrationProfile> profiles{makeSolved("S", 0, 7)};

    // Screen id 7 nam o vi tri THU BA.
    const std::vector<Screen> screens{
        makeScreen(3, "A"), makeScreen(5, "B"), makeScreen(7, "C"),
    };

    const SensorRoutingTable t = buildSensorRoutes(profiles, screens);
    REQUIRE(t.routes.size() == 1u);
    CHECK(t.routes[0].screenIndex == 2u);      // khong phai 0
}

// ★ KHONG duoc lui ve screen 0 khi screen dich khong ton tai.
//
//   Lui ve screen 0 thi diem cham VAN xuat hien, chi la o SAI MAY CHIEU.
//   Trong nhu phan mem dang chay, nen nguoi ta se di tim loi o cho khac
//   (calibration? sensor?) thay vi thay ngay la screen dich da bi xoa.
TEST_CASE("★ G18: screen dich khong ton tai -> BO TUYEN + canh bao") {
    const std::vector<CalibrationProfile> profiles{makeSolved("S", 0, 99)};
    const std::vector<Screen> screens{makeScreen(0, "P1")};

    const SensorRoutingTable t = buildSensorRoutes(profiles, screens);
    CHECK(t.routes.empty());
    REQUIRE(t.warnings.size() == 1u);
    CHECK(t.warnings[0].find("99") != std::string::npos);
    CHECK(t.find(0) == nullptr);
}

// ★ Hai ho so cung sourceId: giu cai DAU, noi ro.
//
//   Lay cai sau nghia la hanh vi phu thuoc THU TU TRONG FILE — nguoi van
//   hanh se thay hieu ung "tu nhien nhay sang cho khac" sau khi luu lai
//   project ma khong doi gi ca.
TEST_CASE("★ G18: hai ho so cung nguon -> giu cai dau, canh bao") {
    const std::vector<CalibrationProfile> profiles{
        makeSolved("Dung",  0, 0),
        makeSolved("Trung", 0, 1),
    };
    const std::vector<Screen> screens{makeScreen(0, "P1"), makeScreen(1, "P2")};

    const SensorRoutingTable t = buildSensorRoutes(profiles, screens);
    REQUIRE(t.routes.size() == 1u);
    CHECK(t.routes[0].profileIndex == 0u);      // ho so DAU
    CHECK(t.routes[0].screenIndex == 0u);
    REQUIRE(t.warnings.size() == 1u);
    CHECK(t.warnings[0].find("Trung") != std::string::npos);
}

// Ho so chua calibrate xong thi khong map duoc — nhung phai NOI RA.
// Nguoi van hanh cam cam bien vao, khong thay gi xay ra, va khong co cach
// nao biet la vi chua calibrate.
TEST_CASE("★ G18: ho so chua calibrate -> bo tuyen kem canh bao ro rang") {
    CalibrationProfile chua;
    chua.name = "Chua xong";
    chua.sourceId = 4;
    chua.targetScreenId = 0;
    chua.addPair(Vec2{0.0, 0.0}, Vec2{0.0, 0.0});   // moi mot cap

    const std::vector<CalibrationProfile> profiles{chua};
    const std::vector<Screen> screens{makeScreen(0, "P1")};

    const SensorRoutingTable t = buildSensorRoutes(profiles, screens);
    CHECK(t.routes.empty());
    REQUIRE(t.warnings.size() == 1u);
    CHECK(t.warnings[0].find("Chua xong") != std::string::npos);
}

// ★ Nguon la KHONG duoc di nho duong cua nguon khac.
//
//   Neu lui ve "ho so bat ky", cam bien thu hai cam vao se di qua phep
//   hieu chinh cua cam bien thu nhat va tha diem o nhung cho TRONG CO VE
//   HOP LY — kieu sai kho phat hien nhat, vi khong co gi bao loi.
TEST_CASE("★ G18: nguon khong co tuyen thi tra ve nullptr, khong muon tam") {
    const std::vector<CalibrationProfile> profiles{makeSolved("S", 0, 0)};
    const std::vector<Screen> screens{makeScreen(0, "P1")};

    const SensorRoutingTable t = buildSensorRoutes(profiles, screens);
    REQUIRE(t.find(0) != nullptr);
    CHECK(t.find(1) == nullptr);      // nguon 1 chua khai bao
    CHECK(t.find(99) == nullptr);
}

TEST_CASE("G18: khong co ho so nao -> bang rong, khong canh bao") {
    const SensorRoutingTable t = buildSensorRoutes({}, {makeScreen(0, "P1")});
    CHECK(t.routes.empty());
    CHECK(t.warnings.empty());
}

TEST_CASE("G18: khong co screen nao -> moi tuyen deu bi bo, co canh bao") {
    const std::vector<CalibrationProfile> profiles{makeSolved("S", 0, 0)};
    const SensorRoutingTable t = buildSensorRoutes(profiles, {});
    CHECK(t.routes.empty());
    CHECK(t.warnings.size() == 1u);
}

// Mot cam bien hong khong duoc keo theo cac cam bien khac.
TEST_CASE("★ G18: mot ho so hong khong lam hong cac tuyen con lai") {
    const std::vector<CalibrationProfile> profiles{
        makeSolved("Tot 1", 0, 0),
        makeSolved("Hong",  1, 404),      // screen khong ton tai
        makeSolved("Tot 2", 2, 1),
    };
    const std::vector<Screen> screens{makeScreen(0, "P1"), makeScreen(1, "P2")};

    const SensorRoutingTable t = buildSensorRoutes(profiles, screens);
    REQUIRE(t.routes.size() == 2u);
    CHECK(t.find(0) != nullptr);
    CHECK(t.find(1) == nullptr);
    CHECK(t.find(2) != nullptr);
    CHECK(t.warnings.size() == 1u);
}

#include "TestHarness.h"

#include "core/util/AppSettings.h"

#include <cstdio>
#include <fstream>
#include <string>

using namespace mikmap;

namespace {

void writeFile(const std::string& path, const std::string& text) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(text.data(), static_cast<std::streamsize>(text.size()));
}

void removeFile(const std::string& path) {
    std::remove(path.c_str());
    std::remove((path + ".tmp").c_str());
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  AppSettings — vong tron luu / nap
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("AppSettings: luu roi nap lai giu nguyen tham so sensor") {
    const std::string path = "mikmap_test_settings.json";

    AppSettings a;
    a.sensorMinCutoff  = 0.4;
    a.sensorBeta       = 0.05;
    a.trackMaxDistance = 220.0;
    a.trackGraceSec    = 0.4;

    std::string err;
    REQUIRE(a.save(path, err));

    AppSettings b;
    std::string warn;
    REQUIRE(b.load(path, warn));

    CHECK_NEAR(b.sensorMinCutoff,  0.4,   1e-9);
    CHECK_NEAR(b.sensorBeta,       0.05,  1e-9);
    CHECK_NEAR(b.trackMaxDistance, 220.0, 1e-9);
    CHECK_NEAR(b.trackGraceSec,    0.4,   1e-9);

    removeFile(path);
}

// ★ File thieu khoa la chuyen BINH THUONG, khong phai loi: settings.json
//   cua ban cu khong co bon khoa nay. Nap phai ra dung mac dinh chu khong
//   phai 0 — minCutoff = 0 lam bo loc dung hinh, va nguoi dung se thay
//   "diem sensor khong nhuc nhich" sau khi cap nhat phan mem.
TEST_CASE("★ AppSettings: file cu thieu khoa sensor -> ve dung mac dinh") {
    const std::string path = "mikmap_test_settings_old.json";
    writeFile(path, "{\"language\":\"en\",\"vsync\":false}");

    AppSettings s;
    std::string warn;
    REQUIRE(s.load(path, warn));

    const AppSettings def;
    CHECK_NEAR(s.sensorMinCutoff,  def.sensorMinCutoff,  1e-9);
    CHECK_NEAR(s.sensorBeta,       def.sensorBeta,       1e-9);
    CHECK_NEAR(s.trackMaxDistance, def.trackMaxDistance, 1e-9);
    CHECK_NEAR(s.trackGraceSec,    def.trackGraceSec,    1e-9);

    removeFile(path);
}

// ★ File sua tay co the chua so vo ly. Hai gia tri nguy hiem nhat:
//     minCutoff = 0        -> bo loc khong bao gio duoi kip, diem dung hinh
//     maxMatchDistance = 0 -> khong diem nao khop frame truoc, ID doi moi
//                             frame, tuc la mat sach tinh ben vung cua G12
TEST_CASE("★ AppSettings: so vo ly bi kep ve khoang dung duoc") {
    const std::string path = "mikmap_test_settings_bad.json";
    writeFile(path,
              "{\"sensorMinCutoff\":0,\"sensorBeta\":-3,"
              "\"trackMaxDistance\":0,\"trackGraceSec\":-1}");

    AppSettings s;
    std::string warn;
    REQUIRE(s.load(path, warn));

    CHECK(s.sensorMinCutoff  >= 0.01);
    CHECK(s.sensorBeta       >= 0.0);
    CHECK(s.trackMaxDistance >= 1.0);
    CHECK(s.trackGraceSec    >= 0.0);

    removeFile(path);
}

TEST_CASE("AppSettings: file hong -> khong sap, dung mac dinh") {
    const std::string path = "mikmap_test_settings_broken.json";
    writeFile(path, "{ khong phai json");

    AppSettings s;
    std::string warn;
    CHECK(!s.load(path, warn));          // bao that bai...
    CHECK(!warn.empty());
    CHECK_NEAR(s.sensorMinCutoff, AppSettings().sensorMinCutoff, 1e-9);  // ...nhung van dung duoc

    removeFile(path);
}

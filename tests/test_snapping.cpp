#include "TestHarness.h"

#include "core/math/Snapping.h"

#include <cmath>
#include <limits>

using namespace mikmap;

// ═══════════════════════════════════════════════════════════════════════
//  F21 — hút điểm về đường gióng
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("F21: khong co duong gion nao -> giu nguyen diem") {
    const SnapResult r = snapPoint(Vec2{100.0, 200.0}, {}, {}, 8.0);
    CHECK_NEAR(r.position.x, 100.0, 1e-12);
    CHECK_NEAR(r.position.y, 200.0, 1e-12);
    CHECK(!r.snappedX);
    CHECK(!r.snappedY);
}

TEST_CASE("F21: trong nguong thi hut, ngoai nguong thi khong") {
    const SnapResult in = snapPoint(Vec2{103.0, 0.0}, {100.0}, {}, 8.0);
    CHECK(in.snappedX);
    CHECK_NEAR(in.position.x, 100.0, 1e-12);
    CHECK_NEAR(in.guideX, 100.0, 1e-12);

    const SnapResult out = snapPoint(Vec2{120.0, 0.0}, {100.0}, {}, 8.0);
    CHECK(!out.snappedX);
    CHECK_NEAR(out.position.x, 120.0, 1e-12);
}

// ★ Hai truc PHAI doc lap.
//
//   Chi hut khi ca x lan y cung khop thi tinh nang gan nhu khong bao gio
//   kich hoat: nguoi dung hay muon "thang cot voi goc kia" ma chieu con
//   lai thi tuy y.
TEST_CASE("★ F21: hut mot truc, truc kia giu nguyen") {
    const SnapResult r = snapPoint(Vec2{102.0, 555.0}, {100.0}, {300.0}, 8.0);
    CHECK(r.snappedX);
    CHECK(!r.snappedY);
    CHECK_NEAR(r.position.x, 100.0, 1e-12);
    CHECK_NEAR(r.position.y, 555.0, 1e-12);   // y khong bi dong vao
}

TEST_CASE("F21: hut duoc ca hai truc cung luc") {
    const SnapResult r = snapPoint(Vec2{102.0, 297.0}, {100.0}, {300.0}, 8.0);
    CHECK(r.snappedX);
    CHECK(r.snappedY);
    CHECK_NEAR(r.position.x, 100.0, 1e-12);
    CHECK_NEAR(r.position.y, 300.0, 1e-12);
}

// ★ Phai chon duong GAN NHAT, khong phai duong dau tien trong nguong.
//
//   Khi nhieu moc nam sat nhau (mep slice va mep man chi cach vai pixel),
//   lay duong dau tien nghia la ket qua phu thuoc THU TU trong mang —
//   tuc la nguoi dung khong doan duoc no se hut vao dau.
TEST_CASE("★ F21: chon duong gan nhat du no dung SAU trong danh sach") {
    // 106 gan 105 hon la gan 100; ca hai deu trong nguong 8.
    const SnapResult r = snapPoint(Vec2{106.0, 0.0}, {100.0, 105.0}, {}, 8.0);
    CHECK(r.snappedX);
    CHECK_NEAR(r.position.x, 105.0, 1e-12);

    // Dao thu tu mang -> ket qua PHAI khong doi.
    const SnapResult r2 = snapPoint(Vec2{106.0, 0.0}, {105.0, 100.0}, {}, 8.0);
    CHECK_NEAR(r2.position.x, 105.0, 1e-12);
}

TEST_CASE("F21: nguong <= 0 nghia la TAT hut") {
    for (const double t : {0.0, -1.0}) {
        const SnapResult r = snapPoint(Vec2{100.5, 0.0}, {100.0}, {}, t);
        CHECK(!r.snappedX);
        CHECK_NEAR(r.position.x, 100.5, 1e-12);
    }
}

TEST_CASE("F21: dung dung tren duong gion thi van bao la da hut") {
    const SnapResult r = snapPoint(Vec2{100.0, 0.0}, {100.0}, {}, 8.0);
    CHECK(r.snappedX);
    CHECK_NEAR(r.position.x, 100.0, 1e-12);
}

// Mot NaN lot vao danh sach moc (vd slice co warp suy bien) khong duoc
// lam hong ca phep hut — nguoi dung se thay hut "thinh thoang chet" ma
// khong co cach nao lan ra nguyen nhan.
TEST_CASE("★ F21: moc NaN bi bo qua, cac moc con lai van hut binh thuong") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const SnapResult r = snapPoint(Vec2{102.0, 0.0}, {nan, 100.0}, {}, 8.0);
    CHECK(r.snappedX);
    CHECK_NEAR(r.position.x, 100.0, 1e-12);
}

TEST_CASE("F21: diem NaN thi tra ve nguyen, khong sap") {
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const SnapResult r = snapPoint(Vec2{nan, 5.0}, {100.0}, {5.0}, 8.0);
    CHECK(!r.snappedX);
    CHECK(!r.snappedY);
}

// ═══════════════════════════════════════════════════════════════════════
//  F21 — ngưỡng phải theo pixel màn hình
// ═══════════════════════════════════════════════════════════════════════

// ★ Do chinh xac cua ban tay la hang so theo PIXEL MAN HINH.
//
//   Neu nguong tinh theo don vi output thi khi thu nho khung nhin, mot
//   nguong "8 don vi" chi con 2 pixel tren man — hut gan nhu khong bao
//   gio an. Con khi phong to thi no thanh 40 pixel va hut loan xa.
TEST_CASE("★ F21: nguong doi theo he so phong de tay luon thay giong nhau") {
    // Khung nhin thu nho: 1 don vi the gioi = 0.25 pixel man hinh.
    // Muon nguong 8 pixel man hinh -> can 32 don vi the gioi.
    CHECK_NEAR(snapThresholdFor(8.0, 0.25), 32.0, 1e-12);

    // Phong to gap doi -> chi can 4 don vi.
    CHECK_NEAR(snapThresholdFor(8.0, 2.0), 4.0, 1e-12);

    // Ti le 1:1 -> giu nguyen.
    CHECK_NEAR(snapThresholdFor(8.0, 1.0), 8.0, 1e-12);
}

TEST_CASE("F21: he so phong vo nghia -> tat hut thay vi chia cho 0") {
    CHECK_NEAR(snapThresholdFor(8.0, 0.0), 0.0, 1e-12);
    CHECK_NEAR(snapThresholdFor(8.0, -1.0), 0.0, 1e-12);
    CHECK_NEAR(snapThresholdFor(-8.0, 1.0), 0.0, 1e-12);

    const double nan = std::numeric_limits<double>::quiet_NaN();
    CHECK_NEAR(snapThresholdFor(8.0, nan), 0.0, 1e-12);
}

// Ghep hai ham lai: dung nhu cach UI se goi.
TEST_CASE("F21: dung ket hop — keo o hai muc phong khac nhau") {
    const std::vector<double> gx{960.0};

    // Phong nho (0.25 px/don vi): diem cach 20 don vi = 5 pixel man hinh
    // -> VAN trong tam tay, phai hut.
    const double tSmall = snapThresholdFor(8.0, 0.25);
    CHECK(snapPoint(Vec2{980.0, 0.0}, gx, {}, tSmall).snappedX);

    // Phong to (4 px/don vi): cung 20 don vi = 80 pixel man hinh
    // -> qua xa so voi tay, KHONG duoc hut.
    const double tBig = snapThresholdFor(8.0, 4.0);
    CHECK(!snapPoint(Vec2{980.0, 0.0}, gx, {}, tBig).snappedX);
}

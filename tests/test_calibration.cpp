#include "TestHarness.h"

#include "core/calib/CalibrationProfile.h"
#include "core/calib/SensorMapper.h"
#include "core/model/Screen.h"
#include "core/model/WarpCornerPin.h"
#include "core/model/WarpMesh.h"

using namespace mikmap;

namespace {

/// 4 goc slice tren may chieu (px) — hinh bi keystone.
const Vec2 kProjQuad[4] = {
    {200.0,  150.0},
    {1750.0, 100.0},
    {1850.0, 950.0},
    {150.0,  1000.0},
};

/// 4 diem sensor (mm) tuong ung — thang do KHAC HAN px.
const Vec2 kSensorQuad[4] = {
    {100.0,  100.0},
    {1900.0, 120.0},
    {1950.0, 1050.0},
    {80.0,   1000.0},
};

CalibrationProfile makeCalibrated(SolveMethod m = SolveMethod::LeastSquares) {
    CalibrationProfile p;
    p.method = m;
    for (int i = 0; i < 4; ++i) p.addPair(kSensorQuad[i], kProjQuad[i]);
    p.solve();
    return p;
}

/// Screen mot slice, warp trung voi vung da calibrate.
Screen makeScreenWithQuad(const Vec2 quad[4],
                          const Vec2& inOrigin = {0.0, 0.0},
                          const Vec2& inSize = {1920.0, 1080.0}) {
    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    Slice s;
    s.inputOrigin = inOrigin;
    s.inputSize   = inSize;
    s.setWarp(std::make_unique<WarpCornerPin>(quad));
    sc.slices.push_back(std::move(s));
    return sc;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  G8 — CalibrationProfile
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("G8: chua co diem thi khong hop le") {
    CalibrationProfile p;
    REQUIRE(!p.isValid());
    REQUIRE(!p.solve().ok);
}

TEST_CASE("G8: duoi 4 diem thi tu choi giai") {
    CalibrationProfile p;
    p.addPair({0, 0}, {0, 0});
    p.addPair({1, 0}, {10, 0});
    p.addPair({1, 1}, {10, 10});
    REQUIRE(!p.solve().ok);
}

TEST_CASE("★ G8: 4 diem -> giai duoc, sai so ~0 du thang do lech nhau") {
    CalibrationProfile p = makeCalibrated();
    REQUIRE(p.isValid());
    REQUIRE(p.rmsError() < 1e-6);
    REQUIRE(p.isAccurate(3.0));

    for (int i = 0; i < 4; ++i) {
        const Vec2 got = p.sensorToOutput().transformPoint(kSensorQuad[i]);
        CHECK_NEAR(got.x, kProjQuad[i].x, 1e-6);
        CHECK_NEAR(got.y, kProjQuad[i].y, 1e-6);
    }
}

TEST_CASE("G8: ma tran nghich dao duoc tinh san va dung") {
    CalibrationProfile p = makeCalibrated();
    const Vec2 sensor{842.0, 517.0};
    const Vec2 back = p.outputToSensor().transformPoint(
        p.sensorToOutput().transformPoint(sensor));
    REQUIRE_VEC_NEAR(back, sensor.x, sensor.y, 1e-8);
}

TEST_CASE("★ G8: them diem moi lam VO HIEU ma tran cu ngay lap tuc") {
    // Dung ma tran loi thoi con te hon khong co ma tran — no sai am tham.
    CalibrationProfile p = makeCalibrated();
    REQUIRE(p.isValid());

    p.addPair({500.0, 500.0}, {600.0, 400.0});
    REQUIRE(!p.isValid());        // ★ vo hieu NGAY

    p.solve();
    REQUIRE(p.isValid());
}

TEST_CASE("G8: tat mot diem cung lam vo hieu ma tran") {
    CalibrationProfile p = makeCalibrated();
    p.setPairEnabled(0, false);
    REQUIRE(!p.isValid());
    REQUIRE(p.enabledPairCount() == 3u);
}

TEST_CASE("★ G8: RANSAC danh dau duoc diem rac de UI to do") {
    CalibrationProfile truth = makeCalibrated();

    CalibrationProfile p;
    p.method = SolveMethod::Ransac;
    p.ransacParams.inlierThreshold = 3.0;
    p.ransacParams.seed = 11;

    const Vec2 srcGrid[12] = {
        {200, 200}, {800, 250}, {1400, 200}, {1800, 300},
        {200, 600}, {900, 550}, {1500, 600}, {1850, 650},
        {150, 950}, {700, 900}, {1300, 980}, {1900, 900},
    };
    for (const Vec2& s : srcGrid) {
        p.addPair(s, truth.sensorToOutput().transformPoint(s));
    }

    const size_t firstBad = p.pairCount();
    p.addPair({600.0, 400.0},  {50.0,   1900.0});
    p.addPair({1000.0, 700.0}, {1900.0, 30.0});
    p.addPair({1400.0, 300.0}, {20.0,   20.0});

    REQUIRE(p.solve().ok);
    REQUIRE(p.inlierCount() == 12);
    REQUIRE(p.rmsError() < 1e-4);

    const auto& flags = p.outlierFlags();
    REQUIRE(flags.size() == p.pairCount());
    for (size_t i = 0; i < firstBad; ++i) CHECK(!flags[i]);
    for (size_t i = firstBad; i < p.pairCount(); ++i) CHECK(flags[i]);
}

TEST_CASE("G8: diem thang hang -> bao loi co thong bao cho UI") {
    CalibrationProfile p;
    p.method = SolveMethod::LeastSquares;
    for (int i = 0; i < 6; ++i) {
        p.addPair(Vec2{300.0 + i * 250.0, 300.0 + i * 110.0},
                  Vec2{100.0 + i * 300.0, 200.0 + i * 132.0});
    }
    REQUIRE(!p.solve().ok);
    REQUIRE(!p.message().empty());
}

TEST_CASE("G8: isAccurate phan biet duoc calibration tot va te") {
    REQUIRE(makeCalibrated().isAccurate(3.0));

    CalibrationProfile bad;
    bad.method = SolveMethod::LeastSquares;
    for (int i = 0; i < 4; ++i) bad.addPair(kSensorQuad[i], kProjQuad[i]);
    bad.addPair({1000.0, 600.0}, {100.0, 1500.0});   // diem rac
    bad.solve();
    REQUIRE(bad.isValid());
    REQUIRE(!bad.isAccurate(3.0));   // ★ bao cho nguoi van hanh biet
}

// ═══════════════════════════════════════════════════════════════════════
//  ★ G7 — SensorMapper: chuoi day du
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("G7: chua calibrate thi bao NotCalibrated") {
    CalibrationProfile p;
    Screen sc = makeScreenWithQuad(kProjQuad);

    SensorMapper m;
    m.setCalibration(&p);
    m.setScreen(&sc);

    REQUIRE(!m.map({500.0, 500.0}).valid);
    REQUIRE(m.lastFailure() == MapFailure::NotCalibrated);
}

TEST_CASE("G7: khong co screen thi bao NoScreen") {
    CalibrationProfile p = makeCalibrated();
    SensorMapper m;
    m.setCalibration(&p);
    REQUIRE(!m.map({500.0, 500.0}).valid);
    REQUIRE(m.lastFailure() == MapFailure::NoScreen);
}

TEST_CASE("★★ G7: cham dung 4 goc vat the -> ra dung 4 goc noi dung") {
    CalibrationProfile p = makeCalibrated();
    Screen sc = makeScreenWithQuad(kProjQuad);

    SensorMapper m;
    m.setCalibration(&p);
    m.setScreen(&sc);

    const Vec2 expected[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i) {
        const MappedPoint r = m.map(kSensorQuad[i]);
        REQUIRE(r.valid);
        CHECK(r.sliceIndex == 0);
        CHECK_NEAR(r.contentUV.x, expected[i].x, 1e-6);
        CHECK_NEAR(r.contentUV.y, expected[i].y, 1e-6);
    }
}

TEST_CASE("★ G7: diem bat ky di xuoi roi ve nguoc dung cho") {
    CalibrationProfile p = makeCalibrated();
    Screen sc = makeScreenWithQuad(kProjQuad);

    SensorMapper m;
    m.setCalibration(&p);
    m.setScreen(&sc);

    const Vec2 content{0.63, 0.29};

    // Wizard calibration hoi: "muon hieu ung o day thi sensor doc gi?"
    Vec2 sensorShouldRead;
    REQUIRE(m.contentToSensor(0, content, sensorShouldRead));

    const MappedPoint r = m.map(sensorShouldRead);
    REQUIRE(r.valid);
    REQUIRE_VEC_NEAR(r.contentUV, content.x, content.y, 1e-7);
}

TEST_CASE("★ G7: cham NGOAI moi slice -> bao NoSliceHit, khong bia toa do") {
    CalibrationProfile p = makeCalibrated();
    const Vec2 smallQuad[4] = {{300, 250}, {700, 250}, {700, 550}, {300, 550}};
    Screen sc = makeScreenWithQuad(smallQuad);

    SensorMapper m;
    m.setCalibration(&p);
    m.setScreen(&sc);

    const MappedPoint r = m.map(kSensorQuad[2]);
    REQUIRE(!r.valid);
    REQUIRE(m.lastFailure() == MapFailure::NoSliceHit);
}

TEST_CASE("★ G7: slice chong nhau thi slice TREN CUNG thang") {
    CalibrationProfile p = makeCalibrated();

    const Vec2 big[4]   = {{200, 150}, {1750, 150}, {1750, 950}, {200, 950}};
    const Vec2 small[4] = {{600, 400}, {1000, 400}, {1000, 700}, {600, 700}};

    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    // Thu tu VE: [0] duoi cung, [1] tren cung.
    Slice sBig;  sBig.setWarp(std::make_unique<WarpCornerPin>(big));
    Slice sSmall; sSmall.setWarp(std::make_unique<WarpCornerPin>(small));
    sc.slices.push_back(std::move(sBig));
    sc.slices.push_back(std::move(sSmall));

    SensorMapper m;
    m.setCalibration(&p);
    m.setScreen(&sc);

    // Diem giua slice nho -> phai thuoc ve slice TREN CUNG (index 1).
    Vec2 sensorPt;
    REQUIRE(m.contentToSensor(1, {0.5, 0.5}, sensorPt));
    const MappedPoint r = m.map(sensorPt);
    REQUIRE(r.valid);
    REQUIRE(r.sliceIndex == 1);
}

TEST_CASE("★ G7: hoat dong voi MESH WARP, khong chi corner pin") {
    CalibrationProfile p = makeCalibrated();

    auto mesh = std::make_unique<WarpMesh>(4, 4, Vec2{300.0, 250.0}, Vec2{1200.0, 600.0});
    for (int cy = 0; cy <= 4; ++cy) {
        const Vec2 cp = mesh->controlPoint(2, cy);
        mesh->setControlPoint(2, cy, Vec2{cp.x, cp.y - 40.0});
    }

    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    Slice s;
    s.setWarp(std::move(mesh));
    sc.slices.push_back(std::move(s));

    SensorMapper m;
    m.setCalibration(&p);
    m.setScreen(&sc);

    const Vec2 content{0.4, 0.6};
    Vec2 sensorPt;
    REQUIRE(m.contentToSensor(0, content, sensorPt));

    const MappedPoint r = m.map(sensorPt);
    REQUIRE(r.valid);
    REQUIRE_VEC_NEAR(r.contentUV, content.x, content.y, 1e-5);
}

TEST_CASE("★ G7: doi contentUV sang toa do Composition Canvas") {
    CalibrationProfile p = makeCalibrated();
    // Slice lay vung 800x600 bat dau tu (100,50) tren canvas.
    Screen sc = makeScreenWithQuad(kProjQuad, Vec2{100.0, 50.0}, Vec2{800.0, 600.0});

    SensorMapper m;
    m.setCalibration(&p);
    m.setScreen(&sc);

    Vec2 sensorPt;
    REQUIRE(m.contentToSensor(0, {0.5, 0.5}, sensorPt));

    const MappedPoint r = m.map(sensorPt);
    REQUIRE(r.valid);
    REQUIRE_VEC_NEAR(r.canvasPx, 100.0 + 400.0, 50.0 + 300.0, 1e-5);
}

TEST_CASE("★ G7: chinh lai keystone KHONG can calibrate lai sensor") {
    // architecture.md §4.2① — ly do H_s va H_w phai tach roi.
    CalibrationProfile p = makeCalibrated();
    Screen sc = makeScreenWithQuad(kProjQuad);

    SensorMapper m;
    m.setCalibration(&p);
    m.setScreen(&sc);

    auto* cp = static_cast<WarpCornerPin*>(sc.slices[0].warp());
    REQUIRE(cp->setCorner(0, Vec2{260.0, 190.0}));

    REQUIRE(p.isValid());        // ★ calibration KHONG bi dong toi

    Vec2 sensorPt;
    REQUIRE(m.contentToSensor(0, {0.5, 0.5}, sensorPt));
    const MappedPoint r = m.map(sensorPt);
    REQUIRE(r.valid);
    REQUIRE_VEC_NEAR(r.contentUV, 0.5, 0.5, 1e-6);
}

TEST_CASE("G7: slice bi tat thi khong nhan diem cham") {
    CalibrationProfile p = makeCalibrated();
    Screen sc = makeScreenWithQuad(kProjQuad);

    SensorMapper m;
    m.setCalibration(&p);
    m.setScreen(&sc);

    REQUIRE(m.map(kSensorQuad[0]).valid);

    sc.slices[0].enabled = false;
    REQUIRE(!m.map(kSensorQuad[0]).valid);
    REQUIRE(m.lastFailure() == MapFailure::NoSliceHit);
}

#include "TestHarness.h"

#include "core/math/Homography.h"

#include <random>
#include <string>

using namespace mikmap;
using namespace mikmap::homography;

namespace {

/// Tu giac keystone tren may chieu (px) — hinh chu nhat bi nghieng.
const Vec2 kProjQuad[4] = {
    {200.0,  150.0},
    {1750.0, 100.0},
    {1850.0, 950.0},
    {150.0,  1000.0},
};

/// 4 diem sensor (mm) tuong ung 4 goc tren — thang do KHAC HAN px.
const Vec2 kSensorQuad[4] = {
    {100.0,  100.0},
    {1900.0, 120.0},
    {1950.0, 1050.0},
    {80.0,   1000.0},
};

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  Corner pin
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("Homography: unitSquareToQuad anh xa 4 goc CHINH XAC") {
    const HomographyResult r = unitSquareToQuad(kProjQuad);
    REQUIRE(r.ok);

    const Vec2 unit[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i) {
        const Vec2 got = r.H.transformPoint(unit[i]);
        CHECK_NEAR(got.x, kProjQuad[i].x, 1e-7);
        CHECK_NEAR(got.y, kProjQuad[i].y, 1e-7);
    }
    REQUIRE(r.rmsError < 1e-7);
}

TEST_CASE("Homography: corner pin nghich dao duoc (round-trip)") {
    const HomographyResult r = unitSquareToQuad(kProjQuad);
    REQUIRE(r.ok);
    REQUIRE(r.H.isInvertible());

    Mat3 inv;
    REQUIRE(r.H.invert(inv));

    // Diem bat ky trong content -> output -> nguoc lai.
    const Vec2 content{0.37, 0.82};
    const Vec2 out  = r.H.transformPoint(content);
    const Vec2 back = inv.transformPoint(out);
    REQUIRE_VEC_NEAR(back, content.x, content.y, 1e-9);
}

TEST_CASE("Homography: tam hinh vuong KHONG roi vao tam tu giac (phoi canh)") {
    const HomographyResult r = unitSquareToQuad(kProjQuad);
    REQUIRE(r.ok);

    const Vec2 mapped = r.H.transformPoint({0.5, 0.5});

    const Vec2 centroid = (kProjQuad[0] + kProjQuad[1] + kProjQuad[2] + kProjQuad[3]) * 0.25;

    // Neu trung nhau thi phep bien doi chi la affine — sai ban chat.
    // Voi keystone that, tam anh bi lech khoi trong tam tu giac.
    REQUIRE(mapped.distanceTo(centroid) > 0.5);
}

TEST_CASE("Homography: tu giac suy bien bi tu choi") {
    // Ba diem thang hang.
    const Vec2 collinear[4] = {{0, 0}, {10, 0}, {20, 0}, {5, 5}};
    REQUIRE(!isValidQuad(collinear));

    const HomographyResult r = unitSquareToQuad(collinear);
    REQUIRE(!r.ok);
}

TEST_CASE("Homography: tu giac hinh no (bow-tie) bi tu choi") {
    // Hai goc bi hoan doi -> canh cat nhau -> khong nghich dao duoc.
    const Vec2 bowtie[4] = {{0, 0}, {100, 0}, {0, 100}, {100, 100}};
    REQUIRE(!isValidQuad(bowtie));
}

TEST_CASE("Homography: tu giac loi hop le duoc chap nhan") {
    REQUIRE(isValidQuad(kProjQuad));
    REQUIRE(isValidQuad(kSensorQuad));
}

// ═══════════════════════════════════════════════════════════════════════
//  Calibration sensor — thang do lech nhau (kiem chung Hartley)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("Homography: giai duoc khi nguon la mm va dich la pixel") {
    // Day la kich ban that: sensor bao mm (0..2000), may chieu dung px
    // (0..3840). Khong chuan hoa Hartley thi sai so o day tang vot.
    const HomographyResult r = solve4Point(kSensorQuad, kProjQuad);
    REQUIRE(r.ok);
    REQUIRE(r.rmsError < 1e-6);
    REQUIRE(r.maxError < 1e-6);

    for (int i = 0; i < 4; ++i) {
        const Vec2 got = r.H.transformPoint(kSensorQuad[i]);
        CHECK_NEAR(got.x, kProjQuad[i].x, 1e-6);
        CHECK_NEAR(got.y, kProjQuad[i].y, 1e-6);
    }
}

TEST_CASE("Homography: least-squares voi 9 diem khong nhieu -> sai so ~0") {
    // Sinh diem tu mot homography da biet.
    const HomographyResult truth = solve4Point(kSensorQuad, kProjQuad);
    REQUIRE(truth.ok);

    std::vector<CorrespondencePair> pairs;
    for (int gy = 0; gy < 3; ++gy) {
        for (int gx = 0; gx < 3; ++gx) {
            CorrespondencePair p;
            p.src = Vec2{200.0 + gx * 800.0, 200.0 + gy * 400.0};
            p.dst = truth.H.transformPoint(p.src);
            pairs.push_back(p);
        }
    }

    const HomographyResult r = solveLeastSquares(pairs);
    REQUIRE(r.ok);
    REQUIRE(r.totalCount == 9);
    REQUIRE(r.rmsError < 1e-6);
}

TEST_CASE("Homography: least-squares can it nhat 4 diem") {
    std::vector<CorrespondencePair> pairs(3);
    const HomographyResult r = solveLeastSquares(pairs);
    REQUIRE(!r.ok);
}

TEST_CASE("Homography: diem bi tat (enabled=false) bi bo qua") {
    const HomographyResult truth = solve4Point(kSensorQuad, kProjQuad);
    REQUIRE(truth.ok);

    // Cac diem phai TRAI RONG THEO 2 CHIEU. Diem thang hang khong xac
    // dinh duoc homography — xem test "diem thang hang bi tu choi".
    const Vec2 srcPts[6] = {
        {300.0,  300.0},
        {1200.0, 350.0},
        {1500.0, 900.0},
        {250.0,  950.0},
        {800.0,  600.0},
        {1000.0, 250.0},
    };

    std::vector<CorrespondencePair> pairs;
    for (const Vec2& s : srcPts) {
        CorrespondencePair p;
        p.src = s;
        p.dst = truth.H.transformPoint(s);
        pairs.push_back(p);
    }

    // Lam hong 1 diem NHUNG tat no di -> ket qua phai van sach.
    pairs[2].dst = Vec2{99999.0, -99999.0};
    pairs[2].enabled = false;

    const HomographyResult r = solveLeastSquares(pairs);
    REQUIRE(r.ok);
    REQUIRE(r.totalCount == 5);
    REQUIRE(r.rmsError < 1e-6);
}

TEST_CASE("Homography: diem THANG HANG bi tu choi (khong tra ve rac)") {
    // Kich ban that: nguoi van hanh cham calibration doc theo mot duong
    // thang (vd men theo mep ban). Homography KHONG xac dinh duoc trong
    // truong hop nay — he 8x8 bi suy bien.
    //
    // Dieu quan trong: solver phai BAO LOI, khong duoc tra ve mot ma tran
    // trong co ve hop le. Neu no tra ve rac, nguoi van hanh se thay hieu
    // ung lech cho ma khong hieu tai sao.
    std::vector<CorrespondencePair> pairs;
    for (int i = 0; i < 6; ++i) {
        CorrespondencePair p;
        p.src = Vec2{300.0 + i * 250.0, 300.0 + i * 110.0};   // thang hang
        p.dst = Vec2{100.0 + i * 300.0, 200.0 + i * 132.0};   // cung thang hang
        pairs.push_back(p);
    }

    const HomographyResult r = solveLeastSquares(pairs);
    REQUIRE(!r.ok);
    REQUIRE(std::string(r.message).size() > 0);   // co thong bao de hien len UI
}

TEST_CASE("Homography: nhieu nho -> sai so cung nho (khong khuech dai)") {
    const HomographyResult truth = solve4Point(kSensorQuad, kProjQuad);
    REQUIRE(truth.ok);

    std::mt19937 rng(4242);
    std::normal_distribution<double> noise(0.0, 1.0);   // sigma = 1 px

    std::vector<CorrespondencePair> pairs;
    for (int gy = 0; gy < 4; ++gy) {
        for (int gx = 0; gx < 4; ++gx) {
            CorrespondencePair p;
            p.src = Vec2{200.0 + gx * 550.0, 200.0 + gy * 270.0};
            p.dst = truth.H.transformPoint(p.src);
            p.dst.x += noise(rng);
            p.dst.y += noise(rng);
            pairs.push_back(p);
        }
    }

    const HomographyResult r = solveLeastSquares(pairs);
    REQUIRE(r.ok);
    // Voi nhieu sigma=1px, RMS phai cung khoang 1px chu khong duoc no ra.
    REQUIRE(r.rmsError < 3.0);
}

// ═══════════════════════════════════════════════════════════════════════
//  RANSAC — chong diem rac
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("RANSAC: loai duoc 4 diem rac trong 16 diem") {
    const HomographyResult truth = solve4Point(kSensorQuad, kProjQuad);
    REQUIRE(truth.ok);

    std::vector<CorrespondencePair> pairs;
    for (int gy = 0; gy < 4; ++gy) {
        for (int gx = 0; gx < 4; ++gx) {
            CorrespondencePair p;
            p.src = Vec2{200.0 + gx * 550.0, 200.0 + gy * 270.0};
            p.dst = truth.H.transformPoint(p.src);
            pairs.push_back(p);
        }
    }
    const size_t goodCount = pairs.size();

    // Them 4 diem rac: cham nham, nhieu hong ngoai, blob depth bat sai.
    const size_t badIdx0 = pairs.size();
    pairs.push_back({{500.0, 500.0},  {50.0,   1900.0}, 1.0, true});
    pairs.push_back({{900.0, 700.0},  {1800.0, 20.0},   1.0, true});
    pairs.push_back({{1200.0, 400.0}, {30.0,   40.0},   1.0, true});
    pairs.push_back({{700.0, 900.0},  {1900.0, 1900.0}, 1.0, true});

    RansacParams rp;
    rp.inlierThreshold = 3.0;
    rp.seed = 7;

    const HomographyResult r = solveRANSAC(pairs, rp);
    REQUIRE(r.ok);
    REQUIRE(r.inlierCount == static_cast<int>(goodCount));
    REQUIRE(r.rmsError < 1e-5);

    // 16 diem tot phai duoc danh dau inlier.
    for (size_t i = 0; i < goodCount; ++i) {
        CHECK(r.inliers[i]);
    }
    // 4 diem rac phai bi loai.
    for (size_t i = badIdx0; i < pairs.size(); ++i) {
        CHECK(!r.inliers[i]);
    }
}

TEST_CASE("RANSAC: dung 4 diem thi giai truc tiep") {
    std::vector<CorrespondencePair> pairs;
    for (int i = 0; i < 4; ++i) {
        pairs.push_back({kSensorQuad[i], kProjQuad[i], 1.0, true});
    }

    const HomographyResult r = solveRANSAC(pairs);
    REQUIRE(r.ok);
    REQUIRE(r.inlierCount == 4);
    REQUIRE(r.rmsError < 1e-6);
}

TEST_CASE("RANSAC: tai lap duoc voi cung seed") {
    const HomographyResult truth = solve4Point(kSensorQuad, kProjQuad);
    REQUIRE(truth.ok);

    std::vector<CorrespondencePair> pairs;
    for (int i = 0; i < 12; ++i) {
        CorrespondencePair p;
        p.src = Vec2{150.0 + i * 140.0, 200.0 + (i % 5) * 170.0};
        p.dst = truth.H.transformPoint(p.src);
        pairs.push_back(p);
    }

    RansacParams rp;
    rp.seed = 99;

    const HomographyResult a = solveRANSAC(pairs, rp);
    const HomographyResult b = solveRANSAC(pairs, rp);
    REQUIRE(a.ok);
    REQUIRE(b.ok);
    REQUIRE(a.H.nearlyEquals(b.H, 1e-12));
}

// ═══════════════════════════════════════════════════════════════════════
//  ★ CONG THUC LOI:  p_content = H_w⁻¹ · H_s · p_sensor
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ Chuoi bien doi: cham goc sensor -> dung goc content") {
    // H_w: content (hinh vuong don vi) -> may chieu
    const HomographyResult hw = unitSquareToQuad(kProjQuad);
    REQUIRE(hw.ok);

    // H_s: sensor (mm) -> may chieu, do calibration sinh ra
    const HomographyResult hs = solve4Point(kSensorQuad, kProjQuad);
    REQUIRE(hs.ok);

    Mat3 hwInv;
    REQUIRE(hw.H.invert(hwInv));

    // Ma tran hop nhat — day chinh la thu SensorMapper se tinh truoc 1 lan.
    const Mat3 sensorToContent = hwInv * hs.H;

    // Cham dung 4 goc vat the that -> phai ra dung 4 goc cua content.
    const Vec2 expected[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i) {
        const Vec2 got = sensorToContent.transformPoint(kSensorQuad[i]);
        CHECK_NEAR(got.x, expected[i].x, 1e-9);
        CHECK_NEAR(got.y, expected[i].y, 1e-9);
    }
}

TEST_CASE("★ Chuoi bien doi: diem giua bat ky di va ve dung cho") {
    const HomographyResult hw = unitSquareToQuad(kProjQuad);
    const HomographyResult hs = solve4Point(kSensorQuad, kProjQuad);
    REQUIRE(hw.ok);
    REQUIRE(hs.ok);

    Mat3 hwInv, hsInv;
    REQUIRE(hw.H.invert(hwInv));
    REQUIRE(hs.H.invert(hsInv));

    // Chon mot diem trong content (vd tam mot nut bam tren video).
    const Vec2 content{0.63, 0.29};

    // Di xuoi: content -> may chieu -> sensor se doc duoc gi?
    const Vec2 onProjector = hw.H.transformPoint(content);
    const Vec2 sensorReads = hsInv.transformPoint(onProjector);

    // Di nguoc bang cong thuc loi: sensor -> content
    const Mat3 sensorToContent = hwInv * hs.H;
    const Vec2 recovered = sensorToContent.transformPoint(sensorReads);

    REQUIRE_VEC_NEAR(recovered, content.x, content.y, 1e-9);
}

TEST_CASE("★ Ma tran hop nhat cho ket qua giong het tinh tung buoc") {
    const HomographyResult hw = unitSquareToQuad(kProjQuad);
    const HomographyResult hs = solve4Point(kSensorQuad, kProjQuad);
    REQUIRE(hw.ok);
    REQUIRE(hs.ok);

    Mat3 hwInv;
    REQUIRE(hw.H.invert(hwInv));

    const Mat3 combined = hwInv * hs.H;
    const Vec2 p{842.0, 517.0};

    const Vec2 stepwise = hwInv.transformPoint(hs.H.transformPoint(p));
    const Vec2 fused    = combined.transformPoint(p);

    // Xac nhan viec tinh truoc ma tran hop nhat trong SensorMapper la an toan:
    // no re hon (1 phep nhan thay vi 2) va khong lam mat do chinh xac.
    REQUIRE_VEC_NEAR(fused, stepwise.x, stepwise.y, 1e-9);
}

TEST_CASE("★ Chinh lai keystone KHONG lam hong calibration sensor") {
    // Day la ly do H_s va H_w phai tach roi.
    const HomographyResult hs = solve4Point(kSensorQuad, kProjQuad);
    REQUIRE(hs.ok);

    // Nguoi van hanh keo lai 4 goc slice (keystone moi).
    const Vec2 adjustedQuad[4] = {
        {220.0,  140.0},
        {1780.0, 130.0},
        {1820.0, 980.0},
        {170.0,  1010.0},
    };
    const HomographyResult hw2 = unitSquareToQuad(adjustedQuad);
    REQUIRE(hw2.ok);

    Mat3 hw2Inv;
    REQUIRE(hw2.H.invert(hw2Inv));
    const Mat3 sensorToContent = hw2Inv * hs.H;

    // H_s KHONG doi. Cham vao diem sensor ung voi goc tren-trai cu:
    // no van phai roi dung len pixel may chieu cu (200,150), va tu do
    // suy ra toa do content moi mot cach nhat quan.
    const Vec2 stillOnProjector = hs.H.transformPoint(kSensorQuad[0]);
    REQUIRE_VEC_NEAR(stillOnProjector, kProjQuad[0].x, kProjQuad[0].y, 1e-6);

    // Va chuoi day du van cho ket qua huu han, dung nguoc lai duoc.
    const Vec2 contentPt = sensorToContent.transformPoint(kSensorQuad[0]);
    REQUIRE(contentPt.isFinite());
    const Vec2 backToProj = hw2.H.transformPoint(contentPt);
    REQUIRE_VEC_NEAR(backToProj, stillOnProjector.x, stillOnProjector.y, 1e-6);
}

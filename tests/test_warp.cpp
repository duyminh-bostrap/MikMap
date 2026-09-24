#include "TestHarness.h"

#include "core/math/BilinearInverse.h"
#include "core/model/WarpCornerPin.h"
#include "core/model/WarpMesh.h"

#include <cmath>

using namespace mikmap;

// ═══════════════════════════════════════════════════════════════════════
//  Nghich dao song tuyen tinh
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("BilinearInverse: hinh vuong don vi") {
    const Vec2 a{0, 0}, b{1, 0}, c{1, 1}, d{0, 1};
    Vec2 uv;
    REQUIRE(invertBilinear(a, b, c, d, Vec2{0.25, 0.75}, uv));
    REQUIRE_VEC_NEAR(uv, 0.25, 0.75, 1e-9);
}

TEST_CASE("BilinearInverse: hinh chu nhat da dich chuyen") {
    const Vec2 a{100, 200}, b{500, 200}, c{500, 400}, d{100, 400};
    Vec2 uv;
    REQUIRE(invertBilinear(a, b, c, d, Vec2{300, 300}, uv));
    REQUIRE_VEC_NEAR(uv, 0.5, 0.5, 1e-9);
}

TEST_CASE("BilinearInverse: o XOAN (nhanh phuong trinh bac 2)") {
    // G != 0 => k2 != 0 => phai di qua cong thuc nghiem bac hai.
    const Vec2 a{0, 0}, b{100, 10}, c{120, 90}, d{20, 110};

    for (double v = 0.1; v < 1.0; v += 0.2) {
        for (double u = 0.1; u < 1.0; u += 0.2) {
            const Vec2 p = bilerp(a, b, c, d, u, v);
            Vec2 uv;
            REQUIRE(invertBilinear(a, b, c, d, p, uv));
            CHECK_NEAR(uv.x, u, 1e-8);
            CHECK_NEAR(uv.y, v, 1e-8);
        }
    }
}

TEST_CASE("BilinearInverse: hinh binh hanh (nhanh suy bien k2=0)") {
    // Hinh binh hanh => G = 0 => k2 = 0 => phuong trinh BAC NHAT.
    // Day KHONG phai truong hop hiem: mesh chua bi keo thi moi o deu vay.
    const Vec2 a{0, 0}, b{100, 0}, c{150, 80}, d{50, 80};
    const Vec2 G = a - b + c - d;
    REQUIRE(std::abs(G.x) < 1e-12);
    REQUIRE(std::abs(G.y) < 1e-12);

    Vec2 uv;
    const Vec2 p = bilerp(a, b, c, d, 0.3, 0.7);
    REQUIRE(invertBilinear(a, b, c, d, p, uv));
    REQUIRE_VEC_NEAR(uv, 0.3, 0.7, 1e-9);
}

TEST_CASE("BilinearInverse: diem ngoai o bi tu choi") {
    const Vec2 a{0, 0}, b{100, 0}, c{100, 100}, d{0, 100};
    Vec2 uv;
    REQUIRE(!invertBilinear(a, b, c, d, Vec2{500, 500}, uv));
    REQUIRE(!invertBilinear(a, b, c, d, Vec2{-50, 50}, uv));
}

TEST_CASE("pointInQuad: trong / ngoai") {
    const Vec2 a{0, 0}, b{100, 0}, c{100, 100}, d{0, 100};
    REQUIRE(pointInQuad(a, b, c, d, Vec2{50, 50}));
    REQUIRE(!pointInQuad(a, b, c, d, Vec2{150, 50}));
    REQUIRE(!pointInQuad(a, b, c, d, Vec2{50, -10}));
}

// ★ Diem nam DUNG TREN duong cheo p00-p11 tung bi tu choi.
//
//   Tich co huong doc duong cheo bang 0 ve mat toan hoc, nhung dau phay
//   dong tra ve nhieu co 1e-13 — co the AM trong khi hai so kia duong.
//   So dau chat voi 0 thi CA HAI tam giac deu noi "nam ngoai", va tu giac
//   tu choi mot diem nam chinh giua no.
//
//   Khong phai chuyen ly thuyet: TAM cua o hinh binh hanh nam dung tren
//   duong cheo, ma luoi warp phang hoac uon deu thi moi o deu la hinh
//   binh hanh. Hau qua la mot VET CHAM CHET chay cheo qua tung o luoi.
TEST_CASE("★ pointInQuad: diem tren duong cheo van tinh la NAM TRONG") {
    // Hinh vuong: tam (50,50) nam dung tren ca hai duong cheo.
    const Vec2 a{0, 0}, b{100, 0}, c{100, 100}, d{0, 100};
    REQUIRE(pointInQuad(a, b, c, d, Vec2{50, 50}));

    // Hinh binh hanh xien — dung hinh dang ma luoi uon sinh ra.
    const Vec2 e{250.0, 335.0}, f{400.0, 335.0},
               g{400.0, 460.62177826491069}, h{250.0, 460.62177826491069};
    const Vec2 center{(e.x + g.x) * 0.5, (e.y + g.y) * 0.5};
    REQUIRE(pointInQuad(e, f, g, h, center));

    // Toa do RAT LON: nguong phai theo ti le, hang so tuyet doi se sai o day.
    const Vec2 p{1.0e6, 1.0e6}, q{2.0e6, 1.0e6},
               r{2.0e6, 2.0e6}, s{1.0e6, 2.0e6};
    REQUIRE(pointInQuad(p, q, r, s, Vec2{1.5e6, 1.5e6}));

    // Va van phai TU CHOI diem that su nam ngoai, ngay sat canh.
    REQUIRE(!pointInQuad(a, b, c, d, Vec2{101.0, 50.0}));
}

// ★ Cung mot loi, nhung o muc WarpMesh: tam cua MOI o phai nghich dao duoc.
//   Day moi la thu nguoi dung cham vao — inverse() la duong sensor -> noi dung.
TEST_CASE("★ Mesh: tam cua moi o deu nghich dao duoc") {
    WarpMesh m(4, 3, Vec2{0.0, 0.0}, Vec2{800.0, 600.0});

    int checked = 0;
    for (int cy = 0; cy < 3; ++cy) {
        for (int cx = 0; cx < 4; ++cx) {
            const Vec2 uv{(cx + 0.5) / 4.0, (cy + 0.5) / 3.0};
            const Vec2 px = m.forward(uv);

            Vec2 back;
            REQUIRE(m.inverse(px, back));
            CHECK_NEAR(back.x, uv.x, 1e-9);
            CHECK_NEAR(back.y, uv.y, 1e-9);
            ++checked;
        }
    }
    REQUIRE(checked == 12);
}

// ═══════════════════════════════════════════════════════════════════════
//  WarpCornerPin
// ═══════════════════════════════════════════════════════════════════════

namespace {
const Vec2 kKeystone[4] = {
    {200.0,  150.0},
    {1750.0, 100.0},
    {1850.0, 950.0},
    {150.0,  1000.0},
};
} // namespace

TEST_CASE("CornerPin: forward dua UV goc ve dung 4 goc") {
    WarpCornerPin w(kKeystone);
    REQUIRE(w.isInvertible());

    const Vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    for (int i = 0; i < 4; ++i) {
        const Vec2 got = w.forward(uv[i]);
        CHECK_NEAR(got.x, kKeystone[i].x, 1e-7);
        CHECK_NEAR(got.y, kKeystone[i].y, 1e-7);
    }
}

TEST_CASE("★ CornerPin: round-trip forward -> inverse tren luoi diem") {
    WarpCornerPin w(kKeystone);
    REQUIRE(w.isInvertible());

    for (int gy = 0; gy <= 10; ++gy) {
        for (int gx = 0; gx <= 10; ++gx) {
            const Vec2 uv{gx / 10.0, gy / 10.0};
            const Vec2 px = w.forward(uv);

            Vec2 back;
            REQUIRE(w.inverse(px, back));
            CHECK_NEAR(back.x, uv.x, 1e-9);
            CHECK_NEAR(back.y, uv.y, 1e-9);
        }
    }
}

TEST_CASE("CornerPin: diem ngoai tu giac bi tu choi") {
    WarpCornerPin w(kKeystone);
    Vec2 uv;
    REQUIRE(!w.inverse(Vec2{5000.0, 5000.0}, uv));
    REQUIRE(!w.inverse(Vec2{-100.0, -100.0}, uv));
}

TEST_CASE("★ CornerPin: keo goc thanh hinh suy bien bi TU CHOI, giu nguyen trang thai") {
    WarpCornerPin w(kKeystone);
    REQUIRE(w.isInvertible());

    const Vec2 before = w.corner(1);

    // Keo goc 1 chong len goc 3 -> tu giac tu cat (hinh no).
    const bool accepted = w.setCorner(1, w.corner(3));
    REQUIRE(!accepted);

    // Quan trong: goc PHAI giu nguyen. Neu khong, UI se ket o trang thai
    // hong va calibration sensor am tham sai.
    REQUIRE_VEC_NEAR(w.corner(1), before.x, before.y, 1e-12);
    REQUIRE(w.isInvertible());
}

TEST_CASE("CornerPin: keo goc hop le duoc chap nhan") {
    WarpCornerPin w(kKeystone);
    REQUIRE(w.setCorner(0, Vec2{250.0, 180.0}));
    REQUIRE_VEC_NEAR(w.corner(0), 250.0, 180.0, 1e-12);
    REQUIRE(w.isInvertible());
}

TEST_CASE("CornerPin: tessellate sinh dung so vertex va index") {
    WarpCornerPin w(kKeystone);
    WarpGeometry g;
    w.tessellate(4, 3, g);

    REQUIRE(g.vertices.size() == 5u * 4u);        // (cols+1)*(rows+1)
    REQUIRE(g.triangleCount() == 4u * 3u * 2u);   // 2 tam giac moi o
}

TEST_CASE("CornerPin: hop bao dung") {
    WarpCornerPin w(kKeystone);
    Vec2 lo, hi;
    w.boundingBox(lo, hi);
    REQUIRE_VEC_NEAR(lo, 150.0, 100.0, 1e-12);
    REQUIRE_VEC_NEAR(hi, 1850.0, 1000.0, 1e-12);
}

TEST_CASE("★ CornerPin: keo goc thanh hinh LOM cung bi tu choi") {
    // Phep bien doi phoi canh anh xa tu giac LOI thanh tu giac LOI.
    // Neu 4 goc tao hinh lom, homography van ton tai ve mat toan hoc
    // nhung phan TRONG hinh vuong bi gap nguoc -> hinh chieu bi lon.
    //
    // Hau qua cho UI (Buoc 5): khi nguoi dung keo mot goc qua xa, handle
    // se "bat nguoc lai" vi tri cu. Can hien chi bao truc quan (vd doi
    // mau vien do) de ho hieu tai sao, thay vi tuong phan mem bi treo.
    WarpCornerPin w(kKeystone);
    const Vec2 before = w.corner(0);

    // (999,888) nam sau ben trong tu giac -> goc 0 tro thanh diem lom.
    REQUIRE(!w.setCorner(0, Vec2{999.0, 888.0}));
    REQUIRE_VEC_NEAR(w.corner(0), before.x, before.y, 1e-12);
    REQUIRE(w.isInvertible());
}

TEST_CASE("CornerPin: clone doc lap voi ban goc") {
    WarpCornerPin w(kKeystone);
    auto copy = w.clone();
    // Di chuyen vua phai, giu tu giac loi.
    REQUIRE(w.setCorner(0, Vec2{260.0, 190.0}));

    Vec2 uv;
    REQUIRE(copy->inverse(copy->forward(Vec2{0.5, 0.5}), uv));
    REQUIRE_VEC_NEAR(uv, 0.5, 0.5, 1e-9);

    // Ban sao KHONG duoc bi anh huong.
    const auto* cp = static_cast<const WarpCornerPin*>(copy.get());
    REQUIRE_VEC_NEAR(cp->corner(0), kKeystone[0].x, kKeystone[0].y, 1e-12);
}

// ═══════════════════════════════════════════════════════════════════════
//  WarpMesh
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("Mesh: luoi phang -> forward tuyen tinh") {
    WarpMesh m(4, 4, Vec2{0.0, 0.0}, Vec2{1000.0, 800.0});
    REQUIRE_VEC_NEAR(m.forward({0.0, 0.0}),  0.0,    0.0,   1e-9);
    REQUIRE_VEC_NEAR(m.forward({1.0, 1.0}),  1000.0, 800.0, 1e-9);
    REQUIRE_VEC_NEAR(m.forward({0.5, 0.5}),  500.0,  400.0, 1e-9);
}

TEST_CASE("★ Mesh: round-trip tren luoi CHUA bien dang (nhanh k2=0)") {
    WarpMesh m(4, 4, Vec2{100.0, 100.0}, Vec2{800.0, 600.0});

    for (int gy = 0; gy <= 8; ++gy) {
        for (int gx = 0; gx <= 8; ++gx) {
            const Vec2 uv{gx / 8.0, gy / 8.0};
            const Vec2 px = m.forward(uv);

            Vec2 back;
            REQUIRE(m.inverse(px, back));
            CHECK_NEAR(back.x, uv.x, 1e-8);
            CHECK_NEAR(back.y, uv.y, 1e-8);
        }
    }
}

TEST_CASE("★ Mesh: round-trip tren luoi DA BIEN DANG (nhanh bac 2)") {
    // Uon luoi thanh mat cong — mo phong chieu len cot tron.
    WarpMesh m(6, 6, Vec2{100.0, 100.0}, Vec2{900.0, 600.0});

    for (int cy = 0; cy <= 6; ++cy) {
        for (int cx = 0; cx <= 6; ++cx) {
            const Vec2 p = m.controlPoint(cx, cy);
            const double u = cx / 6.0;
            // Do phong theo hinh sin: manh nhat o giua, tat dan ra mep.
            const double bulge = 70.0 * std::sin(u * 3.14159265358979);
            m.setControlPoint(cx, cy, Vec2{p.x, p.y + bulge});
        }
    }

    int tested = 0;
    for (int gy = 1; gy < 12; ++gy) {
        for (int gx = 1; gx < 12; ++gx) {
            const Vec2 uv{gx / 12.0, gy / 12.0};
            const Vec2 px = m.forward(uv);

            Vec2 back;
            REQUIRE(m.inverse(px, back));
            CHECK_NEAR(back.x, uv.x, 1e-7);
            CHECK_NEAR(back.y, uv.y, 1e-7);
            ++tested;
        }
    }
    REQUIRE(tested == 121);
}

TEST_CASE("Mesh: diem ngoai luoi bi tu choi") {
    WarpMesh m(4, 4, Vec2{100.0, 100.0}, Vec2{800.0, 600.0});
    Vec2 uv;
    REQUIRE(!m.inverse(Vec2{5000.0, 5000.0}, uv));
    REQUIRE(!m.inverse(Vec2{0.0, 0.0}, uv));
}

TEST_CASE("★ Mesh: resize GIU NGUYEN hinh dang da keo") {
    WarpMesh m(2, 2, Vec2{0.0, 0.0}, Vec2{800.0, 800.0});

    // Keo diem giua len tren.
    m.setControlPoint(1, 1, Vec2{400.0, 250.0});

    const Vec2 beforeCenter = m.forward({0.5, 0.5});
    const Vec2 beforeQuarter = m.forward({0.25, 0.25});

    m.resize(8, 8);
    REQUIRE(m.cols() == 8);
    REQUIRE(m.rows() == 8);

    // Be mat phai gan nhu khong doi. Neu resize xoa ve hinh chu nhat,
    // nguoi dung mat toan bo cong can chinh — loi UX rat kho chiu.
    const Vec2 afterCenter = m.forward({0.5, 0.5});
    REQUIRE_VEC_NEAR(afterCenter, beforeCenter.x, beforeCenter.y, 1e-9);

    // Diem khac cung phai xap xi (lay mau lai co sai so nho la binh thuong).
    const Vec2 afterQuarter = m.forward({0.25, 0.25});
    CHECK(afterQuarter.distanceTo(beforeQuarter) < 20.0);
}

TEST_CASE("Mesh: tessellate sinh dung so luong") {
    WarpMesh m(3, 3, Vec2{0.0, 0.0}, Vec2{600.0, 600.0});
    WarpGeometry g;
    m.tessellate(6, 5, g);
    REQUIRE(g.vertices.size() == 7u * 6u);
    REQUIRE(g.triangleCount() == 6u * 5u * 2u);
}

TEST_CASE("Mesh: clone doc lap") {
    WarpMesh m(2, 2, Vec2{0.0, 0.0}, Vec2{400.0, 400.0});
    auto copy = m.clone();
    m.setControlPoint(1, 1, Vec2{50.0, 50.0});

    const auto* cm = static_cast<const WarpMesh*>(copy.get());
    REQUIRE_VEC_NEAR(cm->controlPoint(1, 1), 200.0, 200.0, 1e-9);
}

// ═══════════════════════════════════════════════════════════════════════
//  Hop dong chung cua IWarp — ap dung cho MOI cai dat
// ═══════════════════════════════════════════════════════════════════════

// ═══════════════════════════════════════════════════════════════════════
//  F6 — API diem dieu khien tong quat
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("F6: CornerPin co dung 4 diem dieu khien, khop voi corner()") {
    WarpCornerPin w(kKeystone);
    REQUIRE(w.controlPointCount() == 4);
    for (int k = 0; k < 4; ++k) {
        CHECK(w.controlPointAt(k).nearlyEquals(w.corner(k), 1e-12));
    }
}

TEST_CASE("F6: Mesh co (cols+1)x(rows+1) diem dieu khien") {
    WarpMesh m(4, 3, Vec2{0, 0}, Vec2{800, 600});
    REQUIRE(m.controlPointCount() == 5 * 4);

    // Chi so chay theo hang: index = cy*(cols+1) + cx
    CHECK(m.controlPointAt(0).nearlyEquals(m.controlPoint(0, 0), 1e-12));
    CHECK(m.controlPointAt(4).nearlyEquals(m.controlPoint(4, 0), 1e-12));
    CHECK(m.controlPointAt(5).nearlyEquals(m.controlPoint(0, 1), 1e-12));
    CHECK(m.controlPointAt(19).nearlyEquals(m.controlPoint(4, 3), 1e-12));
}

TEST_CASE("★ F6: keo diem mesh qua API tong quat lam doi be mat") {
    WarpMesh m(2, 2, Vec2{0, 0}, Vec2{800, 800});
    const Vec2 before = m.forward({0.5, 0.5});

    // Diem giua cua luoi 3x3 la index 4.
    REQUIRE(m.setControlPointAt(4, Vec2{400.0, 250.0}));

    const Vec2 after = m.forward({0.5, 0.5});
    REQUIRE(after.distanceTo(before) > 100.0);
}

TEST_CASE("★★ F6: CornerPin TU CHOI qua API tong quat y het setCorner") {
    // UI dung setControlPointAt cho MOI loai warp. Neu duong nay khong
    // kiem tra hop le, nguoi dung se keo duoc corner pin thanh hinh lom
    // qua UI du setCorner() co chan — mot lo hong am tham.
    WarpCornerPin w(kKeystone);
    const Vec2 before = w.controlPointAt(0);

    REQUIRE(!w.setControlPointAt(0, Vec2{999.0, 888.0}));   // se thanh lom
    REQUIRE(w.controlPointAt(0).nearlyEquals(before, 1e-12));
    REQUIRE(w.isInvertible());

    REQUIRE(w.setControlPointAt(0, Vec2{260.0, 190.0}));    // hop le
    REQUIRE(w.controlPointAt(0).nearlyEquals(Vec2{260.0, 190.0}, 1e-12));
}

TEST_CASE("F6: chi so ngoai pham vi khong lam sap") {
    WarpCornerPin w(kKeystone);
    REQUIRE(!w.setControlPointAt(-1, Vec2{0, 0}));
    REQUIRE(!w.setControlPointAt(99, Vec2{0, 0}));
    REQUIRE(w.controlPointAt(-1).nearlyEquals(Vec2{}, 1e-12));

    WarpMesh m(2, 2, Vec2{0, 0}, Vec2{100, 100});
    REQUIRE(!m.setControlPointAt(-1, Vec2{0, 0}));
    REQUIRE(!m.setControlPointAt(999, Vec2{0, 0}));
}

TEST_CASE("★ F6: sau khi keo diem mesh, inverse() van dung") {
    // Keo diem roi ma khong nghich dao duoc thi sensor mat tac dung —
    // dung loai loi am tham ma quy tac §10.3 muon chan.
    WarpMesh m(3, 3, Vec2{100, 100}, Vec2{600, 600});
    REQUIRE(m.setControlPointAt(5, Vec2{280.0, 240.0}));

    const Vec2 uv{0.45, 0.55};
    Vec2 back;
    REQUIRE(m.inverse(m.forward(uv), back));
    REQUIRE_VEC_NEAR(back, uv.x, uv.y, 1e-6);
}

TEST_CASE("★ Hop dong IWarp: moi warp deu nghich dao duoc") {
    // architecture.md §10.3 — neu mot warp khong nghich dao duoc thi no
    // vo dung voi calibration sensor. Test nay bao ve quy tac do.
    std::vector<std::unique_ptr<IWarp>> warps;
    warps.push_back(std::make_unique<WarpCornerPin>(kKeystone));
    warps.push_back(std::make_unique<WarpMesh>(4, 4, Vec2{50.0, 50.0}, Vec2{900.0, 700.0}));

    for (const auto& w : warps) {
        REQUIRE(w->isInvertible());
        REQUIRE(w->defaultSubdivisions() >= 1);

        const Vec2 uv{0.37, 0.62};
        const Vec2 px = w->forward(uv);

        Vec2 back;
        REQUIRE(w->inverse(px, back));
        CHECK_NEAR(back.x, uv.x, 1e-7);
        CHECK_NEAR(back.y, uv.y, 1e-7);

        Vec2 lo, hi;
        w->boundingBox(lo, hi);
        CHECK(lo.x <= hi.x);
        CHECK(lo.y <= hi.y);
    }
}

#include "TestHarness.h"

#include "core/model/WarpBezier.h"
#include "core/model/WarpCornerPin.h"
#include "core/model/WarpMesh.h"
#include "core/model/Slice.h"

#include <cmath>

using namespace hexmap;

namespace {

/// Uốn mặt phẳng thành mặt cong kiểu CỘT TRÒN: đẩy hàng giữa phồng lên,
/// tắt dần về hai mép. Đây đúng là hình dạng F10 sinh ra để phục vụ.
WarpBezier makeCurved() {
    WarpBezier w(Vec2{100.0, 100.0}, Vec2{800.0, 600.0});
    for (int cy = 0; cy < WarpBezier::kDim; ++cy) {
        for (int cx = 0; cx < WarpBezier::kDim; ++cx) {
            const Vec2 p = w.controlPoint(cx, cy);
            const double u = static_cast<double>(cx) / (WarpBezier::kDim - 1);
            w.setControlPoint(cx, cy,
                              Vec2{p.x, p.y + 90.0 * std::sin(u * 3.14159265358979)});
        }
    }
    return w;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  F10 — forward
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("F10: reset ve hinh chu nhat cho anh xa PHANG, khong cong nhe") {
    WarpBezier w(Vec2{100.0, 200.0}, Vec2{800.0, 400.0});

    const Vec2 tl = w.forward(Vec2{0.0, 0.0});
    const Vec2 br = w.forward(Vec2{1.0, 1.0});
    CHECK_NEAR(tl.x, 100.0, 1e-9);
    CHECK_NEAR(tl.y, 200.0, 1e-9);
    CHECK_NEAR(br.x, 900.0, 1e-9);
    CHECK_NEAR(br.y, 600.0, 1e-9);

    // ★ Diem trong phai dat DUNG 1/3 va 2/3 thi be mat moi phang tuyet
    //   doi. Dat sai mot chut thi "reset" cho ra hinh cong nhe — nguoi
    //   dung se can lai tu dau ma khong hieu tai sao.
    for (int i = 1; i < 10; ++i) {
        const double t = i / 10.0;
        const Vec2 p = w.forward(Vec2{t, 0.5});
        CHECK_NEAR(p.x, 100.0 + t * 800.0, 1e-9);
        CHECK_NEAR(p.y, 400.0, 1e-9);
    }
}

TEST_CASE("F10: bon goc duoc noi suy CHINH XAC ke ca khi da uon") {
    const WarpBezier w = makeCurved();

    // Bernstein bac ba noi suy hai dau, nen goc cua be mat = goc cua luoi
    // dieu khien. Do la thu giu cho viec can chinh khong bi truot khi
    // nguoi dung keo cac diem giua.
    const Vec2 c00 = w.controlPoint(0, 0);
    const Vec2 c30 = w.controlPoint(3, 0);
    const Vec2 c33 = w.controlPoint(3, 3);
    const Vec2 c03 = w.controlPoint(0, 3);

    const Vec2 f00 = w.forward(Vec2{0.0, 0.0});
    const Vec2 f10 = w.forward(Vec2{1.0, 0.0});
    const Vec2 f11 = w.forward(Vec2{1.0, 1.0});
    const Vec2 f01 = w.forward(Vec2{0.0, 1.0});

    CHECK_NEAR(f00.x, c00.x, 1e-9);  CHECK_NEAR(f00.y, c00.y, 1e-9);
    CHECK_NEAR(f10.x, c30.x, 1e-9);  CHECK_NEAR(f10.y, c30.y, 1e-9);
    CHECK_NEAR(f11.x, c33.x, 1e-9);  CHECK_NEAR(f11.y, c33.y, 1e-9);
    CHECK_NEAR(f01.x, c03.x, 1e-9);  CHECK_NEAR(f01.y, c03.y, 1e-9);
}

// ═══════════════════════════════════════════════════════════════════════
//  F10 — nghịch đảo (★ đường đi của điểm chạm sensor)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("F10: round-trip tren be mat PHANG") {
    WarpBezier w(Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0});

    int tested = 0;
    for (int gy = 0; gy <= 10; ++gy) {
        for (int gx = 0; gx <= 10; ++gx) {
            const Vec2 uv{gx / 10.0, gy / 10.0};
            Vec2 back;
            REQUIRE(w.inverse(w.forward(uv), back));
            CHECK_NEAR(back.x, uv.x, 1e-9);
            CHECK_NEAR(back.y, uv.y, 1e-9);
            ++tested;
        }
    }
    REQUIRE(tested == 121);
}

// ★ Day moi la bai kiem tra that cua F10: be mat CONG, noi ma mesh chi
//   xap xi tung o con Bezier phai dung ca trong long o.
//
//   Nguong 1e-9 khong phai lam mau: Newton hoi tu bac hai nen sai so phai
//   xuong toi muc double. Neu chi dat 1e-3 thi tuc la buoc nan da khong
//   chay, va nghich dao dang song bang doan tho — cham vao vat the se
//   lech thay ro tren be mat cong.
TEST_CASE("★ F10: round-trip tren be mat CONG dat do chinh xac double") {
    const WarpBezier w = makeCurved();

    int tested = 0;
    double worst = 0.0;
    for (int gy = 0; gy <= 12; ++gy) {
        for (int gx = 0; gx <= 12; ++gx) {
            const Vec2 uv{gx / 12.0, gy / 12.0};
            const Vec2 px = w.forward(uv);

            Vec2 back;
            REQUIRE(w.inverse(px, back));
            worst = std::max(worst, std::abs(back.x - uv.x));
            worst = std::max(worst, std::abs(back.y - uv.y));
            ++tested;
        }
    }
    REQUIRE(tested == 169);
    CHECK_MSG(worst < 1e-9, "sai so lon nhat = " + std::to_string(worst));
}

TEST_CASE("F10: diem ngoai be mat bi TU CHOI") {
    const WarpBezier w = makeCurved();
    Vec2 uv;
    CHECK(!w.inverse(Vec2{-5000.0, -5000.0}, uv));
    CHECK(!w.inverse(Vec2{9999.0, 9999.0}, uv));
    CHECK(!w.inverse(Vec2{50.0, 50.0}, uv));      // ngay sat goc, van ngoai
}

// ★ Tam moi o cua luoi lay mau nam dung tren duong cheo — dung cai bay da
//   lam hong WarpMesh (xem commit "VET CHAM CHET"). Bezier dung lai chinh
//   bo may do o buoc doan tho nen phai kiem lai o day.
TEST_CASE("★ F10: tam o luoi lay mau van nghich dao duoc") {
    WarpBezier w(Vec2{0.0, 0.0}, Vec2{800.0, 600.0});

    for (int gy = 0; gy < 8; ++gy) {
        for (int gx = 0; gx < 8; ++gx) {
            const Vec2 uv{(gx + 0.5) / 8.0, (gy + 0.5) / 8.0};
            Vec2 back;
            REQUIRE(w.inverse(w.forward(uv), back));
            CHECK_NEAR(back.x, uv.x, 1e-9);
            CHECK_NEAR(back.y, uv.y, 1e-9);
        }
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  F10 — trạng thái hợp lệ
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("F10: phang va cong deu thi nghich dao duoc") {
    WarpBezier flat(Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0});
    CHECK(flat.isInvertible());
    CHECK(makeCurved().isInvertible());
}

// ★ Keo mot diem dieu khien qua han lam be mat GAP LEN CHINH NO. Luc do
//   mot diem output ung voi nhieu (u,v) — nghich dao khong con nghia.
//   isInvertible() phai bao false de UI hien canh bao do (bang thuoc tinh
//   slice da co san cho canh bao nay).
TEST_CASE("★ F10: be mat GAP thi bao khong nghich dao duoc") {
    WarpBezier w(Vec2{0.0, 0.0}, Vec2{800.0, 600.0});
    REQUIRE(w.isInvertible());

    // Nem goc duoi-phai vuot han sang trai qua goc duoi-trai.
    w.setControlPoint(3, 3, Vec2{-1200.0, 600.0});
    CHECK(!w.isInvertible());
}

// ═══════════════════════════════════════════════════════════════════════
//  F10 — hợp đồng IWarp
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("F10: hop dong IWarp — 16 diem, doc/ghi khop nhau") {
    WarpBezier w(Vec2{0.0, 0.0}, Vec2{400.0, 300.0});
    REQUIRE(w.controlPointCount() == 16);
    REQUIRE(w.type() == WarpType::Bezier);

    for (int i = 0; i < 16; ++i) {
        const Vec2 p{static_cast<double>(i) * 7.0, static_cast<double>(i) * 3.0};
        REQUIRE(w.setControlPointAt(i, p));
        const Vec2 got = w.controlPointAt(i);
        CHECK_NEAR(got.x, p.x, 1e-12);
        CHECK_NEAR(got.y, p.y, 1e-12);
    }

    // Chi so ngoai pham vi khong duoc lam sap.
    CHECK(!w.setControlPointAt(-1, Vec2{0.0, 0.0}));
    CHECK(!w.setControlPointAt(16, Vec2{0.0, 0.0}));
}

TEST_CASE("F10: clone doc lap") {
    WarpBezier w(Vec2{0.0, 0.0}, Vec2{400.0, 300.0});
    auto c = w.clone();
    REQUIRE(c != nullptr);
    REQUIRE(c->type() == WarpType::Bezier);

    w.setControlPoint(1, 1, Vec2{-999.0, -999.0});
    const Vec2 cloned = c->controlPointAt(WarpBezier::kDim * 1 + 1);
    CHECK(std::abs(cloned.x + 999.0) > 1.0);   // ban sao KHONG bi anh huong
}

TEST_CASE("F10: hop bao chua toan bo be mat") {
    const WarpBezier w = makeCurved();
    Vec2 lo, hi;
    w.boundingBox(lo, hi);

    for (int gy = 0; gy <= 10; ++gy) {
        for (int gx = 0; gx <= 10; ++gx) {
            const Vec2 p = w.forward(Vec2{gx / 10.0, gy / 10.0});
            CHECK(p.x >= lo.x - 1e-9);
            CHECK(p.x <= hi.x + 1e-9);
            CHECK(p.y >= lo.y - 1e-9);
            CHECK(p.y <= hi.y + 1e-9);
        }
    }
}

TEST_CASE("F10: tessellate sinh dung so luong") {
    WarpBezier w(Vec2{0.0, 0.0}, Vec2{800.0, 600.0});
    WarpGeometry g;
    w.tessellate(4, 3, g);
    CHECK(g.vertices.size() == 5u * 4u);
    CHECK(g.triangleCount() == 4u * 3u * 2u);
}

// ★ Doi tu warp khac sang Bezier phai GIU DUOC bon goc chinh xac.
//   Do la phan nguoi van hanh ton cong nhat; mat no la phai can lai tu dau.
TEST_CASE("★ F10: doi tu CornerPin sang Bezier giu NGUYEN bon goc") {
    WarpCornerPin cp;
    cp.setCorner(0, Vec2{120.0,  80.0});
    cp.setCorner(1, Vec2{900.0, 140.0});
    cp.setCorner(2, Vec2{850.0, 640.0});
    cp.setCorner(3, Vec2{ 60.0, 600.0});

    const WarpBezier b = WarpBezier::fromWarp(cp);

    const Vec2 uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    for (const Vec2& uv : uvs) {
        const Vec2 want = cp.forward(uv);
        const Vec2 got  = b.forward(uv);
        CHECK_NEAR(got.x, want.x, 1e-9);
        CHECK_NEAR(got.y, want.y, 1e-9);
    }
}

TEST_CASE("F10: doi tu Mesh sang Bezier giu bon goc") {
    WarpMesh m(4, 4, Vec2{50.0, 60.0}, Vec2{700.0, 500.0});
    const WarpBezier b = WarpBezier::fromWarp(m);

    const Vec2 uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    for (const Vec2& uv : uvs) {
        const Vec2 want = m.forward(uv);
        const Vec2 got  = b.forward(uv);
        CHECK_NEAR(got.x, want.x, 1e-9);
        CHECK_NEAR(got.y, want.y, 1e-9);
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  F10 — nối vào Slice và file .hexmap
// ═══════════════════════════════════════════════════════════════════════

// ★ Doi loai warp sang Bezier PHAI giu bon goc da can.
//   Doi sang Bezier la viec lam SAU khi da can goc (de uon them cho khop
//   mat cong). Neu no dat lai ve hinh chu nhat thi tinh nang vo dung.
TEST_CASE("★ F10: Slice doi sang Bezier giu nguyen bon goc da can") {
    Slice s(Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0},
            Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0});

    auto* cp = static_cast<WarpCornerPin*>(s.warp());
    cp->setCorner(0, Vec2{130.0,  90.0});
    cp->setCorner(1, Vec2{910.0, 150.0});
    cp->setCorner(2, Vec2{860.0, 650.0});
    cp->setCorner(3, Vec2{ 70.0, 610.0});

    Vec2 want[4];
    const Vec2 uvs[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    for (int i = 0; i < 4; ++i) want[i] = s.warp()->forward(uvs[i]);

    s.convertWarp(WarpType::Bezier);
    REQUIRE(s.warp()->type() == WarpType::Bezier);

    for (int i = 0; i < 4; ++i) {
        const Vec2 got = s.warp()->forward(uvs[i]);
        CHECK_NEAR(got.x, want[i].x, 1e-9);
        CHECK_NEAR(got.y, want[i].y, 1e-9);
    }
}

TEST_CASE("F10: Slice.outputToContent chay qua warp Bezier") {
    Slice s(Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0},
            Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0});
    s.convertWarp(WarpType::Bezier);

    const Vec2 uv{0.31, 0.72};
    const Vec2 px = s.warp()->forward(uv);

    Vec2 back;
    REQUIRE(s.outputToContent(px, back));
    CHECK_NEAR(back.x, uv.x, 1e-8);
    CHECK_NEAR(back.y, uv.y, 1e-8);
}

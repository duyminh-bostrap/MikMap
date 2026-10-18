#include "TestHarness.h"

#include "core/math/EdgeBlend.h"

#include <cmath>

using namespace mikmap::edgeblend;

TEST_CASE("EdgeBlend: hai rect canh nhau -> dai ben phai / ben trai") {
    Band b;
    REQUIRE(overlapBand(Rect{0, 0, 1000, 500}, Rect{800, 0, 1000, 500}, b));
    CHECK(b.side == Side::Right);
    CHECK_NEAR(b.start, 800, 1e-12); CHECK_NEAR(b.end, 1000, 1e-12);
    CHECK_NEAR(b.area.x, 800, 1e-12); CHECK_NEAR(b.area.w, 200, 1e-12); CHECK_NEAR(b.area.h, 500, 1e-12);
    REQUIRE(overlapBand(Rect{800, 0, 1000, 500}, Rect{0, 0, 1000, 500}, b));
    CHECK(b.side == Side::Left);
    CHECK_NEAR(b.start, 800, 1e-12); CHECK_NEAR(b.end, 1000, 1e-12);
}
TEST_CASE("EdgeBlend: xep doc -> dai ben duoi") {
    Band b;
    REQUIRE(overlapBand(Rect{0, 0, 500, 1000}, Rect{0, 900, 500, 1000}, b));
    CHECK(b.side == Side::Bottom);
    CHECK_NEAR(b.start, 900, 1e-12); CHECK_NEAR(b.end, 1000, 1e-12);
}
TEST_CASE("EdgeBlend: cham canh hoac roi nhau -> khong co dai") {
    Band b;
    CHECK(!overlapBand(Rect{0, 0, 100, 100}, Rect{100, 0, 100, 100}, b));
    CHECK(!overlapBand(Rect{0, 0, 100, 100}, Rect{500, 500, 100, 100}, b));
}
TEST_CASE("★ EdgeBlend: rect nay nam trong rect kia -> khong phai ghep mep") {
    Band b;
    CHECK(!overlapBand(Rect{100, 100, 100, 100}, Rect{0, 0, 1000, 1000}, b));
    CHECK(!overlapBand(Rect{0, 0, 1000, 1000}, Rect{100, 100, 100, 100}, b));
}
TEST_CASE("EdgeBlend: bandT 0 o mep minh, 1 o dau trong, kep [0,1]") {
    Band b;
    REQUIRE(overlapBand(Rect{0, 0, 1000, 500}, Rect{800, 0, 1000, 500}, b));
    CHECK_NEAR(bandT(b, 1000, 0), 0.0, 1e-12);
    CHECK_NEAR(bandT(b, 800, 0), 1.0, 1e-12);
    CHECK_NEAR(bandT(b, 900, 0), 0.5, 1e-12);
    CHECK_NEAR(bandT(b, 500, 0), 1.0, 1e-12);
    CHECK_NEAR(bandT(b, 1100, 0), 0.0, 1e-12);
}
TEST_CASE("EdgeBlend: ramp dau cuoi, giua = luminance, khong giam") {
    CHECK_NEAR(ramp(0, 0.5, 2), 0.0, 1e-12);
    CHECK_NEAR(ramp(1, 0.5, 2), 1.0, 1e-12);
    CHECK_NEAR(ramp(0.5, 0.5, 2), 0.5, 1e-12);
    CHECK_NEAR(ramp(0.5, 0.3, 2), 0.3, 1e-12);
    for (double p : {1.0, 2.0, 4.0}) {
        double prev = -1;
        for (int i = 0; i <= 20; ++i) { double v = ramp(i / 20.0, 0.5, p); CHECK(v >= prev); prev = v; }
    }
}
TEST_CASE("★ EdgeBlend: hai phia bu nhau thanh 1 khi luminance 0.5") {
    for (double p : {1.0, 2.0, 3.5})
        for (int i = 0; i <= 20; ++i) { double t = i / 20.0; CHECK_NEAR(ramp(t, 0.5, p) + ramp(1 - t, 0.5, p), 1.0, 1e-12); }
}
TEST_CASE("EdgeBlend: gammaRamp") {
    CHECK_NEAR(gammaRamp(0.3, 0.5, 2, 1), ramp(0.3, 0.5, 2), 1e-12);
    CHECK_NEAR(gammaRamp(0.3, 0.5, 2, 2), std::sqrt(ramp(0.3, 0.5, 2)), 1e-12);
    for (double g : {0.0, -1.0, std::nan("")}) CHECK_NEAR(gammaRamp(0.3, 0.5, 2, g), ramp(0.3, 0.5, 2), 1e-12);
    for (int i = 0; i <= 10; ++i) { double v = gammaRamp(i / 10.0, 0.5, 2, 2.2); CHECK(v >= 0.0); CHECK(v <= 1.0); }
}
TEST_CASE("EdgeBlend: tham so vo ly bi kep, khong NaN") {
    CHECK_NEAR(ramp(-1, 0.5, 2), 0.0, 1e-12);
    CHECK_NEAR(ramp(2, 0.5, 2), 1.0, 1e-12);
    CHECK_NEAR(ramp(0.5, 2.0, 2), 1.0, 1e-12);
    CHECK(std::isfinite(ramp(0.25, 0.5, 0.0)));
}

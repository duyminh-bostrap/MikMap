#include "TestHarness.h"

#include "core/math/Triangulate.h"

#include <cmath>

using namespace mikmap::tri;

namespace {
using Tris = std::vector<std::array<int, 3>>;

double triArea(const std::vector<P>& p, const std::array<int, 3>& t) {
    const P a = p[t[0]], b = p[t[1]], c = p[t[2]];
    return std::fabs((b.x - a.x) * (c.y - a.y) - (c.x - a.x) * (b.y - a.y)) * 0.5;
}
double totalArea(const std::vector<P>& p, const Tris& t) {
    double s = 0;
    for (const auto& x : t) s += triArea(p, x);
    return s;
}
// L: [0,2]x[0,1] ∪ [0,1]x[1,2], diện tích 3.
bool insideL(P q) {
    return (q.x > 0 && q.x < 2 && q.y > 0 && q.y < 1) || (q.x > 0 && q.x < 1 && q.y > 0 && q.y < 2);
}
}  // namespace

TEST_CASE("Triangulate: square_gives_two_triangles") {
    std::vector<P> sq{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    Tris t;
    REQUIRE(triangulate(sq, t));
    CHECK(t.size() == 2);
    CHECK_NEAR(totalArea(sq, t), 1.0, 1e-12);
}
TEST_CASE("Triangulate: concave_L_stays_inside") {
    std::vector<P> L{{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}};
    Tris t;
    REQUIRE(triangulate(L, t));
    CHECK(t.size() == 4);
    for (const auto& x : t) {
        P c{(L[x[0]].x + L[x[1]].x + L[x[2]].x) / 3, (L[x[0]].y + L[x[1]].y + L[x[2]].y) / 3};
        CHECK(insideL(c));
    }
    CHECK_NEAR(totalArea(L, t), 3.0, 1e-12);
}
TEST_CASE("Triangulate: clockwise_and_counter_clockwise_same_area") {
    std::vector<P> ccw{{0, 0}, {2, 0}, {2, 1}, {1, 1}, {1, 2}, {0, 2}};
    std::vector<P> cw(ccw.rbegin(), ccw.rend());
    Tris a, b;
    REQUIRE(triangulate(ccw, a));
    REQUIRE(triangulate(cw, b));
    CHECK(b.size() == 4);
    CHECK_NEAR(totalArea(ccw, a), totalArea(cw, b), 1e-12);
}
TEST_CASE("Triangulate: bow_tie_is_refused") {
    std::vector<P> bow{{0, 0}, {1, 1}, {1, 0}, {0, 1}};
    Tris t{{0, 1, 2}};
    CHECK(!simple(bow));
    CHECK(!triangulate(bow, t));
    CHECK(t.empty());
}
TEST_CASE("Triangulate: fewer_than_three_refused") {
    Tris t;
    CHECK(!simple({{0, 0}, {1, 0}}));
    CHECK(!triangulate({{0, 0}, {1, 0}}, t));
    CHECK(!triangulate({}, t));
    CHECK(!simple({{0, 0}, {1, 0}, {2, 0}}));  // cả ba thẳng hàng: diện tích 0
}
TEST_CASE("Triangulate: collinear_point_ok") {
    std::vector<P> sq{{0, 0}, {0.5, 0}, {1, 0}, {1, 1}, {0, 1}};
    Tris t;
    REQUIRE(simple(sq));
    REQUIRE(triangulate(sq, t));
    CHECK(t.size() == 3);
    CHECK_NEAR(totalArea(sq, t), 1.0, 1e-12);
    for (const auto& x : t) CHECK(triArea(sq, x) > 1e-12);  // có cách tránh tam giác dẹt
}
TEST_CASE("Triangulate: duplicate_point_refused") {
    Tris t;
    CHECK(!simple({{0, 0}, {1, 0}, {1, 0}, {1, 1}, {0, 1}}));
    CHECK(!triangulate({{0, 0}, {1, 0}, {1, 0}, {1, 1}, {0, 1}}, t));
}
TEST_CASE("Triangulate: barycentric_reconstructs_point") {
    P a{0, 0}, b{4, 0}, c{1, 3}, p{1.5, 1.0};
    double u, v, w;
    REQUIRE(barycentric(a, b, c, p, u, v, w));
    CHECK_NEAR(u + v + w, 1.0, 1e-12);
    CHECK_NEAR(u * a.x + v * b.x + w * c.x, p.x, 1e-12);
    CHECK_NEAR(u * a.y + v * b.y + w * c.y, p.y, 1e-12);
    CHECK(!barycentric(a, b, P{8, 0}, p, u, v, w));  // suy biến
}
TEST_CASE("Triangulate: locate_inside_and_outside") {
    std::vector<P> sq{{0, 0}, {1, 0}, {1, 1}, {0, 1}};
    Tris t;
    REQUIRE(triangulate(sq, t));
    int k; double u, v, w;
    CHECK(!locate(sq, t, P{2, 2}, k, u, v, w));
    REQUIRE(locate(sq, t, P{0.25, 0.6}, k, u, v, w));
    const auto& x = t[k];
    CHECK_NEAR(u * sq[x[0]].x + v * sq[x[1]].x + w * sq[x[2]].x, 0.25, 1e-12);
    CHECK_NEAR(u * sq[x[0]].y + v * sq[x[1]].y + w * sq[x[2]].y, 0.6, 1e-12);
    CHECK(locate(sq, t, P{1, 1}, k, u, v, w));  // đỉnh: nằm trên cạnh vẫn tính
}

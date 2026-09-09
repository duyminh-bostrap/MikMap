#include "TestHarness.h"

#include "core/math/Mat3.h"

using namespace hexmap;

TEST_CASE("Mat3: identity khong lam thay doi diem") {
    const Mat3 I;
    const Vec2 p{123.456, -78.9};
    const Vec2 q = I.transformPoint(p);
    REQUIRE_VEC_NEAR(q, p.x, p.y, 1e-12);
}

TEST_CASE("Mat3: tinh tien") {
    const Mat3 T = Mat3::translation(10.0, -5.0);
    const Vec2 q = T.transformPoint({3.0, 4.0});
    REQUIRE_VEC_NEAR(q, 13.0, -1.0, 1e-12);
}

TEST_CASE("Mat3: co gian") {
    const Mat3 S = Mat3::scaling(2.0, 3.0);
    const Vec2 q = S.transformPoint({4.0, 5.0});
    REQUIRE_VEC_NEAR(q, 8.0, 15.0, 1e-12);
}

TEST_CASE("Mat3: xoay 90 do") {
    const double pi = 3.14159265358979323846;
    const Mat3 R = Mat3::rotation(pi / 2.0);
    const Vec2 q = R.transformPoint({1.0, 0.0});
    REQUIRE_VEC_NEAR(q, 0.0, 1.0, 1e-12);
}

TEST_CASE("Mat3: xoay quanh anchor giu nguyen anchor") {
    const double pi = 3.14159265358979323846;
    const Vec2 anchor{100.0, 200.0};
    const Mat3 R = Mat3::rotationAround(pi / 3.0, anchor);
    const Vec2 q = R.transformPoint(anchor);
    REQUIRE_VEC_NEAR(q, anchor.x, anchor.y, 1e-9);
}

TEST_CASE("Mat3: nhan ma tran co tinh ket hop") {
    const Mat3 A = Mat3::translation(5.0, 7.0);
    const Mat3 B = Mat3::scaling(2.0, 2.0);
    const Mat3 C = Mat3::rotation(0.37);

    const Mat3 left  = (A * B) * C;
    const Mat3 right = A * (B * C);
    REQUIRE(left.nearlyEquals(right, 1e-12));
}

TEST_CASE("Mat3: thu tu nhan dung quy uoc (T*S ap dung S truoc)") {
    // T * S nghia la: co gian TRUOC, roi tinh tien.
    const Mat3 T = Mat3::translation(10.0, 0.0);
    const Mat3 S = Mat3::scaling(2.0, 1.0);
    const Vec2 q = (T * S).transformPoint({3.0, 0.0});
    REQUIRE_VEC_NEAR(q, 16.0, 0.0, 1e-12);   // 3*2 + 10
}

TEST_CASE("Mat3: M * M-nghich-dao = identity") {
    const Mat3 M = Mat3::translation(11.0, -4.0)
                 * Mat3::rotation(0.83)
                 * Mat3::scaling(3.0, 0.5);

    Mat3 Minv;
    REQUIRE(M.invert(Minv));
    REQUIRE((M * Minv).nearlyEquals(Mat3::identity(), 1e-9));
    REQUIRE((Minv * M).nearlyEquals(Mat3::identity(), 1e-9));
}

TEST_CASE("Mat3: nghich dao khu bien doi tren diem") {
    const Mat3 M = Mat3::translation(50.0, 60.0) * Mat3::scaling(1.5, 2.5);
    Mat3 Minv;
    REQUIRE(M.invert(Minv));

    const Vec2 p{7.0, -3.0};
    const Vec2 roundTrip = Minv.transformPoint(M.transformPoint(p));
    REQUIRE_VEC_NEAR(roundTrip, p.x, p.y, 1e-9);
}

TEST_CASE("Mat3: ma tran suy bien khong nghich dao duoc") {
    // Hai hang phu thuoc tuyen tinh.
    const Mat3 singular{1.0, 2.0, 3.0,
                        2.0, 4.0, 6.0,
                        0.0, 0.0, 1.0};
    Mat3 out;
    REQUIRE(!singular.invert(out));
    REQUIRE(!singular.isInvertible());
}

TEST_CASE("Mat3: dinh thuc") {
    REQUIRE_NEAR(Mat3::identity().determinant(), 1.0, 1e-12);
    REQUIRE_NEAR(Mat3::scaling(2.0, 3.0).determinant(), 6.0, 1e-12);
    REQUIRE_NEAR(Mat3::translation(9.0, 9.0).determinant(), 1.0, 1e-12);
}

TEST_CASE("Mat3: chuyen vi hai lan quay ve ban dau") {
    const Mat3 M{1, 2, 3, 4, 5, 6, 7, 8, 9};
    REQUIRE(M.transposed().transposed().nearlyEquals(M, 1e-12));
}

TEST_CASE("Mat3: chia dong nhat tao ra hieu ung phoi canh") {
    // Ma tran co hang cuoi khac (0,0,1) => khong con la affine.
    // Day chinh la thu tao ra keystone: cac diem cach deu nhau o khong
    // gian nguon KHONG con cach deu o khong gian dich.
    const Mat3 P{1.0, 0.0, 0.0,
                 0.0, 1.0, 0.0,
                 0.001, 0.0, 1.0};

    const Vec2 a = P.transformPoint({0.0, 100.0});
    const Vec2 b = P.transformPoint({500.0, 100.0});
    const Vec2 c = P.transformPoint({1000.0, 100.0});

    const double gap1 = b.x - a.x;
    const double gap2 = c.x - b.x;

    // Neu la affine thi gap1 == gap2. Phoi canh lam chung khac nhau.
    REQUIRE(std::abs(gap1 - gap2) > 1.0);

    // Va y bi co lai theo khoang cach — dau hieu dac trung cua phoi canh.
    REQUIRE(c.y < a.y);
}

TEST_CASE("Mat3: transformDirection bo qua tinh tien") {
    const Mat3 M = Mat3::translation(1000.0, 2000.0) * Mat3::scaling(2.0, 2.0);
    const Vec2 d = M.transformDirection({1.0, 0.0});
    REQUIRE_VEC_NEAR(d, 2.0, 0.0, 1e-12);
}

#include "TestHarness.h"

#include "core/license/License.h"

using namespace mikmap::license;

namespace {

// 2026-10-10 00:00:00 UTC
constexpr int64_t kNow = 1791590400;
constexpr int64_t kDay = 86400;
constexpr int kBuild = 20261010;

Claims claims(State s, int64_t expires = 0, int64_t updates = 0) {
    Claims c;
    c.state = s;
    c.expiresAt = expires;
    c.updatesUntil = updates;
    return c;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  Mặc định an toàn
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("License: mac dinh la KHONG co license") {
    const Entitlements e;
    CHECK(e.watermark);
    CHECK(!e.sensorOut);
    CHECK(!e.ndiOut);
    CHECK(!e.licensed());

    const Entitlements r = evaluate(Claims{}, kBuild, kNow);
    CHECK(r.watermark);
    CHECK(!r.sensorOut);
}

TEST_CASE("★ License: chi Licensed va Trial moi mo khoa, trang thai khac thi khong") {
    CHECK(evaluate(claims(State::Licensed), kBuild, kNow).licensed());
    CHECK(evaluate(claims(State::Trial), kBuild, kNow).licensed());

    for (const State s : {State::None, State::Expired, State::OtherMachine,
                          State::Revoked, State::Invalid}) {
        const Entitlements e = evaluate(claims(s), kBuild, kNow);
        CHECK_MSG(e.watermark, stateName(s));
        CHECK_MSG(!e.sensorOut, stateName(s));
        CHECK_MSG(!e.ndiOut, stateName(s));
    }
}

TEST_CASE("License: ba quyen cung bat khi co license") {
    const Entitlements e = evaluate(claims(State::Licensed), kBuild, kNow);
    CHECK(!e.watermark);
    CHECK(e.sensorOut);
    CHECK(e.ndiOut);
}

// ═══════════════════════════════════════════════════════════════════════
//  Hết hạn dùng
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("License: expiresAt trong tuong lai thi con, da qua thi het") {
    CHECK(evaluate(claims(State::Licensed, kNow + kDay), kBuild, kNow).licensed());
    CHECK(!evaluate(claims(State::Licensed, kNow), kBuild, kNow).licensed());       // dung luc het
    CHECK(!evaluate(claims(State::Licensed, kNow - 1), kBuild, kNow).licensed());
    CHECK(!evaluate(claims(State::Trial, kNow - kDay), kBuild, kNow).licensed());
}

// ═══════════════════════════════════════════════════════════════════════
//  updates_until (cập nhật đến ngày)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ License: ban build SAU updates_until chay nhu khong license") {
    const int64_t until = kNow;                       // 2026-10-10
    CHECK(evaluate(claims(State::Licensed, 0, until), 20261010, kNow).licensed());   // cung ngay: duoc phu
    CHECK(evaluate(claims(State::Licensed, 0, until), 20260901, kNow).licensed());   // truoc: duoc phu
    CHECK(!evaluate(claims(State::Licensed, 0, until), 20261011, kNow).licensed());  // sau: khong
}

TEST_CASE("License: updates_until = 0 nghia la khong gioi han") {
    CHECK(evaluate(claims(State::Licensed, 0, 0), 20991231, kNow).licensed());
}

TEST_CASE("★ License: Trial KHONG bi rang buoc boi updates_until") {
    CHECK(evaluate(claims(State::Trial, 0, kNow - 400 * kDay), kBuild, kNow).licensed());
}

TEST_CASE("★ License: khong biet ngay build ma co updates_until -> khong tin") {
    CHECK(!evaluate(claims(State::Licensed, 0, kNow), 0, kNow).licensed());
    CHECK(!evaluate(claims(State::Licensed, 0, kNow), -1, kNow).licensed());
    // khong co updates_until thi ngay build khong quan trong
    CHECK(evaluate(claims(State::Licensed), 0, kNow).licensed());
}

// ═══════════════════════════════════════════════════════════════════════
//  Ngày tháng
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("License: ymdFromUnix dung o cac moc quen thuoc") {
    CHECK(ymdFromUnix(0) == 19700101);
    CHECK(ymdFromUnix(kNow) == 20261010);
    CHECK(ymdFromUnix(kNow + kDay - 1) == 20261010);   // 23:59:59 van la ngay do
    CHECK(ymdFromUnix(kNow + kDay) == 20261011);
    CHECK(ymdFromUnix(951782400) == 20000229);         // nam nhuan
    CHECK(ymdFromUnix(-1) == 19691231);                // gio am
}

TEST_CASE("License: parseBuildDate chap nhan dung dinh dang, tu choi phan con lai") {
    int ymd = 7;
    REQUIRE(parseBuildDate("2026-10-10", ymd));
    CHECK(ymd == 20261010);

    ymd = 7;
    for (const char* bad : {"", "2026-1-10", "20261010", "2026/10/10", "2026-13-10",
                            "2026-10-32", "2026-00-10", "abcd-10-10", "2026-10-10 "}) {
        CHECK_MSG(!parseBuildDate(bad, ymd), bad);
    }
    CHECK(!parseBuildDate(nullptr, ymd));
    CHECK(ymd == 7);                                    // that bai thi khong ghi de
}

// ═══════════════════════════════════════════════════════════════════════
//  Mất mạng
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ License: mat mang tin ket qua cu toi da graceDays ngay") {
    const Claims c = claims(State::Licensed);
    const int64_t last = kNow;
    CHECK(evaluateCached(c, last, last + 29 * kDay, kBuild, 30).licensed());
    CHECK(evaluateCached(c, last, last + 30 * kDay, kBuild, 30).licensed());
    CHECK(!evaluateCached(c, last, last + 30 * kDay + 1, kBuild, 30).licensed());
}

TEST_CASE("★ License: mat mang KHONG bao gio vuot qua expiresAt") {
    const Claims c = claims(State::Licensed, kNow + 3 * kDay);
    CHECK(evaluateCached(c, kNow, kNow + 2 * kDay, kBuild, 30).licensed());
    CHECK(!evaluateCached(c, kNow, kNow + 4 * kDay, kBuild, 30).licensed());   // con ca grace
}

TEST_CASE("License: dong ho may bi lui qua mot ngay thi khong tin") {
    const Claims c = claims(State::Licensed);
    CHECK(evaluateCached(c, kNow, kNow - 3600, kBuild, 30).licensed());        // lui it: sai so binh thuong
    CHECK(!evaluateCached(c, kNow, kNow - 2 * kDay, kBuild, 30).licensed());
}

TEST_CASE("License: graceDays am coi nhu 0") {
    const Claims c = claims(State::Licensed);
    CHECK(evaluateCached(c, kNow, kNow, kBuild, -5).licensed());
    CHECK(!evaluateCached(c, kNow, kNow + 1, kBuild, -5).licensed());
}

// ═══════════════════════════════════════════════════════════════════════
//  Tên trạng thái
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("License: ten trang thai round-trip, ten la thi ve None") {
    for (const State s : {State::None, State::Licensed, State::Trial, State::Expired,
                          State::OtherMachine, State::Revoked, State::Invalid}) {
        CHECK(stateFromName(stateName(s)) == s);
    }
    CHECK(stateFromName("khong_ton_tai") == State::None);
    CHECK(stateFromName(nullptr) == State::None);
    CHECK(stateFromName("LICENSED") == State::None);   // phan biet hoa/thuong: khong doan
}

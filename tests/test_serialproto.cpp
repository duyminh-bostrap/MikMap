#include "TestHarness.h"

#include "io/proto/SerialProtocol.h"

#include <string>
#include <vector>

using namespace hexmap;

namespace {

void feedStr(SerialProtocol& p, const std::string& s,
             std::vector<SerialCommand>& out) {
    p.feed(reinterpret_cast<const uint8_t*>(s.data()), s.size(), out);
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  G3 — phương ngữ cơ bản
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("G3: doc duoc ba lenh T / U / C") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    feedStr(p, "T 3 812 455\nU 3\nC\n", out);

    REQUIRE(out.size() == 3u);

    CHECK(out[0].kind == SerialCommand::Kind::Touch);
    CHECK(out[0].id == 3);
    CHECK_NEAR(out[0].x, 812.0, 1e-9);
    CHECK_NEAR(out[0].y, 455.0, 1e-9);

    CHECK(out[1].kind == SerialCommand::Kind::Up);
    CHECK(out[1].id == 3);

    CHECK(out[2].kind == SerialCommand::Kind::Clear);
    CHECK(p.badLines() == 0u);
}

TEST_CASE("G3: nhan ca CRLF lan LF, va chu thuong") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    // Arduino IDE mac dinh gui CRLF; firmware viet tay thuong chi gui LF.
    feedStr(p, "T 1 10 20\r\nt 2 30 40\n", out);
    REQUIRE(out.size() == 2u);
    CHECK(out[0].id == 1);
    CHECK(out[1].id == 2);
}

TEST_CASE("G3: khoang trang thua khong lam hong dong") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    feedStr(p, "  T   7    100   200  \n", out);
    REQUIRE(out.size() == 1u);
    CHECK(out[0].id == 7);
    CHECK_NEAR(out[0].y, 200.0, 1e-9);
}

TEST_CASE("G3: toa do am doc duoc (sensor co the co goc o giua)") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    feedStr(p, "T 1 -500 -250\n", out);
    REQUIRE(out.size() == 1u);
    CHECK_NEAR(out[0].x, -500.0, 1e-9);
    CHECK_NEAR(out[0].y, -250.0, 1e-9);
}

// ═══════════════════════════════════════════════════════════════════════
//  G3 — ★ cái bẫy thật của serial: dòng bị cắt ngang
// ═══════════════════════════════════════════════════════════════════════

// ★ Serial KHONG giao hang theo dong. Mot lan read() tra ve dung nhung
//   byte vua toi — co the la nua dong, co the la hai dong ruoi.
//
//   Ai gia dinh "moi lan doc la mot dong" se co phan mem chay hoan hao
//   tren ban (du lieu thua, moi dong toi tron ven) roi hong ngay khi cam
//   thiet bi that gui nhanh — va hong theo kieu mat rai rac vai diem, rat
//   kho lan ra.
TEST_CASE("★ G3: dong bi cat lam doi -> van ghep lai dung") {
    SerialProtocol p;
    std::vector<SerialCommand> out;

    feedStr(p, "T 5 12", out);
    CHECK(out.empty());              // chua co gi hoan chinh

    feedStr(p, "3 456\n", out);
    REQUIRE(out.size() == 1u);
    CHECK(out[0].id == 5);
    CHECK_NEAR(out[0].x, 123.0, 1e-9);
    CHECK_NEAR(out[0].y, 456.0, 1e-9);
}

TEST_CASE("★ G3: cat TUNG BYTE MOT van cho ket qua y het") {
    const std::string stream = "T 1 100 200\nU 1\nT 2 300 400\n";

    // Nap ca cuc.
    std::vector<SerialCommand> whole;
    {
        SerialProtocol p;
        feedStr(p, stream, whole);
    }

    // Nap tung byte.
    std::vector<SerialCommand> drip;
    {
        SerialProtocol p;
        for (const char c : stream) {
            p.feed(reinterpret_cast<const uint8_t*>(&c), 1, drip);
        }
    }

    REQUIRE(whole.size() == drip.size());
    REQUIRE(whole.size() == 3u);
    for (size_t i = 0; i < whole.size(); ++i) {
        CHECK(whole[i].kind == drip[i].kind);
        CHECK(whole[i].id   == drip[i].id);
        CHECK_NEAR(whole[i].x, drip[i].x, 1e-9);
    }
}

TEST_CASE("G3: nhieu dong trong MOT lo deu duoc doc") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    feedStr(p, "T 1 1 1\nT 2 2 2\nT 3 3 3\nT 4 4 4\n", out);
    CHECK(out.size() == 4u);
}

TEST_CASE("G3: reset() vut phan dong do") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    feedStr(p, "T 9 111", out);
    p.reset();
    feedStr(p, " 222\n", out);
    // Phan dau da bi vut nen " 222" khong tao thanh lenh nao hop le.
    CHECK(out.empty());
}

// ═══════════════════════════════════════════════════════════════════════
//  G3 — ★ firmware gửi rác không được thành điểm chạm ma
// ═══════════════════════════════════════════════════════════════════════

// ★ std::atoi tra 0 cho chuoi rac, nen "T abc def" se thanh mot diem cham
//   o (0,0). Goc tren-trai la toa do HOP LE, nen rac tu firmware se hien
//   ra nhu nhung cu cham that o goc man hinh — va se kich hoat trigger
//   zone o do.
TEST_CASE("★ G3: 'T abc def' KHONG duoc thanh diem cham o goc (0,0)") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    feedStr(p, "T abc def ghi\n", out);
    CHECK(out.empty());
    CHECK(p.badLines() == 1u);
}

TEST_CASE("★ G3: thieu mot truong thi BO CA DONG") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    feedStr(p, "T 5 100\n", out);        // thieu y
    CHECK(out.empty());                   // khong duoc bia y = 0
    CHECK(p.badLines() == 1u);
}

TEST_CASE("G3: log go loi cua firmware bi bo qua, co dem lai") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    feedStr(p, "boot ok\nsensor ready\nT 1 50 60\n", out);
    REQUIRE(out.size() == 1u);
    CHECK(out[0].id == 1);
    CHECK(p.badLines() == 2u);
}

TEST_CASE("G3: dong rong bi bo qua lang le, khong tinh la rac") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    feedStr(p, "\n\n\nT 1 5 6\n\n", out);
    CHECK(out.size() == 1u);
    CHECK(p.badLines() == 0u);
}

// ★ Thiet bi hong gui rac khong co ky tu xuong dong se lam bo dem phinh
//   VO HAN cho toi khi het RAM. Thiet bi hong thi phai lam MAT DU LIEU,
//   khong duoc lam sap phan mem dieu khien.
TEST_CASE("★ G3: dong dai vo han khong lam no bo nho") {
    SerialProtocol p;
    std::vector<SerialCommand> out;

    const std::string flood(10000, 'x');
    feedStr(p, flood, out);
    CHECK(out.empty());
    CHECK(p.badLines() >= 1u);

    // ★ Va phai DONG BO LAI duoc sau do: dong ke tiep van doc binh thuong.
    feedStr(p, "\nT 1 11 22\n", out);
    REQUIRE(out.size() == 1u);
    CHECK(out[0].id == 1);
}

TEST_CASE("G3: 'U' thieu id bi bo") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    feedStr(p, "U\n", out);
    CHECK(out.empty());
    CHECK(p.badLines() == 1u);
}

TEST_CASE("G3: 'C' khong can tham so") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    feedStr(p, "C\nc\n", out);
    REQUIRE(out.size() == 2u);
    CHECK(out[0].kind == SerialCommand::Kind::Clear);
    CHECK(p.badLines() == 0u);
}

TEST_CASE("G3: nap con tro null khong lam sap") {
    SerialProtocol p;
    std::vector<SerialCommand> out;
    p.feed(nullptr, 10, out);
    CHECK(out.empty());
}

#include "TestHarness.h"

#include "io/SensorLog.h"
#include "io/sources/OscSource.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

using namespace hexmap;

namespace {

std::vector<uint8_t> bytes(std::initializer_list<int> v) {
    std::vector<uint8_t> b;
    for (const int x : v) b.push_back(static_cast<uint8_t>(x));
    return b;
}

void removeFile(const std::string& p) { std::remove(p.c_str()); }

void writeRaw(const std::string& path, const std::vector<uint8_t>& b) {
    std::ofstream f(path, std::ios::binary | std::ios::trunc);
    f.write(reinterpret_cast<const char*>(b.data()),
            static_cast<std::streamsize>(b.size()));
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  G16 — ghi rồi đọc lại
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("G16: ghi roi doc lai ra dung goi, dung thu tu") {
    const std::string path = "hexmap_test_log.hexslog";

    {
        SensorLogWriter w;
        REQUIRE(w.open(path));
        w.write(1'000'000'000LL, bytes({1, 2, 3}).data(), 3);
        w.write(1'000'500'000LL, bytes({9, 9}).data(), 2);
        w.write(1'002'000'000LL, bytes({7}).data(), 1);
        CHECK(w.packetCount() == 3u);
    }

    std::vector<LoggedPacket> log;
    std::string warn;
    REQUIRE(readSensorLog(path, log, warn));
    CHECK(warn.empty());
    REQUIRE(log.size() == 3u);

    // ★ Moc thoi gian la DO LECH so voi goi dau, khong phai gio tuyet doi:
    //   file phat lai duoc o bat ky thoi diem nao.
    CHECK(log[0].tOffsetNs == 0);
    CHECK(log[1].tOffsetNs == 500'000);
    CHECK(log[2].tOffsetNs == 2'000'000);   // 1.002s - 1.000s

    REQUIRE(log[0].data.size() == 3u);
    CHECK(log[0].data[0] == 1);
    CHECK(log[2].data[0] == 7);

    removeFile(path);
}

TEST_CASE("G16: file rong (chi co header) doc ra 0 goi, khong loi") {
    const std::string path = "hexmap_test_log_empty.hexslog";
    { SensorLogWriter w; REQUIRE(w.open(path)); }

    std::vector<LoggedPacket> log;
    std::string warn;
    REQUIRE(readSensorLog(path, log, warn));
    CHECK(log.empty());
    CHECK(warn.empty());
    removeFile(path);
}

TEST_CASE("G16: file khong ton tai / khong phai log -> tu choi") {
    std::vector<LoggedPacket> log;
    std::string warn;
    CHECK(!readSensorLog("khong_ton_tai_98765.hexslog", log, warn));

    const std::string path = "hexmap_test_notlog.hexslog";
    writeRaw(path, bytes({'x','x','x','x','x','x','x','x', 1,0,0,0}));
    CHECK(!readSensorLog(path, log, warn));
    removeFile(path);
}

// ★ Phien ghi do vi MAT DIEN van la bang chung dung duoc — va do chinh la
//   loai phien hay can xem lai nhat. Doc duoc toi dau lay toi do, kem
//   canh bao, thay vi vut ca file.
TEST_CASE("★ G16: file cut giua chung van doc duoc phan lanh") {
    const std::string path = "hexmap_test_log_trunc.hexslog";
    {
        SensorLogWriter w;
        REQUIRE(w.open(path));
        w.write(0, bytes({1, 2, 3, 4}).data(), 4);
        w.write(1000, bytes({5, 6, 7, 8}).data(), 4);
    }

    // Cat bot 6 byte cuoi -> ban ghi thu hai do dang.
    std::vector<uint8_t> all;
    {
        std::ifstream f(path, std::ios::binary);
        all.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    }
    REQUIRE(all.size() > 6u);
    all.resize(all.size() - 6);
    writeRaw(path, all);

    std::vector<LoggedPacket> log;
    std::string warn;
    REQUIRE(readSensorLog(path, log, warn));     // van la true
    CHECK(!warn.empty());                         // nhung phai noi ro
    REQUIRE(log.size() == 1u);                    // giu duoc goi lanh
    CHECK(log[0].data.size() == 4u);

    removeFile(path);
}

// ★ File hong khai do dai 4 ti byte se lam resize() co cap phat 4 GB —
//   chuong trinh chet vi het bo nho khi nguoi dung chi dinh MO mot file.
//   Doc file la cho du lieu KHONG dang tin, ke ca khi chinh ta ghi ra.
TEST_CASE("★ G16: do dai goi vo ly khong duoc lam no bo nho") {
    const std::string path = "hexmap_test_log_huge.hexslog";

    std::vector<uint8_t> b;
    const char magic[8] = {'H','E','X','S','L','O','G','1'};
    for (const char c : magic) b.push_back(static_cast<uint8_t>(c));
    b.insert(b.end(), {1, 0, 0, 0});                 // version
    for (int i = 0; i < 8; ++i) b.push_back(0);      // tOffset = 0
    b.insert(b.end(), {0xFF, 0xFF, 0xFF, 0xFF});     // do dai = 4 ti
    writeRaw(path, b);

    std::vector<LoggedPacket> log;
    std::string warn;
    REQUIRE(readSensorLog(path, log, warn));
    CHECK(log.empty());
    CHECK(!warn.empty());
    removeFile(path);
}

TEST_CASE("G16: goi qua to hoac rong thi khong duoc ghi") {
    const std::string path = "hexmap_test_log_size.hexslog";
    std::vector<uint8_t> big(70000, 0xAB);

    {
        SensorLogWriter w;
        REQUIRE(w.open(path));
        w.write(0, big.data(), big.size());          // > 64 KB
        w.write(0, big.data(), 0);                   // rong
        w.write(0, big.data(), 10);                  // hop le
        CHECK(w.packetCount() == 1u);
    }

    std::vector<LoggedPacket> log;
    std::string warn;
    REQUIRE(readSensorLog(path, log, warn));
    REQUIRE(log.size() == 1u);
    CHECK(log[0].data.size() == 10u);
    removeFile(path);
}

// ═══════════════════════════════════════════════════════════════════════
//  G16 — phát lại đúng thời điểm
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("G16: chi bom goi DA TOI HAN") {
    std::vector<LoggedPacket> log(3);
    log[0].tOffsetNs = 0;          log[0].data = bytes({1});
    log[1].tOffsetNs = 1'000'000;  log[1].data = bytes({2});
    log[2].tOffsetNs = 5'000'000;  log[2].data = bytes({3});

    size_t cursor = 0;
    std::vector<const LoggedPacket*> due;

    CHECK(collectDuePackets(log, 0, cursor, due) == 1u);
    CHECK(collectDuePackets(log, 999'999, cursor, due) == 0u);
    CHECK(collectDuePackets(log, 1'000'000, cursor, due) == 1u);
    CHECK(collectDuePackets(log, 4'000'000, cursor, due) == 0u);
    CHECK(collectDuePackets(log, 9'000'000, cursor, due) == 1u);
    CHECK(cursor == 3u);
    CHECK(due.size() == 3u);
}

// ★ Mot lan goi phai bom duoc NHIEU goi.
//
//   Sensor 40 Hz ma render 60 fps thi thuong mot goi moi frame — nhung
//   chi can mot lan khung (nap media, doi cua so) la da don hang chuc
//   goi. Bom moi lan mot goi thi ban phat lai TUT HAU dan va khong bao
//   gio duoi kip, tuc la mat dung tinh chat quan trong nhat cua replay.
TEST_CASE("★ G16: mot lan goi bom duoc NHIEU goi bi don lai") {
    std::vector<LoggedPacket> log(50);
    for (size_t i = 0; i < log.size(); ++i) {
        log[i].tOffsetNs = static_cast<int64_t>(i) * 25'000'000LL;   // 40 Hz
        log[i].data = bytes({static_cast<int>(i & 0xFF)});
    }

    size_t cursor = 0;
    std::vector<const LoggedPacket*> due;

    // Khung mot giay -> 41 goi da toi han cung luc.
    CHECK(collectDuePackets(log, 1'000'000'000LL, cursor, due) == 41u);
    CHECK(cursor == 41u);
}

TEST_CASE("G16: log rong thi khong bom gi, khong sap") {
    const std::vector<LoggedPacket> log;
    size_t cursor = 0;
    std::vector<const LoggedPacket*> due;
    CHECK(collectDuePackets(log, 1'000'000'000LL, cursor, due) == 0u);
    CHECK(cursor == 0u);
}

// ═══════════════════════════════════════════════════════════════════════
//  G16 — ★ chạy cả vòng: ghi → đọc → phát lại vào OscSource
// ═══════════════════════════════════════════════════════════════════════

namespace {

void pushStr(std::vector<uint8_t>& b, const std::string& s) {
    for (const char c : s) b.push_back(static_cast<uint8_t>(c));
    b.push_back(0);
    while (b.size() % 4 != 0) b.push_back(0);
}
void pushI32(std::vector<uint8_t>& b, int32_t v) {
    const auto u = static_cast<uint32_t>(v);
    b.push_back(static_cast<uint8_t>((u >> 24) & 0xFF));
    b.push_back(static_cast<uint8_t>((u >> 16) & 0xFF));
    b.push_back(static_cast<uint8_t>((u >>  8) & 0xFF));
    b.push_back(static_cast<uint8_t>( u        & 0xFF));
}
void pushF32(std::vector<uint8_t>& b, float v) {
    uint32_t u = 0;
    std::memcpy(&u, &v, 4);
    pushI32(b, static_cast<int32_t>(u));
}

std::vector<uint8_t> touchPacket(int id, float x, float y) {
    std::vector<uint8_t> b;
    pushStr(b, "/hexmap/touch");
    pushStr(b, ",iff");
    pushI32(b, id);
    pushF32(b, x);
    pushF32(b, y);
    return b;
}

} // namespace

// ★ Day moi la thu chung minh G16 dung duoc that: ghi mot phien, doc lai,
//   bom vao dung cai nguon that dung luc dien — va ra dung diem cham.
//   `OscSource::feedPacket` ton tai san cho viec nay tu dau.
TEST_CASE("★ G16: ca vong ghi -> doc -> phat lai vao OscSource") {
    const std::string path = "hexmap_test_log_e2e.hexslog";

    const auto p1 = touchPacket(1, 400.0f, 300.0f);
    const auto p2 = touchPacket(1, 410.0f, 305.0f);

    {
        SensorLogWriter w;
        REQUIRE(w.open(path));
        w.write(0,            p1.data(), p1.size());
        w.write(25'000'000LL, p2.data(), p2.size());
    }

    std::vector<LoggedPacket> log;
    std::string warn;
    REQUIRE(readSensorLog(path, log, warn));
    REQUIRE(log.size() == 2u);

    OscSource src;
    size_t cursor = 0;
    std::vector<const LoggedPacket*> due;

    // Moc t = 0: chi goi dau toi han.
    collectDuePackets(log, 0, cursor, due);
    REQUIRE(due.size() == 1u);
    src.feedPacket(due[0]->data.data(), due[0]->data.size());

    REQUIRE(src.frames().consume());
    CHECK_NEAR(src.frames().readSlot().points[0].x, 400.0, 1e-3);

    // Moc t = 30 ms: goi thu hai toi han.
    due.clear();
    collectDuePackets(log, 30'000'000LL, cursor, due);
    REQUIRE(due.size() == 1u);
    src.feedPacket(due[0]->data.data(), due[0]->data.size());

    REQUIRE(src.frames().consume());
    CHECK_NEAR(src.frames().readSlot().points[0].x, 410.0, 1e-3);

    removeFile(path);
}

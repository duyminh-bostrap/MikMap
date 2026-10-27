#include "TestHarness.h"

#include "io/proto/Scip2.h"

#include <cstdint>
#include <string>
#include <vector>

using namespace mikmap::scip2;

namespace {

// ── Bộ mã hoá phía thiết bị, chỉ đủ để dựng block test ──────────────────
// SCIP 2.0: mỗi ký tự mang 6 bit = (c - 0x30) & 0x3F, ghép big-endian.
std::string encN(uint32_t v, int n) {
    std::string s;
    for (int i = n - 1; i >= 0; --i)
        s.push_back(static_cast<char>(((v >> (6 * i)) & 0x3Fu) + 0x30u));
    return s;
}

std::string enc3(uint32_t v) { return encN(v, 3); }
std::string enc4(uint32_t v) { return encN(v, 4); }

// Ký tự cuối dòng = (tổng các byte trước nó & 0x3F) + 0x30.
std::string withSum(const std::string& s) {
    unsigned sum = 0;
    for (char c : s) sum += static_cast<unsigned char>(c);
    return s + static_cast<char>((sum & 0x3Fu) + 0x30u);
}

// Block MD hoàn chỉnh: status 99, timestamp, các dòng dữ liệu ≤64 ký tự, dòng rỗng.
std::string dataBlock(const std::vector<uint32_t>& vals, uint32_t ts = 0x1234) {
    std::string data;
    for (uint32_t v : vals) data += enc3(v);

    std::string block = withSum("99") + "\n";
    block += withSum(enc4(ts)) + "\n";
    for (size_t i = 0; i < data.size(); i += 64) block += withSum(data.substr(i, 64)) + "\n";
    block += "\n";   // dòng rỗng kết thúc block
    return block;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  Giải mã ký tự + checksum dòng
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("SCIP: decode 3 and 2 characters") {
    REQUIRE(decode("1Dh", 3) == 5432u);
    REQUIRE(decode("0m", 2) == 61u);
}

TEST_CASE("SCIP: line checksum") {
    REQUIRE(lineChecksumOk("00P"));
    REQUIRE(lineChecksumOk("99b"));
    REQUIRE(!lineChecksumOk("00Q"));
}

TEST_CASE("SCIP: MD command") {
    REQUIRE(mdCommand(0, 1080) == "MD0000108001000\n");
}

TEST_CASE("SCIP: PP parameters") {
    std::string pp;
    for (const char* line : {"MODL:UST-10LX;", "DMIN:20;", "DMAX:30000;", "ARES:1440;",
                             "AMIN:0;", "AMAX:1080;", "AFRT:540;", "SCAN:2400;"})
        pp += withSum(line) + "\n";

    DeviceParams p;
    REQUIRE(parsePP(pp, p));
    CHECK(p.dmin == 20);
    CHECK(p.dmax == 30000);
    CHECK(p.ares == 1440);
    CHECK(p.amin == 0);
    CHECK(p.amax == 1080);
    CHECK(p.afrt == 540);
}

// ═══════════════════════════════════════════════════════════════════════
//  Luồng MD: block, ack, echo, TCP cắt giữa nhóm 3 ký tự
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("SCIP: nhom 3 ky tu vat qua 2 dong") {
    std::vector<uint32_t> vals;
    for (int i = 0; i < 30; ++i) vals.push_back(1000u + static_cast<uint32_t>(i));

    const std::string block = dataBlock(vals);   // 90 ký tự dữ liệu -> dòng 64 + dòng 26
    MdStream s;
    s.feed(block.data(), block.size());

    uint32_t ranges[64]{};
    int count = -1;
    uint32_t ts = 0;
    REQUIRE(s.next(ranges, 64, count, ts) == MdResult::Scan);
    REQUIRE(count == 30);
    REQUIRE(ts == 0x1234u);
    REQUIRE(ranges[21] == 1021u);   // nhóm cắt qua ranh giới dòng 64 ký tự
    for (int i = 0; i < 30; ++i) CHECK_MSG(ranges[i] == 1000u + static_cast<uint32_t>(i), std::to_string(i));
}

TEST_CASE("SCIP: nap tung byte cho cung ket qua") {
    std::vector<uint32_t> vals;
    for (int i = 0; i < 30; ++i) vals.push_back(1000u + static_cast<uint32_t>(i));

    // Block ack `00P` đứng trước: không có dữ liệu, phải bị bỏ qua.
    const std::string stream = withSum("00") + "\n\n" + dataBlock(vals);

    MdStream s;
    uint32_t ranges[64]{};
    int count = 0;
    uint32_t ts = 0;
    int scans = 0;

    for (char c : stream) {
        s.feed(&c, 1);
        if (s.next(ranges, 64, count, ts) == MdResult::Scan) ++scans;
    }

    REQUIRE(scans == 1);
    REQUIRE(count == 30);
    REQUIRE(ts == 0x1234u);
    for (int i = 0; i < 30; ++i) CHECK_MSG(ranges[i] == 1000u + static_cast<uint32_t>(i), std::to_string(i));
}

TEST_CASE("SCIP: echo MD truoc status bi bo qua") {
    const std::string block = "MD0000108001000\n" + dataBlock({7, 8, 9});
    MdStream s;
    s.feed(block.data(), block.size());

    uint32_t ranges[8]{};
    int count = 0;
    uint32_t ts = 0;
    REQUIRE(s.next(ranges, 8, count, ts) == MdResult::Scan);
    REQUIRE(count == 3);
    CHECK(ranges[0] == 7u);
    CHECK(ranges[1] == 8u);
    CHECK(ranges[2] == 9u);
}

TEST_CASE("SCIP: sai checksum dong du lieu -> Error, block sau van doc duoc") {
    std::string bad = withSum("99") + "\n";
    bad += withSum(enc4(1)) + "\n";
    bad += "1DhZ\n";   // dòng dữ liệu có checksum sai
    bad += "\n";

    MdStream s;
    s.feed(bad.data(), bad.size());

    uint32_t ranges[8]{};
    int count = 0;
    uint32_t ts = 0;
    REQUIRE(s.next(ranges, 8, count, ts) == MdResult::Error);
    REQUIRE(s.error() == "checksum");

    const std::string good = dataBlock({42});
    s.feed(good.data(), good.size());
    REQUIRE(s.next(ranges, 8, count, ts) == MdResult::Scan);
    REQUIRE(count == 1);
    REQUIRE(ranges[0] == 42u);
}

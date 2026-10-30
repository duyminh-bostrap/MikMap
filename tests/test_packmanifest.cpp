#include "TestHarness.h"

#include "io/packs/PackManifest.h"

#include <string>

using namespace mikmap;
using namespace mikmap::packs;

namespace {

const std::string H(64, 'a');

// Một gói hợp lệ dạng JSON; `extra` chèn thêm vào đầu object (vd "requires").
std::string packJson(const std::string& id, const std::string& ver, const std::string& kind,
                     const std::string& requires_ = "[]", const std::string& filePath = "a.dll",
                     const std::string& sizeBytes = "10") {
    return "{\"id\":\"" + id + "\",\"name\":\"N " + id + "\",\"version\":\"" + ver + "\",\"kind\":\"" + kind +
           "\",\"sizeBytes\":" + sizeBytes + ",\"files\":[{\"path\":\"" + filePath + "\",\"sha256\":\"" + H +
           "\",\"size\":10,\"url\":\"https://example.com/" + id + "\"}],\"requires\":" + requires_ +
           ",\"provides\":[\"sensor.x\"]}";
}

bool parseOne(const std::string& json, std::string* errOut = nullptr) {
    JsonValue v; std::string e;
    if (!JsonValue::parse(json, v, e)) return false;
    PackInfo p; std::string err;
    const bool ok = parsePackInfo(v, p, err);
    if (errOut) *errOut = err;
    return ok;
}

// Thay đoạn `from` đầu tiên trong packJson mặc định.
std::string mutated(const std::string& from, const std::string& to) {
    std::string s = packJson("p", "1.0.0", "plugin");
    const size_t i = s.find(from);
    if (i == std::string::npos) return "{}";
    return s.replace(i, from.size(), to);
}

std::string catalogOf(const std::vector<std::string>& packs) {
    std::string s = "{\"packs\":[";
    for (size_t i = 0; i < packs.size(); ++i) s += (i ? "," : "") + packs[i];
    return s + "]}";
}

Catalog mk(const std::vector<std::string>& packs) {
    Catalog c; std::string e;
    parseCatalog(catalogOf(packs), c, e);
    return c;
}

} // namespace

TEST_CASE("packs: isSafeRelPath accepts normal relative paths") {
    CHECK(isSafeRelPath("a.dll"));
    CHECK(isSafeRelPath("models/pose.onnx"));
    CHECK(isSafeRelPath("a/b/c-d_e.f.bin"));
}

TEST_CASE("packs: isSafeRelPath rejects every hostile class") {
    const std::vector<std::string> bad = {
        "", ".", "..", "../x", "a/../b", "a/./b", "a/..", "./a",
        "/abs", "\\abs", "\\\\server\\x", "C:\\x", "C:/x", "C:x", "a:stream", "a.dll:ads",
        "a//b", "a/", "a\\b", "a/b\\c",
        std::string("a\0b", 3), std::string("a\x01" "b"), "a\nb", "a\x7F" "b",
        "evil.dll.", "evil.dll ", "dir./x", "dir /x",
        "CON", "con", "NUL.txt", "a/Prn.dll", "aux", "COM1", "com9.dll", "LPT1", "lpt9.x.y", "a/CoN/b",
        "a<b", "a>b", "a\"b", "a|b", "a?b", "a*b",
        "caf\xC3\xA9.dll",
        std::string(241, 'a'),
        std::string(101, 'a'),
        "ok/" + std::string(101, 'b'),
    };
    for (const std::string& p : bad) CHECK_MSG(!isSafeRelPath(p), "should reject: " + p);
    CHECK(isSafeRelPath(std::string(100, 'a')));     // đúng ngưỡng thành phần
    CHECK(isSafeRelPath("com0.dll"));                // COM0 / LPT0 không thuộc danh sách
    CHECK(isSafeRelPath("console.dll"));             // chỉ tên đúng "con", không phải tiền tố
}

TEST_CASE("packs: compareVersions is numeric per component") {
    CHECK(compareVersions("1.10.0", "1.9.0") == 1);
    CHECK(compareVersions("1.0.0", "1.0.1") == -1);
    CHECK(compareVersions("2.0.0", "2.0.0") == 0);
    CHECK(compareVersions("10.0.0", "9.99.99") == 1);
}

TEST_CASE("packs: parsePackInfo accepts a valid pack, url optional") {
    PackInfo p; std::string err; JsonValue v;
    REQUIRE(JsonValue::parse(packJson("pose-onnx", "1.2.3", "model"), v, err));
    REQUIRE(parsePackInfo(v, p, err));
    CHECK(p.id == "pose-onnx");
    CHECK(p.kind == Kind::Model);
    CHECK(p.sizeBytes == 10);
    CHECK(p.files.size() == 1 && p.files[0].url == "https://example.com/pose-onnx");
    CHECK(p.provides.size() == 1);
    // Không có url (kiểu .mikpack)
    const std::string noUrl = "{\"id\":\"p\",\"name\":\"n\",\"version\":\"1.0.0\",\"kind\":\"runtime\",\"sizeBytes\":1,"
                              "\"files\":[{\"path\":\"a.dll\",\"sha256\":\"" + H + "\",\"size\":1}]}";
    CHECK(parseOne(noUrl));
    // loopback cho test
    CHECK(parseOne(mutated("https://example.com/p", "http://127.0.0.1:8080/p")));
    CHECK(parseOne(mutated("https://example.com/p", "http://localhost:8080/p")));
}

TEST_CASE("packs: parsePackInfo rejects bad urls") {
    for (const char* u : {"file:///c:/x", "ftp://h/x", "javascript:alert(1)", "http://example.com/x", "//h/x", ""})
        CHECK_MSG(!parseOne(mutated("https://example.com/p", u)), u);
}

TEST_CASE("packs: parsePackInfo rejects bad ids and versions") {
    for (const char* id : {"", ".", "..", "A", "a b", "a/b", "a\\b", "é"})
        CHECK_MSG(!parseOne(packJson(id, "1.0.0", "plugin")), id);
    CHECK(!parseOne(packJson(std::string(65, 'a'), "1.0.0", "plugin")));
    CHECK(parseOne(packJson(std::string(64, 'a'), "1.0.0", "plugin")));
    for (const char* ver : {"", "1", "1.0", "1.0.0.0", "01.0.0", "1.00.0", "+1.0.0", "-1.0.0", "1.0.-1",
                            "1.0.x", "1000000000.0.0", "1.0.0-beta", " 1.0.0", "1.0.0 "})
        CHECK_MSG(!parseOne(packJson("p", ver, "plugin")), ver);
    CHECK(parseOne(packJson("p", "999999999.0.0", "plugin")));
    CHECK(parseOne(packJson("p", "0.0.0", "plugin")));
    CHECK(!parseOne(packJson("p", "1.0.0", "driver")));
}

TEST_CASE("packs: parsePackInfo rejects hostile file paths") {
    CHECK(!parseOne(mutated("a.dll", "../evil.dll")));
    CHECK(!parseOne(mutated("a.dll", "evil.dll.")));
    CHECK(!parseOne(mutated("a.dll", "CON.dll")));
    CHECK(!parseOne(mutated("a.dll", "a:b")));
    CHECK(!parseOne(mutated("a.dll", "")));
}

TEST_CASE("packs: parsePackInfo hash rules") {
    CHECK(!parseOne(mutated(H, std::string(63, 'a'))));
    CHECK(!parseOne(mutated(H, std::string(65, 'a'))));
    CHECK(!parseOne(mutated(H, std::string(64, 'A'))));     // chỉ chữ thường
    CHECK(!parseOne(mutated(H, std::string(63, 'a') + "g")));
}

TEST_CASE("packs: parsePackInfo size rules") {
    CHECK(!parseOne(packJson("p", "1.0.0", "plugin", "[]", "a.dll", "11")));   // sizeBytes != tổng
    CHECK(!parseOne(packJson("p", "1.0.0", "plugin", "[]", "a.dll", "-10")));
    CHECK(!parseOne(packJson("p", "1.0.0", "plugin", "[]", "a.dll", "10.5")));
    CHECK(!parseOne(mutated("\"size\":10", "\"size\":-1")));
    CHECK(!parseOne(mutated("\"size\":10", "\"size\":2147483649")));   // > 2 GiB
    CHECK(!parseOne(mutated("\"size\":10", "\"size\":\"10\"")));
    // Đúng 2 GiB một tệp được; sizeBytes phải khớp.
    CHECK(parseOne(packJson("p", "1.0.0", "plugin", "[]", "a.dll", "10")));
    std::string big = mutated("\"size\":10", "\"size\":2147483648");
    big = big.replace(big.find("\"sizeBytes\":10"), 14, "\"sizeBytes\":2147483648");
    CHECK(parseOne(big));
}

TEST_CASE("packs: parsePackInfo duplicates and file/folder clash") {
    auto two = [&](const std::string& p1, const std::string& p2) {
        auto f = [&](const std::string& p) { return "{\"path\":\"" + p + "\",\"sha256\":\"" + H + "\",\"size\":5}"; };
        return "{\"id\":\"p\",\"name\":\"n\",\"version\":\"1.0.0\",\"kind\":\"plugin\",\"sizeBytes\":10,\"files\":[" +
               f(p1) + "," + f(p2) + "]}";
    };
    CHECK(parseOne(two("a.dll", "b.dll")));
    CHECK(!parseOne(two("a.dll", "a.dll")));
    CHECK(!parseOne(two("A.dll", "a.DLL")));          // không phân biệt hoa thường
    CHECK(!parseOne(two("a", "a/b.dll")));            // tệp trùng tên thư mục
    CHECK(!parseOne(two("a/b.dll", "A")));
}

TEST_CASE("packs: parsePackInfo file count limit") {
    auto many = [&](int n) {
        std::string s = "{\"id\":\"p\",\"name\":\"n\",\"version\":\"1.0.0\",\"kind\":\"plugin\",\"sizeBytes\":" +
                        std::to_string(n) + ",\"files\":[";
        for (int i = 0; i < n; ++i)
            s += std::string(i ? "," : "") + "{\"path\":\"f" + std::to_string(i) + ".bin\",\"sha256\":\"" + H + "\",\"size\":1}";
        return s + "]}";
    };
    CHECK(parseOne(many(64)));
    CHECK(!parseOne(many(65)));
    CHECK(!parseOne(many(0)));
}

TEST_CASE("packs: parsePackInfo requires entries") {
    CHECK(parseOne(packJson("p", "1.0.0", "plugin", "[{\"id\":\"rt\",\"min\":\"1.0.0\"}]")));
    CHECK(!parseOne(packJson("p", "1.0.0", "plugin", "[{\"id\":\"../x\",\"min\":\"1.0.0\"}]")));
    CHECK(!parseOne(packJson("p", "1.0.0", "plugin", "[{\"id\":\"rt\",\"min\":\"1.0\"}]")));
    CHECK(!parseOne(packJson("p", "1.0.0", "plugin", "[{\"id\":\"rt\"}]")));
    CHECK(!parseOne(packJson("p", "1.0.0", "plugin", "[{\"id\":\"p\",\"min\":\"1.0.0\"}]")));   // tự phụ thuộc
    CHECK(!parseOne(packJson("p", "1.0.0", "plugin", "[{\"id\":\"rt\",\"min\":\"1.0.0\"},{\"id\":\"rt\",\"min\":\"1.0.0\"}]")));
    CHECK(!parseOne(packJson("p", "1.0.0", "plugin", "{}")));
}

TEST_CASE("packs: parseCatalog parses 3 packs and rejects duplicate ids") {
    const std::string rt  = packJson("onnx-runtime", "1.18.0", "runtime");
    const std::string pose = packJson("pose", "1.0.0", "plugin", "[{\"id\":\"onnx-runtime\",\"min\":\"1.18.0\"}]");
    const std::string mdl = packJson("pose-model", "2.0.0", "model", "[{\"id\":\"pose\",\"min\":\"1.0.0\"}]");
    Catalog c; std::string err;
    REQUIRE(parseCatalog(catalogOf({rt, pose, mdl}), c, err));
    CHECK(c.packs.size() == 3);
    CHECK(findPack(c, "pose") != nullptr);
    CHECK(findPack(c, "nope") == nullptr);
    CHECK(!parseCatalog(catalogOf({rt, rt}), c, err));
    CHECK(err.find("duplicate") != std::string::npos);
    CHECK(!parseCatalog("not json", c, err));
    CHECK(!parseCatalog("{}", c, err));
    CHECK(!parseCatalog("[]", c, err));
    CHECK(!parseCatalog(catalogOf({packJson("p", "1.0.0", "plugin", "[]", "../evil.dll")}), c, err));
}

TEST_CASE("packs: parseCatalog size and count limits") {
    Catalog c; std::string err;
    std::vector<std::string> packs;
    for (int i = 0; i < 256; ++i) packs.push_back(packJson("p" + std::to_string(i), "1.0.0", "plugin"));
    CHECK(parseCatalog(catalogOf(packs), c, err));
    packs.push_back(packJson("p256", "1.0.0", "plugin"));
    CHECK(!parseCatalog(catalogOf(packs), c, err));
    CHECK(!parseCatalog(std::string(4u * 1024 * 1024 + 1, ' '), c, err));
}

TEST_CASE("packs: parseCatalog survives deeply nested JSON") {
    Catalog c; std::string err;
    const std::string deepArr = std::string(200000, '[') + std::string(200000, ']');
    CHECK(!parseCatalog(deepArr, c, err));
    CHECK(!err.empty());
    std::string deepObj = "{\"packs\":[],\"x\":";
    for (int i = 0; i < 100000; ++i) deepObj += "{\"a\":";
    CHECK(!parseCatalog(deepObj, c, err));
    // '[' trong chuỗi không tính là lồng nhau
    CHECK(parseCatalog("{\"packs\":[],\"note\":\"" + std::string(1000, '[') + "\"}", c, err));
}

TEST_CASE("packs: resolveInstallOrder puts dependencies first") {
    const Catalog c = mk({packJson("model", "1.0.0", "model", "[{\"id\":\"pose\",\"min\":\"1.0.0\"}]"),
                          packJson("pose", "1.0.0", "plugin", "[{\"id\":\"rt\",\"min\":\"1.18.0\"}]"),
                          packJson("rt", "1.18.0", "runtime")});
    REQUIRE(c.packs.size() == 3);
    std::vector<std::string> order; std::string err;
    REQUIRE(resolveInstallOrder(c, "model", order, err));
    REQUIRE(order.size() == 3);
    CHECK(order[0] == "rt");
    CHECK(order[1] == "pose");
    CHECK(order[2] == "model");
}

TEST_CASE("packs: resolveInstallOrder de-duplicates diamond dependencies") {
    const Catalog c = mk({packJson("top", "1.0.0", "plugin", "[{\"id\":\"l\",\"min\":\"1.0.0\"},{\"id\":\"r\",\"min\":\"1.0.0\"}]"),
                          packJson("l", "1.0.0", "plugin", "[{\"id\":\"base\",\"min\":\"1.0.0\"}]"),
                          packJson("r", "1.0.0", "plugin", "[{\"id\":\"base\",\"min\":\"1.0.0\"}]"),
                          packJson("base", "1.0.0", "runtime")});
    std::vector<std::string> order; std::string err;
    REQUIRE(resolveInstallOrder(c, "top", order, err));
    CHECK(order.size() == 4);
    CHECK(order.front() == "base");
    CHECK(order.back() == "top");
}

TEST_CASE("packs: resolveInstallOrder failures give clear messages") {
    std::vector<std::string> order; std::string err;
    {   // chu trình
        const Catalog c = mk({packJson("a", "1.0.0", "plugin", "[{\"id\":\"b\",\"min\":\"1.0.0\"}]"),
                              packJson("b", "1.0.0", "plugin", "[{\"id\":\"a\",\"min\":\"1.0.0\"}]")});
        REQUIRE(c.packs.size() == 2);
        CHECK(!resolveInstallOrder(c, "a", order, err));
        CHECK(err.find("cycle") != std::string::npos);
        CHECK(order.empty());
    }
    {   // thiếu phụ thuộc
        const Catalog c = mk({packJson("a", "1.0.0", "plugin", "[{\"id\":\"ghost\",\"min\":\"1.0.0\"}]")});
        CHECK(!resolveInstallOrder(c, "a", order, err));
        CHECK(err.find("ghost") != std::string::npos);
    }
    {   // phiên bản quá thấp
        const Catalog c = mk({packJson("a", "1.0.0", "plugin", "[{\"id\":\"rt\",\"min\":\"1.19.0\"}]"),
                              packJson("rt", "1.18.0", "runtime")});
        CHECK(!resolveInstallOrder(c, "a", order, err));
        CHECK(err.find("1.19.0") != std::string::npos);
    }
    {   // id không tồn tại
        const Catalog c = mk({packJson("a", "1.0.0", "plugin")});
        CHECK(!resolveInstallOrder(c, "zzz", order, err));
        CHECK(err.find("zzz") != std::string::npos);
    }
}

TEST_CASE("packs: resolveInstallOrder survives a 1000-deep chain") {
    std::vector<std::string> packs;
    for (int i = 0; i < 200; ++i) {   // catalog tối đa 256 gói: dùng 200 gói nối đuôi, giới hạn chuỗi là 64
        const std::string need = i + 1 < 200 ? "[{\"id\":\"p" + std::to_string(i + 1) + "\",\"min\":\"1.0.0\"}]" : "[]";
        packs.push_back(packJson("p" + std::to_string(i), "1.0.0", "plugin", need));
    }
    const Catalog c = mk(packs);
    REQUIRE(c.packs.size() == 200);
    std::vector<std::string> order; std::string err;
    CHECK(!resolveInstallOrder(c, "p0", order, err));
    CHECK(err.find("too deep") != std::string::npos);
    // Cùng bộ gói, bắt đầu gần cuối thì vẫn cài được.
    CHECK(resolveInstallOrder(c, "p150", order, err));
    CHECK(order.size() == 50);
    // Catalog dựng tay 1000 gói (bỏ qua giới hạn parse) cũng không tràn stack.
    Catalog big;
    for (int i = 0; i < 1000; ++i) {
        PackInfo p; p.id = "q" + std::to_string(i); p.version = "1.0.0";
        if (i + 1 < 1000) p.needs.push_back({"q" + std::to_string(i + 1), "1.0.0"});
        big.packs.push_back(p);
    }
    CHECK(!resolveInstallOrder(big, "q0", order, err));
    CHECK(!err.empty());
}

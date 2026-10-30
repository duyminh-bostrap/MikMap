#include "TestHarness.h"

#include "core/util/Sha2.h"
#include "io/packs/PackStore.h"

#include <atomic>
#include <filesystem>
#include <fstream>
#include <string>

using namespace mikmap;
using namespace mikmap::packs;

namespace {

namespace fs = std::filesystem;

fs::path fromUtf8(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}

std::string toUtf8(const fs::path& p) {
    const std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

struct TempDir {
    fs::path path;
    explicit TempDir(const std::string& suffixUtf8) {
        static std::atomic<int> seq{0};
        const std::string sub = "mikmap_test_packstore_" + std::to_string(++seq) + "_" + suffixUtf8;
        path = fs::temp_directory_path() / fromUtf8(sub);
        std::error_code ec;
        fs::remove_all(path, ec);
        fs::create_directories(path, ec);
    }
    ~TempDir() {
        std::error_code ec;
        fs::remove_all(path, ec);
    }
    std::string utf8() const { return toUtf8(path); }
};

void writeBytes(const std::string& dirUtf8, const std::string& relPath, const std::string& content) {
    const fs::path p = fromUtf8(dirUtf8) / fromUtf8(relPath);
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(content.data(), static_cast<std::streamsize>(content.size()));
}

PackInfo makePack(const std::string& id, const std::string& ver, const std::string& relPath, const std::string& data) {
    const Sha256Digest d = sha256(data.data(), data.size());
    PackInfo p;
    p.id        = id;
    p.name      = "Pack " + id;
    p.version   = ver;
    p.kind      = Kind::Plugin;
    p.sizeBytes = data.size();
    FileEntry fe;
    fe.path      = relPath;
    fe.sha256Hex = toHex(d.data(), d.size());
    fe.size      = data.size();
    p.files.push_back(fe);
    p.provides.push_back("sensor.depth");
    return p;
}

} // namespace

TEST_CASE("packstore: commit persists across restart") {
    TempDir tmp("persist");
    PackStore store(tmp.utf8());
    std::string err;
    REQUIRE(store.startup(err));
    CHECK(store.list().empty());

    const std::string data = "binary-plugin-bytes-v1";
    const PackInfo p = makePack("orbbec-femto", "1.0.0", "sub/mikmap_sensor_orbbec.dll", data);
    const std::string stage = store.stagingDir(p.id, p.version);
    writeBytes(stage, "sub/mikmap_sensor_orbbec.dll", data);

    REQUIRE(store.commit(p, err));
    CHECK(store.isInstalled("orbbec-femto"));
    CHECK(store.isInstalled("orbbec-femto", "1.0.0"));
    CHECK(!store.isInstalled("orbbec-femto", "1.0.1"));
    REQUIRE(store.find("orbbec-femto") != nullptr);
    CHECK(fs::exists(fromUtf8(store.find("orbbec-femto")->dir) / "pack.json"));

    // Mở PackStore mới trên cùng thư mục gốc -> vẫn đọc lại được từ installed.json
    PackStore store2(tmp.utf8());
    REQUIRE(store2.startup(err));
    REQUIRE(store2.list().size() == 1);
    CHECK(store2.isInstalled("orbbec-femto", "1.0.0"));
    CHECK(store2.find("orbbec-femto")->entry.provides.size() == 1);
}

TEST_CASE("packstore: commit rejects bad sha256 and leaves no installed dir") {
    TempDir tmp("badhash");
    PackStore store(tmp.utf8());
    std::string err;
    REQUIRE(store.startup(err));

    const PackInfo p = makePack("orbbec-femto", "1.0.0", "plugin.dll", "expected-bytes");
    const std::string stage = store.stagingDir(p.id, p.version);
    writeBytes(stage, "plugin.dll", "tampered-bytes!");

    CHECK(!store.commit(p, err));
    CHECK(!err.empty());
    CHECK(!store.isInstalled("orbbec-femto"));
    CHECK(!fs::exists(tmp.path / "installed" / "orbbec-femto"));
}

TEST_CASE("packstore: remove immediate vs pendingRemoval on startup") {
    TempDir tmp("remove");
    PackStore store(tmp.utf8());
    std::string err;
    REQUIRE(store.startup(err));

    // 1) loadedInProcess = false -> xoá ngay lập tức
    const PackInfo p1 = makePack("pack-a", "1.0.0", "a.dll", "aaaa");
    writeBytes(store.stagingDir(p1.id, p1.version), "a.dll", "aaaa");
    REQUIRE(store.commit(p1, err));
    const std::string dirA = store.find("pack-a")->dir;
    REQUIRE(store.remove("pack-a", false, err));
    CHECK(!store.isInstalled("pack-a"));
    CHECK(store.find("pack-a") == nullptr);
    CHECK(!fs::exists(fromUtf8(dirA)));

    // 2) loadedInProcess = true -> giữ thư mục, đánh dấu pendingRemoval, xoá ở lần startup() kế tiếp
    const PackInfo p2 = makePack("pack-b", "1.0.0", "b.dll", "bbbb");
    writeBytes(store.stagingDir(p2.id, p2.version), "b.dll", "bbbb");
    REQUIRE(store.commit(p2, err));
    const std::string dirB = store.find("pack-b")->dir;
    REQUIRE(store.remove("pack-b", true, err));
    REQUIRE(store.find("pack-b") != nullptr);
    CHECK(store.find("pack-b")->pendingRemoval);
    CHECK(fs::exists(fromUtf8(dirB)));

    PackStore store2(tmp.utf8());
    REQUIRE(store2.startup(err));
    CHECK(store2.find("pack-b") == nullptr);
    CHECK(!store2.isInstalled("pack-b"));
    CHECK(!fs::exists(fromUtf8(dirB)));
}

TEST_CASE("packstore: corrupt installed.json is renamed to .bad and startup succeeds") {
    TempDir tmp("corrupt");
    writeBytes(tmp.utf8(), "installed.json", "{ broken json [[[");

    PackStore store(tmp.utf8());
    std::string err;
    REQUIRE(store.startup(err));
    CHECK(!err.empty());
    CHECK(store.list().empty());
    CHECK(fs::exists(tmp.path / "installed.json.bad"));
}

TEST_CASE("packstore: quickVerify checks size while deepVerify checks sha256") {
    TempDir tmp("verify");
    PackStore store(tmp.utf8());
    std::string err;
    REQUIRE(store.startup(err));

    const std::string data = "0123456789abcdef";
    const PackInfo p = makePack("pose-model", "1.0.0", "pose.onnx", data);
    writeBytes(store.stagingDir(p.id, p.version), "pose.onnx", data);
    REQUIRE(store.commit(p, err));
    CHECK(store.quickVerify("pose-model", err));
    CHECK(store.deepVerify("pose-model", err));

    // Lật 1 byte nhưng giữ nguyên kích thước -> quickVerify vẫn true, deepVerify false
    const std::string dir = store.find("pose-model")->dir;
    writeBytes(dir, "pose.onnx", "X123456789abcdef");
    CHECK(store.quickVerify("pose-model", err));
    CHECK(!store.deepVerify("pose-model", err));

    // Cắt ngắn file -> quickVerify false
    writeBytes(dir, "pose.onnx", "short");
    CHECK(!store.quickVerify("pose-model", err));
}

TEST_CASE("packstore: supports Vietnamese UTF-8 root directory") {
    TempDir tmp("Th\xE1\xBB\xAD" "Nghi\xE1\xBB\x87" "m_G\xC3\xB3" "i");
    PackStore store(tmp.utf8());
    std::string err;
    REQUIRE(store.startup(err));

    const std::string data = "xin-chao-sensor-pack";
    const PackInfo p = makePack("orbbec-femto", "1.2.0", "orbbec.dll", data);
    writeBytes(store.stagingDir(p.id, p.version), "orbbec.dll", data);
    REQUIRE(store.commit(p, err));
    CHECK(store.deepVerify("orbbec-femto", err));
    REQUIRE(store.remove("orbbec-femto", false, err));
    CHECK(!store.isInstalled("orbbec-femto"));
}

TEST_CASE("packstore: upgrading version replaces old entry and cleans old version dir") {
    TempDir tmp("upgrade");
    PackStore store(tmp.utf8());
    std::string err;
    REQUIRE(store.startup(err));

    const PackInfo v1 = makePack("orbbec-femto", "0.1.0", "orbbec.dll", "v1-data");
    writeBytes(store.stagingDir(v1.id, v1.version), "orbbec.dll", "v1-data");
    REQUIRE(store.commit(v1, err));
    const std::string dirV1 = store.find("orbbec-femto")->dir;
    CHECK(fs::exists(fromUtf8(dirV1)));

    const PackInfo v2 = makePack("orbbec-femto", "0.2.0", "orbbec.dll", "v2-data-newer");
    writeBytes(store.stagingDir(v2.id, v2.version), "orbbec.dll", "v2-data-newer");
    REQUIRE(store.commit(v2, err));

    REQUIRE(store.list().size() == 1);
    CHECK(store.find("orbbec-femto")->entry.version == "0.2.0");
    CHECK(!fs::exists(fromUtf8(dirV1)));
    CHECK(store.deepVerify("orbbec-femto", err));
}

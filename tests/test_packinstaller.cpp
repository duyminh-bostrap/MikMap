#include "TestHarness.h"

#include "FakeHttpServer.h"
#include "core/util/Ed25519.h"
#include "core/util/Sha2.h"
#include "io/packs/PackInstaller.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <string>
#include <thread>

using namespace mikmap;
using namespace mikmap::packs;
using namespace mikmap::test;

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
    explicit TempDir(const std::string& suffix) {
        static std::atomic<int> seq{0};
        path = fs::temp_directory_path() / fromUtf8("mikmap_test_installer_" + std::to_string(++seq) + "_" + suffix);
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

std::string hexSha256(const std::string& s) {
    const Sha256Digest d = sha256(s.data(), s.size());
    return toHex(d.data(), d.size());
}

PackProgress waitTerminal(PackInstaller& inst, int maxMs = 4000) {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(maxMs);
    while (std::chrono::steady_clock::now() < deadline) {
        const PackProgress p = inst.snapshot();
        if (p.state == PackJobState::Done ||
            p.state == PackJobState::Failed ||
            p.state == PackJobState::Cancelled) {
            return p;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return inst.snapshot();
}

} // namespace

TEST_CASE("packinstaller: verified catalog refresh and dependency-ordered install") {
    TempDir tmp("ok");
    PackStore store(tmp.utf8());
    std::string err;
    REQUIRE(store.startup(err));

    uint8_t seed[32]{};
    for (int i = 0; i < 32; ++i) seed[i] = static_cast<uint8_t>(i + 1);
    uint8_t pub[32]{};
    ed25519PublicFromSeed(seed, pub);

    FakeHttpServer srv;
    REQUIRE(srv.port() != 0);

    const std::string rtBytes   = "onnxruntime-binary-dll-bytes";
    const std::string poseBytes = "pose-plugin-binary-dll-bytes-1234";
    srv.addRoute("/rt.dll", rtBytes, 200);
    srv.addRoute("/pose.dll", poseBytes, 200);

    const std::string catJson =
        "{\"packs\":["
        "{\"id\":\"onnx-runtime\",\"name\":\"ONNX Runtime\",\"version\":\"1.18.0\",\"kind\":\"runtime\","
        "\"sizeBytes\":" + std::to_string(rtBytes.size()) + ","
        "\"files\":[{\"path\":\"onnxruntime.dll\",\"sha256\":\"" + hexSha256(rtBytes) + "\","
        "\"size\":" + std::to_string(rtBytes.size()) + ",\"url\":\"" + srv.url("/rt.dll") + "\"}]},"
        "{\"id\":\"pose-plugin\",\"name\":\"Pose Plugin\",\"version\":\"1.0.0\",\"kind\":\"plugin\","
        "\"sizeBytes\":" + std::to_string(poseBytes.size()) + ","
        "\"requires\":[{\"id\":\"onnx-runtime\",\"min\":\"1.18.0\"}],"
        "\"files\":[{\"path\":\"mikmap_sensor_pose.dll\",\"sha256\":\"" + hexSha256(poseBytes) + "\","
        "\"size\":" + std::to_string(poseBytes.size()) + ",\"url\":\"" + srv.url("/pose.dll") + "\"}]}"
        "]}";

    uint8_t sig[64]{};
    ed25519Sign(sig, reinterpret_cast<const uint8_t*>(catJson.data()), catJson.size(), seed);
    srv.addRoute("/catalog.json", catJson, 200);
    srv.addRoute("/catalog.json.sig", toHex(sig, 64), 200);

    PackInstaller inst(store, pub, srv.url("/catalog.json"), "1.0.0", true);
    CHECK(inst.keyConfigured());

    inst.refreshCatalogAsync();
    const PackProgress rprog = waitTerminal(inst);
    REQUIRE(rprog.state == PackJobState::Done);
    CHECK(inst.catalog().packs.size() == 2);

    store.protectUpdates({"onnx-runtime"});
    inst.installAsync("pose-plugin");
    REQUIRE(waitTerminal(inst).state == PackJobState::Failed);
    CHECK(inst.snapshot().error.find("in use or loaded") != std::string::npos);
    CHECK(store.list().empty()); CHECK(srv.hits("/rt.dll") == 0); CHECK(srv.hits("/pose.dll") == 0);
    store.protectUpdates({});
    inst.installAsync("pose-plugin");
    const PackProgress iprog = waitTerminal(inst);
    REQUIRE(iprog.state == PackJobState::Done);
    CHECK(store.isInstalled("onnx-runtime", "1.18.0"));
    CHECK(store.isInstalled("pose-plugin", "1.0.0"));
    CHECK(store.deepVerify("onnx-runtime", err));
    CHECK(store.deepVerify("pose-plugin", err));
}

TEST_CASE("packinstaller: bad catalog signature is rejected before any pack file download") {
    TempDir tmp("badsig");
    PackStore store(tmp.utf8());
    std::string err;
    REQUIRE(store.startup(err));

    uint8_t seed[32]{7};
    uint8_t pub[32]{};
    ed25519PublicFromSeed(seed, pub);

    FakeHttpServer srv;
    REQUIRE(srv.port() != 0);

    const std::string poseBytes = "pose-dll";
    srv.addRoute("/pose.dll", poseBytes, 200);
    const std::string catJson =
        "{\"packs\":[{\"id\":\"pose-plugin\",\"name\":\"Pose\",\"version\":\"1.0.0\",\"kind\":\"plugin\","
        "\"sizeBytes\":" + std::to_string(poseBytes.size()) + ","
        "\"files\":[{\"path\":\"pose.dll\",\"sha256\":\"" + hexSha256(poseBytes) + "\","
        "\"size\":" + std::to_string(poseBytes.size()) + ",\"url\":\"" + srv.url("/pose.dll") + "\"}]}]}";

    uint8_t sig[64]{};
    ed25519Sign(sig, reinterpret_cast<const uint8_t*>(catJson.data()), catJson.size(), seed);
    sig[0] ^= 0x01; // lật 1 bit chữ ký
    srv.addRoute("/catalog.json", catJson, 200);
    srv.addRoute("/catalog.json.sig", std::string(reinterpret_cast<const char*>(sig), 64), 200);

    PackInstaller inst(store, pub, srv.url("/catalog.json"), "1.0.0", true);
    inst.refreshCatalogAsync();
    const PackProgress prog = waitTerminal(inst);
    CHECK(prog.state == PackJobState::Failed);
    CHECK(inst.catalog().packs.empty());
    CHECK(srv.hits("/pose.dll") == 0);
}

TEST_CASE("packinstaller: tampered pack file fails sha256 and does not install") {
    TempDir tmp("tampered");
    PackStore store(tmp.utf8());
    std::string err;
    REQUIRE(store.startup(err));

    uint8_t seed[32]{9};
    uint8_t pub[32]{};
    ed25519PublicFromSeed(seed, pub);

    FakeHttpServer srv;
    REQUIRE(srv.port() != 0);

    const std::string goodBytes = "good-binary-content";
    const std::string evilBytes = "evil-binary-content"; // cùng độ dài nhưng khác byte
    srv.addRoute("/pose.dll", evilBytes, 200);

    const std::string catJson =
        "{\"packs\":[{\"id\":\"pose-plugin\",\"name\":\"Pose\",\"version\":\"1.0.0\",\"kind\":\"plugin\","
        "\"sizeBytes\":" + std::to_string(goodBytes.size()) + ","
        "\"files\":[{\"path\":\"pose.dll\",\"sha256\":\"" + hexSha256(goodBytes) + "\","
        "\"size\":" + std::to_string(goodBytes.size()) + ",\"url\":\"" + srv.url("/pose.dll") + "\"}]}]}";

    uint8_t sig[64]{};
    ed25519Sign(sig, reinterpret_cast<const uint8_t*>(catJson.data()), catJson.size(), seed);
    srv.addRoute("/catalog.json", catJson, 200);
    srv.addRoute("/catalog.json.sig", toHex(sig, 64), 200);

    PackInstaller inst(store, pub, srv.url("/catalog.json"), "1.0.0", true);
    inst.refreshCatalogAsync();
    REQUIRE(waitTerminal(inst).state == PackJobState::Done);

    inst.installAsync("pose-plugin");
    const PackProgress iprog = waitTerminal(inst);
    CHECK(iprog.state == PackJobState::Failed);
    CHECK(!store.isInstalled("pose-plugin"));
}

TEST_CASE("packinstaller: cancel during download leaves no installed pack") {
    TempDir tmp("cancel");
    PackStore store(tmp.utf8());
    std::string err;
    REQUIRE(store.startup(err));

    uint8_t seed[32]{11};
    uint8_t pub[32]{};
    ed25519PublicFromSeed(seed, pub);

    FakeHttpServer srv;
    REQUIRE(srv.port() != 0);

    const std::string bigBytes(128 * 1024, 'K');
    FakeHttpServer::Response slow;
    slow.status       = 200;
    slow.body         = bigBytes;
    slow.chunkSize    = 4096;
    slow.chunkDelayMs = 35;
    srv.addRoute("/slow.dll", slow);

    const std::string catJson =
        "{\"packs\":[{\"id\":\"slow-pack\",\"name\":\"Slow\",\"version\":\"1.0.0\",\"kind\":\"plugin\","
        "\"sizeBytes\":" + std::to_string(bigBytes.size()) + ","
        "\"files\":[{\"path\":\"slow.dll\",\"sha256\":\"" + hexSha256(bigBytes) + "\","
        "\"size\":" + std::to_string(bigBytes.size()) + ",\"url\":\"" + srv.url("/slow.dll") + "\"}]}]}";

    uint8_t sig[64]{};
    ed25519Sign(sig, reinterpret_cast<const uint8_t*>(catJson.data()), catJson.size(), seed);
    srv.addRoute("/catalog.json", catJson, 200);
    srv.addRoute("/catalog.json.sig", toHex(sig, 64), 200);

    PackInstaller inst(store, pub, srv.url("/catalog.json"), "1.0.0", true);
    inst.refreshCatalogAsync();
    REQUIRE(waitTerminal(inst).state == PackJobState::Done);

    inst.installAsync("slow-pack");
    std::this_thread::sleep_for(std::chrono::milliseconds(80));
    inst.cancel();

    const PackProgress cprog = waitTerminal(inst);
    CHECK(cprog.state == PackJobState::Cancelled);
    CHECK(!store.isInstalled("slow-pack"));
    CHECK(!fs::exists(tmp.path / "installed" / "slow-pack"));
}

TEST_CASE("packinstaller: all-zero public key disables online catalog") {
    TempDir tmp("zerokey");
    PackStore store(tmp.utf8());
    std::string err;
    REQUIRE(store.startup(err));

    const uint8_t zeroKey[32]{};
    PackInstaller inst(store, zeroKey, "https://example.com/catalog.json", "1.0.0", false);
    CHECK(!inst.keyConfigured());

    inst.refreshCatalogAsync();
    const PackProgress prog = waitTerminal(inst);
    CHECK(prog.state == PackJobState::Failed);
    CHECK(prog.error.find("signing key") != std::string::npos);
}

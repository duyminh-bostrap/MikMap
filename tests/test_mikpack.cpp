#include "TestHarness.h"

#include "core/util/Ed25519.h"
#include "core/util/Sha2.h"
#include "io/packs/MikPack.h"
#include "io/packs/PackInstaller.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

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
    explicit TempDir(const std::string& suffix) {
        static std::atomic<int> seq{0};
        path = fs::temp_directory_path() / fromUtf8("mikmap_test_mikpack_" + std::to_string(++seq) + "_" + suffix);
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

void writeFileBytes(const fs::path& p, const std::string& data) {
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
}

std::string readFileBytes(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

PackEntry buildSampleEntry(const std::string& f1Data, const std::string& f2Data) {
    const Sha256Digest d1 = sha256(f1Data.data(), f1Data.size());
    const Sha256Digest d2 = sha256(f2Data.data(), f2Data.size());
    PackEntry e;
    e.id        = "orbbec-femto";
    e.name      = "Orbbec Femto Bolt / Mega";
    e.version   = "1.0.0";
    e.kind      = Kind::Plugin;
    e.sizeBytes = f1Data.size() + f2Data.size();
    e.provides  = {"sensor.depth"};

    FileEntry fe1;
    fe1.path      = "mikmap_sensor_orbbec.dll";
    fe1.sha256Hex = toHex(d1.data(), d1.size());
    fe1.size      = f1Data.size();
    e.files.push_back(fe1);

    FileEntry fe2;
    fe2.path      = "models/hand.onnx";
    fe2.sha256Hex = toHex(d2.data(), d2.size());
    fe2.size      = f2Data.size();
    e.files.push_back(fe2);
    return e;
}

} // namespace

TEST_CASE("mikpack: write and install via PackInstaller::installFromFileAsync") {
    TempDir tmp("roundtrip");
    const fs::path srcDir   = tmp.path / "src_files";
    const fs::path packFile = tmp.path / "orbbec-femto-1.0.0.mikpack";
    const fs::path storeDir = tmp.path / "store";

    const std::string f1 = "dll-binary-payload-orbbec-femto";
    const std::string f2 = "onnx-model-weights-payload-1234567890";
    writeFileBytes(srcDir / "mikmap_sensor_orbbec.dll", f1);
    writeFileBytes(srcDir / "models" / "hand.onnx", f2);

    uint8_t seed[32]{};
    for (int i = 0; i < 32; ++i) seed[i] = static_cast<uint8_t>(0x30 + i);
    uint8_t pub[32]{};
    ed25519PublicFromSeed(seed, pub);

    const PackEntry entry = buildSampleEntry(f1, f2);
    std::string err;
    REQUIRE(WriteMikPackFile(entry, toUtf8(srcDir), seed, toUtf8(packFile), err));
    CHECK(fs::exists(packFile));

    PackStore store(toUtf8(storeDir));
    REQUIRE(store.startup(err));

    PackInstaller inst(store, pub, "https://example.com/catalog.json");
    inst.installFromFileAsync(toUtf8(packFile));

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(3000);
    while (std::chrono::steady_clock::now() < deadline) {
        const PackProgress pr = inst.snapshot();
        if (pr.state == PackJobState::Done || pr.state == PackJobState::Failed) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    REQUIRE(inst.snapshot().state == PackJobState::Done);
    CHECK(store.isInstalled("orbbec-femto", "1.0.0"));
    CHECK(store.deepVerify("orbbec-femto", err));
}

TEST_CASE("mikpack: rejects corrupted signature or payload byte") {
    TempDir tmp("corrupt");
    const fs::path srcDir   = tmp.path / "src_files";
    const fs::path packFile = tmp.path / "pack.mikpack";
    const fs::path storeDir = tmp.path / "store";

    const std::string f1 = "dll-binary-payload-orbbec-femto";
    const std::string f2 = "onnx-model-weights-payload-1234567890";
    writeFileBytes(srcDir / "mikmap_sensor_orbbec.dll", f1);
    writeFileBytes(srcDir / "models" / "hand.onnx", f2);

    uint8_t seed[32]{42};
    uint8_t pub[32]{};
    ed25519PublicFromSeed(seed, pub);

    const PackEntry entry = buildSampleEntry(f1, f2);
    std::string err;
    REQUIRE(WriteMikPackFile(entry, toUtf8(srcDir), seed, toUtf8(packFile), err));

    const std::string orig = readFileBytes(packFile);
    REQUIRE(orig.size() > 64);

    PackStore store(toUtf8(storeDir));
    REQUIRE(store.startup(err));

    // 1. Sửa 1 byte ở cuối (trong nội dung tệp đính kèm)
    {
        std::string badPayload = orig;
        badPayload[badPayload.size() - 5] ^= 0x55;
        const fs::path badFile = tmp.path / "bad_payload.mikpack";
        writeFileBytes(badFile, badPayload);

        PackEntry out;
        CHECK(!InstallMikPackFile(toUtf8(badFile), pub, store, out, err));
        CHECK(!store.isInstalled("orbbec-femto"));
    }

    // 2. Sửa 1 byte ở giữa (trong entry.json / chữ ký)
    {
        std::string badSig = orig;
        badSig[20] ^= 0x01;
        const fs::path badFile = tmp.path / "bad_sig.mikpack";
        writeFileBytes(badFile, badSig);

        PackEntry out;
        CHECK(!InstallMikPackFile(toUtf8(badFile), pub, store, out, err));
        CHECK(!store.isInstalled("orbbec-femto"));
    }
}

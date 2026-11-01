#include "TestHarness.h"
#include "io/ai/OrtRuntime.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
using namespace mikmap;
namespace {
std::string testEnv(const char* key) {
#ifdef _MSC_VER
    char* raw = nullptr; size_t length = 0;
    if (_dupenv_s(&raw, &length, key) != 0) return {};
    const std::string value = raw ? raw : ""; std::free(raw); return value;
#else
    const char* value = std::getenv(key); return value ? value : "";
#endif
}
}
TEST_CASE("ORT missing runtime reports its path") {
    OrtRuntime ort; std::string err;
    const auto path = (std::filesystem::temp_directory_path() / "missing-ort-task4").u8string();
    REQUIRE(!ort.load(std::string(path.begin(), path.end()), err));
    CHECK(err.find("missing-ort-task4") != std::string::npos);
}
TEST_CASE("ORT refuses an invalid DLL without crashing") {
    const auto dir = std::filesystem::temp_directory_path() / "mikmap-ort-invalid-task4";
    std::filesystem::create_directories(dir);
    { std::ofstream file(dir / "onnxruntime.dll"); file << "not a DLL"; }
    OrtRuntime ort; std::string err;
    const auto utf8 = dir.u8string();
    const bool ok = ort.load(std::string(utf8.begin(), utf8.end()), err);
    std::filesystem::remove_all(dir);
    CHECK(!ok); CHECK(!err.empty());
}
TEST_CASE("ORT session and run require initialization") {
    OrtRuntime ort; std::string err; bool cuda = true;
    REQUIRE(!ort.createSession("missing.onnx", OrtDevice::Auto, err, &cuda));
    CHECK(!err.empty()); CHECK(!cuda);
    std::vector<std::vector<float>> out; std::vector<std::vector<int64_t>> shapes;
    CHECK(!ort.run(nullptr, {}, out, shapes, err));
}
TEST_CASE("ORT typed run requires session and metadata defaults to float") {
    OrtTensorInfo info; CHECK(info.type==OrtTensorType::Float32);
    OrtRuntime ort; std::string err;
    std::vector<std::vector<float>> out;
    std::vector<std::vector<int64_t>> integers,shapes;
    CHECK(!ort.run(nullptr,{},out,integers,shapes,err)); CHECK(!err.empty());
}
TEST_CASE("ORT optional real model Auto and CPU inference") {
    const auto dir = testEnv("MIKMAP_TEST_ORT_DIR");
    const auto model = testEnv("MIKMAP_TEST_POSE_MODEL");
    if (dir.empty() || model.empty()) { std::cout << "[SKIP] ORT integration: set MIKMAP_TEST_ORT_DIR and MIKMAP_TEST_POSE_MODEL\n"; return; }
    OrtRuntime ort; std::string err;
    REQUIRE(ort.load(dir, err));
    for (auto device : {OrtDevice::Auto, OrtDevice::Cpu}) {
        bool cuda = false;
        REQUIRE(ort.createSession(model, device, err, &cuda));
        std::cout << "ORT usedCuda=" << cuda << '\n';
        if (device == OrtDevice::Cpu) CHECK(!cuda);
        const auto in = ort.inputs(); REQUIRE(in.size() == 1); REQUIRE(!ort.outputs().empty());
        auto shape = in[0].shape; size_t count = 1;
        for (auto& d : shape) { if (d < 0) d = 1; REQUIRE(d > 0); REQUIRE(static_cast<uint64_t>(d) <= std::numeric_limits<size_t>::max() / count); count *= static_cast<size_t>(d); }
        std::vector<float> zero(count);
        std::vector<std::vector<float>> out; std::vector<std::vector<int64_t>> shapes;
        REQUIRE(ort.run(zero.data(), shape, out, shapes, err));
        CHECK(out.size() == ort.outputs().size()); CHECK(out.size() == shapes.size());
        REQUIRE(ort.run(zero.data(), shape, out, shapes, err));
        CHECK(!ort.run(nullptr, shape, out, shapes, err));
        CHECK(!ort.run(zero.data(), {-1}, out, shapes, err));
    }
}

#include "TestHarness.h"

#include "io/camera/CameraDevice.h"

#include <chrono>
#include <cstdint>
#include <string>
#include <type_traits>
#include <vector>

using namespace mikmap;

TEST_CASE("Camera new modes require Color and reject unknown modes") {
    for(CamMode m:{CamMode::Hand,CamMode::Face,CamMode::Full}) {
        CameraCaps caps; std::string why;
        CHECK(!modeSupported(m,caps,why)); CHECK(why.find("Color")!=std::string::npos);
        caps.hasDepth=true; CHECK(!modeSupported(m,caps,why));
        caps.hasColor=true; CHECK(modeSupported(m,caps,why)); CHECK(why.empty());
    }
    std::string why; CHECK(!modeSupported(static_cast<CamMode>(99),CameraCaps{},why)); CHECK(!why.empty());
    static_assert(static_cast<int>(CamMode::Hand)==2 && static_cast<int>(CamMode::Face)==3 && static_cast<int>(CamMode::Full)==4);
}

static_assert(std::is_base_of_v<ICameraDevice, FakeCamera>,
              "FakeCamera phai cai ICameraDevice de CameraSource inject duoc");

namespace {

CameraCaps colorCaps() {
    CameraCaps c;
    c.hasColor = true;
    c.color = {640, 480, 30.0f};
    return c;
}

/// Khung RGB24 dac, byte dau = seed de kiem thu tu.
FakeCamera::Frame makeFrame(int w, int h, uint8_t seed, int64_t tCaptureNs = 0) {
    FakeCamera::Frame f;
    f.width = w;
    f.height = h;
    f.rgb.resize(static_cast<size_t>(w) * static_cast<size_t>(h) * 3u);
    for (size_t i = 0; i < f.rgb.size(); ++i) {
        f.rgb[i] = static_cast<uint8_t>(seed + i);
    }
    f.tCaptureNs = tCaptureNs;
    return f;
}

int64_t elapsedMs(std::chrono::steady_clock::time_point t0) {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
               std::chrono::steady_clock::now() - t0).count();
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  Bang kha nang: che do nao can luong thiet bi nao
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("Camera: bang kha nang — Off luon duoc, Body can Color") {
    std::string why = "stale";
    CameraCaps none{};

    CHECK(modeSupported(CamMode::Off, none, why));
    CHECK(why.empty());

    why.clear();
    CHECK(!modeSupported(CamMode::Body, none, why));
    CHECK(why.find("Color") != std::string::npos);

    // Chi co Depth khong du cho Body: can anh mau de chay mo hinh.
    CameraCaps depthOnly{};
    depthOnly.hasDepth = true;
    depthOnly.depth = {640, 480, 30.0f};
    why.clear();
    CHECK(!modeSupported(CamMode::Body, depthOnly, why));
    CHECK(why.find("Color") != std::string::npos);

    // Camera 3D (Color + Depth) van chay Body bang luong mau.
    CameraCaps colorDepth = colorCaps();
    colorDepth.hasDepth = true;
    colorDepth.depth = {640, 480, 30.0f};
    why = "stale";
    CHECK(modeSupported(CamMode::Body, colorDepth, why));
    CHECK(why.empty());
}

// ═══════════════════════════════════════════════════════════════════════
//  FakeCamera: kich ban, thu tu, tai su dung bo dem
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("Camera: FakeCamera tra khung theo thu tu va giu bo dem") {
    FakeCamera cam(colorCaps());
    CHECK(cam.caps().hasColor);

    FakeCamera::Frame withDepth = makeFrame(4, 2, 3, 300);
    withDepth.depthW = 2;
    withDepth.depthH = 1;
    withDepth.depthMm = {123, 456};

    cam.setFrames({makeFrame(4, 2, 1, 100),
                   makeFrame(4, 2, 2, 200),
                   withDepth});

    std::string err = "stale";
    CameraFrame out;
    REQUIRE(cam.open("fake:0", 4, 2, err));
    CHECK(err.empty());

    REQUIRE(cam.read(out, 50, err));
    CHECK(err.empty());
    CHECK(out.width == 4);
    CHECK(out.height == 2);
    CHECK(out.rgb.size() == 24u);
    CHECK(out.rgb[0] == 1);
    CHECK(out.tCaptureNs == 100);
    CHECK(out.index == 0u);
    CHECK(out.depthMm.empty());

    // Bo dem cua caller duoc tai su dung khi kich thuoc khong doi.
    const uint8_t* rgbPtr = out.rgb.data();

    REQUIRE(cam.read(out, 50, err));
    CHECK(out.rgb[0] == 2);
    CHECK(out.tCaptureNs == 200);
    CHECK(out.index == 1u);
    CHECK(out.rgb.data() == rgbPtr);

    REQUIRE(cam.read(out, 50, err));
    CHECK(out.rgb[0] == 3);
    CHECK(out.tCaptureNs == 300);
    CHECK(out.index == 2u);
    CHECK(out.depthW == 2);
    CHECK(out.depthH == 1);
    REQUIRE(out.depthMm.size() == 2u);
    CHECK(out.depthMm[0] == 123);
    CHECK(out.depthMm[1] == 456);
}

TEST_CASE("Camera: FakeCamera het kich ban thi cho den timeout roi bao loi") {
    FakeCamera cam(colorCaps());
    cam.setFrames({makeFrame(2, 2, 7)});

    std::string err;
    CameraFrame out;
    REQUIRE(cam.open("fake:0", 2, 2, err));
    REQUIRE(cam.read(out, 50, err));

    const auto t0 = std::chrono::steady_clock::now();
    const bool got = cam.read(out, 40, err);
    const int64_t waited = elapsedMs(t0);

    CHECK(!got);
    CHECK(!err.empty());
    CHECK(waited >= 25);
    CHECK(waited < 2000);
}

TEST_CASE("Camera: FakeCamera nhip khung theo frameIntervalMs") {
    FakeCamera cam(colorCaps());
    cam.setFrames({makeFrame(2, 2, 1), makeFrame(2, 2, 2)});
    cam.setFrameIntervalMs(60);

    std::string err;
    CameraFrame out;
    REQUIRE(cam.open("fake:0", 2, 2, err));
    REQUIRE(cam.read(out, 500, err));   // khung dau san co ngay

    const auto t0 = std::chrono::steady_clock::now();
    REQUIRE(cam.read(out, 500, err));   // khung thu hai phai cho ~60 ms
    CHECK(elapsedMs(t0) >= 30);
}

TEST_CASE("Camera: FakeCamera rut cap sau N khung roi cam lai") {
    FakeCamera cam(colorCaps());
    cam.setFrames({makeFrame(4, 2, 1), makeFrame(4, 2, 2), makeFrame(4, 2, 3)});
    cam.setUnplugAfter(2);

    std::string err;
    CameraFrame out;
    REQUIRE(cam.open("fake:cam", 4, 2, err));

    REQUIRE(cam.read(out, 50, err));
    REQUIRE(cam.read(out, 50, err));
    CHECK(cam.framesDelivered() == 2);

    CHECK(!cam.read(out, 50, err));
    CHECK(err.find("unplugged") != std::string::npos);
    CHECK(err.find("fake:cam") != std::string::npos);   // loi phai neu ten camera

    // Dang rut thi mo lai phai that bai — CameraSource se thu lai moi 2 s.
    CHECK(!cam.open("fake:cam", 4, 2, err));

    cam.replug();
    REQUIRE(cam.open("fake:cam", 4, 2, err));
    REQUIRE(cam.read(out, 50, err));                    // kich ban chay lai tu dau
    CHECK(out.rgb[0] == 1);
    CHECK(out.index == 0u);
}

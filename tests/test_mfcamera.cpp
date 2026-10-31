// ════════════════════════════════════════════════════════════════════════
//  tests/test_mfcamera.cpp — camera 2D qua Media Foundation
//
//  Test phần không cần phần cứng chạy trên mọi máy: dò thiết bị không ném,
//  id lạ phải báo lỗi có chính id đó. Test đọc khung thật chỉ chạy khi có
//  biến môi trường MIKMAP_TEST_CAMERA (id hoặc chỉ số do listCameras in ra)
//  — CI không có webcam vẫn xanh, và không bao giờ giả lập một cú pass.
// ════════════════════════════════════════════════════════════════════════
#include "TestHarness.h"

#include "io/camera/MfCamera.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace mikmap;

// ═══════════════════════════════════════════════════════════════════════
//  Dò thiết bị + id không tồn tại — không cần webcam
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("MfCamera: listCameras khong nem, id la bao loi co id") {
    // Hợp đồng: trả danh sách, hoặc danh sách rỗng kèm lỗi — không bao giờ
    // ném ra ngoài (CameraSource gọi hàm này từ thread điều khiển).
    std::vector<CameraInfo> cams;
    std::string err;
    bool threw = false;
    try {
        cams = listCameras(err);
    } catch (...) {
        threw = true;
    }
    CHECK(!threw);

#ifdef _WIN32
    // Có webcam thì danh sách khác rỗng; máy không có webcam là chuyện bình
    // thường (CI), nên ở đây chỉ đòi hỏi không ném.
    (void)cams;
#else
    CHECK(cams.empty());
    CHECK(err.find("unsupported") != std::string::npos);
#endif

    // Id không tồn tại: mở phải thất bại và message phải nhắc lại id để
    // người dùng biết camera nào đang thiếu.
    MfCamera cam;
    const std::string bogus = "no-such-camera-42";
    std::string openErr;
    CHECK(!cam.open(bogus, 640, 480, openErr));
    CHECK(openErr.find(bogus) != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════
//  Đọc khung thật — chỉ khi có MIKMAP_TEST_CAMERA
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("MfCamera: doc 10 khung tu webcam that") {
#ifndef _WIN32
    std::cout << "camera test skipped (set MIKMAP_TEST_CAMERA)\n";
    return;
#else
    char* raw = nullptr;
    size_t len = 0;
    if (_dupenv_s(&raw, &len, "MIKMAP_TEST_CAMERA") != 0 || !raw || !*raw) {
        std::free(raw);
        std::cout << "camera test skipped (set MIKMAP_TEST_CAMERA)\n";
        return;
    }
    const std::string want(raw);
    std::free(raw);

    MfCamera cam;
    std::string err;
    REQUIRE(cam.open(want, 640, 480, err));
    CHECK(err.empty());

    // open() chọn cỡ gần nhất >= 640x480 (hoặc lớn nhất nếu không có), và
    // caps() phải mô tả đúng cỡ mà read() trả về.
    const CameraCaps caps = cam.caps();
    REQUIRE(caps.hasColor);
    CHECK(caps.color.width >= 640);
    CHECK(caps.color.height >= 480);

    CameraFrame f;
    int64_t prevT = -1;
    uint64_t prevIndex = 0;
    for (int i = 0; i < 10; ++i) {
        const bool got = cam.read(f, 5000, err);
        if (!got) std::cout << "frame " << i << " read failed: " << err << "\n";
        REQUIRE(got);
        CHECK(err.empty());
        CHECK(f.width == caps.color.width);
        CHECK(f.height == caps.color.height);
        CHECK(f.rgb.size() == static_cast<size_t>(f.width) *
                                  static_cast<size_t>(f.height) * 3u);
        CHECK(f.tCaptureNs > prevT);
        CHECK(i == 0 || f.index > prevIndex);
        prevT = f.tCaptureNs;
        prevIndex = f.index;
    }

    cam.close();
#endif
}

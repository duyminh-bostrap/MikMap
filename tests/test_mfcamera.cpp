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

#include "io/camera/MfCameraConvert.h"

#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

using namespace mikmap;

// ═══════════════════════════════════════════════════════════════════════
//  Chuyển màu trên bộ đệm tổng hợp — không cần webcam
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("MfCamera convert: RGB24 cua MF la BGR, doi thanh R-first") {
    // MFVideoFormat_RGB24 là DIB: byte trong bộ đệm là B,G,R. Đường copy
    // nhanh phải đảo kênh, nếu không sẽ lệch đỏ/xanh so với NV12/YUY2.
    const int w = 3, h = 2;
    const long pitch = 11;   // 3*3 + 2 byte đệm: pitch lẻ, không tight
    std::vector<uint8_t> src(static_cast<size_t>(pitch) * h, 0xEE);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int i = y * w + x;
            uint8_t* p = src.data() + static_cast<size_t>(y) * static_cast<size_t>(pitch) +
                         static_cast<size_t>(x) * 3u;
            p[0] = static_cast<uint8_t>(10 + i);    // B
            p[1] = static_cast<uint8_t>(100 + i);   // G
            p[2] = static_cast<uint8_t>(200 + i);   // R
        }
    }

    std::vector<uint8_t> dst(static_cast<size_t>(w) * h * 3u, 0);
    camera_detail::bgr24ToRgb(src.data(), pitch, w, h, dst.data());

    for (int i = 0; i < w * h; ++i) {
        const size_t o = static_cast<size_t>(i) * 3u;
        const std::string at = "pixel " + std::to_string(i);
        CHECK_MSG(dst[o + 0] == static_cast<uint8_t>(200 + i), at + " R");
        CHECK_MSG(dst[o + 1] == static_cast<uint8_t>(100 + i), at + " G");
        CHECK_MSG(dst[o + 2] == static_cast<uint8_t>(10 + i), at + " B");
    }
}

TEST_CASE("MfCamera convert: RGB24 bottom-up (stride am) dung thu tu hang") {
    // Stride âm: hàng logic y nằm ở cuối bộ đệm. Đảo kênh phải đi kèm đảo
    // hàng, nếu không ảnh vừa lộn màu vừa lộn trên/dưới.
    const int w = 2, h = 3;
    const long pitch = 9;   // 2*3 + 3 byte đệm: pitch lẻ
    std::vector<uint8_t> src(static_cast<size_t>(pitch) * h, 0xEE);
    for (int physical = 0; physical < h; ++physical) {
        const int y = h - 1 - physical;   // hàng vật lý đầu = hàng logic cuối
        for (int x = 0; x < w; ++x) {
            const int i = y * w + x;
            uint8_t* p = src.data() + static_cast<size_t>(physical) * static_cast<size_t>(pitch) +
                         static_cast<size_t>(x) * 3u;
            p[0] = static_cast<uint8_t>(10 + i);    // B
            p[1] = static_cast<uint8_t>(50 + i);    // G
            p[2] = static_cast<uint8_t>(90 + i);    // R
        }
    }

    std::vector<uint8_t> dst(static_cast<size_t>(w) * h * 3u, 0);
    camera_detail::bgr24ToRgb(src.data(), -pitch, w, h, dst.data());

    for (int i = 0; i < w * h; ++i) {
        const size_t o = static_cast<size_t>(i) * 3u;
        const std::string at = "pixel " + std::to_string(i);
        CHECK_MSG(dst[o + 0] == static_cast<uint8_t>(90 + i), at + " R");
        CHECK_MSG(dst[o + 1] == static_cast<uint8_t>(50 + i), at + " G");
        CHECK_MSG(dst[o + 2] == static_cast<uint8_t>(10 + i), at + " B");
    }
}

TEST_CASE("MfCamera convert: NV12 BT.601, pitch le, dung R-first") {
    // Y=81, U=90, V=240 là đỏ BT.601; U/V đảo lại thành xanh dương. Kiểm
    // đúng cả thứ tự kênh lẫn cách ghép cặp chroma (x/2).
    const int w = 4, h = 2;
    const long pitch = 7;   // w + 3 byte đệm
    const size_t yBytes = static_cast<size_t>(pitch) * h;
    std::vector<uint8_t> src(yBytes + static_cast<size_t>(pitch) * (h / 2), 0xEE);
    for (size_t i = 0; i < yBytes; ++i) src[i] = 81;   // Y phẳng
    uint8_t* uv = src.data() + yBytes;
    uv[0] = 90;  uv[1] = 240;   // cặp 0: đỏ
    uv[2] = 240; uv[3] = 90;    // cặp 1: xanh dương

    std::vector<uint8_t> dst(static_cast<size_t>(w) * h * 3u, 0);
    camera_detail::nv12ToRgb(src.data(), pitch, w, h, dst.data());

    const uint8_t expected[4][3] = {{255, 0, 0}, {255, 0, 0}, {15, 63, 255}, {15, 63, 255}};
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t o = (static_cast<size_t>(y) * w + static_cast<size_t>(x)) * 3u;
            const std::string at = "pixel " + std::to_string(y) + "," + std::to_string(x);
            CHECK_MSG(dst[o + 0] == expected[x][0], at + " R");
            CHECK_MSG(dst[o + 1] == expected[x][1], at + " G");
            CHECK_MSG(dst[o + 2] == expected[x][2], at + " B");
        }
    }
}

TEST_CASE("MfCamera convert: YUY2 ghep cap Y0 U Y1 V, dung R-first") {
    const int w = 4, h = 2;
    const long pitch = 13;   // w*2 + 5 byte đệm
    std::vector<uint8_t> src(static_cast<size_t>(pitch) * h, 0xEE);
    for (int y = 0; y < h; ++y) {
        uint8_t* row = src.data() + static_cast<size_t>(y) * static_cast<size_t>(pitch);
        row[0] = 81; row[1] = 90; row[2] = 81; row[3] = 240;   // cặp 0: đỏ
        row[4] = 81; row[5] = 240; row[6] = 81; row[7] = 90;   // cặp 1: xanh dương
    }

    std::vector<uint8_t> dst(static_cast<size_t>(w) * h * 3u, 0);
    camera_detail::yuy2ToRgb(src.data(), pitch, w, h, dst.data());

    const uint8_t expected[4][3] = {{255, 0, 0}, {255, 0, 0}, {15, 63, 255}, {15, 63, 255}};
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t o = (static_cast<size_t>(y) * w + static_cast<size_t>(x)) * 3u;
            const std::string at = "pixel " + std::to_string(y) + "," + std::to_string(x);
            CHECK_MSG(dst[o + 0] == expected[x][0], at + " R");
            CHECK_MSG(dst[o + 1] == expected[x][1], at + " G");
            CHECK_MSG(dst[o + 2] == expected[x][2], at + " B");
        }
    }
}

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

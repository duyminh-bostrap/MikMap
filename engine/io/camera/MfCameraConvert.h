// ════════════════════════════════════════════════════════════════════════
//  io/camera/MfCameraConvert.h — chuyển khung Media Foundation sang RGB24
//
//  Tách khỏi MfCamera.cpp để test thuần tuý (không cần webcam, không cần
//  Windows) kiểm được đúng phép chuyển màu. Chỉ dùng STL.
//
//  ── Thứ tự kênh ───────────────────────────────────────────────────────
//  Mọi hàm ở đây ghi R,G,B vào đích — đúng hợp đồng CameraFrame::rgb
//  ("RGB24, R-first"). Lưu ý MFVideoFormat_RGB24 của Media Foundation là
//  định dạng DIB, byte trong bộ đệm là B,G,R, nên bgr24ToRgb phải đảo kênh;
//  nếu copy thẳng sẽ lệch đỏ/xanh so với đường NV12/YUY2.
//
//  ── Stride ────────────────────────────────────────────────────────────
//  pitch dương: hàng y ở base + y*pitch. pitch âm: ảnh bottom-up, hàng y
//  logic ở base + (h-1-y)*(-pitch).
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <algorithm>
#include <cstddef>
#include <cstdint>

namespace mikmap {
namespace camera_detail {

inline uint8_t clamp8(int v) {
    return static_cast<uint8_t>(v < 0 ? 0 : (v > 255 ? 255 : v));
}

/// Con trỏ tới hàng logic y. Stride âm = ảnh lưu từ dưới lên.
inline const uint8_t* rowPtr(const uint8_t* base, long pitch, int y, int h) {
    if (pitch >= 0) {
        return base + static_cast<std::ptrdiff_t>(y) * static_cast<std::ptrdiff_t>(pitch);
    }
    return base + static_cast<std::ptrdiff_t>(h - 1 - y) * static_cast<std::ptrdiff_t>(-pitch);
}

/// Ghi một điểm ảnh theo thứ tự R,G,B từ Y (limited range) và U/V đã trừ 128.
inline void writePixel(uint8_t* dst, int yRaw, int u, int v) {
    const int c = std::max(0, yRaw - 16);
    dst[0] = clamp8((298 * c + 409 * v + 128) >> 8);
    dst[1] = clamp8((298 * c - 100 * u - 208 * v + 128) >> 8);
    dst[2] = clamp8((298 * c + 516 * u + 128) >> 8);
}

/// MFVideoFormat_RGB24 là BGR trong bộ đệm (định dạng DIB). Đảo kênh để
/// CameraFrame::rgb luôn là R-first, giống đường NV12/YUY2 và FakeCamera.
inline void bgr24ToRgb(const uint8_t* src, long pitch, int w, int h, uint8_t* dst) {
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = rowPtr(src, pitch, y, h);
        uint8_t* out = dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 3u;
        for (int x = 0; x < w; ++x) {
            const uint8_t* p = row + static_cast<std::size_t>(x) * 3u;
            out[0] = p[2];
            out[1] = p[1];
            out[2] = p[0];
            out += 3;
        }
    }
}

/// NV12: mặt Y (h hàng) rồi mặt UV xen kẽ (h/2 hàng), BT.601 limited range.
/// Chỉ hỗ trợ pitch dương — caller kiểm tra trước (MfCamera từ chối stride âm).
inline void nv12ToRgb(const uint8_t* src, long pitch, int w, int h, uint8_t* dst) {
    const uint8_t* uvPlane = src + static_cast<std::ptrdiff_t>(pitch) * h;
    for (int y = 0; y < h; ++y) {
        const uint8_t* yRow = src + static_cast<std::ptrdiff_t>(y) * pitch;
        const uint8_t* uvRow = uvPlane + static_cast<std::ptrdiff_t>(y / 2) * pitch;
        uint8_t* out = dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 3u;
        for (int x = 0; x < w; ++x) {
            const int u = static_cast<int>(uvRow[(x / 2) * 2]) - 128;
            const int v = static_cast<int>(uvRow[(x / 2) * 2 + 1]) - 128;
            writePixel(out + static_cast<std::size_t>(x) * 3u, yRow[x], u, v);
        }
    }
}

/// YUY2: mỗi cặp điểm ảnh là 4 byte Y0 U Y1 V.
inline void yuy2ToRgb(const uint8_t* src, long pitch, int w, int h, uint8_t* dst) {
    for (int y = 0; y < h; ++y) {
        const uint8_t* row = rowPtr(src, pitch, y, h);
        uint8_t* out = dst + static_cast<std::size_t>(y) * static_cast<std::size_t>(w) * 3u;
        for (int x = 0; x + 1 < w; x += 2) {
            const uint8_t* p = row + static_cast<std::size_t>(x) * 2u;
            const int u = static_cast<int>(p[1]) - 128;
            const int v = static_cast<int>(p[3]) - 128;
            writePixel(out + static_cast<std::size_t>(x) * 3u, p[0], u, v);
            writePixel(out + static_cast<std::size_t>(x + 1) * 3u, p[2], u, v);
        }
        if (w & 1) {   // chiều rộng lẻ không xảy ra với YUY2; để đen cho an toàn
            uint8_t* last = out + static_cast<std::size_t>(w - 1) * 3u;
            last[0] = last[1] = last[2] = 0;
        }
    }
}

} // namespace camera_detail
} // namespace mikmap

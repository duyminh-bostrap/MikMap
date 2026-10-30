// ════════════════════════════════════════════════════════════════════════
//  io/packs/HttpGet.h — tải tệp qua WinHTTP có tiến độ, huỷ và chính sách URL
//
//  Chính sách URL:
//  - Chỉ chấp nhận https://
//  - Ngoại lệ duy nhất: http://127.0.0.1[:port]/... hoặc http://localhost[:port]/...
//    KHI VÀ CHỈ KHI opts.allowLoopbackHttp == true (dành riêng cho unit test).
//  - Mọi redirect sang http:// không thuộc loopback đều bị huỷ ngay.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>

namespace mikmap {

struct HttpOptions {
    int                connectTimeoutMs  = 8000;
    int                receiveTimeoutMs  = 15000;
    uint64_t           maxBytes          = 2ull * 1024 * 1024 * 1024;
    bool               allowLoopbackHttp = false;  // chỉ bật trong unit test
    std::atomic<bool>* cancel            = nullptr;
};

using HttpProgressFn = std::function<void(uint64_t doneBytes, uint64_t totalBytes)>;

bool HttpGetToFile(const std::string& url, const std::string& dstFileUtf8,
                   const HttpOptions& opts, HttpProgressFn progress, std::string& errOut);

bool HttpGetToString(const std::string& url, std::string& bodyOut,
                     const HttpOptions& opts, std::string& errOut);

namespace packs {
using mikmap::HttpOptions;
using mikmap::HttpProgressFn;
using mikmap::HttpGetToFile;
using mikmap::HttpGetToString;
} // namespace packs

} // namespace mikmap

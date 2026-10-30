// ════════════════════════════════════════════════════════════════════════
//  core/util/Sha2.h — SHA-256 / SHA-512 (FIPS 180-4), STL only
//
//  Used to verify downloaded sensor packs against the signed catalog.
//  Streaming: update() any number of times, then finish() once.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>

namespace mikmap {

using Sha256Digest = std::array<uint8_t, 32>;
using Sha512Digest = std::array<uint8_t, 64>;

class Sha256 {
public:
    Sha256();
    void         update(const void* data, size_t n);
    Sha256Digest finish();

private:
    void block(const uint8_t* p);
    uint32_t h_[8];
    uint8_t  buf_[64];
    size_t   bufLen_ = 0;
    uint64_t total_  = 0;  // bytes
};

class Sha512 {
public:
    Sha512();
    void         update(const void* data, size_t n);
    Sha512Digest finish();

private:
    void block(const uint8_t* p);
    uint64_t h_[8];
    uint8_t  buf_[128];
    size_t   bufLen_ = 0;
    uint64_t total_  = 0;  // bytes (messages < 2^61 bytes, so the high 64 length bits stay 0)
};

Sha256Digest sha256(const void* data, size_t n);
Sha512Digest sha512(const void* data, size_t n);

/// Lowercase hex.
std::string toHex(const uint8_t* d, size_t n);
/// Exactly 2*n hex digits (either case) -> n bytes. False on wrong length / non-hex.
bool fromHex(const std::string& hex, uint8_t* out, size_t n);

/// Streams the file in 64 KiB chunks. False if it cannot be opened/read or
/// `cancel` is (or becomes) set. `utf8Path` may contain non-ASCII letters.
bool sha256File(const std::string& utf8Path, Sha256Digest& out, std::atomic<bool>* cancel = nullptr);

} // namespace mikmap

// ════════════════════════════════════════════════════════════════════════
//  io/packs/MikPack.h — container nhị phân .mikpack cho cài đặt ngoại tuyến
//
//  Định dạng tệp .mikpack (little-endian):
//    8 byte   magic: "MIKPACK1"
//    uint32   entryJsonLen (<= 64 KiB)
//    byte[]   entry.json (một đối tượng PackEntry JSON duy nhất)
//    64 byte  chữ ký Ed25519 trên entry.json
//    uint32   fileCount (<= 64, khớp entry.files.size())
//    Lặp fileCount lần:
//      uint32 pathLen (<= 240)
//      byte[] path UTF-8 (khớp entry.files[i].path và qua isSafeRelPath)
//      uint64 size (khớp entry.files[i].size)
//      byte[] nội dung tệp
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "io/packs/PackManifest.h"
#include "io/packs/PackStore.h"

#include <cstdint>
#include <string>

namespace mikmap {

bool WriteMikPackFile(const PackEntry& entry, const std::string& stagingDirUtf8,
                      const uint8_t secretSeed[32], const std::string& dstFileUtf8,
                      std::string& errOut);

bool InstallMikPackFile(const std::string& mikpackPathUtf8, const uint8_t publicKey[32],
                        PackStore& store, PackEntry& installedOut, std::string& errOut);

namespace packs {
using mikmap::WriteMikPackFile;
using mikmap::InstallMikPackFile;
} // namespace packs

} // namespace mikmap

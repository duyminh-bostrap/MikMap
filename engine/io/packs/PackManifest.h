// ════════════════════════════════════════════════════════════════════════
//  io/packs/PackManifest.h — manifest gói sensor + danh mục (catalog)
//
//  Chỉ STL + core/util/Json. Đây là RANH GIỚI BẢO MẬT: pack.json / catalog
//  có thể độc hại, nên mọi trường được kiểm chặt trước khi bộ cài dùng
//  (đường dẫn không được thoát khỏi thư mục gói; id/phiên bản/băm ở dạng
//  chuẩn duy nhất). Không ném ngoại lệ — lỗi trả qua bool + err.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/util/Json.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mikmap::packs {

enum class Kind { Runtime, Plugin, Model };

struct FileEntry {
    std::string path, sha256Hex;   // sha256Hex: 64 ký tự hex THƯỜNG
    std::string url;               // tuỳ chọn: có trong catalog, vắng trong .mikpack
    uint64_t    size = 0;
};
struct Dependency { std::string id, minVersion; };

struct PackInfo {
    std::string id, name, version;
    Kind        kind = Kind::Plugin;
    uint64_t    sizeBytes = 0;
    std::vector<FileEntry>   files;
    std::vector<Dependency>  needs;     // khoá JSON là "requires" (từ khoá C++20)
    std::vector<std::string> provides;
};
struct Catalog { std::vector<PackInfo> packs; };

constexpr size_t   kMaxCatalogBytes = 4u * 1024u * 1024u;
constexpr size_t   kMaxFilesPerPack = 64;
constexpr size_t   kMaxPacks        = 256;
constexpr uint64_t kMaxFileBytes    = 2ull * 1024 * 1024 * 1024;

/// Đường dẫn tương đối an toàn, chỉ dùng '/'; xem PackManifest.cpp cho danh sách luật.
bool isSafeRelPath(const std::string& p);

bool parsePackInfo(const JsonValue& v, PackInfo& out, std::string& err);
bool parseCatalog(const std::string& text, Catalog& out, std::string& err);

/// -1/0/1, so số từng thành phần (N.N.N; chuỗi sai dạng coi như 0.0.0).
int compareVersions(const std::string& a, const std::string& b);

/// Phụ thuộc trước, chính `id` cuối cùng; không trùng lặp. Lặp (không đệ quy).
bool resolveInstallOrder(const Catalog& c, const std::string& id,
                         std::vector<std::string>& order, std::string& err);

const PackInfo* findPack(const Catalog& c, const std::string& id);

} // namespace mikmap::packs

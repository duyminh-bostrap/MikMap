#include "io/packs/MikPack.h"

#include "core/util/Ed25519.h"
#include "core/util/Json.h"
#include "core/util/Sha2.h"

#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <vector>

namespace mikmap {

namespace {

namespace fs = std::filesystem;

constexpr char     kMagic[8]      = {'M', 'I', 'K', 'P', 'A', 'C', 'K', '1'};
constexpr uint32_t kMaxEntryBytes = 64u * 1024u;
constexpr uint32_t kMaxPathLen    = 240u;

fs::path fromUtf8(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}

bool fail(std::string& err, const std::string& msg) {
    err = msg;
    return false;
}

bool writeU32LE(std::ofstream& out, uint32_t v) {
    const uint8_t b[4] = {
        static_cast<uint8_t>(v & 0xFFu),
        static_cast<uint8_t>((v >> 8) & 0xFFu),
        static_cast<uint8_t>((v >> 16) & 0xFFu),
        static_cast<uint8_t>((v >> 24) & 0xFFu),
    };
    out.write(reinterpret_cast<const char*>(b), 4);
    return static_cast<bool>(out);
}

bool writeU64LE(std::ofstream& out, uint64_t v) {
    uint8_t b[8];
    for (int i = 0; i < 8; ++i) b[i] = static_cast<uint8_t>((v >> (8 * i)) & 0xFFu);
    out.write(reinterpret_cast<const char*>(b), 8);
    return static_cast<bool>(out);
}

bool readExact(std::ifstream& in, void* dst, size_t n) {
    in.read(static_cast<char*>(dst), static_cast<std::streamsize>(n));
    return static_cast<size_t>(in.gcount()) == n;
}

bool readU32LE(std::ifstream& in, uint32_t& out) {
    uint8_t b[4]{};
    if (!readExact(in, b, 4)) return false;
    out = static_cast<uint32_t>(b[0]) |
          (static_cast<uint32_t>(b[1]) << 8) |
          (static_cast<uint32_t>(b[2]) << 16) |
          (static_cast<uint32_t>(b[3]) << 24);
    return true;
}

bool readU64LE(std::ifstream& in, uint64_t& out) {
    uint8_t b[8]{};
    if (!readExact(in, b, 8)) return false;
    out = 0;
    for (int i = 0; i < 8; ++i) out |= (static_cast<uint64_t>(b[i]) << (8 * i));
    return true;
}

} // namespace

bool WriteMikPackFile(const PackEntry& entry, const std::string& stagingDirUtf8,
                      const uint8_t secretSeed[32], const std::string& dstFileUtf8,
                      std::string& errOut) {
    errOut.clear();
    if (!secretSeed) return fail(errOut, "mikpack: null secret seed");

    PackEntry checked;
    if (!packs::parsePackInfo(packs::toJson(entry), checked, errOut)) return false;

    const std::string entryJson = packs::toJson(entry).dump(2);
    if (entryJson.empty() || entryJson.size() > kMaxEntryBytes)
        return fail(errOut, "mikpack: entry.json exceeds 64 KiB");

    uint8_t sig[64]{};
    ed25519Sign(sig, reinterpret_cast<const uint8_t*>(entryJson.data()), entryJson.size(), secretSeed);

    const fs::path srcBase = fromUtf8(stagingDirUtf8);
    const fs::path dstPath = fromUtf8(dstFileUtf8);
    std::error_code ec;
    if (dstPath.has_parent_path()) fs::create_directories(dstPath.parent_path(), ec);

    std::ofstream out(dstPath, std::ios::binary | std::ios::trunc);
    if (!out) return fail(errOut, "mikpack: cannot open output file");

    auto abortOut = [&](const std::string& msg) -> bool {
        out.close();
        std::error_code rec;
        fs::remove(dstPath, rec);
        return fail(errOut, msg);
    };

    out.write(kMagic, 8);
    if (!writeU32LE(out, static_cast<uint32_t>(entryJson.size()))) return abortOut("mikpack: write error");
    out.write(entryJson.data(), static_cast<std::streamsize>(entryJson.size()));
    out.write(reinterpret_cast<const char*>(sig), 64);
    if (!writeU32LE(out, static_cast<uint32_t>(entry.files.size()))) return abortOut("mikpack: write error");

    std::vector<char> chunk(64 * 1024);
    for (const PackFile& fe : entry.files) {
        if (fe.path.size() > kMaxPathLen || !packs::isSafeRelPath(fe.path))
            return abortOut("mikpack: unsafe path '" + fe.path + "'");

        const fs::path fp = srcBase / fromUtf8(fe.path);
        if (!fs::is_regular_file(fp, ec))
            return abortOut("mikpack: missing source file '" + fe.path + "'");
        const uint64_t fsz = fs::file_size(fp, ec);
        if (ec || fsz != fe.size)
            return abortOut("mikpack: size mismatch on source file '" + fe.path + "'");

        if (!writeU32LE(out, static_cast<uint32_t>(fe.path.size()))) return abortOut("mikpack: write error");
        out.write(fe.path.data(), static_cast<std::streamsize>(fe.path.size()));
        if (!writeU64LE(out, fe.size)) return abortOut("mikpack: write error");

        std::ifstream in(fp, std::ios::binary);
        if (!in) return abortOut("mikpack: cannot read '" + fe.path + "'");
        uint64_t rem = fe.size;
        Sha256 hasher;
        while (rem > 0) {
            const size_t take = static_cast<size_t>(std::min<uint64_t>(rem, chunk.size()));
            if (!readExact(in, chunk.data(), take))
                return abortOut("mikpack: short read on '" + fe.path + "'");
            hasher.update(chunk.data(), take);
            out.write(chunk.data(), static_cast<std::streamsize>(take));
            if (!out) return abortOut("mikpack: write error");
            rem -= take;
        }
        const Sha256Digest digest = hasher.finish();
        if (toHex(digest.data(), digest.size()) != fe.sha256Hex)
            return abortOut("mikpack: sha256 mismatch on source file '" + fe.path + "'");
    }

    out.flush();
    if (!out) return abortOut("mikpack: flush error");
    return true;
}

bool InstallMikPackFile(const std::string& mikpackPathUtf8, const uint8_t publicKey[32],
                        PackStore& store, PackEntry& installedOut, std::string& errOut) {
    errOut.clear();
    installedOut = PackEntry{};
    if (!publicKey) return fail(errOut, "mikpack: null public key");

    std::ifstream in(fromUtf8(mikpackPathUtf8), std::ios::binary);
    if (!in) return fail(errOut, "mikpack: cannot open file");

    char magic[8]{};
    if (!readExact(in, magic, 8) || std::memcmp(magic, kMagic, 8) != 0)
        return fail(errOut, "mikpack: invalid magic header");

    uint32_t entryLen = 0;
    if (!readU32LE(in, entryLen) || entryLen == 0 || entryLen > kMaxEntryBytes)
        return fail(errOut, "mikpack: invalid entry.json length");

    std::string entryJson(entryLen, '\0');
    if (!readExact(in, entryJson.data(), entryLen))
        return fail(errOut, "mikpack: truncated entry.json");

    uint8_t sig[64]{};
    if (!readExact(in, sig, 64))
        return fail(errOut, "mikpack: truncated signature");

    if (!ed25519Verify(sig, reinterpret_cast<const uint8_t*>(entryJson.data()), entryJson.size(), publicKey))
        return fail(errOut, "mikpack: Ed25519 signature verification failed");

    JsonValue jv;
    std::string jerr;
    if (!JsonValue::parse(entryJson, jv, jerr))
        return fail(errOut, "mikpack: invalid entry.json: " + jerr);

    PackEntry entry;
    if (!packs::parsePackInfo(jv, entry, errOut)) return false;

    uint32_t fileCount = 0;
    if (!readU32LE(in, fileCount) || fileCount != entry.files.size())
        return fail(errOut, "mikpack: fileCount does not match manifest");

    const std::string stageDir = store.stagingDir(entry.id, entry.version);
    const fs::path stagePath   = fromUtf8(stageDir);
    auto cleanupStage = [&]() {
        std::error_code ec;
        fs::remove_all(stagePath, ec);
    };

    std::vector<char> chunk(64 * 1024);
    for (size_t i = 0; i < entry.files.size(); ++i) {
        const PackFile& expected = entry.files[i];
        uint32_t pathLen = 0;
        if (!readU32LE(in, pathLen) || pathLen == 0 || pathLen > kMaxPathLen) {
            cleanupStage();
            return fail(errOut, "mikpack: invalid path length");
        }
        std::string path(pathLen, '\0');
        if (!readExact(in, path.data(), pathLen)) {
            cleanupStage();
            return fail(errOut, "mikpack: truncated path");
        }
        if (path != expected.path || !packs::isSafeRelPath(path)) {
            cleanupStage();
            return fail(errOut, "mikpack: file path mismatch or unsafe path '" + path + "'");
        }

        uint64_t fsize = 0;
        if (!readU64LE(in, fsize) || fsize != expected.size || fsize > packs::kMaxFileBytes) {
            cleanupStage();
            return fail(errOut, "mikpack: file size mismatch for '" + path + "'");
        }

        const fs::path dstFile = stagePath / fromUtf8(path);
        std::error_code ec;
        if (dstFile.has_parent_path()) fs::create_directories(dstFile.parent_path(), ec);
        std::ofstream out(dstFile, std::ios::binary | std::ios::trunc);
        if (!out) {
            cleanupStage();
            return fail(errOut, "mikpack: cannot create staged file '" + path + "'");
        }

        Sha256 hasher;
        uint64_t rem = fsize;
        while (rem > 0) {
            const size_t take = static_cast<size_t>(std::min<uint64_t>(rem, chunk.size()));
            if (!readExact(in, chunk.data(), take)) {
                out.close();
                cleanupStage();
                return fail(errOut, "mikpack: truncated payload for '" + path + "'");
            }
            hasher.update(chunk.data(), take);
            out.write(chunk.data(), static_cast<std::streamsize>(take));
            if (!out) {
                out.close();
                cleanupStage();
                return fail(errOut, "mikpack: write error on '" + path + "'");
            }
            rem -= take;
        }
        out.flush();
        out.close();

        const Sha256Digest digest = hasher.finish();
        if (toHex(digest.data(), digest.size()) != expected.sha256Hex) {
            cleanupStage();
            return fail(errOut, "mikpack: sha256 mismatch for '" + path + "'");
        }
    }

    // Không cho phép dữ liệu thừa ở cuối tệp .mikpack
    if (in.peek() != std::char_traits<char>::eof()) {
        cleanupStage();
        return fail(errOut, "mikpack: unexpected trailing bytes");
    }

    if (!store.commit(entry, errOut)) {
        cleanupStage();
        return false;
    }

    installedOut = std::move(entry);
    return true;
}

} // namespace mikmap

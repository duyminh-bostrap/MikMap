// ════════════════════════════════════════════════════════════════════════
//  tools/packs/packtool.cpp — công cụ dòng lệnh tạo khoá, ký catalog, đóng gói .mikpack
//
//  Cách dùng:
//    mikmap_packtool keygen <secret.key> <public.pub>
//    mikmap_packtool sign   <secret.key> <input-file> <output.sig>
//    mikmap_packtool pack   <secret.key> <entry.json> <staging-dir> <out.mikpack>
// ════════════════════════════════════════════════════════════════════════

#include "core/util/Ed25519.h"
#include "core/util/Json.h"
#include "core/util/Sha2.h"
#include "io/packs/MikPack.h"
#include "io/packs/PackManifest.h"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <bcrypt.h>
#  if defined(_MSC_VER)
#    pragma comment(lib, "bcrypt.lib")
#  endif
#endif

namespace {

namespace fs = std::filesystem;

fs::path fromUtf8(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}

bool readFileBytes(const std::string& pathUtf8, std::string& out) {
    std::ifstream f(fromUtf8(pathUtf8), std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    if (!f && !f.eof()) return false;
    out = ss.str();
    return true;
}

bool writeFileBytes(const std::string& pathUtf8, const std::string& data) {
    const fs::path p = fromUtf8(pathUtf8);
    std::error_code ec;
    if (p.has_parent_path()) fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    if (!f) return false;
    f.write(data.data(), static_cast<std::streamsize>(data.size()));
    f.flush();
    return static_cast<bool>(f);
}

bool readSeedHex(const std::string& pathUtf8, uint8_t seed[32]) {
    std::string text;
    if (!readFileBytes(pathUtf8, text)) return false;
    while (!text.empty() &&
           (text.back() == '\r' || text.back() == '\n' || text.back() == ' ' || text.back() == '\t')) {
        text.pop_back();
    }
    return mikmap::fromHex(text, seed, 32);
}

bool randomBytes32(uint8_t out[32]) {
#if defined(_WIN32)
    return ::BCryptGenRandom(nullptr, out, 32, BCRYPT_USE_SYSTEM_PREFERRED_RNG) == 0;
#else
    std::ifstream urandom("/dev/urandom", std::ios::binary);
    if (!urandom) return false;
    urandom.read(reinterpret_cast<char*>(out), 32);
    return urandom.gcount() == 32;
#endif
}

int usage() {
    std::fprintf(stderr,
        "Usage:\n"
        "  mikmap_packtool keygen <secret.key> <public.pub>\n"
        "  mikmap_packtool sign   <secret.key> <input-file> <output.sig>\n"
        "  mikmap_packtool pack   <secret.key> <entry.json> <staging-dir> <out.mikpack>\n");
    return 1;
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) return usage();
    const std::string cmd = argv[1];

    if (cmd == "keygen") {
        if (argc != 4) return usage();
        uint8_t seed[32]{}, pub[32]{};
        if (!randomBytes32(seed)) {
            std::fprintf(stderr, "packtool: RNG failure\n");
            return 2;
        }
        mikmap::ed25519PublicFromSeed(seed, pub);
        if (!writeFileBytes(argv[2], mikmap::toHex(seed, 32) + "\n") ||
            !writeFileBytes(argv[3], mikmap::toHex(pub, 32) + "\n")) {
            std::fprintf(stderr, "packtool: failed writing key files\n");
            return 2;
        }
        return 0;
    }

    if (cmd == "sign") {
        if (argc != 5) return usage();
        uint8_t seed[32]{};
        if (!readSeedHex(argv[2], seed)) {
            std::fprintf(stderr, "packtool: invalid secret key '%s'\n", argv[2]);
            return 2;
        }
        std::string msg;
        if (!readFileBytes(argv[3], msg)) {
            std::fprintf(stderr, "packtool: cannot read input '%s'\n", argv[3]);
            return 2;
        }
        uint8_t sig[64]{};
        mikmap::ed25519Sign(sig, reinterpret_cast<const uint8_t*>(msg.data()), msg.size(), seed);
        if (!writeFileBytes(argv[4], mikmap::toHex(sig, 64) + "\n")) {
            std::fprintf(stderr, "packtool: cannot write signature '%s'\n", argv[4]);
            return 2;
        }
        return 0;
    }

    if (cmd == "pack") {
        if (argc != 6) return usage();
        uint8_t seed[32]{};
        if (!readSeedHex(argv[2], seed)) {
            std::fprintf(stderr, "packtool: invalid secret key '%s'\n", argv[2]);
            return 2;
        }
        std::string entryText;
        if (!readFileBytes(argv[3], entryText)) {
            std::fprintf(stderr, "packtool: cannot read entry.json '%s'\n", argv[3]);
            return 2;
        }
        mikmap::JsonValue jv;
        std::string err;
        if (!mikmap::JsonValue::parse(entryText, jv, err)) {
            std::fprintf(stderr, "packtool: invalid JSON in '%s': %s\n", argv[3], err.c_str());
            return 2;
        }
        mikmap::PackEntry entry;
        if (!mikmap::packs::parsePackInfo(jv, entry, err)) {
            std::fprintf(stderr, "packtool: invalid pack manifest: %s\n", err.c_str());
            return 2;
        }
        if (!mikmap::WriteMikPackFile(entry, argv[4], seed, argv[5], err)) {
            std::fprintf(stderr, "packtool: pack failed: %s\n", err.c_str());
            return 2;
        }
        return 0;
    }

    return usage();
}

#include "TestHarness.h"

#include "core/util/Sha2.h"

#include <atomic>
#include <cstdio>
#include <algorithm>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

using namespace mikmap;

namespace {

std::string hex256(const std::string& s) {
    const Sha256Digest d = sha256(s.data(), s.size());
    return toHex(d.data(), d.size());
}

std::string hex512(const std::string& s) {
    const Sha512Digest d = sha512(s.data(), s.size());
    return toHex(d.data(), d.size());
}

const char* kMsg448 = "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  FIPS 180-4 / NIST CSRC example vectors
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("Sha256: FIPS 180-4 vectors") {
    CHECK(hex256("") == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    CHECK(hex256("abc") == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    CHECK(hex256(kMsg448) == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
}

TEST_CASE("Sha256: one million 'a' fed in 1000-byte chunks") {
    const std::string chunk(1000, 'a');
    Sha256 h;
    for (int i = 0; i < 1000; ++i) h.update(chunk.data(), chunk.size());
    const Sha256Digest d = h.finish();
    CHECK(toHex(d.data(), d.size()) ==
          "cdc76e5c9914fb9281a1c7e284d73e67f1809a48a497200e046d39ccc7112cd0");
}

TEST_CASE("Sha512: FIPS 180-4 vectors") {
    CHECK(hex512("abc") ==
          "ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a"
          "2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f");
    CHECK(hex512("") ==
          "cf83e1357eefb8bdf1542850d66d8007d620e4050b5715dc83f4a921d36ce9ce"
          "47d0d13c5d85f2b0ff8318d2877eec2f63b931bd47417a81a538327af927da3e");
    CHECK(hex512("abcdefghbcdefghicdefghijdefghijkefghijklfghijklmghijklmnhijklmnoijklmnop"
                 "jklmnopqklmnopqrlmnopqrsmnopqrstnopqrstu") ==
          "8e959b75dae313da8cf4f72814fc143f8f7779c6eb9f7fa17299aeadb6889018"
          "501d289e4900f7e4331b99dec4b5433ac7d329eeb6dd26545e96e55b874be909");
}

TEST_CASE("Sha512: one million 'a' fed in 1000-byte chunks") {
    const std::string chunk(1000, 'a');
    Sha512 h;
    for (int i = 0; i < 1000; ++i) h.update(chunk.data(), chunk.size());
    const Sha512Digest d = h.finish();
    CHECK(toHex(d.data(), d.size()) ==
          "e718483d0ce769644e2e42c7bc15b4638e1f98b13b2044285632a803afa973eb"
          "de0ff244877ea60a4cb0432ce577c31beb009c5c2c49aa2e4eadb217ad8cc09b");
}

// Padding boundaries: last length that fits one block, one short of a block, exactly a block.
// Expected values from coreutils: head -c N /dev/zero | tr '\0' 'a' | sha256sum / sha512sum
TEST_CASE("Sha2: 'a' x n at the padding boundaries") {
    CHECK(hex256(std::string(55, 'a')) == "9f4390f8d30c2dd92ec9f095b65e2b9ae9b0a925a5258e241c9f1e910f734318");
    CHECK(hex256(std::string(63, 'a')) == "7d3e74a05d7db15bce4ad9ec0658ea98e3f06eeecf16b4c6fff2da457ddc2f34");
    CHECK(hex256(std::string(64, 'a')) == "ffe054fe7ae0cb6dc65c3af9b61d5209f439851db43d0ba5997337df154668eb");
    CHECK(hex512(std::string(111, 'a')) ==
          "fa9121c7b32b9e01733d034cfc78cbf67f926c7ed83e82200ef86818196921760b4beff48404df811b953828274461673c68d04e297b0eb7b2b4d60fc6b566a2");
    CHECK(hex512(std::string(127, 'a')) ==
          "828613968b501dc00a97e08c73b118aa8876c26b8aac93df128502ab360f91bab50a51e088769a5c1eff4782ace147dce3642554199876374291f5d921629502");
    CHECK(hex512(std::string(128, 'a')) ==
          "b73d1929aa615934e61a871596b3f3b33359f42b8175602e89f7e06e5f658a243667807ed300314b95cacdd579f3e33abdfbe351909519a846d465c59582f321");
}

TEST_CASE("Sha2: update(nullptr, 0) is a no-op; finish() resets so the object can be reused") {
    Sha256 a;
    a.update(nullptr, 0);
    a.update("ab", 2);
    a.update(nullptr, 0);
    a.update("c", 1);
    CHECK(a.finish() == sha256("abc", 3));
    CHECK(a.finish() == sha256("", 0));      // second finish = fresh empty hash
    a.update("abc", 3);
    CHECK(a.finish() == sha256("abc", 3));   // reusable after finish

    Sha512 b;
    b.update(nullptr, 0);
    b.update("abc", 3);
    CHECK(b.finish() == sha512("abc", 3));
    CHECK(b.finish() == sha512("", 0));
    b.update("abc", 3);
    CHECK(b.finish() == sha512("abc", 3));
}

TEST_CASE("Sha2: streaming equals one-shot for chunk sizes 1, 63, 64, 65") {
    std::string msg;
    for (int i = 0; i < 1000; ++i) msg.push_back(static_cast<char>(i * 31 + 7));
    const Sha256Digest one256 = sha256(msg.data(), msg.size());
    const Sha512Digest one512 = sha512(msg.data(), msg.size());
    for (size_t step : {size_t(1), size_t(63), size_t(64), size_t(65), size_t(127), size_t(128), size_t(129)}) {
        Sha256 a;
        Sha512 b;
        for (size_t off = 0; off < msg.size(); off += step) {
            const size_t n = std::min(step, msg.size() - off);
            a.update(msg.data() + off, n);
            b.update(msg.data() + off, n);
        }
        CHECK(a.finish() == one256);
        CHECK(b.finish() == one512);
    }
}

TEST_CASE("Sha2: toHex / fromHex round trip and rejects bad input") {
    const uint8_t src[4] = {0x00, 0xab, 0x7f, 0xff};
    const std::string h = toHex(src, 4);
    CHECK(h == "00ab7fff");
    uint8_t back[4] = {};
    REQUIRE(fromHex(h, back, 4));
    CHECK(std::memcmp(src, back, 4) == 0);
    CHECK(fromHex("00AB7FFF", back, 4));       // uppercase is fine
    CHECK(!fromHex("00ab7ff", back, 4));       // odd length
    CHECK(!fromHex("00ab7f", back, 4));        // too short for n
    CHECK(!fromHex("00ab7fffee", back, 4));    // too long for n
    CHECK(!fromHex("00ab7fzz", back, 4));      // non-hex
}

TEST_CASE("Sha256: sha256File of a ~200 KB file (Vietnamese path) equals one-shot; cancel returns false") {
    std::string data(200 * 1024 + 17, '\0');
    for (size_t i = 0; i < data.size(); ++i) data[i] = static_cast<char>((i * 2654435761u) >> 24);

    const std::filesystem::path fsPath =
        std::filesystem::temp_directory_path() / std::filesystem::path(u8"mikmap_test_sha2_tệp_thử.bin");
    const std::u8string u8 = fsPath.u8string();
    const std::string path(u8.begin(), u8.end());
    {
        std::ofstream f(fsPath, std::ios::binary | std::ios::trunc);
        f.write(data.data(), static_cast<std::streamsize>(data.size()));
        REQUIRE(f.good());
    }

    Sha256Digest d{};
    CHECK(sha256File(path, d));
    CHECK(d == sha256(data.data(), data.size()));

    std::atomic<bool> cancel{true};
    Sha256Digest d2{};
    CHECK(!sha256File(path, d2, &cancel));

    CHECK(!sha256File("mikmap_test_sha2_does_not_exist.bin", d2));

    std::error_code ec;
    std::filesystem::remove(fsPath, ec);
}

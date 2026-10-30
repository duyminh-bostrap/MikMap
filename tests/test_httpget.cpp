#include "TestHarness.h"

#include "FakeHttpServer.h"
#include "io/packs/HttpGet.h"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <thread>

using namespace mikmap;
using namespace mikmap::test;

namespace {

namespace fs = std::filesystem;

fs::path fromUtf8(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}

std::string toUtf8(const fs::path& p) {
    const std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

std::string readFile(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

} // namespace

TEST_CASE("httpget: downloads 200 KB to file and reports progress") {
    FakeHttpServer srv;
    REQUIRE(srv.port() != 0);

    std::string payload(200 * 1024, '\0');
    for (size_t i = 0; i < payload.size(); ++i) payload[i] = static_cast<char>((i * 131 + 17) & 0xFF);
    srv.addRoute("/blob.bin", payload, 200);

    const fs::path dst = fs::temp_directory_path() / "mikmap_test_httpget_200k.bin";
    std::error_code ec;
    fs::remove(dst, ec);

    HttpOptions opts;
    opts.allowLoopbackHttp = true;

    int calls = 0;
    uint64_t lastDone = 0, lastTotal = 0;
    std::string err;
    REQUIRE(HttpGetToFile(srv.url("/blob.bin"), toUtf8(dst), opts,
                          [&](uint64_t done, uint64_t total) {
                              ++calls;
                              lastDone  = done;
                              lastTotal = total;
                          },
                          err));
    CHECK(calls >= 2);
    CHECK(lastDone == payload.size());
    CHECK(lastTotal == payload.size());
    CHECK(readFile(dst) == payload);
    fs::remove(dst, ec);
}

TEST_CASE("httpget: 404 returns false, mentions 404, and removes partial file") {
    FakeHttpServer srv;
    REQUIRE(srv.port() != 0);

    const fs::path dst = fs::temp_directory_path() / "mikmap_test_httpget_404.bin";
    std::error_code ec;
    fs::remove(dst, ec);

    HttpOptions opts;
    opts.allowLoopbackHttp = true;
    std::string err;
    CHECK(!HttpGetToFile(srv.url("/missing.bin"), toUtf8(dst), opts, nullptr, err));
    CHECK(err.find("404") != std::string::npos);
    CHECK(!fs::exists(dst, ec));
}

TEST_CASE("httpget: cancel stops slow download and removes partial file") {
    FakeHttpServer srv;
    REQUIRE(srv.port() != 0);

    FakeHttpServer::Response slow;
    slow.status       = 200;
    slow.body         = std::string(128 * 1024, 'z');
    slow.chunkSize    = 4096;
    slow.chunkDelayMs = 30;
    srv.addRoute("/slow.bin", slow);

    const fs::path dst = fs::temp_directory_path() / "mikmap_test_httpget_cancel.bin";
    std::error_code ec;
    fs::remove(dst, ec);

    std::atomic<bool> cancel{false};
    HttpOptions opts;
    opts.allowLoopbackHttp = true;
    opts.cancel            = &cancel;

    std::thread canceller([&]() {
        std::this_thread::sleep_for(std::chrono::milliseconds(70));
        cancel.store(true);
    });

    const auto t0 = std::chrono::steady_clock::now();
    std::string err;
    const bool ok = HttpGetToFile(srv.url("/slow.bin"), toUtf8(dst), opts, nullptr, err);
    const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now() - t0).count();
    canceller.join();

    CHECK(!ok);
    CHECK(elapsedMs < 500);
    CHECK(!fs::exists(dst, ec));
}

TEST_CASE("httpget: strict URL policy rejects non-https and non-loopback http") {
    FakeHttpServer srv;
    REQUIRE(srv.port() != 0);
    srv.addRoute("/a", "hello", 200);

    const fs::path dst = fs::temp_directory_path() / "mikmap_test_httpget_policy.bin";
    std::error_code ec;
    fs::remove(dst, ec);

    // 1. allowLoopbackHttp = false (mặc định) -> từ chối http://127.0.0.1 ngay trước khi mở kết nối
    HttpOptions strictOpts;
    std::string err;
    CHECK(!HttpGetToFile(srv.url("/a"), toUtf8(dst), strictOpts, nullptr, err));
    CHECK(err.find("https") != std::string::npos);
    CHECK(srv.hits("/a") == 0);

    // 2. allowLoopbackHttp = true nhưng host không phải loopback -> vẫn từ chối
    HttpOptions loopOpts;
    loopOpts.allowLoopbackHttp = true;
    CHECK(!HttpGetToFile("http://example.com/a", toUtf8(dst), loopOpts, nullptr, err));
    CHECK(!err.empty());
    CHECK(!fs::exists(dst, ec));

    // 3. Redirect từ loopback sang http://example.com -> bị chặn
    FakeHttpServer::Response redir;
    redir.status     = 302;
    redir.redirectTo = "http://example.com/evil.dll";
    srv.addRoute("/redir", redir);
    CHECK(!HttpGetToFile(srv.url("/redir"), toUtf8(dst), loopOpts, nullptr, err));
    CHECK(!fs::exists(dst, ec));
}

TEST_CASE("httpget: maxBytes limit aborts and deletes partial file") {
    FakeHttpServer srv;
    REQUIRE(srv.port() != 0);
    srv.addRoute("/big.bin", std::string(4096, 'B'), 200);

    const fs::path dst = fs::temp_directory_path() / "mikmap_test_httpget_maxbytes.bin";
    std::error_code ec;
    fs::remove(dst, ec);

    HttpOptions opts;
    opts.allowLoopbackHttp = true;
    opts.maxBytes          = 1024;
    std::string err;
    CHECK(!HttpGetToFile(srv.url("/big.bin"), toUtf8(dst), opts, nullptr, err));
    CHECK(!err.empty());
    CHECK(!fs::exists(dst, ec));
}

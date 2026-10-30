#include "io/packs/HttpGet.h"

#include <algorithm>
#include <cwctype>
#include <filesystem>
#include <fstream>
#include <vector>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <winhttp.h>
#  if defined(_MSC_VER)
#    pragma comment(lib, "winhttp.lib")
#  endif
#endif

namespace mikmap {

namespace {

namespace fs = std::filesystem;

fs::path fromUtf8(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}

bool fail(std::string& err, const std::string& msg) {
    err = msg;
    return false;
}

#if defined(_WIN32)

bool utf8ToWide(const std::string& s, std::wstring& out) {
    out.clear();
    if (s.empty()) return true;
    const int n = ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                        s.data(), static_cast<int>(s.size()), nullptr, 0);
    if (n <= 0) return false;
    out.resize(static_cast<size_t>(n));
    return ::MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS,
                                 s.data(), static_cast<int>(s.size()), out.data(), n) == n;
}

struct CrackedUrl {
    INTERNET_SCHEME scheme = 0;
    std::wstring    host;
    INTERNET_PORT   port   = 0;
    std::wstring    pathAndQuery;
};

bool crackAndValidateUrl(const std::wstring& wurl, bool allowLoopbackHttp,
                         CrackedUrl& out, std::string& errOut) {
    if (wurl.empty() || wurl.size() > 2048) return fail(errOut, "httpget: invalid URL length");
    for (wchar_t c : wurl) {
        if (c <= 0x20 || c >= 0x7F) return fail(errOut, "httpget: invalid character in URL");
    }

    URL_COMPONENTS uc{};
    uc.dwStructSize      = sizeof(uc);
    uc.dwSchemeLength    = static_cast<DWORD>(-1);
    uc.dwHostNameLength  = static_cast<DWORD>(-1);
    uc.dwUserNameLength  = static_cast<DWORD>(-1);
    uc.dwPasswordLength  = static_cast<DWORD>(-1);
    uc.dwUrlPathLength   = static_cast<DWORD>(-1);
    uc.dwExtraInfoLength = static_cast<DWORD>(-1);

    if (!::WinHttpCrackUrl(wurl.c_str(), static_cast<DWORD>(wurl.size()), 0, &uc))
        return fail(errOut, "httpget: malformed URL");

    if (uc.dwUserNameLength > 0 || uc.dwPasswordLength > 0)
        return fail(errOut, "httpget: userinfo in URL is not allowed");

    if (!uc.lpszHostName || uc.dwHostNameLength == 0)
        return fail(errOut, "httpget: empty host in URL");

    out.scheme = uc.nScheme;
    out.host.assign(uc.lpszHostName, uc.dwHostNameLength);
    out.port = uc.nPort;
    out.pathAndQuery.clear();
    if (uc.lpszUrlPath && uc.dwUrlPathLength > 0)
        out.pathAndQuery.append(uc.lpszUrlPath, uc.dwUrlPathLength);
    if (out.pathAndQuery.empty()) out.pathAndQuery = L"/";
    if (uc.lpszExtraInfo && uc.dwExtraInfoLength > 0)
        out.pathAndQuery.append(uc.lpszExtraInfo, uc.dwExtraInfoLength);

    if (out.scheme == INTERNET_SCHEME_HTTPS) return true;

    if (out.scheme == INTERNET_SCHEME_HTTP) {
        if (!allowLoopbackHttp)
            return fail(errOut, "httpget: only https:// URLs are allowed");
        std::wstring h = out.host;
        for (wchar_t& c : h) c = static_cast<wchar_t>(std::towlower(c));
        if (h == L"127.0.0.1" || h == L"localhost") return true;
        return fail(errOut, "httpget: http:// is only allowed for 127.0.0.1 or localhost in tests");
    }

    return fail(errOut, "httpget: unsupported URL scheme (only https:// is allowed)");
}

struct WinHttpHandle {
    HINTERNET h = nullptr;
    explicit WinHttpHandle(HINTERNET handle = nullptr) : h(handle) {}
    ~WinHttpHandle() { if (h) ::WinHttpCloseHandle(h); }
    WinHttpHandle(const WinHttpHandle&) = delete;
    WinHttpHandle& operator=(const WinHttpHandle&) = delete;
    operator HINTERNET() const { return h; }
};

struct RedirectContext {
    bool allowLoopbackHttp = false;
    bool blocked           = false;
};

void CALLBACK redirectCallback(HINTERNET hInternet, DWORD_PTR dwContext, DWORD dwInternetStatus,
                               LPVOID lpvStatusInformation, DWORD dwStatusInformationLength) {
    if (dwInternetStatus != WINHTTP_CALLBACK_STATUS_REDIRECT || !dwContext || !lpvStatusInformation)
        return;
    auto* ctx = reinterpret_cast<RedirectContext*>(dwContext);
    const auto* wtarget = static_cast<const wchar_t*>(lpvStatusInformation);
    size_t len = dwStatusInformationLength;
    if (len > 0 && wtarget[len - 1] == L'\0') --len;
    const std::wstring targetUrl(wtarget, len);
    CrackedUrl dummy;
    std::string dummyErr;
    if (!crackAndValidateUrl(targetUrl, ctx->allowLoopbackHttp, dummy, dummyErr)) {
        ctx->blocked = true;
        ::WinHttpCloseHandle(hInternet);
    }
}

template <class SinkFn>
bool httpGetImpl(const std::string& url, const HttpOptions& opts,
                 HttpProgressFn progress, SinkFn&& sink, std::string& errOut) {
    errOut.clear();
    if (opts.cancel && opts.cancel->load()) return fail(errOut, "httpget: cancelled");

    std::wstring wurl;
    if (!utf8ToWide(url, wurl)) return fail(errOut, "httpget: URL is not valid UTF-8");

    CrackedUrl cracked;
    if (!crackAndValidateUrl(wurl, opts.allowLoopbackHttp, cracked, errOut)) return false;

    WinHttpHandle hSession(::WinHttpOpen(L"MikMap-PackInstaller/1.0",
                                         WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
                                         WINHTTP_NO_PROXY_NAME,
                                         WINHTTP_NO_PROXY_BYPASS, 0));
    if (!hSession) return fail(errOut, "httpget: WinHttpOpen failed");

    ::WinHttpSetTimeouts(hSession,
                         opts.connectTimeoutMs,
                         opts.connectTimeoutMs,
                         opts.receiveTimeoutMs,
                         opts.receiveTimeoutMs);

    WinHttpHandle hConnect(::WinHttpConnect(hSession, cracked.host.c_str(), cracked.port, 0));
    if (!hConnect) return fail(errOut, "httpget: WinHttpConnect failed");

    const DWORD flags = (cracked.scheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    WinHttpHandle hRequest(::WinHttpOpenRequest(hConnect, L"GET", cracked.pathAndQuery.c_str(),
                                                nullptr, WINHTTP_NO_REFERER,
                                                WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!hRequest) return fail(errOut, "httpget: WinHttpOpenRequest failed");

    RedirectContext redirCtx{opts.allowLoopbackHttp, false};
    ::WinHttpSetStatusCallback(hRequest, &redirectCallback,
                               WINHTTP_CALLBACK_FLAG_REDIRECT, 0);

    if (!::WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                              WINHTTP_NO_REQUEST_DATA, 0, 0,
                              reinterpret_cast<DWORD_PTR>(&redirCtx))) {
        if (redirCtx.blocked) return fail(errOut, "httpget: redirect blocked by URL policy");
        return fail(errOut, "httpget: WinHttpSendRequest failed");
    }

    if (!::WinHttpReceiveResponse(hRequest, nullptr)) {
        if (redirCtx.blocked) return fail(errOut, "httpget: redirect blocked by URL policy");
        return fail(errOut, "httpget: WinHttpReceiveResponse failed");
    }
    if (redirCtx.blocked) return fail(errOut, "httpget: redirect blocked by URL policy");

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    if (!::WinHttpQueryHeaders(hRequest,
                               WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                               WINHTTP_HEADER_NAME_BY_INDEX,
                               &statusCode, &statusSize, WINHTTP_NO_HEADER_INDEX)) {
        return fail(errOut, "httpget: cannot read HTTP status code");
    }
    if (statusCode != 200) {
        return fail(errOut, "httpget: HTTP status " + std::to_string(statusCode));
    }

    uint64_t totalBytes = 0;
    wchar_t clBuf[64] = {};
    DWORD clBytes = sizeof(clBuf);
    if (::WinHttpQueryHeaders(hRequest, WINHTTP_QUERY_CONTENT_LENGTH,
                              WINHTTP_HEADER_NAME_BY_INDEX,
                              clBuf, &clBytes, WINHTTP_NO_HEADER_INDEX)) {
        totalBytes = std::wcstoull(clBuf, nullptr, 10);
        if (totalBytes > opts.maxBytes) {
            return fail(errOut, "httpget: Content-Length exceeds maxBytes");
        }
    }

    std::vector<char> chunk(64 * 1024);
    uint64_t doneBytes = 0;
    while (true) {
        if (opts.cancel && opts.cancel->load()) {
            return fail(errOut, "httpget: cancelled");
        }
        DWORD got = 0;
        if (!::WinHttpReadData(hRequest, chunk.data(), static_cast<DWORD>(chunk.size()), &got)) {
            return fail(errOut, "httpget: WinHttpReadData failed");
        }
        if (got == 0) break;
        doneBytes += got;
        if (doneBytes > opts.maxBytes) {
            return fail(errOut, "httpget: response exceeds maxBytes");
        }
        if (opts.cancel && opts.cancel->load()) {
            return fail(errOut, "httpget: cancelled");
        }
        if (!sink(chunk.data(), static_cast<size_t>(got), errOut)) {
            return false;
        }
        if (progress) {
            progress(doneBytes, totalBytes);
        }
    }

    if (totalBytes > 0 && doneBytes != totalBytes) {
        return fail(errOut, "httpget: truncated response body");
    }
    return true;
}

#endif // _WIN32

} // namespace

bool HttpGetToFile(const std::string& url, const std::string& dstFileUtf8,
                   const HttpOptions& opts, HttpProgressFn progress, std::string& errOut) {
#if defined(_WIN32)
    const fs::path dstPath = fromUtf8(dstFileUtf8);
    std::ofstream out;
    bool opened = false;

    auto openIfNeeded = [&]() -> bool {
        if (opened) return true;
        std::error_code ec;
        if (dstPath.has_parent_path()) fs::create_directories(dstPath.parent_path(), ec);
        out.open(dstPath, std::ios::binary | std::ios::trunc);
        if (!out) return fail(errOut, "httpget: cannot open destination file");
        opened = true;
        return true;
    };

    const bool ok = httpGetImpl(
        url, opts, std::move(progress),
        [&](const char* data, size_t n, std::string& sinkErr) -> bool {
            if (!openIfNeeded()) return false;
            out.write(data, static_cast<std::streamsize>(n));
            if (!out) return fail(sinkErr, "httpget: write error");
            return true;
        },
        errOut);

    if (ok && !opened) {
        // Phản hồi 200 nhưng 0 byte: vẫn tạo tệp rỗng
        if (!openIfNeeded()) return false;
    }
    if (opened) {
        out.flush();
        const bool good = static_cast<bool>(out);
        out.close();
        if (!ok || !good) {
            std::error_code ec;
            fs::remove(dstPath, ec);
            if (ok && !good) return fail(errOut, "httpget: flush error");
            return false;
        }
    }
    return ok;
#else
    (void)url; (void)dstFileUtf8; (void)opts; (void)progress;
    return fail(errOut, "httpget: only implemented on Windows");
#endif
}

bool HttpGetToString(const std::string& url, std::string& bodyOut,
                     const HttpOptions& opts, std::string& errOut) {
    bodyOut.clear();
#if defined(_WIN32)
    const bool ok = httpGetImpl(
        url, opts, nullptr,
        [&](const char* data, size_t n, std::string&) -> bool {
            bodyOut.append(data, n);
            return true;
        },
        errOut);
    if (!ok) bodyOut.clear();
    return ok;
#else
    (void)url; (void)opts;
    return fail(errOut, "httpget: only implemented on Windows");
#endif
}

} // namespace mikmap

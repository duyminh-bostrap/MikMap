// ════════════════════════════════════════════════════════════════════════
//  io/net/Socket.h — kiểu socket + helper dùng chung cho mọi nguồn mạng
//
//  Trước đây khối #if _WIN32 / POSIX nằm trong namespace ẩn danh của
//  OscSource.cpp. Hokuyo (TCP) cần đúng những thứ đó, nên tách ra đây để
//  chỉ còn MỘT bản ifdef trong toàn bộ engine/io.
//
//  Mọi hàm đều inline (header-only) để không phải thêm file .cpp và để
//  test có thể dựng server giả bằng cùng bộ helper.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <cerrno>
#include <cstdint>
#include <cstring>
#include <string>

#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#  if defined(_MSC_VER)
#    pragma comment(lib, "ws2_32.lib")
#  endif
using SocketHandle = SOCKET;
static constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
#  include <fcntl.h>
#  include <netinet/in.h>
#  include <sys/select.h>
#  include <sys/socket.h>
#  include <unistd.h>
using SocketHandle = int;
static constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace mikmap {

inline void closeSocketHandle(SocketHandle s) {
    if (s == kInvalidSocket) return;
#if defined(_WIN32)
    ::closesocket(s);
#else
    ::close(s);
#endif
}

inline void shutdownSocket(SocketHandle s) {
    if (s == kInvalidSocket) return;
#if defined(_WIN32)
    ::shutdown(s, SD_BOTH);
#else
    ::shutdown(s, SHUT_RDWR);
#endif
}

#if defined(_WIN32)
/// Winsock cần khởi tạo trước khi dùng và chỉ nên làm một lần cho cả
/// tiến trình. Hàm static cục bộ trong inline function ⇒ một bản duy nhất
/// cho mọi translation unit (chuẩn C++ bảo đảm).
inline bool ensureWinsock() {
    struct Init {
        bool ok = false;
        Init() {
            WSADATA d{};
            ok = (::WSAStartup(MAKEWORD(2, 2), &d) == 0);
        }
        ~Init() { if (ok) ::WSACleanup(); }
    };
    static Init init;
    return init.ok;
}
#else
inline bool ensureWinsock() { return true; }
#endif

inline bool setNonBlocking(SocketHandle s, bool on) {
#if defined(_WIN32)
    u_long mode = on ? 1ul : 0ul;
    return ::ioctlsocket(s, FIONBIO, &mode) == 0;
#else
    const int fl = ::fcntl(s, F_GETFL, 0);
    if (fl < 0) return false;
    return ::fcntl(s, F_SETFL, on ? (fl | O_NONBLOCK) : (fl & ~O_NONBLOCK)) == 0;
#endif
}

inline bool setRecvTimeoutMs(SocketHandle s, int ms) {
#if defined(_WIN32)
    const DWORD t = static_cast<DWORD>(ms);
    return ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO,
                        reinterpret_cast<const char*>(&t), sizeof(t)) == 0;
#else
    timeval tv{};
    tv.tv_sec = ms / 1000;
    tv.tv_usec = (ms % 1000) * 1000;
    return ::setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv)) == 0;
#endif
}

inline std::string lastNetError() {
#if defined(_WIN32)
    return "winsock error " + std::to_string(static_cast<int>(::WSAGetLastError()));
#else
    return std::strerror(errno);
#endif
}

inline bool sendAll(SocketHandle s, const char* data, size_t n) {
    size_t sent = 0;
    while (sent < n) {
        const auto rc = ::send(s, data + sent, static_cast<int>(n - sent), 0);
        if (rc <= 0) return false;
        sent += static_cast<size_t>(rc);
    }
    return true;
}

enum class RecvResult { Data, Timeout, Closed };

/// recv một lần. Timeout (SO_RCVTIMEO) KHÔNG phải lỗi: vòng lặp phải tỉnh
/// dậy định kỳ để thấy cờ dừng, đúng như OscSource làm với recvfrom.
inline RecvResult recvSome(SocketHandle s, char* buf, int cap, int& n) {
    n = static_cast<int>(::recv(s, buf, cap, 0));
    if (n > 0) return RecvResult::Data;
    if (n == 0) return RecvResult::Closed;

#if defined(_WIN32)
    const int e = ::WSAGetLastError();
    if (e == WSAETIMEDOUT || e == WSAEWOULDBLOCK) return RecvResult::Timeout;
#else
    if (errno == EAGAIN || errno == EWOULDBLOCK || errno == EINTR) return RecvResult::Timeout;
#endif
    return RecvResult::Closed;
}

/// Kết nối TCP có timeout. Trả kInvalidSocket và đặt err khi hỏng.
inline SocketHandle tcpConnect(const std::string& host, uint16_t port,
                               int timeoutMs, std::string& err) {
    err.clear();
    if (!ensureWinsock()) {
        err = "Winsock init failed";
        return kInvalidSocket;
    }

    addrinfo hints{};
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;

    addrinfo* res = nullptr;
    const std::string portStr = std::to_string(port);
    if (::getaddrinfo(host.c_str(), portStr.c_str(), &hints, &res) != 0 || !res) {
        err = "cannot resolve " + host;
        return kInvalidSocket;
    }

    SocketHandle s = kInvalidSocket;
    std::string last;

    for (addrinfo* ai = res; ai; ai = ai->ai_next) {
        s = ::socket(ai->ai_family, ai->ai_socktype, ai->ai_protocol);
        if (s == kInvalidSocket) continue;

        if (!setNonBlocking(s, true)) {
            last = "non-blocking failed";
            closeSocketHandle(s);
            s = kInvalidSocket;
            continue;
        }

        const int rc = ::connect(s, ai->ai_addr, static_cast<int>(ai->ai_addrlen));

        bool inProgress = false;
        if (rc != 0) {
#if defined(_WIN32)
            const int e = ::WSAGetLastError();
            inProgress = (e == WSAEWOULDBLOCK || e == WSAEINPROGRESS);
#else
            inProgress = (errno == EINPROGRESS);
#endif
        }

        if (rc != 0 && !inProgress) {
            last = "connect: " + lastNetError();
            closeSocketHandle(s);
            s = kInvalidSocket;
            continue;
        }

        if (inProgress) {
            fd_set wfds, efds;
            FD_ZERO(&wfds);
            FD_ZERO(&efds);
            FD_SET(s, &wfds);
            FD_SET(s, &efds);

            timeval tv{};
            tv.tv_sec = timeoutMs / 1000;
            tv.tv_usec = (timeoutMs % 1000) * 1000;

            const int sel = ::select(static_cast<int>(s) + 1, nullptr, &wfds, &efds, &tv);
            if (sel <= 0) {
                last = (sel == 0) ? "connect timed out" : "select failed";
                closeSocketHandle(s);
                s = kInvalidSocket;
                continue;
            }

            int soErr = 0;
#if defined(_WIN32)
            int len = sizeof(soErr);
            const int got = ::getsockopt(s, SOL_SOCKET, SO_ERROR,
                                         reinterpret_cast<char*>(&soErr), &len);
#else
            socklen_t len = sizeof(soErr);
            const int got = ::getsockopt(s, SOL_SOCKET, SO_ERROR, &soErr, &len);
#endif
            if (got != 0 || soErr != 0) {
                last = "connect: " + lastNetError();
                closeSocketHandle(s);
                s = kInvalidSocket;
                continue;
            }
        }

        setNonBlocking(s, false);
        ::freeaddrinfo(res);
        return s;
    }

    ::freeaddrinfo(res);
    err = last.empty() ? "connect failed" : last;
    return kInvalidSocket;
}

/// Server giả cho test: chỉ nghe trên loopback, port 0 ⇒ OS cấp.
inline SocketHandle tcpListenLoopback(uint16_t port, uint16_t& boundPort, std::string& err) {
    err.clear();
    boundPort = 0;
    if (!ensureWinsock()) {
        err = "Winsock init failed";
        return kInvalidSocket;
    }

    const SocketHandle s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == kInvalidSocket) {
        err = "socket: " + lastNetError();
        return kInvalidSocket;
    }

    int yes = 1;
    ::setsockopt(s, SOL_SOCKET, SO_REUSEADDR,
                 reinterpret_cast<const char*>(&yes), sizeof(yes));

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port = htons(port);

    if (::bind(s, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        err = "bind: " + lastNetError();
        closeSocketHandle(s);
        return kInvalidSocket;
    }
    if (::listen(s, 4) != 0) {
        err = "listen: " + lastNetError();
        closeSocketHandle(s);
        return kInvalidSocket;
    }

    sockaddr_in got{};
#if defined(_WIN32)
    int len = sizeof(got);
#else
    socklen_t len = sizeof(got);
#endif
    if (::getsockname(s, reinterpret_cast<sockaddr*>(&got), &len) == 0)
        boundPort = ntohs(got.sin_port);

    return s;
}

/// accept có timeout; kInvalidSocket khi hết giờ (để thread kiểm cờ dừng).
inline SocketHandle tcpAccept(SocketHandle listener, int timeoutMs) {
    fd_set rfds;
    FD_ZERO(&rfds);
    FD_SET(listener, &rfds);

    timeval tv{};
    tv.tv_sec = timeoutMs / 1000;
    tv.tv_usec = (timeoutMs % 1000) * 1000;

    const int sel = ::select(static_cast<int>(listener) + 1, &rfds, nullptr, nullptr, &tv);
    if (sel <= 0) return kInvalidSocket;
    return ::accept(listener, nullptr, nullptr);
}

} // namespace mikmap

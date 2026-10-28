// Winsock's fd_set holds FD_SETSIZE sockets (64 by default): one batch of 256 must fit. Must come before any winsock include.
#if defined(_WIN32) && !defined(FD_SETSIZE)
#  define FD_SETSIZE 512
#endif
#include "io/net/NetScan.h"
#include "io/net/Socket.h"

#include <algorithm>
#include <chrono>

#if defined(_WIN32)
#  include <iphlpapi.h>
#  if defined(_MSC_VER)
#    pragma comment(lib, "iphlpapi.lib")
#  endif
#else
#  include <arpa/inet.h>
#  include <ifaddrs.h>
#  include <net/if.h>
#  include <poll.h>
#endif

namespace mikmap {

namespace {

bool parseIpv4(const std::string& s, uint32_t& out) {
    uint32_t v = 0, part = 0;
    int parts = 0, digits = 0;
    for (size_t i = 0; i <= s.size(); ++i) {
        const char ch = i < s.size() ? s[i] : '.';
        if (ch >= '0' && ch <= '9') {
            part = part * 10 + static_cast<uint32_t>(ch - '0');
            if (++digits > 3 || part > 255) return false;
        } else if (ch == '.' && digits > 0 && parts < 4) {
            v = (v << 8) | part; ++parts; part = 0; digits = 0;
        } else return false;
    }
    if (parts != 4) return false;
    out = v;
    return true;
}

std::string ipv4Text(uint32_t v) {
    return std::to_string(v >> 24) + "." + std::to_string((v >> 16) & 255) + "." +
           std::to_string((v >> 8) & 255) + "." + std::to_string(v & 255);
}

#if defined(_WIN32)
std::string narrow(const wchar_t* w) {
    if (!w) return {};
    const int n = ::WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(static_cast<size_t>(n - 1), '\0');
    ::WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}
#else
int prefixOfMask(uint32_t mask) {   // host byte order
    int n = 0;
    while (n < 32 && (mask & (0x80000000u >> n))) ++n;
    return n;
}
#endif

/// Một lô: mọi socket connect cùng lúc, rồi chờ chung một deadline.
void scanBatch(const std::vector<std::string>& hosts, size_t begin, size_t end, uint16_t port,
               int timeoutMs, std::vector<char>& ok) {
    struct Probe { size_t idx; SocketHandle s; };
    std::vector<Probe> pending;
    for (size_t i = begin; i < end; ++i) {
        uint32_t ip = 0;
        if (!parseIpv4(hosts[i], ip)) continue;
        const SocketHandle s = ::socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
        if (s == kInvalidSocket) continue;
        if (!setNonBlocking(s, true)) { closeSocketHandle(s); continue; }
        sockaddr_in a{};
        a.sin_family = AF_INET;
        a.sin_port = htons(port);
        a.sin_addr.s_addr = htonl(ip);
        if (::connect(s, reinterpret_cast<sockaddr*>(&a), sizeof a) == 0) { ok[i] = 1; closeSocketHandle(s); continue; }
#if defined(_WIN32)
        const int e = ::WSAGetLastError();
        const bool inProgress = (e == WSAEWOULDBLOCK || e == WSAEINPROGRESS);
#else
        const bool inProgress = (errno == EINPROGRESS);
#endif
        if (!inProgress) { closeSocketHandle(s); continue; }
        pending.push_back({i, s});
    }

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(timeoutMs);
    while (!pending.empty()) {
        const auto left = std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
        if (left <= 0) break;
        std::vector<char> done(pending.size(), 0), good(pending.size(), 0);
#if defined(_WIN32)
        fd_set wfds, efds;   // Windows reports a refused connect in the except set
        FD_ZERO(&wfds);
        FD_ZERO(&efds);
        for (auto& p : pending) { FD_SET(p.s, &wfds); FD_SET(p.s, &efds); }
        timeval tv{};
        tv.tv_sec = static_cast<long>(left / 1000);
        tv.tv_usec = static_cast<long>((left % 1000) * 1000);
        if (::select(0, nullptr, &wfds, &efds, &tv) <= 0) break;
        for (size_t k = 0; k < pending.size(); ++k) {
            if (FD_ISSET(pending[k].s, &efds)) done[k] = 1;
            else if (FD_ISSET(pending[k].s, &wfds)) { done[k] = 1; good[k] = 1; }
        }
#else
        std::vector<pollfd> pf(pending.size());
        for (size_t k = 0; k < pending.size(); ++k) pf[k] = pollfd{pending[k].s, POLLOUT, 0};
        if (::poll(pf.data(), static_cast<nfds_t>(pf.size()), static_cast<int>(left)) <= 0) break;
        for (size_t k = 0; k < pending.size(); ++k) {
            if (!pf[k].revents) continue;
            done[k] = 1;
            int soErr = 0; socklen_t len = sizeof soErr;
            good[k] = (pf[k].revents & POLLOUT) && ::getsockopt(pending[k].s, SOL_SOCKET, SO_ERROR, &soErr, &len) == 0 && soErr == 0;
        }
#endif
        std::vector<Probe> still;
        for (size_t k = 0; k < pending.size(); ++k) {
            if (!done[k]) { still.push_back(pending[k]); continue; }
            if (good[k]) ok[pending[k].idx] = 1;
            closeSocketHandle(pending[k].s);
        }
        pending.swap(still);
    }
    for (auto& p : pending) closeSocketHandle(p.s);
}

} // namespace

std::vector<std::string> hostsInSubnet(const std::string& address, int prefix) {
    uint32_t ip = 0;
    if (prefix < 22 || prefix > 32 || !parseIpv4(address, ip)) return {};
    const uint32_t mask = 0xffffffffu << (32 - prefix);   // prefix >= 22: no undefined 32-bit shift
    const uint32_t net = ip & mask, bcast = net | ~mask;
    std::vector<std::string> out;
    for (uint32_t h = net + 1; h < bcast; ++h)
        if (h != ip) out.push_back(ipv4Text(h));
    return out;
}

std::vector<std::string> scanTcpPort(const std::vector<std::string>& hosts, uint16_t port, int timeoutMs) {
    if (!ensureWinsock()) return {};
    std::vector<char> ok(hosts.size(), 0);
    constexpr size_t kBatch = 256;
    for (size_t b = 0; b < hosts.size(); b += kBatch)
        scanBatch(hosts, b, std::min(hosts.size(), b + kBatch), port, timeoutMs, ok);
    std::vector<std::string> out;
    for (size_t i = 0; i < hosts.size(); ++i) if (ok[i]) out.push_back(hosts[i]);
    return out;
}

std::vector<Ipv4Iface> localIpv4Interfaces() {
    std::vector<Ipv4Iface> out;
#if defined(_WIN32)
    ULONG size = 16 * 1024;
    std::vector<unsigned char> buf;
    ULONG rc = ERROR_BUFFER_OVERFLOW;
    for (int tries = 0; tries < 3 && rc == ERROR_BUFFER_OVERFLOW; ++tries) {
        buf.resize(size);
        rc = ::GetAdaptersAddresses(AF_INET, GAA_FLAG_SKIP_ANYCAST | GAA_FLAG_SKIP_MULTICAST | GAA_FLAG_SKIP_DNS_SERVER,
                                    nullptr, reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()), &size);
    }
    if (rc != NO_ERROR) return out;
    for (auto* ad = reinterpret_cast<IP_ADAPTER_ADDRESSES*>(buf.data()); ad; ad = ad->Next) {
        if (ad->OperStatus != IfOperStatusUp || ad->IfType == IF_TYPE_SOFTWARE_LOOPBACK) continue;
        for (auto* u = ad->FirstUnicastAddress; u; u = u->Next) {
            if (!u->Address.lpSockaddr || u->Address.lpSockaddr->sa_family != AF_INET) continue;
            const auto* sin = reinterpret_cast<const sockaddr_in*>(u->Address.lpSockaddr);
            const uint32_t ip = ntohl(sin->sin_addr.s_addr);
            if ((ip >> 24) == 127) continue;
            out.push_back({narrow(ad->FriendlyName), ipv4Text(ip), static_cast<int>(u->OnLinkPrefixLength)});
        }
    }
#else
    ifaddrs* list = nullptr;
    if (::getifaddrs(&list) != 0) return out;
    for (ifaddrs* i = list; i; i = i->ifa_next) {
        if (!i->ifa_addr || i->ifa_addr->sa_family != AF_INET) continue;
        if (!(i->ifa_flags & IFF_UP) || (i->ifa_flags & IFF_LOOPBACK)) continue;
        const uint32_t ip = ntohl(reinterpret_cast<const sockaddr_in*>(i->ifa_addr)->sin_addr.s_addr);
        const uint32_t mask = i->ifa_netmask ? ntohl(reinterpret_cast<const sockaddr_in*>(i->ifa_netmask)->sin_addr.s_addr) : 0xffffff00u;
        out.push_back({i->ifa_name ? i->ifa_name : "", ipv4Text(ip), prefixOfMask(mask)});
    }
    ::freeifaddrs(list);
#endif
    return out;
}

} // namespace mikmap

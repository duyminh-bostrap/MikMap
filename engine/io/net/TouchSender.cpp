#include "io/net/TouchSender.h"

#include "io/net/Socket.h"

#include <array>
#include <chrono>
#include <cstdio>
#include <cstring>

namespace mikmap {
namespace {

// ── OSC writer: bounds-checked, big-endian, 4-byte padded ──
struct W {
    uint8_t* b; size_t cap, n = 0; bool ok = true;
    void bytes(const void* p, size_t k) {
        if (!ok || n + k > cap) { ok = false; return; }
        std::memcpy(b + n, p, k); n += k;
    }
    void u32(uint32_t v) { const uint8_t q[4] = {uint8_t(v >> 24), uint8_t(v >> 16), uint8_t(v >> 8), uint8_t(v)}; bytes(q, 4); }
    void f32(float f) { uint32_t u; std::memcpy(&u, &f, 4); u32(u); }
    void str(const char* s) {   // the terminating NUL, then zeros up to a multiple of 4
        const size_t k = std::strlen(s);
        bytes(s, k);
        static const uint8_t z[4] = {};
        bytes(z, 4 - (k % 4));
    }
    void addr(const char* dev, const char* tail) {   // "/mikmap/<dev>/<tail>" without building a std::string
        char a[8 + 32 + 8];
        std::snprintf(a, sizeof a, "/mikmap/%s/%s", dev, tail);
        str(a);
    }
};

bool safeChar(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == ':' || c == '-';
}

using Clk = std::chrono::steady_clock;

// Resolved targets, sender thread only. A failure is retried after 2 s so a bad name does not query DNS per packet.
// shortcut: a name is resolved once per host:port; a host whose IP changes needs its Host edited (or an app restart) to re-resolve.
struct Target { std::string host; uint16_t port = 0; sockaddr_in addr{}; bool ok = false; Clk::time_point at{}; };

} // namespace

void setOutDevice(OutPacket& p, const char* name) {
    size_t i = 0;
    if (name) for (; name[i] && i < sizeof p.device - 1; ++i) p.device[i] = safeChar(name[i]) ? name[i] : '_';
    p.device[i] = '\0';
    if (i == 0) std::snprintf(p.device, sizeof p.device, "device");
}

size_t encodeOsc(const OutPacket& p, uint8_t* buf, size_t cap) {
    const int count = p.count < kMaxTouchPoints ? p.count : kMaxTouchPoints;
    W w{buf, cap};
    w.str("#bundle");
    w.u32(0); w.u32(1);   // timetag 1 = "immediately"

    // each element: int32 size, then the message; the size is patched in once the message is written
    auto element = [&](auto body) {
        const size_t at = w.n;
        w.u32(0);
        body();
        if (!w.ok) return;
        const uint32_t sz = uint32_t(w.n - at - 4);
        const uint8_t q[4] = {uint8_t(sz >> 24), uint8_t(sz >> 16), uint8_t(sz >> 8), uint8_t(sz)};
        std::memcpy(buf + at, q, 4);
    };
    element([&] { w.addr(p.device, "frame"); w.str(",i"); w.u32(uint32_t(count)); });
    for (int i = 0; i < count; ++i)
        element([&] { w.addr(p.device, "touch"); w.str(",iff"); w.u32(p.ids[i]); w.f32(p.xy[2 * i]); w.f32(p.xy[2 * i + 1]); });
    return w.ok ? w.n : 0;
}

size_t encodeText(const OutPacket& p, uint8_t* buf, size_t cap) {
    const int count = p.count < kMaxTouchPoints ? p.count : kMaxTouchPoints;
    char* out = reinterpret_cast<char*>(buf);
    size_t n = 0;
    auto line = [&](int k) { if (k < 0 || n + size_t(k) >= cap) return false; n += size_t(k); return true; };   // >= : snprintf needs room for its NUL
    if (cap == 0 || !line(std::snprintf(out, cap, "%s frame %d\n", p.device, count))) return 0;
    for (int i = 0; i < count; ++i)
        if (!line(std::snprintf(out + n, cap - n, "%s %u %.4f %.4f\n", p.device, unsigned(p.ids[i]), double(p.xy[2 * i]), double(p.xy[2 * i + 1])))) return 0;
    return n;
}

// ── TouchSender ──

TouchSender::TouchSender() {
    for (size_t i = 0; i < kCap; ++i) m_cells[i].seq.store(i, std::memory_order_relaxed);
}

bool TouchSender::tryEnqueue(const OutPacket& p) {
    const size_t pos = m_enq.load(std::memory_order_relaxed);
    Cell& c = m_cells[pos & (kCap - 1)];
    if (c.seq.load(std::memory_order_acquire) != pos) return false;   // full, or a consumer is still copying that cell
    c.data = p;
    c.seq.store(pos + 1, std::memory_order_release);
    m_enq.store(pos + 1, std::memory_order_relaxed);
    return true;
}

bool TouchSender::tryDequeue(OutPacket* out) {
    size_t pos = m_deq.load(std::memory_order_relaxed);
    Cell* c = nullptr;
    for (;;) {
        c = &m_cells[pos & (kCap - 1)];
        const size_t seq = c->seq.load(std::memory_order_acquire);
        const auto dif = static_cast<std::ptrdiff_t>(seq) - static_cast<std::ptrdiff_t>(pos + 1);
        if (dif == 0) { if (m_deq.compare_exchange_weak(pos, pos + 1, std::memory_order_relaxed)) break; }
        else if (dif < 0) return false;   // empty
        else pos = m_deq.load(std::memory_order_relaxed);
    }
    if (out) *out = c->data;
    c->seq.store(pos + kCap, std::memory_order_release);
    return true;
}

bool TouchSender::push(const OutPacket& p) {
    if (!tryEnqueue(p)) {
        if (tryDequeue(nullptr)) m_dropped.fetch_add(1, std::memory_order_relaxed);   // drop the oldest
        if (!tryEnqueue(p)) m_dropped.fetch_add(1, std::memory_order_relaxed);       // the sender was mid-copy of that very cell: drop this one
    }
    m_wake.fetch_add(1, std::memory_order_release);
    m_wake.notify_one();
    return true;
}

bool TouchSender::start() {
    if (m_running.load()) return true;
    m_running.store(true, std::memory_order_release);
    try { m_thread = std::thread(&TouchSender::threadLoop, this); }
    catch (...) { m_running.store(false); return false; }
    return true;
}

void TouchSender::stop() {
    if (!m_running.exchange(false)) return;
    m_wake.fetch_add(1, std::memory_order_release);
    m_wake.notify_one();
    if (m_thread.joinable()) m_thread.join();
    while (tryDequeue(nullptr)) m_dropped.fetch_add(1, std::memory_order_relaxed);
}

std::string TouchSender::lastError() const {
    switch (m_errKind.load(std::memory_order_acquire)) {
    case 0:  return {};
    case 1:  return "Cannot resolve host";
    case 2:  return "Port must be 1-65535";
    case 3:  return "Host is empty";
    case 4:  return "Send failed (error " + std::to_string(m_errCode.load()) + ")";
    case 5:  return "Cannot open a UDP socket";
    default: return "Packet too large";
    }
}

void TouchSender::threadLoop() {
    SocketHandle sock = kInvalidSocket;
    if (ensureWinsock()) sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);

    std::array<Target, 8> cache;
    size_t nextSlot = 0;
    std::string failKey;   // host:port of the last failure; its next success clears lastError()
    uint8_t buf[16384];   // 64 points of either format fit (OSC ~4.7 KB, text ~9 KB worst case)
    OutPacket p;

    auto fail = [&](int kind, int code, const std::string& key) {
        m_errCode.store(code, std::memory_order_relaxed);
        m_errKind.store(kind, std::memory_order_release);
        failKey = key;
        m_dropped.fetch_add(1, std::memory_order_relaxed);
    };

    auto sendOne = [&] {
        const void* nul = std::memchr(p.host, 0, sizeof p.host);
        const std::string host(p.host, nul ? static_cast<const char*>(nul) - p.host : sizeof p.host);
        const std::string key = host + ":" + std::to_string(p.port);
        if (sock == kInvalidSocket) return fail(5, 0, key);
        if (host.empty()) return fail(3, 0, key);
        if (p.port == 0) return fail(2, 0, key);

        Target* t = nullptr;
        for (auto& c : cache) if (c.port == p.port && c.host == host) { t = &c; break; }
        if (!t || (!t->ok && Clk::now() - t->at > std::chrono::seconds(2))) {   // new target, or retry a failed name
            if (!t) t = &cache[nextSlot++ % cache.size()];
            *t = Target{host, p.port, {}, false, Clk::now()};
            addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;   // shortcut: IPv4 only (lidar networks); add an AF_INET6 socket if a receiver needs v6
            addrinfo* res = nullptr;
            if (::getaddrinfo(host.c_str(), std::to_string(p.port).c_str(), &hints, &res) == 0 && res) {
                std::memcpy(&t->addr, res->ai_addr, sizeof t->addr);
                t->ok = true;
            }
            if (res) ::freeaddrinfo(res);
        }
        if (!t->ok) return fail(1, 0, key);

        const size_t n = p.proto == 1 ? encodeText(p, buf, sizeof buf) : encodeOsc(p, buf, sizeof buf);
        if (n == 0) return fail(6, 0, key);
        if (::sendto(sock, reinterpret_cast<const char*>(buf), static_cast<int>(n), 0,
                     reinterpret_cast<const sockaddr*>(&t->addr), sizeof t->addr) < 0) {
#if defined(_WIN32)
            return fail(4, ::WSAGetLastError(), key);
#else
            return fail(4, errno, key);
#endif
        }
        m_sent.fetch_add(1, std::memory_order_relaxed);
        if (key == failKey) { m_errKind.store(0, std::memory_order_release); failKey.clear(); }
    };

    while (m_running.load(std::memory_order_acquire)) {
        const uint32_t w = m_wake.load(std::memory_order_acquire);
        while (tryDequeue(&p)) sendOne();
        if (!m_running.load(std::memory_order_acquire)) break;
        m_wake.wait(w, std::memory_order_acquire);   // push() and stop() bump m_wake and notify
    }
    closeSocketHandle(sock);
}

} // namespace mikmap

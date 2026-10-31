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

/// Writes one OSC element (int32 size + message) into `w`. Rolls the buffer back and
/// returns false when the element does not fit — the caller then stops the chunk there.
template <class Body>
bool element(W& w, Body body) {
    const size_t at = w.n;
    w.u32(0);                       // placeholder for the size, patched below
    body();
    if (!w.ok) { w.ok = true; w.n = at; return false; }
    const uint32_t sz = uint32_t(w.n - at - 4);
    const uint8_t q[4] = {uint8_t(sz >> 24), uint8_t(sz >> 16), uint8_t(sz >> 8), uint8_t(sz)};
    std::memcpy(w.b + at, q, 4);
    return true;
}

/// The point count a chunk encoder works with: OutPacket::count clamped to kMaxTouchPoints.
int frameCount(const OutPacket& p) {
    return p.count < kMaxTouchPoints ? int(p.count) : kMaxTouchPoints;
}

/// Clamps `first` into [0, count] and reports it through `next`.
int firstPoint(int first, int count, int& next) {
    int i = first < 0 ? 0 : first;
    if (i > count) i = count;
    next = i;
    return i;
}

using Clk = std::chrono::steady_clock;
constexpr auto kRetry = std::chrono::seconds(5);

// Resolved targets, sender thread only. A failure is retried kRetry after its lookup ended so a bad name does not query DNS per packet.
// shortcut: a name is resolved once per host:port; a host whose IP changes needs its Host edited (or an app restart) to re-resolve.
struct Target { std::string host; uint16_t port = 0; sockaddr_in addr{}; bool ok = false; Clk::time_point at{}; };

} // namespace

void setOutDevice(OutPacket& p, const char* name) {
    size_t i = 0;
    if (name) for (; name[i] && i < sizeof p.device - 1; ++i) p.device[i] = safeChar(name[i]) ? name[i] : '_';
    p.device[i] = '\0';
    if (i == 0) std::snprintf(p.device, sizeof p.device, "device");
}

size_t encodeOscChunk(const OutPacket& p, int first, uint8_t* buf, size_t cap, int& next) {
    const int count = frameCount(p);
    int i = firstPoint(first, count, next);
    const int start = i;

    W w{buf, cap};
    w.str("#bundle");
    w.u32(0); w.u32(1);   // timetag 1 = "immediately"
    if (i == 0) {
        if (!element(w, [&] { w.addr(p.device, "frame"); w.str(",i"); w.u32(uint32_t(count)); })) {
            next = 0;
            return 0;         // even the frame message does not fit
        }
    } else if (i == count) {
        return 0;             // nothing left, and a later chunk carries no frame message
    }

    while (i < count) {
        if (!element(w, [&] { w.addr(p.device, "touch"); w.str(",iff"); w.u32(p.ids[i]); w.f32(p.xy[2 * i]); w.f32(p.xy[2 * i + 1]); })) break;
        ++i;
    }
    next = i;
    if (count > start && i == start) return 0;   // not even one point fits in cap
    return w.n;
}

size_t encodeTextChunk(const OutPacket& p, int first, uint8_t* buf, size_t cap, int& next) {
    const int count = frameCount(p);
    int i = firstPoint(first, count, next);
    const int start = i;

    if (cap == 0) { next = i; return 0; }
    char* out = reinterpret_cast<char*>(buf);
    size_t n = 0;

    if (i == 0) {
        const int k = std::snprintf(out, cap, "%s frame %d\n", p.device, count);
        if (k < 0 || size_t(k) >= cap) { next = 0; return 0; }   // >= : snprintf needs room for its NUL
        n = size_t(k);
    } else if (i == count) {
        return 0;             // nothing left, and a later chunk carries no frame line
    }

    while (i < count) {
        const int k = std::snprintf(out + n, cap - n, "%s %u %.4f %.4f\n", p.device, unsigned(p.ids[i]),
                                    double(p.xy[2 * i]), double(p.xy[2 * i + 1]));
        if (k < 0 || size_t(k) >= cap - n) break;   // never split a line across datagrams
        n += size_t(k);
        ++i;
    }
    next = i;
    if (count > start && i == start) return 0;   // not even one point fits in cap
    return n;
}

size_t encodeOsc(const OutPacket& p, uint8_t* buf, size_t cap) {
    int next = 0;
    const size_t n = encodeOscChunk(p, 0, buf, cap, next);
    return (n != 0 && next == frameCount(p)) ? n : 0;   // the whole frame must fit in cap
}

size_t encodeText(const OutPacket& p, uint8_t* buf, size_t cap) {
    int next = 0;
    const size_t n = encodeTextChunk(p, 0, buf, cap, next);
    return (n != 0 && next == frameCount(p)) ? n : 0;   // the whole frame must fit in cap
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
    uint8_t buf[kMaxDatagram];   // one datagram at a time: a frame is split across as many as it needs
    OutPacket p;

    auto fail = [&](int kind, int code, const std::string& key) {
        m_dropped.fetch_add(1, std::memory_order_relaxed);   // counted BEFORE the error is published (release), so lastError() != "" implies dropped() counts it
        m_errCode.store(code, std::memory_order_relaxed);
        failKey = key;
        m_errKind.store(kind, std::memory_order_release);
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
        // A failed name is retried only kRetry after its lookup RETURNED (that can take seconds: LLMNR/NetBIOS on a LAN
        // without DNS); until then its packets drop at once and every other target keeps flowing.
        if (!t || (!t->ok && Clk::now() - t->at > kRetry)) {   // new target, or retry a failed name
            if (!t) t = &cache[nextSlot++ % cache.size()];
            *t = Target{host, p.port, {}, false, {}};
            const std::string port = std::to_string(p.port);
            addrinfo hints{}; hints.ai_family = AF_INET; hints.ai_socktype = SOCK_DGRAM;   // shortcut: IPv4 only (lidar networks); add an AF_INET6 socket if a receiver needs v6
            hints.ai_flags = AI_NUMERICHOST;   // an IP literal first: no DNS at all
            addrinfo* res = nullptr;
            if (::getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0 || !res) {
                if (res) { ::freeaddrinfo(res); res = nullptr; }
                hints.ai_flags = 0;
                m_lookups.fetch_add(1, std::memory_order_relaxed);
                if (::getaddrinfo(host.c_str(), port.c_str(), &hints, &res) != 0) res = nullptr;
            }
            if (res) {
                std::memcpy(&t->addr, res->ai_addr, sizeof t->addr);
                t->ok = true;
                ::freeaddrinfo(res);
            }
            t->at = Clk::now();
        }
        if (!t->ok) return fail(1, 0, key);

        const int total = frameCount(p);
        int first = 0;
        for (;;) {
            int next = 0;
            const size_t n = p.proto == 1 ? encodeTextChunk(p, first, buf, kMaxDatagram, next)
                                          : encodeOscChunk(p, first, buf, kMaxDatagram, next);
            if (n == 0) return fail(6, 0, key);   // not even one point fits kMaxDatagram
            if (::sendto(sock, reinterpret_cast<const char*>(buf), static_cast<int>(n), 0,
                         reinterpret_cast<const sockaddr*>(&t->addr), sizeof t->addr) < 0) {
#if defined(_WIN32)
                return fail(4, ::WSAGetLastError(), key);
#else
                return fail(4, errno, key);
#endif
            }
            if (next >= total) break;   // the whole frame is out (an empty frame sends one chunk)
            first = next;
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

#include "io/sources/HokuyoSource.h"

#include <algorithm>
#include <chrono>
#include <utility>

namespace mikmap {
namespace {

constexpr double kTwoPi = 6.283185307179586476925286766559;
constexpr int    kHandshakeMs = 2000;
constexpr int    kMaxConsecutiveErrors = 32;

} // namespace

HokuyoSource::HokuyoSource(HokuyoConfig cfg) : m_cfg(std::move(cfg)) {}

HokuyoSource::~HokuyoSource() { stop(); }

std::string HokuyoSource::lastError() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_error;
}

void HokuyoSource::setError(const std::string& e) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_error = e;
}

bool HokuyoSource::start() {
    if (m_running.exchange(true)) return true;   // đã chạy

    setError({});
    m_status.store(SourceStatus::Connecting);
    m_thread = std::thread(&HokuyoSource::threadLoop, this);

    // KHÔNG trả false khi chưa nối được: cáp mạng cắm muộn là chuyện
    // thường. Trả false ở đây sẽ khiến src/ đánh dấu nguồn hỏng vĩnh viễn.
    return true;
}

void HokuyoSource::stop() {
    if (!m_running.exchange(false)) return;

    {
        std::lock_guard<std::mutex> lock(m_sockMutex);
        if (m_sock != kInvalidSocket) {
            sendAll(m_sock, "QT\n", 3);
            shutdownSocket(m_sock);   // đánh thức recv() đang chặn
        }
    }

    if (m_thread.joinable()) m_thread.join();
    m_status.store(SourceStatus::Stopped);
}

void HokuyoSource::closeCurrent(SocketHandle sock) {
    std::lock_guard<std::mutex> lock(m_sockMutex);
    if (m_sock == sock) {
        closeSocketHandle(m_sock);
        m_sock = kInvalidSocket;
    }
}

void HokuyoSource::waitReconnect() {
    const int totalMs = static_cast<int>(std::max(0.0, m_cfg.reconnectSec) * 1000.0);
    for (int waited = 0; waited < totalMs && m_running.load(); waited += 10)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void HokuyoSource::threadLoop() {
    while (m_running.load(std::memory_order_relaxed)) {
        m_status.store(SourceStatus::Connecting);

        std::string err;
        const SocketHandle sock = tcpConnect(m_cfg.host, m_cfg.port, 1000, err);
        if (sock == kInvalidSocket) {
            setError("connect " + m_cfg.host + ":" + std::to_string(m_cfg.port) + " — " + err);
            m_status.store(SourceStatus::Error);
            waitReconnect();
            continue;
        }

        setRecvTimeoutMs(sock, 100);
        {
            std::lock_guard<std::mutex> lock(m_sockMutex);
            m_sock = sock;
        }

        if (!m_running.load()) {
            closeCurrent(sock);
            break;
        }

        scip2::DeviceParams pp;
        if (!handshake(sock, pp)) {
            closeCurrent(sock);
            onLost();
            m_status.store(SourceStatus::Error);
            waitReconnect();
            continue;
        }

        setError({});
        m_status.store(SourceStatus::Running);

        readScans(sock, pp);
        closeCurrent(sock);
        onLost();

        if (m_running.load()) {
            setError("connection lost");
            m_status.store(SourceStatus::Error);
            waitReconnect();
        }
    }

    m_status.store(SourceStatus::Stopped);
}

bool HokuyoSource::handshake(SocketHandle sock, scip2::DeviceParams& out) {
    if (!sendAll(sock, "PP\n", 3)) {
        setError("PP: send failed");
        return false;
    }

    std::string block;
    char buf[512];
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(kHandshakeMs);

    while (m_running.load()) {
        int n = 0;
        const RecvResult r = recvSome(sock, buf, static_cast<int>(sizeof buf), n);

        if (r == RecvResult::Data) {
            block.append(buf, static_cast<size_t>(n));
            if (block.find("\n\n") != std::string::npos) break;
            if (block.size() > 64 * 1024) {
                setError("PP: response too long");
                return false;
            }
        } else if (r == RecvResult::Closed) {
            setError("PP: connection closed");
            return false;
        }

        if (std::chrono::steady_clock::now() > deadline) {
            setError("PP: timeout");
            return false;
        }
    }

    if (!scip2::parsePP(block, out)) {
        setError("PP: no parameters");
        return false;
    }
    if (out.ares <= 0 || out.amax <= out.amin || out.dmax <= out.dmin) {
        setError("PP: bad parameters (ares/amax/dmax)");
        return false;
    }

    const std::string md = scip2::mdCommand(out.amin, out.amax);
    if (!sendAll(sock, md.data(), md.size())) {
        setError("MD: send failed");
        return false;
    }
    return true;
}

void HokuyoSource::readScans(SocketHandle sock, const scip2::DeviceParams& pp) {
    scip2::MdStream md;
    char buf[4096];
    int consecutiveErrors = 0;

    const uint32_t lo = static_cast<uint32_t>(std::max(pp.dmin, 20));
    const uint32_t hi = static_cast<uint32_t>(pp.dmax);

    while (m_running.load(std::memory_order_relaxed)) {
        int n = 0;
        const RecvResult r = recvSome(sock, buf, static_cast<int>(sizeof buf), n);
        if (r == RecvResult::Timeout) continue;
        if (r == RecvResult::Closed) return;

        md.feed(buf, static_cast<size_t>(n));

        for (;;) {
            uint32_t ranges[kMaxScanPoints];
            int count = 0;
            uint32_t ts = 0;
            const scip2::MdResult res = md.next(ranges, kMaxScanPoints, count, ts);

            if (res == scip2::MdResult::None) break;

            if (res == scip2::MdResult::Error) {
                // Block hỏng đã bị bỏ nguyên khối; luồng vẫn đồng bộ nên
                // đọc tiếp. Chỉ bỏ cuộc khi lỗi kéo dài (dây nhiễu/không
                // phải Hokuyo) để không quay vòng reconnect mãi.
                countMalformed();
                if (++consecutiveErrors > kMaxConsecutiveErrors) {
                    setError("SCIP: " + md.error());
                    return;
                }
                continue;
            }

            consecutiveErrors = 0;

            LaserScan scan;
            scan.count = static_cast<uint16_t>(count);
            for (int i = 0; i < count; ++i) {
                scan.angleRad[i] = static_cast<float>(
                    (static_cast<double>(pp.amin + i - pp.afrt) * kTwoPi) / pp.ares);

                const uint32_t v = ranges[i];
                scan.rangeMm[i] = (v >= lo && v <= hi) ? static_cast<float>(v) : 0.0f;
            }
            onScan(scan);
        }
    }
}

} // namespace mikmap

#include "TestHarness.h"

#include "io/net/Socket.h"
#include "io/sources/HokuyoSource.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

using namespace mikmap;

namespace {

constexpr double kPi = 3.14159265358979323846;

// ── Mã hoá SCIP 2.0 phía \"thiết bị\" ────────────────────────────────────
std::string encN(uint32_t v, int n) {
    std::string s;
    for (int i = n - 1; i >= 0; --i)
        s.push_back(static_cast<char>(((v >> (6 * i)) & 0x3Fu) + 0x30u));
    return s;
}
std::string enc3(uint32_t v) { return encN(v, 3); }
std::string enc4(uint32_t v) { return encN(v, 4); }

std::string withSum(const std::string& s) {
    unsigned sum = 0;
    for (char c : s) sum += static_cast<unsigned char>(c);
    return s + static_cast<char>((sum & 0x3Fu) + 0x30u);
}

/// Server SCIP 2.0 giả trên thread. Mỗi lần `send` chỉ 7 byte — đúng thứ
/// làm lộ lỗi ghép dòng/nhóm khi TCP cắt gói.
class FakeScipServer {
public:
    /// @param scansBeforeClose  <0: gửi mãi; >=0: gửi chừng ấy scan rồi đóng.
    explicit FakeScipServer(int scansBeforeClose = -1) : m_limit(scansBeforeClose) {
        std::string err;
        m_listener = tcpListenLoopback(0, m_port, err);
        if (m_listener == kInvalidSocket) {
            m_err = err;
            return;
        }
        m_thread = std::thread([this] { run(); });
    }

    ~FakeScipServer() { stop(); }

    FakeScipServer(const FakeScipServer&) = delete;
    FakeScipServer& operator=(const FakeScipServer&) = delete;

    uint16_t port() const { return m_port; }
    int connections() const { return m_conns.load(); }
    const std::string& error() const { return m_err; }

    void stop() {
        if (!m_run.exchange(false)) return;

        if (m_listener != kInvalidSocket) shutdownSocket(m_listener);
        {
            std::lock_guard<std::mutex> lock(m_clientMutex);
            if (m_client != kInvalidSocket) shutdownSocket(m_client);
        }

        if (m_thread.joinable()) m_thread.join();

        closeSocketHandle(m_listener);
        m_listener = kInvalidSocket;
    }

private:
    void run() {
        while (m_run.load()) {
            const SocketHandle c = tcpAccept(m_listener, 100);
            if (c == kInvalidSocket) continue;

            m_conns.fetch_add(1);
            setRecvTimeoutMs(c, 100);

            {
                std::lock_guard<std::mutex> lock(m_clientMutex);
                m_client = c;
            }

            serve(c);

            {
                std::lock_guard<std::mutex> lock(m_clientMutex);
                if (m_client == c) m_client = kInvalidSocket;
            }
            closeSocketHandle(c);
        }
    }

    bool readLine(SocketHandle c, std::string& out) {
        out.clear();
        char tmp[128];

        while (m_run.load()) {
            int n = 0;
            const RecvResult r = recvSome(c, tmp, static_cast<int>(sizeof tmp), n);

            if (r == RecvResult::Data) {
                out.append(tmp, static_cast<size_t>(n));
                const size_t nl = out.find('\n');
                if (nl != std::string::npos) {
                    out.resize(nl);
                    return true;
                }
                if (out.size() > 512) return false;
            } else if (r == RecvResult::Closed) {
                return false;
            }
        }
        return false;
    }

    bool sendChunked(SocketHandle c, const std::string& data) {
        size_t at = 0;
        while (at < data.size() && m_run.load()) {
            const size_t n = std::min<size_t>(7, data.size() - at);
            if (!sendAll(c, data.data() + at, n)) return false;
            at += n;
        }
        return at == data.size();
    }

    static std::string ppBlock() {
        std::string b = "PP\n" + withSum("00") + "\n";
        for (const char* line : {"MODL:UST-10LX;", "DMIN:20;", "DMAX:30000;", "ARES:1440;",
                                 "AMIN:0;", "AMAX:1080;", "AFRT:540;", "SCAN:1081;"})
            b += withSum(line) + "\n";
        b += "\n";
        return b;
    }

    static std::string dataBlock(uint32_t ts) {
        std::string data;
        const double objectHalfAngle = std::atan2(20.0, 1000.0);

        for (int i = 0; i < 1081; ++i) {
            const double a = (-135.0 + 0.25 * i) * kPi / 180.0;
            data += enc3(std::abs(a) <= objectHalfAngle ? 1000u : 2000u);
        }

        std::string b = withSum("99") + "\n";
        b += withSum(enc4(ts)) + "\n";
        for (size_t i = 0; i < data.size(); i += 64)
            b += withSum(data.substr(i, 64)) + "\n";
        b += "\n";
        return b;
    }

    void serve(SocketHandle c) {
        std::string line;
        if (!readLine(c, line) || line.rfind("PP", 0) != 0) return;
        if (!sendChunked(c, ppBlock())) return;

        line.clear();
        if (!readLine(c, line) || line.rfind("MD", 0) != 0) return;
        if (!sendChunked(c, withSum("00") + "\n\n")) return;   // ack trước dữ liệu

        for (int i = 0; m_run.load() && (m_limit < 0 || i < m_limit); ++i) {
            if (!sendChunked(c, dataBlock(static_cast<uint32_t>(1000 + i)))) return;
            for (int k = 0; k < 2 && m_run.load(); ++k)
                std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }

        if (m_limit < 0) {
            // Giữ kết nối mở; chỉ thoát khi client đóng hoặc gửi QT.
            for (;;) {
                if (!m_run.load()) return;
                char tmp[64];
                int n = 0;
                const RecvResult r = recvSome(c, tmp, static_cast<int>(sizeof tmp), n);
                if (r == RecvResult::Closed) return;
                if (r == RecvResult::Data) {
                    const std::string s(tmp, static_cast<size_t>(n));
                    if (s.find("QT") != std::string::npos) return;
                }
            }
        }
    }

    int m_limit = -1;
    uint16_t m_port = 0;
    std::string m_err;

    SocketHandle m_listener = kInvalidSocket;
    SocketHandle m_client = kInvalidSocket;
    std::mutex m_clientMutex;

    std::atomic<bool> m_run{true};
    std::atomic<int> m_conns{0};
    std::thread m_thread;
};

template <typename Pred>
bool waitFor(Pred pred, int ms) {
    const auto end = std::chrono::steady_clock::now() + std::chrono::milliseconds(ms);
    while (std::chrono::steady_clock::now() < end) {
        if (pred()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }
    return pred();
}

} // namespace

TEST_CASE("Hokuyo: ket noi, nhan scan, ra 1 touch") {
    FakeScipServer srv;
    REQUIRE(srv.error().empty());

    HokuyoSource src({"127.0.0.1", srv.port(), 0.1});
    REQUIRE(src.start());

    REQUIRE(waitFor([&] {
        return src.status() == SourceStatus::Running && src.scansReceived() >= 3;
    }, 2000));

    REQUIRE(src.scans().consume());
    REQUIRE(src.scans().readSlot().count == 1081);

    bool one = false;
    REQUIRE(waitFor([&] {
        if (src.frames().consume()) one = (src.frames().readSlot().count == 1);
        return one;
    }, 1000));
    REQUIRE(one);

    const float x = src.frames().readSlot().points[0].x;
    CHECK(x > 975.0f);
    CHECK(x < 1025.0f);

    src.stop();
}

TEST_CASE("★ Hokuyo: server dong ket noi → Up, het touch, tu noi lai") {
    FakeScipServer srv(5);
    REQUIRE(srv.error().empty());

    HokuyoSource src({"127.0.0.1", srv.port(), 0.1});
    REQUIRE(src.start());

    uint32_t downId = 0;
    bool upSeen = false;
    bool reconnected = false;

    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
    while (std::chrono::steady_clock::now() < end) {
        TouchEvent e;
        while (src.events().pop(e)) {
            if (e.state == TouchState::Down) downId = e.id;
            if (e.state == TouchState::Up && downId != 0 && e.id == downId) upSeen = true;
        }

        if (srv.connections() >= 2 && src.status() == SourceStatus::Running) {
            reconnected = true;
            if (upSeen) break;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    }

    CHECK(downId != 0);
    CHECK(upSeen);
    CHECK(reconnected);

    src.stop();
}

TEST_CASE("Hokuyo: khong co server → Error co lastError") {
    uint16_t port = 0;
    {
        FakeScipServer tmp;
        REQUIRE(tmp.error().empty());
        port = tmp.port();
    }   // destructor đóng listener

    HokuyoSource src({"127.0.0.1", port, 0.2});
    REQUIRE(src.start());

    REQUIRE(waitFor([&] {
        const SourceStatus s = src.status();
        return (s == SourceStatus::Error || s == SourceStatus::Connecting) && !src.lastError().empty();
    }, 2000));

    src.stop();
}

TEST_CASE("Hokuyo: setParams ROI loai vat") {
    FakeScipServer srv;
    REQUIRE(srv.error().empty());

    HokuyoSource src({"127.0.0.1", srv.port(), 0.1});
    REQUIRE(src.start());

    bool one = false;
    REQUIRE(waitFor([&] {
        if (src.frames().consume()) one = (src.frames().readSlot().count == 1);
        return one;
    }, 2000));

    LidarParams p;
    p.roi[0] = {3500, 500};
    p.roi[1] = {3500, -500};
    p.roi[2] = {2500, -500};
    p.roi[3] = {2500, 500};
    src.setParams(p);

    bool zero = false;
    REQUIRE(waitFor([&] {
        if (src.frames().consume()) zero = (src.frames().readSlot().count == 0);
        return zero;
    }, 2000));
    REQUIRE(zero);

    src.stop();
}

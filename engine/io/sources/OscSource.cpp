#include "io/sources/OscSource.h"

#include "core/util/Clock.h"

#include <algorithm>
#include <vector>

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
// Chỉ MSVC hiểu pragma này. MinGW/clang trên Windows bỏ qua (kèm cảnh
// báo -Wunknown-pragmas) — ở đó ws2_32 được nối trong CMakeLists.txt.
#  pragma comment(lib, "ws2_32.lib")
#  endif
using SocketHandle = SOCKET;
static constexpr SocketHandle kInvalidSocket = INVALID_SOCKET;
#else
#  include <arpa/inet.h>
#  include <fcntl.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
using SocketHandle = int;
static constexpr SocketHandle kInvalidSocket = -1;
#endif

namespace mikmap {
namespace {

constexpr size_t kMaxPacketSize = 65536;

void closeSocketHandle(SocketHandle s) {
    if (s == kInvalidSocket) return;
#if defined(_WIN32)
    ::closesocket(s);
#else
    ::close(s);
#endif
}

#if defined(_WIN32)
/// Winsock cần khởi tạo trước khi dùng và chỉ nên làm một lần cho cả
/// tiến trình. Bọc trong biến tĩnh cục bộ để C++ tự lo an toàn đa luồng.
struct WinsockInit {
    bool ok = false;
    WinsockInit() {
        WSADATA d{};
        ok = (::WSAStartup(MAKEWORD(2, 2), &d) == 0);
    }
    ~WinsockInit() { if (ok) ::WSACleanup(); }
};
bool ensureWinsock() {
    static WinsockInit init;
    return init.ok;
}
#endif

} // namespace

OscSource::OscSource(OscConfig cfg)
    : m_cfg(cfg), m_startTime(std::chrono::steady_clock::now()) {}

OscSource::~OscSource() {
    stop();
}

void OscSource::setConfig(const OscConfig& cfg) {
    if (m_running.load()) return;   // cổng đã bind, không đổi giữa chừng
    m_cfg = cfg;
}

std::string OscSource::lastError() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_error;
}

void OscSource::setError(const std::string& e) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_error = e;
}

double OscSource::nowSec() const {
    return std::chrono::duration<double>(
               std::chrono::steady_clock::now() - m_startTime).count();
}

bool OscSource::start() {
    if (m_running.load()) return true;

    setError({});
    m_status.store(SourceStatus::Connecting);
    m_running.store(true);
    m_points.clear();
    m_seq = 0;
    m_startTime = std::chrono::steady_clock::now();

    m_thread = std::thread(&OscSource::threadLoop, this);

    // Chờ ngắn để biết bind thành công hay không — người dùng bấm Start
    // rồi cần thấy ngay "đang chạy" hoặc "cổng đã bị chiếm".
    for (int i = 0; i < 100; ++i) {
        const SourceStatus s = m_status.load();
        if (s == SourceStatus::Running || s == SourceStatus::Error) break;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    if (m_status.load() == SourceStatus::Error) {
        stop();
        return false;
    }
    return true;
}

void OscSource::stop() {
    if (!m_running.exchange(false)) return;
    if (m_thread.joinable()) m_thread.join();

    // Phát Up cho mọi điểm còn sống. Thiếu một Up = hiệu ứng kẹt vĩnh viễn.
    for (const auto& kv : m_points) {
        TouchEvent ev;
        ev.id = kv.first;
        ev.x = kv.second.x;
        ev.y = kv.second.y;
        ev.state = TouchState::Up;
        ev.sourceId = m_sourceId;
        ev.tCaptureNs = Clock::nowNs();
        m_events.push(ev);
    }
    m_points.clear();

    m_status.store(SourceStatus::Stopped);
}

void OscSource::threadLoop() {
#if defined(_WIN32)
    if (!ensureWinsock()) {
        setError("Khong khoi tao duoc Winsock");
        m_status.store(SourceStatus::Error);
        return;
    }
#endif

    SocketHandle sock = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == kInvalidSocket) {
        setError("Khong tao duoc UDP socket");
        m_status.store(SourceStatus::Error);
        return;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(m_cfg.port);

    if (::bind(sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0) {
        setError("Khong bind duoc cong UDP " + std::to_string(m_cfg.port)
                 + " — cong dang bi chuong trinh khac chiem?");
        m_status.store(SourceStatus::Error);
        closeSocketHandle(sock);
        return;
    }

    // Timeout khi nhận: thread phải tỉnh dậy định kỳ để (a) thấy cờ dừng
    // và (b) cho điểm cũ hết hạn. Nếu chặn vô hạn ở recvfrom, stop() sẽ
    // treo cho tới khi có gói tin kế tiếp — có thể là không bao giờ.
#if defined(_WIN32)
    DWORD timeoutMs = 100;
    ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO,
                 reinterpret_cast<const char*>(&timeoutMs), sizeof(timeoutMs));
#else
    timeval tv{};
    tv.tv_sec = 0;
    tv.tv_usec = 100000;
    ::setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
#endif

    m_status.store(SourceStatus::Running);

    std::vector<uint8_t> buf(kMaxPacketSize);

    while (m_running.load(std::memory_order_relaxed)) {
        const auto n = ::recvfrom(sock, reinterpret_cast<char*>(buf.data()),
                                  static_cast<int>(buf.size()), 0, nullptr, nullptr);
        if (n > 0) {
            feedPacket(buf.data(), static_cast<size_t>(n));
        }
        // n <= 0 thường là timeout — không phải lỗi, vòng lặp chạy tiếp.

        expireStalePoints();
    }

    closeSocketHandle(sock);
}

void OscSource::feedPacket(const uint8_t* data, size_t size) {
    m_packets.fetch_add(1, std::memory_order_relaxed);

    std::vector<OscMessage> msgs;
    if (!parseOscPacket(data, size, msgs)) {
        m_malformed.fetch_add(1, std::memory_order_relaxed);
        if (msgs.empty()) return;   // không cứu được gì
    }

    handleMessages(msgs);
    publishFrame();
}

void OscSource::handleTuio(const std::vector<OscMessage>& msgs) {
    TuioFrame frame;
    if (!m_tuio.feed(msgs, frame)) return;   // gói chưa đóng frame

    const double now = nowSec();

    // ── Điểm đã nhấc lên ───────────────────────────────────────────────
    //
    // ★ TUIO không có message "nhấc tay"; `TuioDecoder` suy ra từ việc id
    //   vắng mặt trong `alive`. Phát Up ở đây là chỗ DUY NHẤT điểm chạm
    //   TUIO kết thúc — thiếu nó thì mọi cú chạm sống mãi cho tới khi
    //   `expireStalePoints` dọn sau 1 giây, tức là hiệu ứng trễ đúng một
    //   giây mỗi lần nhấc tay.
    for (const int32_t endedId : frame.ended) {
        const auto id = static_cast<uint32_t>(endedId);
        const auto it = m_points.find(id);
        if (it == m_points.end()) continue;

        TouchEvent ev;
        ev.id = id;
        ev.x = it->second.x;
        ev.y = it->second.y;
        ev.state = TouchState::Up;
        ev.sourceId = m_sourceId;
        ev.tCaptureNs = Clock::nowNs();
        m_events.push(ev);
        m_points.erase(it);
    }

    // ── Điểm còn sống ──────────────────────────────────────────────────
    for (const TuioCursor& c : frame.cursors) {
        const auto id = static_cast<uint32_t>(c.id);

        // ★ TUIO LUÔN gửi toạ độ chuẩn hoá [0,1] — đó là quy định của
        //   giao thức, không phải tuỳ chọn. Nên KHÔNG hỏi `normalizedInput`
        //   ở đây: cờ đó dành cho phương ngữ MikMap, nơi bên gửi tự chọn.
        //   Đọc cờ đó ở đây nghĩa là cấu hình sai một lần sẽ dồn mọi điểm
        //   chạm về góc trên-trái, trong một ô vuông 1x1 pixel.
        const double x = c.x * m_cfg.sensorRange.x;
        const double y = c.y * m_cfg.sensorRange.y;

        const bool existed = (m_points.find(id) != m_points.end());

        LivePoint& p = m_points[id];
        p.x = static_cast<float>(x);
        p.y = static_cast<float>(y);
        p.z = 0.0f;
        p.lastSeenSec = now;

        if (!existed) {
            p.isNew = false;
            TouchEvent ev;
            ev.id = id;
            ev.x = p.x;
            ev.y = p.y;
            ev.state = TouchState::Down;
            ev.sourceId = m_sourceId;
            ev.tCaptureNs = Clock::nowNs();
            m_events.push(ev);
        }
    }
}

void OscSource::handleMessages(const std::vector<OscMessage>& msgs) {
    if (m_cfg.protocol == OscProtocol::Tuio) {
        handleTuio(msgs);
        return;
    }

    const std::string& prefix = m_cfg.addressPrefix;
    const double now = nowSec();

    for (const OscMessage& m : msgs) {
        if (m.address == prefix + "/clear") {
            for (const auto& kv : m_points) {
                TouchEvent ev;
                ev.id = kv.first;
                ev.state = TouchState::Up;
                ev.sourceId = m_sourceId;
                ev.tCaptureNs = Clock::nowNs();
                m_events.push(ev);
            }
            m_points.clear();
            continue;
        }

        const bool isUp   = (m.address == prefix + "/up");
        const bool isDown = (m.address == prefix + "/down");
        const bool isMove = (m.address == prefix);

        if (!isUp && !isDown && !isMove) continue;

        const auto id = static_cast<uint32_t>(m.argInt(0, -1));
        if (m.args.empty()) continue;

        if (isUp) {
            const auto it = m_points.find(id);
            if (it != m_points.end()) {
                TouchEvent ev;
                ev.id = id;
                ev.x = it->second.x;
                ev.y = it->second.y;
                ev.state = TouchState::Up;
                ev.sourceId = m_sourceId;
                ev.tCaptureNs = Clock::nowNs();
                m_events.push(ev);
                m_points.erase(it);
            }
            continue;
        }

        double x = m.argDouble(1, 0.0);
        double y = m.argDouble(2, 0.0);
        const double z = m.argDouble(3, 0.0);

        if (m_cfg.normalizedInput) {
            x *= m_cfg.sensorRange.x;
            y *= m_cfg.sensorRange.y;
        }

        const bool existed = (m_points.find(id) != m_points.end());

        LivePoint& p = m_points[id];
        p.x = static_cast<float>(x);
        p.y = static_cast<float>(y);
        p.z = static_cast<float>(z);
        p.lastSeenSec = now;

        // Điểm mới xuất hiện qua message "move" cũng tính là Down.
        // Nhiều nguồn không gửi /down bao giờ — chúng chỉ bắt đầu gửi
        // toạ độ. Nếu chỉ tin vào /down, hiệu ứng sẽ không bao giờ kích hoạt.
        if (!existed || isDown) {
            p.isNew = false;
            TouchEvent ev;
            ev.id = id;
            ev.x = p.x;
            ev.y = p.y;
            ev.state = TouchState::Down;
            ev.sourceId = m_sourceId;
            ev.tCaptureNs = Clock::nowNs();
            m_events.push(ev);
        }
    }
}

void OscSource::expireStalePoints() {
    if (m_cfg.pointTimeoutSec <= 0.0) return;

    const double now = nowSec();
    bool changed = false;

    for (auto it = m_points.begin(); it != m_points.end();) {
        if (now - it->second.lastSeenSec <= m_cfg.pointTimeoutSec) {
            ++it;
            continue;
        }

        // UDP không đảm bảo giao hàng: một message /up bị rớt sẽ làm điểm
        // kẹt lại vĩnh viễn. Hết hạn theo thời gian là lưới an toàn.
        TouchEvent ev;
        ev.id = it->first;
        ev.x = it->second.x;
        ev.y = it->second.y;
        ev.state = TouchState::Up;
        ev.sourceId = m_sourceId;
        ev.tCaptureNs = Clock::nowNs();
        m_events.push(ev);

        it = m_points.erase(it);
        changed = true;
    }

    if (changed) publishFrame();
}

void OscSource::publishFrame() {
    SensorFrame& f = m_frames.writeSlot();
    f.clear();
    f.seq = ++m_seq;
    f.tCaptureNs = Clock::nowNs();
    f.sourceId = m_sourceId;

    for (const auto& kv : m_points) {
        TouchPoint tp;
        tp.id = kv.first;
        tp.x = kv.second.x;
        tp.y = kv.second.y;
        tp.z = kv.second.z;
        tp.confidence = 1.0f;
        tp.state = TouchState::Move;
        if (!f.addPoint(tp)) break;   // vượt kMaxTouchPoints
    }

    m_frames.publish();
}

} // namespace mikmap

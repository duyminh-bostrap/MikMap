// ════════════════════════════════════════════════════════════════════════
//  io/sources/HokuyoSource.h — lidar Hokuyo URG/UST qua SCIP 2.0 (TCP)
//
//  Thread riêng: nối TCP → PP → MD → đọc block liên tục. Đóng cáp thì
//  phát Up cho mọi blob rồi tự nối lại sau reconnectSec.
//
//  Toạ độ giữ nguyên mm, CHƯA áp H_s — render thread áp tại nơi dùng.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "io/net/Socket.h"
#include "io/proto/Scip2.h"
#include "io/sources/ScanSource.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace mikmap {

struct HokuyoConfig {
    std::string host = "192.168.0.10";
    uint16_t    port = 10940;
    double      reconnectSec = 1.0;
};

class HokuyoSource final : public ScanSource {
public:
    explicit HokuyoSource(HokuyoConfig cfg = {});
    ~HokuyoSource() override;

    const char* typeName() const override { return "Hokuyo"; }

    bool start() override;
    void stop() override;

    SourceStatus status() const override { return m_status.load(std::memory_order_acquire); }
    std::string lastError() const override;

private:
    void threadLoop();
    bool handshake(SocketHandle sock, scip2::DeviceParams& out);
    void readScans(SocketHandle sock, const scip2::DeviceParams& pp);
    void closeCurrent(SocketHandle sock);
    void waitReconnect();
    void setError(const std::string& e);

    HokuyoConfig m_cfg;
    std::thread  m_thread;
    std::atomic<bool>         m_running{false};
    std::atomic<SourceStatus> m_status{SourceStatus::Stopped};

    mutable std::mutex m_errorMutex;
    std::string        m_error;

    /// stop() cần shutdown() socket đang bị thread chặn ở recv().
    std::mutex   m_sockMutex;
    SocketHandle m_sock = kInvalidSocket;
};

} // namespace mikmap

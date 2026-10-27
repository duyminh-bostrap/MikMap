// ════════════════════════════════════════════════════════════════════════
//  io/sources/PluginSource.h — lidar 2D cung cấp bởi DLL plugin
//
//  Chỉ là một ScanSource như Hokuyo: thread riêng gọi poll() của plugin,
//  đóng gói scan vào LaserScan rồi onScan() → blob → PointTracker.
//  Plugin hỏng/không có thì onLost() (mọi blob phát Up), báo Error, chờ
//  reconnectSec rồi create() lại.
//
//  Plugin không cần thread-safe: mọi lời gọi ABI đi từ đúng thread này.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "io/plugin/PluginLoader.h"
#include "io/sources/ScanSource.h"

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace mikmap {

struct PluginConfig {
    std::string configJson;
    double      reconnectSec = 1.0;
};

class PluginSource final : public ScanSource {
public:
    /// `plugin` phải sống lâu hơn nguồn này (loader không bao giờ unload DLL).
    PluginSource(const SensorPlugin& plugin, PluginConfig cfg);
    ~PluginSource() override;

    const char* typeName() const override { return m_plugin.name.c_str(); }

    bool start() override;
    void stop() override;

    SourceStatus status() const override { return m_status.load(std::memory_order_acquire); }
    std::string lastError() const override;

private:
    void threadLoop();
    void waitReconnect();
    void setError(const std::string& e);
    std::string instanceError(void* inst) const;

    const SensorPlugin& m_plugin;   // không sở hữu
    PluginConfig        m_cfg;

    std::thread              m_thread;
    std::atomic<bool>        m_running{false};
    std::atomic<SourceStatus> m_status{SourceStatus::Stopped};

    mutable std::mutex m_errorMutex;
    std::string        m_error;

    // Bộ đệm của plugin: thành viên lớp, KHÔNG cấp phát trong vòng poll.
    float    m_angle[kMaxScanPoints]{};
    float    m_range[kMaxScanPoints]{};
    LaserScan m_scan;   // dùng lại mỗi scan; count giới hạn phần hợp lệ
};

} // namespace mikmap

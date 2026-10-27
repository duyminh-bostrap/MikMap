#include "io/sources/PluginSource.h"

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <utility>

namespace mikmap {

PluginSource::PluginSource(const SensorPlugin& plugin, PluginConfig cfg)
    : m_plugin(plugin), m_cfg(std::move(cfg)) {}

PluginSource::~PluginSource() { stop(); }

std::string PluginSource::lastError() const {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    return m_error;
}

void PluginSource::setError(const std::string& e) {
    std::lock_guard<std::mutex> lock(m_errorMutex);
    m_error = e;
}

std::string PluginSource::instanceError(void* inst) const {
    if (!m_plugin.error) return {};
    const char* e = m_plugin.error(inst);
    return e ? e : "";
}

bool PluginSource::start() {
    if (m_running.exchange(true)) return true;   // đã chạy

    setError({});
    m_status.store(SourceStatus::Connecting);
    m_thread = std::thread(&PluginSource::threadLoop, this);

    // Giống HokuyoSource: lỗi cấu hình/cắm muộn là tạm thời, thread tự thử
    // lại. Trả false ở đây sẽ khiến src/ đánh dấu nguồn hỏng vĩnh viễn.
    return true;
}

void PluginSource::stop() {
    if (!m_running.exchange(false)) return;
    if (m_thread.joinable()) m_thread.join();   // ABI: poll trả về trong ≤ 1 s
    m_status.store(SourceStatus::Stopped);
}

void PluginSource::waitReconnect() {
    const int totalMs = static_cast<int>(std::max(0.0, m_cfg.reconnectSec) * 1000.0);
    for (int waited = 0; waited < totalMs && m_running.load(); waited += 10)
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
}

void PluginSource::threadLoop() {
    while (m_running.load(std::memory_order_relaxed)) {
        m_status.store(SourceStatus::Connecting);

        void* inst = m_plugin.create(m_cfg.configJson.c_str());
        if (!inst) {
            setError("plugin rejected config");
            onLost();
            m_status.store(SourceStatus::Error);
            waitReconnect();
            continue;
        }

        if (m_plugin.start(inst) != 0) {
            setError(instanceError(inst));   // chuỗi của instance, phải chép trước destroy
            m_plugin.stop(inst);
            m_plugin.destroy(inst);
            onLost();
            m_status.store(SourceStatus::Error);
            waitReconnect();
            continue;
        }

        m_status.store(SourceStatus::Running);

        bool failed = false;
        while (m_running.load(std::memory_order_relaxed)) {
            const int n = m_plugin.poll(inst, m_angle, m_range, kMaxScanPoints);
            if (!m_running.load(std::memory_order_relaxed)) break;

            if (n < 0) {
                setError(instanceError(inst));
                failed = true;
                break;
            }
            if (n == 0) {
                // Hợp đồng ABI nói poll được phép chặn; plugin trả 0 ngay lập
                // tức (không chặn) thì đừng quay cuồng một core.
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
                continue;
            }

            const int count = std::min(n, kMaxScanPoints);
            for (int i = 0; i < count; ++i) {
                m_scan.angleRad[i] = m_angle[i];
                m_scan.rangeMm[i] = m_range[i];
            }
            m_scan.count = static_cast<uint16_t>(count);
            onScan(m_scan);
        }

        m_plugin.stop(inst);
        m_plugin.destroy(inst);
        if (failed) onLost();

        if (m_running.load(std::memory_order_relaxed)) {
            m_status.store(SourceStatus::Error);
            waitReconnect();
        }
    }

    m_status.store(SourceStatus::Stopped);
}

} // namespace mikmap

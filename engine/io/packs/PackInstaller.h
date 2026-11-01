// ════════════════════════════════════════════════════════════════════════
//  io/packs/PackInstaller.h — tải danh mục ký số Ed25519 + cài gói theo thứ tự
//
//  Toàn bộ mạng và đĩa chạy trên worker thread nội bộ; luồng giao diện chỉ
//  đọc snapshot() và catalog() (khoá mutex ngắn trên dữ liệu nhỏ).
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "io/packs/PackManifest.h"
#include "io/packs/PackStore.h"

#include <atomic>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>

namespace mikmap {

enum class PackJobState {
    Idle,
    RefreshingCatalog,
    Downloading,
    Verifying,
    Done,
    Failed,
    Cancelled
};

struct PackProgress {
    PackJobState state      = PackJobState::Idle;
    std::string  packId;
    std::string  fileName;
    uint64_t     doneBytes  = 0;
    uint64_t     totalBytes = 0;
    std::string  error;
};

class PackInstaller {
public:
    PackInstaller(PackStore& store, const uint8_t publicKey[32],
                  std::string catalogUrl, std::string appVersion = "1.0.0",
                  bool allowLoopbackHttp = false);
    ~PackInstaller();

    PackInstaller(const PackInstaller&) = delete;
    PackInstaller& operator=(const PackInstaller&) = delete;

    /// false nếu khoá công khai gồm 32 byte 0 (bản build dev chưa gắn khoá).
    bool keyConfigured() const { return m_keyConfigured; }

    void refreshCatalogAsync();
    PackCatalog catalog() const;

    void installAsync(const std::string& packId);
    void installFromFileAsync(const std::string& mikpackPathUtf8);
    void cancel();

    PackProgress snapshot() const;
    bool busy() const { return m_busy.load(std::memory_order_acquire); }

private:
    bool prepareWorker();
    void runRefreshCatalog();
    void runInstall(std::string packId);
    void runInstallFromFile(std::string pathUtf8);

    PackStore&        m_store;
    uint8_t           m_pubKey[32]{};
    bool              m_keyConfigured     = false;
    std::string       m_catalogUrl;
    std::string       m_appVersion;
    bool              m_allowLoopbackHttp = false;

    mutable std::mutex m_mu;
    PackCatalog        m_catalog;
    PackProgress       m_progress;
    std::atomic<bool>  m_cancel{false};
    std::atomic<bool>  m_busy{false};
    std::thread        m_worker;
};

namespace packs {
using mikmap::PackJobState;
using mikmap::PackProgress;
using mikmap::PackInstaller;
} // namespace packs

} // namespace mikmap

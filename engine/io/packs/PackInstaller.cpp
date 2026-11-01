#include "io/packs/PackInstaller.h"

#include "core/util/Ed25519.h"
#include "core/util/Sha2.h"
#include "io/packs/HttpGet.h"
#include "io/packs/MikPack.h"

#include <cstring>
#include <filesystem>
#include <vector>

namespace mikmap {

namespace {

namespace fs = std::filesystem;

fs::path fromUtf8(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}

bool decodeSignature(const std::string& raw, uint8_t out[64]) {
    if (raw.size() == 64) {
        std::memcpy(out, raw.data(), 64);
        return true;
    }
    std::string trimmed = raw;
    while (!trimmed.empty() &&
           (trimmed.back() == '\r' || trimmed.back() == '\n' ||
            trimmed.back() == ' '  || trimmed.back() == '\t')) {
        trimmed.pop_back();
    }
    if (trimmed.size() == 128) {
        return fromHex(trimmed, out, 64);
    }
    return false;
}

} // namespace

PackInstaller::PackInstaller(PackStore& store, const uint8_t publicKey[32],
                             std::string catalogUrl, std::string appVersion,
                             bool allowLoopbackHttp)
    : m_store(store),
      m_catalogUrl(std::move(catalogUrl)),
      m_appVersion(std::move(appVersion)),
      m_allowLoopbackHttp(allowLoopbackHttp) {
    if (publicKey) {
        std::memcpy(m_pubKey, publicKey, 32);
        for (int i = 0; i < 32; ++i) {
            if (m_pubKey[i] != 0) {
                m_keyConfigured = true;
                break;
            }
        }
    }
}

PackInstaller::~PackInstaller() {
    cancel();
    if (m_worker.joinable()) m_worker.join();
}

bool PackInstaller::prepareWorker() {
    if (m_busy.load(std::memory_order_acquire)) return false;
    {
        std::lock_guard<std::mutex> lk(m_mu);
        if (m_progress.state == PackJobState::RefreshingCatalog ||
            m_progress.state == PackJobState::Downloading ||
            m_progress.state == PackJobState::Verifying) {
            return false;
        }
    }
    if (m_worker.joinable()) m_worker.join();
    m_cancel.store(false);
    return true;
}

void PackInstaller::refreshCatalogAsync() {
    if (!prepareWorker()) return;
    {
        std::lock_guard<std::mutex> lk(m_mu);
        m_progress = PackProgress{};
        m_progress.state = PackJobState::RefreshingCatalog;
    }
    m_busy.store(true, std::memory_order_release);
    m_worker = std::thread([this]() { runRefreshCatalog(); m_busy.store(false, std::memory_order_release); });
}

PackCatalog PackInstaller::catalog() const {
    std::lock_guard<std::mutex> lk(m_mu);
    return m_catalog;
}

void PackInstaller::installAsync(const std::string& packId) {
    if (!prepareWorker()) return;
    {
        std::lock_guard<std::mutex> lk(m_mu);
        m_progress = PackProgress{};
        m_progress.state  = PackJobState::Downloading;
        m_progress.packId = packId;
    }
    m_busy.store(true, std::memory_order_release);
    m_worker = std::thread([this, packId]() { runInstall(packId); m_busy.store(false, std::memory_order_release); });
}

void PackInstaller::installFromFileAsync(const std::string& mikpackPathUtf8) {
    if (!prepareWorker()) return;
    {
        std::lock_guard<std::mutex> lk(m_mu);
        m_progress = PackProgress{};
        m_progress.state    = PackJobState::Verifying;
        m_progress.fileName = mikpackPathUtf8;
    }
    m_busy.store(true, std::memory_order_release);
    m_worker = std::thread([this, mikpackPathUtf8]() { runInstallFromFile(mikpackPathUtf8); m_busy.store(false, std::memory_order_release); });
}

void PackInstaller::cancel() {
    m_cancel.store(true);
}

PackProgress PackInstaller::snapshot() const {
    std::lock_guard<std::mutex> lk(m_mu);
    auto result = m_progress;
    if (m_busy.load(std::memory_order_acquire) && (result.state == PackJobState::Done || result.state == PackJobState::Failed || result.state == PackJobState::Cancelled)) result.state = PackJobState::Verifying;
    return result;
}

void PackInstaller::runRefreshCatalog() {
    auto setFailed = [this](const std::string& msg) {
        std::lock_guard<std::mutex> lk(m_mu);
        m_progress.state = m_cancel.load() ? PackJobState::Cancelled : PackJobState::Failed;
        m_progress.error = msg;
    };

    if (!m_keyConfigured || m_catalogUrl.empty()) {
        setFailed("Pack signing key or catalog URL is not configured");
        return;
    }

    HttpOptions catOpts;
    catOpts.allowLoopbackHttp = m_allowLoopbackHttp;
    catOpts.maxBytes          = packs::kMaxCatalogBytes;
    catOpts.cancel            = &m_cancel;

    std::string catText, err;
    if (!HttpGetToString(m_catalogUrl, catText, catOpts, err)) {
        setFailed("catalog download failed: " + err);
        return;
    }

    HttpOptions sigOpts = catOpts;
    sigOpts.maxBytes = 256;
    std::string sigRaw;
    if (!HttpGetToString(m_catalogUrl + ".sig", sigRaw, sigOpts, err)) {
        setFailed("catalog signature download failed: " + err);
        return;
    }

    uint8_t sig[64]{};
    if (!decodeSignature(sigRaw, sig)) {
        setFailed("invalid catalog signature format");
        return;
    }

    if (!ed25519Verify(sig, reinterpret_cast<const uint8_t*>(catText.data()), catText.size(), m_pubKey)) {
        setFailed("catalog signature verification failed");
        return;
    }

    PackCatalog parsed;
    if (!packs::parseCatalog(catText, parsed, err)) {
        setFailed(err);
        return;
    }

    std::lock_guard<std::mutex> lk(m_mu);
    m_catalog        = std::move(parsed);
    m_progress.state = PackJobState::Done;
    m_progress.error.clear();
}

void PackInstaller::runInstall(std::string packId) {
    auto setTerminal = [this](PackJobState st, const std::string& msg) {
        std::lock_guard<std::mutex> lk(m_mu);
        m_progress.state = st;
        m_progress.error = msg;
    };

    if (!m_keyConfigured || m_catalogUrl.empty()) {
        setTerminal(PackJobState::Failed, "Pack signing key or catalog URL is not configured");
        return;
    }

    PackCatalog cat;
    {
        std::lock_guard<std::mutex> lk(m_mu);
        cat = m_catalog;
    }
    if (cat.packs.empty()) {
        runRefreshCatalog();
        std::lock_guard<std::mutex> lk(m_mu);
        if (m_progress.state != PackJobState::Done) return;
        cat = m_catalog;
    }

    std::vector<std::string> order;
    std::string err;
    if (!packs::resolveInstallOrder(cat, packId, order, err)) {
        setTerminal(PackJobState::Failed, err);
        return;
    }

    uint64_t overallTotal = 0;
    for (const std::string& id : order) {
        const PackEntry* e = packs::findPack(cat, id);
        if (!e) {
            setTerminal(PackJobState::Failed, "unknown pack '" + id + "'");
            return;
        }
        if (m_store.isInstalled(id, e->version)) continue;
        if (!m_store.canUpdate(id, err)) { setTerminal(PackJobState::Failed, err); return; }
        if (!e->minAppVersion.empty() && packs::compareVersions(m_appVersion, e->minAppVersion) < 0) {
            setTerminal(PackJobState::Failed,
                        "pack '" + e->id + "' requires app >= " + e->minAppVersion + " (current " + m_appVersion + ")");
            return;
        }
        overallTotal += e->sizeBytes;
    }

    {
        std::lock_guard<std::mutex> lk(m_mu);
        m_progress.totalBytes = overallTotal;
        m_progress.doneBytes  = 0;
    }

    uint64_t cumulativeDone = 0;
    for (const std::string& id : order) {
        const PackEntry* e = packs::findPack(cat, id);
        if (!e || m_store.isInstalled(id, e->version)) continue;

        const std::string stageDir = m_store.stagingDir(e->id, e->version);
        auto cleanupStage = [&]() {
            std::error_code ec;
            fs::remove_all(fromUtf8(stageDir), ec);
        };

        for (const PackFile& fe : e->files) {
            if (m_cancel.load()) {
                cleanupStage();
                setTerminal(PackJobState::Cancelled, "cancelled");
                return;
            }
            if (fe.url.empty()) {
                cleanupStage();
                setTerminal(PackJobState::Failed, "pack '" + e->id + "' missing URL for '" + fe.path + "'");
                return;
            }

            {
                std::lock_guard<std::mutex> lk(m_mu);
                m_progress.state      = PackJobState::Downloading;
                m_progress.packId     = e->id;
                m_progress.fileName   = fe.path;
                m_progress.doneBytes  = cumulativeDone;
                m_progress.totalBytes = overallTotal;
            }

            HttpOptions opts;
            opts.allowLoopbackHttp = m_allowLoopbackHttp;
            opts.maxBytes          = fe.size > 0 ? fe.size : packs::kMaxFileBytes;
            opts.cancel            = &m_cancel;

            const std::string dstFile = stageDir + "/" + fe.path;
            const bool dlOk = HttpGetToFile(
                fe.url, dstFile, opts,
                [this, cumulativeDone, overallTotal](uint64_t fileDone, uint64_t) {
                    std::lock_guard<std::mutex> lk(m_mu);
                    m_progress.doneBytes  = cumulativeDone + fileDone;
                    m_progress.totalBytes = overallTotal;
                },
                err);

            if (!dlOk) {
                cleanupStage();
                if (m_cancel.load()) setTerminal(PackJobState::Cancelled, "cancelled");
                else setTerminal(PackJobState::Failed, err);
                return;
            }

            {
                std::lock_guard<std::mutex> lk(m_mu);
                m_progress.state    = PackJobState::Verifying;
                m_progress.packId   = e->id;
                m_progress.fileName = fe.path;
            }

            Sha256Digest digest{};
            if (!sha256File(dstFile, digest, &m_cancel)) {
                cleanupStage();
                if (m_cancel.load()) setTerminal(PackJobState::Cancelled, "cancelled");
                else setTerminal(PackJobState::Failed, "cannot hash '" + fe.path + "'");
                return;
            }
            if (toHex(digest.data(), digest.size()) != fe.sha256Hex) {
                cleanupStage();
                setTerminal(PackJobState::Failed, "sha256 mismatch for '" + fe.path + "'");
                return;
            }

            cumulativeDone += fe.size;
            {
                std::lock_guard<std::mutex> lk(m_mu);
                m_progress.doneBytes = cumulativeDone;
            }
        }

        if (!m_store.commit(*e, err)) {
            cleanupStage();
            setTerminal(PackJobState::Failed, err);
            return;
        }
    }

    std::lock_guard<std::mutex> lk(m_mu);
    m_progress.state      = PackJobState::Done;
    m_progress.packId     = packId;
    m_progress.doneBytes  = overallTotal;
    m_progress.totalBytes = overallTotal;
    m_progress.error.clear();
}

void PackInstaller::runInstallFromFile(std::string pathUtf8) {
    if (!m_keyConfigured) {
        std::lock_guard<std::mutex> lk(m_mu);
        m_progress.state = PackJobState::Failed;
        m_progress.error = "Pack signing key is not configured";
        return;
    }
    PackEntry installed;
    std::string err;
    if (!InstallMikPackFile(pathUtf8, m_pubKey, m_store, installed, err)) {
        std::lock_guard<std::mutex> lk(m_mu);
        m_progress.state = PackJobState::Failed;
        m_progress.error = err;
        return;
    }
    std::lock_guard<std::mutex> lk(m_mu);
    m_progress.state      = PackJobState::Done;
    m_progress.packId     = installed.id;
    m_progress.doneBytes  = installed.sizeBytes;
    m_progress.totalBytes = installed.sizeBytes;
    m_progress.error.clear();
}

} // namespace mikmap

#include "io/packs/PackStore.h"

#include "core/util/Json.h"
#include "core/util/Sha2.h"

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace mikmap::packs {

namespace {

namespace fs = std::filesystem;

fs::path fromUtf8(const std::string& s) {
    return fs::path(std::u8string(s.begin(), s.end()));
}

std::string toUtf8(const fs::path& p) {
    const std::u8string u = p.u8string();
    return std::string(u.begin(), u.end());
}

bool fail(std::string& err, const std::string& msg) {
    err = msg;
    return false;
}

bool readFileText(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    std::ostringstream ss;
    ss << f.rdbuf();
    if (!f && !f.eof()) return false;
    out = ss.str();
    return true;
}

bool writeTextAtomic(const fs::path& dst, const std::string& text, std::string& errOut) {
    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    const fs::path tmp = dst.wstring() + L".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) return fail(errOut, "packstore: cannot open temp file for writing");
        f.write(text.data(), static_cast<std::streamsize>(text.size()));
        f.flush();
        if (!f) {
            f.close();
            fs::remove(tmp, ec);
            return fail(errOut, "packstore: failed writing temp file");
        }
    }
    fs::rename(tmp, dst, ec);
    if (ec) {
        fs::remove(dst, ec);
        ec.clear();
        fs::rename(tmp, dst, ec);
        if (ec) {
            fs::remove(tmp, ec);
            return fail(errOut, "packstore: atomic rename failed: " + ec.message());
        }
    }
    return true;
}

bool hasSymlinkAlong(const fs::path& base, const std::string& relUtf8) {
    std::error_code ec;
    if (fs::is_symlink(base, ec)) return true;
    fs::path cur = base;
    size_t start = 0;
    while (start <= relUtf8.size()) {
        const size_t slash = relUtf8.find('/', start);
        const std::string comp = relUtf8.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        if (!comp.empty()) {
            cur /= fromUtf8(comp);
            if (fs::is_symlink(cur, ec)) return true;
        }
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return false;
}

void removeEmptyParent(const fs::path& dir, const fs::path& stopAt) {
    std::error_code ec;
    const fs::path parent = dir.parent_path();
    if (parent != stopAt && fs::exists(parent, ec) && fs::is_empty(parent, ec)) {
        fs::remove(parent, ec);
    }
}

} // namespace

PackStore::PackStore(std::string rootDirUtf8) : m_root(std::move(rootDirUtf8)) {}

bool PackStore::startup(std::string& errOut) {
    std::lock_guard<std::recursive_mutex> lk(m_mu);
    errOut.clear();
    m_installed.clear();
    m_pendingDirs.clear();

    const fs::path root = fromUtf8(m_root);
    const fs::path staging = root / "staging";
    const fs::path installedDir = root / "installed";

    std::error_code ec;
    fs::create_directories(root, ec);
    if (ec) return fail(errOut, "packstore: cannot create root dir: " + ec.message());

    // 1. Dọn sạch staging/
    fs::remove_all(staging, ec);
    ec.clear();
    fs::create_directories(staging, ec);
    fs::create_directories(installedDir, ec);

    // 2. Xử lý pending.json (xoá các thư mục đã đánh dấu từ phiên trước)
    const fs::path pendingPath = root / "pending.json";
    if (fs::exists(pendingPath, ec)) {
        std::string ptext;
        JsonValue pv;
        std::string jerr;
        if (readFileText(pendingPath, ptext) && JsonValue::parse(ptext, pv, jerr)) {
            const std::vector<JsonValue>* items = nullptr;
            if (pv.isArray()) items = &pv.arrayItems();
            else if (pv.isObject() && pv.has("remove") && pv["remove"].isArray()) items = &pv["remove"].arrayItems();
            if (items) {
                for (const JsonValue& it : *items) {
                    if (!it.isString()) continue;
                    const std::string dUtf8 = it.asString();
                    const fs::path dp = fromUtf8(dUtf8);
                    std::error_code rec;
                    fs::remove_all(dp, rec);
                    if (fs::exists(dp, rec)) {
                        m_pendingDirs.push_back(dUtf8);
                    } else {
                        removeEmptyParent(dp, installedDir);
                    }
                }
            }
        }
        std::string dummyErr;
        savePendingLocked(dummyErr);
    }

    // 3. Đọc installed.json
    const fs::path instPath = root / "installed.json";
    if (!fs::exists(instPath, ec)) return true;

    std::string text;
    JsonValue iv;
    std::string jerr;
    bool valid = readFileText(instPath, text) && JsonValue::parse(text, iv, jerr) &&
                 iv.isObject() && iv.has("installed") && iv["installed"].isArray();

    std::vector<InstalledPack> loaded;
    bool prunedPending = false;
    if (valid) {
        for (const JsonValue& item : iv["installed"].arrayItems()) {
            if (!item.isObject() || !item.has("entry") || !item.has("dir") || !item["dir"].isString()) {
                valid = false;
                break;
            }
            InstalledPack ip;
            std::string perr;
            if (!parsePackInfo(item["entry"], ip.entry, perr)) {
                valid = false;
                break;
            }
            ip.dir = item["dir"].asString();
            ip.pendingRemoval = item["pendingRemoval"].asBool(false);

            const bool stillPending = std::find(m_pendingDirs.begin(), m_pendingDirs.end(), ip.dir) != m_pendingDirs.end();
            if (ip.pendingRemoval && !stillPending && !fs::exists(fromUtf8(ip.dir), ec)) {
                // Đã xoá thành công ở bước 2 bên trên
                prunedPending = true;
                continue;
            }
            if (stillPending) ip.pendingRemoval = true;
            loaded.push_back(std::move(ip));
        }
    }

    if (!valid) {
        const fs::path badPath = root / "installed.json.bad";
        fs::remove(badPath, ec);
        ec.clear();
        fs::rename(instPath, badPath, ec);
        m_installed.clear();
        errOut = "installed.json was corrupt and has been backed up to installed.json.bad";
        return true;
    }

    m_installed = std::move(loaded);
    if (prunedPending) {
        std::string saveErr;
        saveInstalledLocked(saveErr);
    }
    return true;
}

std::vector<InstalledPack> PackStore::list() const {
    std::lock_guard<std::recursive_mutex> lk(m_mu);
    return m_installed;
}

const InstalledPack* PackStore::find(const std::string& id) const {
    std::lock_guard<std::recursive_mutex> lk(m_mu);
    for (const InstalledPack& ip : m_installed) {
        if (ip.entry.id == id) return &ip;
    }
    return nullptr;
}

bool PackStore::isInstalled(const std::string& id, const std::string& minVersion) const {
    std::lock_guard<std::recursive_mutex> lk(m_mu);
    for (const InstalledPack& ip : m_installed) {
        if (ip.entry.id != id) continue;
        if (ip.pendingRemoval) return false;
        if (!minVersion.empty() && compareVersions(ip.entry.version, minVersion) < 0) return false;
        return true;
    }
    return false;
}

std::string PackStore::stagingDir(const std::string& id, const std::string& version) const {
    const fs::path stage = fromUtf8(m_root) / "staging" / fromUtf8(id + "-" + version);
    std::error_code ec;
    fs::remove_all(stage, ec);
    ec.clear();
    fs::create_directories(stage, ec);
    return toUtf8(stage);
}

void PackStore::protectUpdates(std::vector<std::string> ids) {
    std::lock_guard<std::recursive_mutex> lock(m_mu); m_protected = std::move(ids);
}
bool PackStore::canUpdate(const std::string& id, std::string& errOut) const {
    std::lock_guard<std::recursive_mutex> lock(m_mu);
    if (std::find(m_protected.begin(), m_protected.end(), id) == m_protected.end()) return true;
    return fail(errOut, "Pack '" + id + "' is in use or loaded; turn sensors off and restart before updating");
}
bool PackStore::commit(const PackInfo& entry, std::string& errOut) {
    errOut.clear();
    // Kiểm lại entry thông qua parsePackInfo(toJson(entry)) để bảo đảm mọi bất biến
    PackInfo checked;
    if (!parsePackInfo(toJson(entry), checked, errOut)) return false;

    const fs::path root = fromUtf8(m_root);
    const fs::path stage = root / "staging" / fromUtf8(entry.id + "-" + entry.version);
    std::error_code ec;
    if (!fs::exists(stage, ec) || fs::is_symlink(stage, ec))
        return fail(errOut, "packstore: staging dir missing for '" + entry.id + "'");

    auto cleanupStage = [&]() {
        std::error_code cec;
        fs::remove_all(stage, cec);
    };

    // Kiểm tra kích thước và SHA-256 từng file trong staging trước khi giữ khoá
    for (const FileEntry& fe : entry.files) {
        if (!isSafeRelPath(fe.path)) {
            cleanupStage();
            return fail(errOut, "packstore: unsafe file path '" + fe.path + "'");
        }
        if (hasSymlinkAlong(stage, fe.path)) {
            cleanupStage();
            return fail(errOut, "packstore: symlink rejected at '" + fe.path + "'");
        }
        const fs::path fp = stage / fromUtf8(fe.path);
        if (!fs::is_regular_file(fp, ec)) {
            cleanupStage();
            return fail(errOut, "packstore: missing staged file '" + fe.path + "'");
        }
        const uint64_t actualSize = fs::file_size(fp, ec);
        if (ec || actualSize != fe.size) {
            cleanupStage();
            return fail(errOut, "packstore: size mismatch for '" + fe.path + "'");
        }
        Sha256Digest digest{};
        if (!sha256File(toUtf8(fp), digest)) {
            cleanupStage();
            return fail(errOut, "packstore: cannot hash '" + fe.path + "'");
        }
        const std::string gotHex = toHex(digest.data(), digest.size());
        if (gotHex != fe.sha256Hex) {
            cleanupStage();
            return fail(errOut, "packstore: sha256 mismatch for '" + fe.path + "'");
        }
    }

    // Ghi pack.json vào staging
    const std::string packJsonText = toJson(entry).dump(2) + "\n";
    if (!writeTextAtomic(stage / "pack.json", packJsonText, errOut)) {
        cleanupStage();
        return false;
    }

    // Đổi tên nguyên tử sang installed/<id>/<version>
    std::lock_guard<std::recursive_mutex> lk(m_mu);
    if (!canUpdate(entry.id, errOut)) { cleanupStage(); return false; }
    const fs::path installedBase = root / "installed" / fromUtf8(entry.id);
    const fs::path dst = installedBase / fromUtf8(entry.version);
    fs::create_directories(installedBase, ec);
    if (ec) {
        cleanupStage();
        return fail(errOut, "packstore: cannot create installed dir: " + ec.message());
    }
    if (fs::exists(dst, ec)) {
        fs::remove_all(dst, ec);
        if (ec) {
            cleanupStage();
            return fail(errOut, "packstore: cannot replace existing version dir: " + ec.message());
        }
    }
    fs::rename(stage, dst, ec);
    if (ec) {
        cleanupStage();
        return fail(errOut, "packstore: rename staging to installed failed: " + ec.message());
    }

    const std::string dstUtf8 = toUtf8(dst);

    // Nếu đã có bản cũ của cùng id: thử xoá bản cũ hoặc đưa vào pending
    bool replaced = false;
    for (InstalledPack& ip : m_installed) {
        if (ip.entry.id == entry.id) {
            if (ip.dir != dstUtf8) {
                std::error_code rec;
                const fs::path oldPath = fromUtf8(ip.dir);
                fs::remove_all(oldPath, rec);
                if (fs::exists(oldPath, rec)) {
                    if (std::find(m_pendingDirs.begin(), m_pendingDirs.end(), ip.dir) == m_pendingDirs.end())
                        m_pendingDirs.push_back(ip.dir);
                    std::string perr;
                    savePendingLocked(perr);
                }
            }
            ip.entry = entry;
            ip.dir = dstUtf8;
            ip.pendingRemoval = false;
            replaced = true;
            break;
        }
    }
    if (!replaced) {
        InstalledPack ip;
        ip.entry = entry;
        ip.dir = dstUtf8;
        ip.pendingRemoval = false;
        m_installed.push_back(std::move(ip));
    }

    return saveInstalledLocked(errOut);
}

bool PackStore::remove(const std::string& id, bool loadedInProcess, std::string& errOut) {
    std::lock_guard<std::recursive_mutex> lk(m_mu);
    errOut.clear();
    auto it = std::find_if(m_installed.begin(), m_installed.end(),
                           [&](const InstalledPack& ip) { return ip.entry.id == id; });
    if (it == m_installed.end()) return fail(errOut, "packstore: pack '" + id + "' is not installed");

    const fs::path installedRoot = fromUtf8(m_root) / "installed";
    const fs::path dirPath = fromUtf8(it->dir);

    if (!loadedInProcess) {
        std::error_code ec;
        fs::remove_all(dirPath, ec);
        if (!ec && !fs::exists(dirPath, ec)) {
            removeEmptyParent(dirPath, installedRoot);
            m_installed.erase(it);
            return saveInstalledLocked(errOut);
        }
    }

    // Đang nạp trong tiến trình hoặc file bị giữ bởi OS -> xoá ở lần startup kế tiếp
    it->pendingRemoval = true;
    if (std::find(m_pendingDirs.begin(), m_pendingDirs.end(), it->dir) == m_pendingDirs.end())
        m_pendingDirs.push_back(it->dir);
    if (!savePendingLocked(errOut)) return false;
    return saveInstalledLocked(errOut);
}

bool PackStore::quickVerify(const std::string& id, std::string& errOut) const {
    InstalledPack ip;
    {
        std::lock_guard<std::recursive_mutex> lk(m_mu);
        const InstalledPack* found = find(id);
        if (!found) return fail(errOut, "packstore: pack '" + id + "' not installed");
        ip = *found;
    }
    const fs::path dirPath = fromUtf8(ip.dir);
    std::error_code ec;
    if (!fs::exists(dirPath, ec)) return fail(errOut, "packstore: directory missing for '" + id + "'");
    for (const FileEntry& fe : ip.entry.files) {
        if (hasSymlinkAlong(dirPath, fe.path))
            return fail(errOut, "packstore: symlink detected in '" + id + "': " + fe.path);
        const fs::path fp = dirPath / fromUtf8(fe.path);
        if (!fs::is_regular_file(fp, ec))
            return fail(errOut, "packstore: missing file '" + fe.path + "' in '" + id + "'");
        const uint64_t sz = fs::file_size(fp, ec);
        if (ec || sz != fe.size)
            return fail(errOut, "packstore: size mismatch for '" + fe.path + "' in '" + id + "'");
    }
    return true;
}

bool PackStore::deepVerify(const std::string& id, std::string& errOut) const {
    if (!quickVerify(id, errOut)) return false;
    InstalledPack ip;
    {
        std::lock_guard<std::recursive_mutex> lk(m_mu);
        const InstalledPack* found = find(id);
        if (!found) return fail(errOut, "packstore: pack '" + id + "' not installed");
        ip = *found;
    }
    const fs::path dirPath = fromUtf8(ip.dir);
    for (const FileEntry& fe : ip.entry.files) {
        const fs::path fp = dirPath / fromUtf8(fe.path);
        Sha256Digest digest{};
        if (!sha256File(toUtf8(fp), digest))
            return fail(errOut, "packstore: cannot read '" + fe.path + "' in '" + id + "'");
        if (toHex(digest.data(), digest.size()) != fe.sha256Hex)
            return fail(errOut, "packstore: sha256 mismatch for '" + fe.path + "' in '" + id + "'");
    }
    return true;
}

bool PackStore::saveInstalledLocked(std::string& errOut) const {
    JsonValue root = JsonValue::object();
    JsonValue arr = JsonValue::array();
    for (const InstalledPack& ip : m_installed) {
        JsonValue item = JsonValue::object();
        item.set("entry", toJson(ip.entry));
        item.set("dir", ip.dir);
        if (ip.pendingRemoval) item.set("pendingRemoval", true);
        arr.push(std::move(item));
    }
    root.set("installed", std::move(arr));
    return writeTextAtomic(fromUtf8(m_root) / "installed.json", root.dump(2) + "\n", errOut);
}

bool PackStore::savePendingLocked(std::string& errOut) const {
    const fs::path pendingPath = fromUtf8(m_root) / "pending.json";
    if (m_pendingDirs.empty()) {
        std::error_code ec;
        fs::remove(pendingPath, ec);
        return true;
    }
    JsonValue root = JsonValue::object();
    JsonValue arr = JsonValue::array();
    for (const std::string& d : m_pendingDirs) arr.push(d);
    root.set("remove", std::move(arr));
    return writeTextAtomic(pendingPath, root.dump(2) + "\n", errOut);
}

} // namespace mikmap::packs

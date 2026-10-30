// Sensor packs (P1-P7): app-side PackManager bridging PackStore + PackInstaller to the UI and live sensor threads.
// All network and disk writes run on PackInstaller's worker thread; the render thread only reads cached state.
#include "app.h"

#include "core/util/Sha2.h"
#include "io/packs/PackInstaller.h"
#include "io/packs/PackManifest.h"
#include "io/packs/PackStore.h"
#include "io/plugin/PluginLoader.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <memory>
#include <set>
#include <string>
#include <vector>

#ifndef MIKMAP_PACK_PUBKEY_HEX
#  define MIKMAP_PACK_PUBKEY_HEX ""
#endif
#ifndef MIKMAP_PACK_CATALOG_URL
#  define MIKMAP_PACK_CATALOG_URL ""
#endif

namespace {

struct PackManagerState {
  std::string rootDir;
  uint8_t pubKey[32]{};
  bool hasKey = false;
  std::string catalogUrl;
  bool allowLoopbackHttp = false;

  std::unique_ptr<mikmap::PackStore> store;
  std::unique_ptr<mikmap::PackInstaller> installer;
  mikmap::PackJobState lastState = mikmap::PackJobState::Idle;
  std::string lastError;
  std::string lastFailedPackId;
  std::set<std::string> loadedDirs;
  std::string demoMode;
};

PackManagerState g_pm;

bool ParseCompileTimePubKey(uint8_t out[32]) {
  std::memset(out, 0, 32);
  const std::string hex = MIKMAP_PACK_PUBKEY_HEX;
  if (hex.size() != 64) return false;
  if (!mikmap::fromHex(hex, out, 32)) {
    std::memset(out, 0, 32);
    return false;
  }
  for (int i = 0; i < 32; ++i) {
    if (out[i] != 0) return true;
  }
  return false;
}

std::string BaseNameOf(const std::string& p) {
  const size_t k = p.find_last_of("/\\");
  return k == std::string::npos ? p : p.substr(k + 1);
}

bool PathStartsWithDir(const std::string& path, const std::string& dir) {
  if (dir.empty() || path.size() <= dir.size()) return false;
  for (size_t i = 0; i < dir.size(); ++i) {
    char a = path[i] == '\\' ? '/' : path[i];
    char b = dir[i] == '\\' ? '/' : dir[i];
    if (a != b) return false;
  }
  char sep = path[dir.size()];
  return sep == '/' || sep == '\\';
}

bool PackInUseByDevices(const std::string& id, const mikmap::InstalledPack* inst) {
  for (const auto& d : A.devices) {
    if (!d.enabled) continue;
    if (d.needsPack == id) return true;
    if (inst && d.kind == DevKind::Plugin && !d.plugin.empty()) {
      for (const auto& f : inst->entry.files) {
        if (BaseNameOf(f.path) == d.plugin) return true;
      }
    }
  }
  return false;
}

void StartWithConfig(const std::string& rootDir, const uint8_t pubKey[32],
                     const std::string& catalogUrl, bool allowLoopback) {
  g_pm.installer.reset();
  g_pm.store.reset();
  g_pm.lastState = mikmap::PackJobState::Idle;
  g_pm.lastError.clear();
  g_pm.lastFailedPackId.clear();

  g_pm.rootDir = rootDir;
  std::memset(g_pm.pubKey, 0, 32);
  g_pm.hasKey = false;
  if (pubKey) {
    std::memcpy(g_pm.pubKey, pubKey, 32);
    for (int i = 0; i < 32; ++i) {
      if (g_pm.pubKey[i] != 0) { g_pm.hasKey = true; break; }
    }
  }
  g_pm.catalogUrl = catalogUrl;
  g_pm.allowLoopbackHttp = allowLoopback;

  g_pm.store = std::make_unique<mikmap::PackStore>(g_pm.rootDir);
  std::string err;
  g_pm.store->startup(err);
  if (!err.empty()) A.notify("Sensor packs: " + err);

  const auto installed = g_pm.store->list();
  for (const auto& p : installed) {
    if (p.pendingRemoval) continue;
    std::string verr;
    if (!g_pm.store->quickVerify(p.entry.id, verr)) {
      std::string rerr;
      g_pm.store->remove(p.entry.id, false, rerr);
      A.notify("Removed damaged pack '" + p.entry.id + "': " + verr);
    }
  }

  g_pm.installer = std::make_unique<mikmap::PackInstaller>(
      *g_pm.store, g_pm.pubKey, g_pm.catalogUrl, "1.0.0", g_pm.allowLoopbackHttp);
  RescanSensorPlugins();
}

}  // namespace

void PacksStartup() {
  uint8_t key[32]{};
  ParseCompileTimePubKey(key);
  StartWithConfig(PacksDir(), key, MIKMAP_PACK_CATALOG_URL, false);
}

void PacksShutdown() {
  g_pm.installer.reset();
  g_pm.store.reset();
  g_pm.loadedDirs.clear();
}

void PacksResetForTest(const std::string& rootDir, const uint8_t pubKey[32],
                       const std::string& catalogUrl, bool allowLoopbackHttp) {
  g_pm.loadedDirs.clear();
  g_pm.demoMode.clear();
  StartWithConfig(rootDir, pubKey, catalogUrl, allowLoopbackHttp);
}

void SetPacksDemoMode(const std::string& mode) { g_pm.demoMode = mode; }
const std::string& PacksDemoMode() { return g_pm.demoMode; }

bool PackKeyConfigured() {
  if (!g_pm.demoMode.empty()) return g_pm.demoMode != "unconfigured";
  return g_pm.installer && g_pm.installer->keyConfigured() && !g_pm.catalogUrl.empty();
}

void MarkPackDirLoaded(const std::string& dir) {
  if (!dir.empty()) g_pm.loadedDirs.insert(dir);
}

bool IsPackDirLoaded(const std::string& dir) {
  if (g_pm.loadedDirs.count(dir) > 0) return true;
  for (const auto& p : SensorPlugins()) {
    if (PathStartsWithDir(p.path, dir)) return true;
  }
  return false;
}

std::vector<std::string> InstalledPluginPackDirs() {
  std::vector<std::string> dirs;
  if (!g_pm.store) return dirs;
  for (const auto& p : g_pm.store->list()) {
    if (p.pendingRemoval) continue;
    if (p.entry.kind == mikmap::PackKind::Plugin || p.entry.kind == mikmap::PackKind::Runtime) {
      dirs.push_back(p.dir);
    }
  }
  return dirs;
}

void PacksTick() {
  if (!g_pm.installer) return;
  const mikmap::PackProgress snap = g_pm.installer->snapshot();
  if (snap.state != g_pm.lastState) {
    const bool wasBusy = (g_pm.lastState == mikmap::PackJobState::RefreshingCatalog ||
                          g_pm.lastState == mikmap::PackJobState::Downloading ||
                          g_pm.lastState == mikmap::PackJobState::Verifying);
    g_pm.lastState = snap.state;
    if (wasBusy && snap.state == mikmap::PackJobState::Done) {
      g_pm.lastError.clear();
      g_pm.lastFailedPackId.clear();
      RescanSensorPlugins();
      if (!snap.packId.empty()) A.notify("Pack installed: " + snap.packId);
      else A.notify("Sensor pack catalog updated");
    } else if (snap.state == mikmap::PackJobState::Failed) {
      g_pm.lastError = snap.error;
      g_pm.lastFailedPackId = snap.packId;
      if (!snap.error.empty()) A.notify("Pack error: " + snap.error);
    }
  }
}

void PackRefreshCatalog() {
  if (!g_pm.installer) return;
  const auto snap = g_pm.installer->snapshot();
  if (snap.state == mikmap::PackJobState::RefreshingCatalog ||
      snap.state == mikmap::PackJobState::Downloading ||
      snap.state == mikmap::PackJobState::Verifying) {
    A.notify("Another pack operation is already in progress");
    return;
  }
  g_pm.lastError.clear();
  g_pm.lastFailedPackId.clear();
  g_pm.installer->refreshCatalogAsync();
}

void PackInstall(const std::string& id) {
  if (!g_pm.installer) return;
  const auto snap = g_pm.installer->snapshot();
  if (snap.state == mikmap::PackJobState::RefreshingCatalog ||
      snap.state == mikmap::PackJobState::Downloading ||
      snap.state == mikmap::PackJobState::Verifying) {
    A.notify("Another pack operation is already in progress");
    return;
  }
  g_pm.lastError.clear();
  g_pm.lastFailedPackId = id;
  g_pm.installer->installAsync(id);
}

void PackInstallFromFile(const std::string& path) {
  if (!g_pm.installer) return;
  if (path.empty()) {
    A.notify("Enter a .mikpack file path first");
    return;
  }
  const auto snap = g_pm.installer->snapshot();
  if (snap.state == mikmap::PackJobState::RefreshingCatalog ||
      snap.state == mikmap::PackJobState::Downloading ||
      snap.state == mikmap::PackJobState::Verifying) {
    A.notify("Another pack operation is already in progress");
    return;
  }
  g_pm.lastError.clear();
  g_pm.lastFailedPackId.clear();
  g_pm.installer->installFromFileAsync(path);
}

void PackCancel() {
  if (g_pm.installer) g_pm.installer->cancel();
}

bool PackRemove(const std::string& id) {
  if (!g_pm.store) return false;
  const mikmap::InstalledPack* inst = g_pm.store->find(id);
  if (!inst) return false;
  if (PackInUseByDevices(id, inst)) {
    A.notify("Turn the sensor off first");
    return false;
  }
  const bool loaded = IsPackDirLoaded(inst->dir);
  std::string err;
  if (!g_pm.store->remove(id, loaded, err)) {
    A.notify("Cannot remove pack: " + err);
    return false;
  }
  if (loaded) A.notify("Pack '" + id + "' will be removed when MikMap restarts");
  else A.notify("Pack removed: " + id);
  return true;
}

void PackVerifyAll() {
  if (!g_pm.store) return;
  const auto list = g_pm.store->list();
  if (list.empty()) {
    A.notify("No installed packs to verify");
    return;
  }
  int okCount = 0;
  for (const auto& p : list) {
    if (p.pendingRemoval) continue;
    std::string err;
    if (!g_pm.store->deepVerify(p.entry.id, err)) {
      g_pm.lastError = "Verify failed for '" + p.entry.id + "': " + err;
      g_pm.lastFailedPackId = p.entry.id;
      A.notify(g_pm.lastError);
      return;
    }
    ++okCount;
  }
  A.notify("Verified " + std::to_string(okCount) + " installed pack(s) — all SHA-256 hashes match");
}

std::vector<PackRow> PackRows() {
  if (!g_pm.demoMode.empty()) {
    std::vector<PackRow> rows;
    if (g_pm.demoMode == "empty") {
      rows.push_back({"orbbec-femto", "Orbbec Femto Bolt / Mega Depth", "1.2.0", "", "", 18ull * 1024 * 1024, PackState::NotInstalled, false, false, false, 0.f});
      rows.push_back({"pose-onnx", "MediaPipe Pose + ONNX Runtime", "1.4.0", "", "", 180ull * 1024 * 1024, PackState::NotInstalled, false, false, false, 0.f});
      rows.push_back({"realsense-d435", "Intel RealSense D435 / D455", "1.1.0", "", "signature check failed: catalog.json.sig", 24ull * 1024 * 1024, PackState::Failed, false, false, false, 0.f});
    } else if (g_pm.demoMode == "installed") {
      rows.push_back({"orbbec-femto", "Orbbec Femto Bolt / Mega Depth", "1.2.0", "1.2.0", "", 18ull * 1024 * 1024, PackState::Installed, true, true, false, 1.f});
      rows.push_back({"pose-onnx", "MediaPipe Pose + ONNX Runtime", "1.4.0", "1.3.0", "", 180ull * 1024 * 1024, PackState::UpdateAvailable, false, false, false, 0.f});
      rows.push_back({"realsense-d435", "Intel RealSense D435 / D455", "1.1.0", "1.1.0", "", 24ull * 1024 * 1024, PackState::Installed, false, true, true, 0.f});
    } else if (g_pm.demoMode == "downloading") {
      rows.push_back({"orbbec-femto", "Orbbec Femto Bolt / Mega Depth", "1.2.0", "", "", 18ull * 1024 * 1024, PackState::Downloading, false, false, false, 0.42f});
      rows.push_back({"pose-onnx", "MediaPipe Pose + ONNX Runtime", "1.4.0", "", "", 180ull * 1024 * 1024, PackState::NotInstalled, false, false, false, 0.f});
      rows.push_back({"realsense-d435", "Intel RealSense D435 / D455", "1.1.0", "1.1.0", "", 24ull * 1024 * 1024, PackState::Installed, false, false, false, 1.f});
    } else {
      rows.push_back({"orbbec-femto", "Orbbec Femto Bolt / Mega Depth", "1.2.0", "", "", 18ull * 1024 * 1024, PackState::Unavailable, false, false, false, 0.f});
    }
    return rows;
  }

  std::vector<PackRow> out;
  if (!g_pm.store || !g_pm.installer) return out;

  const auto installed = g_pm.store->list();
  const auto cat = g_pm.installer->catalog();
  const auto snap = g_pm.installer->snapshot();
  const bool keyOk = PackKeyConfigured();

  std::set<std::string> seen;
  for (const auto& ce : cat.packs) {
    seen.insert(ce.id);
    PackRow r;
    r.id = ce.id;
    r.name = ce.name;
    r.version = ce.version;
    r.sizeBytes = ce.sizeBytes;

    const mikmap::InstalledPack* inst = nullptr;
    for (const auto& ip : installed) {
      if (ip.entry.id == ce.id) { inst = &ip; break; }
    }
    if (inst) {
      r.installedVersion = inst->entry.version;
      r.pendingRemoval = inst->pendingRemoval;
      r.loaded = IsPackDirLoaded(inst->dir);
      r.inUse = PackInUseByDevices(ce.id, inst);
      if (mikmap::packs::compareVersions(inst->entry.version, ce.version) < 0) {
        r.state = keyOk ? PackState::UpdateAvailable : PackState::Installed;
      } else {
        r.state = PackState::Installed;
      }
    } else {
      r.state = keyOk ? PackState::NotInstalled : PackState::Unavailable;
    }

    if ((snap.state == mikmap::PackJobState::Downloading || snap.state == mikmap::PackJobState::Verifying) &&
        snap.packId == ce.id) {
      r.state = PackState::Downloading;
      r.progress = snap.totalBytes > 0 ? (float)((double)snap.doneBytes / (double)snap.totalBytes) : 0.f;
    } else if (snap.state == mikmap::PackJobState::Failed && g_pm.lastFailedPackId == ce.id) {
      r.state = PackState::Failed;
      r.error = snap.error;
    }
    out.push_back(std::move(r));
  }

  for (const auto& ip : installed) {
    if (seen.count(ip.entry.id) > 0) continue;
    seen.insert(ip.entry.id);
    PackRow r;
    r.id = ip.entry.id;
    r.name = ip.entry.name;
    r.version = ip.entry.version;
    r.installedVersion = ip.entry.version;
    r.sizeBytes = ip.entry.sizeBytes;
    r.pendingRemoval = ip.pendingRemoval;
    r.loaded = IsPackDirLoaded(ip.dir);
    r.inUse = PackInUseByDevices(ip.entry.id, &ip);
    r.state = PackState::Installed;
    out.push_back(std::move(r));
  }

  // Known first-party packs shown before a catalog is fetched so the operator sees sizes and status upfront.
  struct BuiltinPackMeta { const char* id; const char* name; const char* ver; uint64_t sizeBytes; };
  static const BuiltinPackMeta kKnown[] = {
    {"orbbec-femto", "Orbbec Femto Bolt / Mega Depth", "1.2.0", 18ull * 1024 * 1024},
    {"pose-onnx", "MediaPipe Pose + ONNX Runtime", "1.4.0", 180ull * 1024 * 1024},
    {"realsense-d435", "Intel RealSense D435 / D455", "1.1.0", 24ull * 1024 * 1024},
  };
  for (const auto& km : kKnown) {
    if (seen.count(km.id) > 0) continue;
    PackRow r;
    r.id = km.id;
    r.name = km.name;
    r.version = km.ver;
    r.sizeBytes = km.sizeBytes;
    r.state = keyOk ? PackState::NotInstalled : PackState::Unavailable;
    if ((snap.state == mikmap::PackJobState::Downloading || snap.state == mikmap::PackJobState::Verifying) &&
        snap.packId == r.id) {
      r.state = PackState::Downloading;
      r.progress = snap.totalBytes > 0 ? (float)((double)snap.doneBytes / (double)snap.totalBytes) : 0.f;
    } else if (snap.state == mikmap::PackJobState::Failed && g_pm.lastFailedPackId == r.id) {
      r.state = PackState::Failed;
      r.error = snap.error;
    }
    out.push_back(std::move(r));
  }

  return out;
}

bool PackAvailableForDevice(const Device& d) {
  if (d.needsPack.empty()) return true;
  if (!g_pm.demoMode.empty()) {
    for (const auto& r : PackRows()) {
      if (r.id == d.needsPack) {
        return (r.state == PackState::Installed || r.state == PackState::UpdateAvailable) && !r.pendingRemoval;
      }
    }
    return false;
  }
  if (!g_pm.store) return false;
  const mikmap::InstalledPack* ip = g_pm.store->find(d.needsPack);
  return ip != nullptr && !ip->pendingRemoval;
}

PackState PackStateFor(const std::string& id) {
  if (id.empty()) return PackState::Installed;
  for (const auto& r : PackRows()) {
    if (r.id == id) return r.state;
  }
  if (g_pm.store) {
    const mikmap::InstalledPack* ip = g_pm.store->find(id);
    if (ip && !ip->pendingRemoval) return PackState::Installed;
  }
  return PackKeyConfigured() ? PackState::NotInstalled : PackState::Unavailable;
}

std::string FormatBytes(uint64_t bytes) {
  if (bytes < 1024ull) return std::to_string(bytes) + " B";
  auto fmtUnit = [](double val, const char* unit) {
    char buf[48];
    std::snprintf(buf, sizeof buf, "%.1f", val);
    std::string s(buf);
    if (s.size() >= 2 && s.substr(s.size() - 2) == ".0") s.erase(s.size() - 2);
    return s + " " + unit;
  };
  if (bytes < 1024ull * 1024ull) return fmtUnit((double)bytes / 1024.0, "KB");
  if (bytes < 1024ull * 1024ull * 1024ull) return fmtUnit((double)bytes / (1024.0 * 1024.0), "MB");
  return fmtUnit((double)bytes / (1024.0 * 1024.0 * 1024.0), "GB");
}

const char* PackActionLabel(const PackRow& r) {
  switch (r.state) {
    case PackState::NotInstalled:    return "Install";
    case PackState::Downloading:     return "Cancel";
    case PackState::Installed:       return "Remove";
    case PackState::UpdateAvailable: return "Update";
    case PackState::Failed:          return "Retry";
    case PackState::Unavailable:
    default:                         return "";
  }
}

bool PackActionEnabled(const PackRow& r) {
  if (r.state == PackState::Unavailable) return false;
  if (r.pendingRemoval) return false;
  if ((r.state == PackState::Installed || r.state == PackState::UpdateAvailable) && r.inUse) return false;
  return true;
}

const char* PackActionHint(const PackRow& r) {
  if (r.pendingRemoval) return "Removes when the app restarts";
  if ((r.state == PackState::Installed || r.state == PackState::UpdateAvailable) && r.inUse)
    return "Turn the sensor off first";
  if (r.state == PackState::Unavailable)
    return "Pack signing key is not configured";
  return "";
}

const char* SensorPackHint(const Device& d, PackState st) {
  if (d.needsPack.empty() || st == PackState::Installed || st == PackState::UpdateAvailable)
    return nullptr;

  std::string name = d.needsPack;
  uint64_t sz = 0;
  float prog = 0.f;
  for (const auto& r : PackRows()) {
    if (r.id == d.needsPack) {
      if (!r.name.empty()) name = r.name;
      sz = r.sizeBytes;
      prog = r.progress;
      break;
    }
  }

  static thread_local std::string s_buf[6];
  std::string& out = s_buf[(unsigned)st % 6u];
  if (st == PackState::Downloading) {
    const int pct = std::clamp((int)std::lround(prog * 100.f), 0, 100);
    out = "Installing " + name + " " + std::to_string(pct) + "%";
    return out.c_str();
  }
  if (st == PackState::Failed) {
    out = "Pack install failed \xE2\x80\x94 Retry";
    return out.c_str();
  }
  if (sz > 0) {
    out = "Needs pack: " + name + " \xE2\x80\x94 Install (" + FormatBytes(sz) + ")";
  } else {
    out = "Needs pack: " + name + " \xE2\x80\x94 Install";
  }
  return out.c_str();
}

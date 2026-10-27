// Live sensors: the real ISensorSource threads (engine/io), owned here and not by App so the undo snapshot never sees them.
// The sensor threads hand over RAW coordinates through TripleBuffer / SpscRingBuffer only; H_s is applied where the points are used.
#if defined(_WIN32)
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <winsock2.h>
#  include <ws2tcpip.h>
#else
#  include <arpa/inet.h>
#  include <netinet/in.h>
#  include <sys/socket.h>
#  include <unistd.h>
#endif
#include "FakeScipServer.h"
#include "app.h"
#include "core/util/Clock.h"
#include "io/ISensorSource.h"
#include "io/plugin/PluginLoader.h"
#include "io/sources/HokuyoSource.h"
#include "io/sources/OscSource.h"
#include "io/sources/PluginSource.h"
#include "io/sources/ScanSource.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <memory>
#include <system_error>
#include <thread>
#ifdef __APPLE__
#  include <mach-o/dyld.h>
#endif

namespace {

struct Live {
  std::unique_ptr<mikmap::ISensorSource> src;   // null when the source could not start
  mikmap::OscSource* osc = nullptr;             // the same object, for the OSC-only counters
  mikmap::ScanSource* scan = nullptr;           // lidar sources: raw scans + params for the radar view
  std::string sig, err;                         // sig = kind|host|port|plugin|config: a change restarts the source
  bool failed = false;                          // start() refused (port taken, bad port, no driver): kept so it is not retried every frame
  double winStart = 0; int winFrames = 0, fps = 0;
  float latencyMs = 0;
};
std::map<std::string, Live> g_live;   // keyed by Device::id

// Calibration capture: the average of one point over 30 consecutive new frames.
constexpr int kCaptureFrames = 30;
struct Capture { bool on = false; std::string dev; int step = 0, n = 0; double sx = 0, sy = 0, lastFrame = -1; } g_cap;

std::string Sig(const Device& d) {
  // plugin + config are part of the signature: editing the JSON restarts the source, exactly like a new address
  return std::to_string((int)d.kind) + "|" + d.host + "|" + std::to_string(d.port) + "|" + d.plugin + "|" + d.config;
}

std::string FileName(const std::string& path) {
  const size_t k = path.find_last_of("/\\");
  return k == std::string::npos ? path : path.substr(k + 1);
}

Live Start(const Device& d, double now) {
  Live L; L.sig = Sig(d); L.winStart = now;

  if (d.kind == DevKind::Plugin) {
    const mikmap::SensorPlugin* found = nullptr;
    for (auto& p : SensorPlugins()) if (FileName(p.path) == d.plugin) { found = &p; break; }
    if (!found) { L.failed = true; L.err = "plugin not found: " + d.plugin + " in " + SensorPluginDir(); return L; }
    mikmap::PluginConfig pc; pc.configJson = d.config; pc.reconnectSec = 1.0;
    auto ps = std::make_unique<mikmap::PluginSource>(*found, pc);
    ps->start();   // a plugin that rejects its config is temporary: status()/lastError() report it, the source retries
    L.scan = ps.get(); L.src = std::move(ps);
    return L;
  }

  if (d.port < 1 || d.port > 65535) { L.failed = true; L.err = "Port must be 1-65535"; return L; }

  if (d.kind == DevKind::Skeleton) {
    mikmap::OscConfig c;
    c.port = (uint16_t)d.port; c.protocol = mikmap::OscProtocol::Mikmap; c.addressPrefix = "/mikmap/touch";
    c.normalizedInput = false; c.pointTimeoutSec = 0.25;   // a person who leaves the picture lets go of every joint after 250 ms
    auto o = std::make_unique<mikmap::OscSource>(c);
    if (!o->start()) { L.failed = true; L.err = o->lastError(); return L; }
    L.osc = o.get(); L.src = std::move(o);
    return L;
  }

  if (d.kind == DevKind::Hokuyo) {
    if (d.host.empty()) { L.failed = true; L.err = "Host is required"; return L; }
    mikmap::HokuyoConfig c;
    c.host = d.host; c.port = (uint16_t)d.port; c.reconnectSec = 1.0;
    auto h = std::make_unique<mikmap::HokuyoSource>(c);
    h->start();   // a missing cable is temporary: status()/lastError() report it, the source keeps retrying
    L.scan = h.get(); L.src = std::move(h);
    return L;
  }

  L.failed = true; L.err = "No driver for this sensor yet";
  return L;
}

// One new frame of the device being calibrated.
void CaptureFrame(Device& d, double now) {
  // A lidar blob has no joint id: every touch of a ScanSource is a candidate. Skeleton keeps the wrist filter.
  const bool lidar = dynamic_cast<mikmap::ScanSource*>(DeviceSource(d)) != nullptr;
  int seen = 0; const Touch* pt = nullptr;
  for (auto& t : d.touches) if (lidar || t.id == kCalibJoint) { ++seen; pt = &t; }
  if (seen != 1) { g_cap.on = false; A.notify("Need exactly one point \xE2\x80\x94 seen " + std::to_string(seen)); return; }
  g_cap.sx += pt->x; g_cap.sy += pt->y; g_cap.lastFrame = now;
  if (++g_cap.n < kCaptureFrames) return;
  Calib& c = d.calib[g_cap.step - 1];
  c.mx = std::round((float)(g_cap.sx / g_cap.n) * 1000.f) / 1000.f; c.my = std::round((float)(g_cap.sy / g_cap.n) * 1000.f) / 1000.f;
  if (g_cap.step == 4) d.calibrated = true;
  A.notify("Point " + std::to_string(g_cap.step) + " of 4 captured");
  A.wizardStep = g_cap.step == 4 ? 0 : g_cap.step + 1;
  g_cap.on = false;
}

}  // namespace

std::string SensorPluginDir() {
#if defined(_WIN32)
  std::vector<wchar_t> buf(32768);
  const DWORD n = GetModuleFileNameW(nullptr, buf.data(), (DWORD)buf.size());
  if (n == 0 || n >= buf.size()) return {};
  const std::filesystem::path dir = std::filesystem::path(std::wstring(buf.data(), n)).parent_path() / "plugins" / "sensors";
#elif defined(__APPLE__)
  char buf[4096]; uint32_t size = sizeof buf;
  if (_NSGetExecutablePath(buf, &size) != 0) return {};
  const std::filesystem::path dir = std::filesystem::path(buf).parent_path() / "plugins" / "sensors";
#else
  char buf[4096];
  const ssize_t n = readlink("/proc/self/exe", buf, sizeof buf - 1);
  if (n <= 0) return {};
  buf[n] = '\0';
  const std::filesystem::path dir = std::filesystem::path(buf).parent_path() / "plugins" / "sensors";
#endif
  const std::u8string u8 = dir.u8string();   // UTF-8 even when the install path is Vietnamese
  return std::string(reinterpret_cast<const char*>(u8.data()), u8.size());
}

const std::vector<mikmap::SensorPlugin>& SensorPlugins() {
  static const std::vector<mikmap::SensorPlugin> plugins = [] {
    std::vector<std::string> errors;
    auto found = mikmap::scanPluginDir(SensorPluginDir(), errors);
    for (auto& e : errors) A.notify("Sensor plugin: " + e);   // once per bad DLL, not on every tick
    return found;
  }();
  return plugins;
}

void SensorsTick(double now) {
  SensorPlugins();   // scan the plugin folder once on the first tick (a missing folder is not an error)
  for (auto it = g_live.begin(); it != g_live.end();) {   // a removed or disabled device, or a changed address, stops its source
    const Device* d = nullptr; for (auto& x : A.devices) if (x.id == it->first) { d = &x; break; }
    if (!d || !d->enabled || d->kind == DevKind::Sim || it->second.sig != Sig(*d)) it = g_live.erase(it); else ++it;
  }
  if (g_cap.on && A.wizardStep != g_cap.step) g_cap.on = false;   // wizard cancelled, or another device selected
  bool fire = false;
  for (auto& d : A.devices) {
    if (d.kind == DevKind::Sim) { d.status = d.enabled ? (int)mikmap::SourceStatus::Running : (int)mikmap::SourceStatus::Stopped; continue; }
    d.fps = 0; d.latencyMs = 0; d.packets = 0;
    if (!d.enabled) { d.status = (int)mikmap::SourceStatus::Stopped; d.error.clear(); d.touches.clear(); continue; }
    auto it = g_live.find(d.id);
    if (it == g_live.end()) it = g_live.emplace(d.id, Start(d, now)).first;
    Live& L = it->second;
    if (!L.src) { d.status = (int)mikmap::SourceStatus::Error; d.error = L.err; d.touches.clear(); continue; }
    mikmap::ISensorSource& S = *L.src;
    if (L.scan) {   // the ROI/blob sliders live in the app; the source picks up the latest copy on its next scan
      mikmap::LidarParams p;
      p.minBlobMm = std::clamp(A.blobSize, 5.f, 300.f);
      for (int i = 0; i < 4; ++i) p.roi[i] = mikmap::Vec2{d.roi[i].x, d.roi[i].y};
      L.scan->setParams(p);
    }
    bool got = false;
    if (S.frames().consume()) {
      got = true; ++L.winFrames;
      const mikmap::SensorFrame& f = S.frames().readSlot();
      d.touches.clear();
      for (int i = 0; i < f.count; ++i) d.touches.push_back(Touch{(int)f.points[i].id, f.points[i].x, f.points[i].y});
      L.latencyMs = (float)mikmap::Clock::nsToMs(mikmap::Clock::nowNs() - f.tCaptureNs) + (L.osc ? L.osc->senderLatencyMs() : 0.f);
    }
    mikmap::TouchEvent ev;
    while (S.events().pop(ev)) if (ev.state == mikmap::TouchState::Down) fire = true;
    double dt = now - L.winStart;
    if (dt < 0 || dt >= 1.0) { L.fps = dt > 0 ? (int)std::lround(L.winFrames / dt) : 0; L.winStart = now; L.winFrames = 0; }
    d.fps = L.fps; d.latencyMs = L.latencyMs; d.packets = L.osc ? L.osc->packetsReceived() : L.scan ? L.scan->scansReceived() : 0;
    d.status = (int)S.status();
    if (S.status() == mikmap::SourceStatus::Error) d.error = S.lastError(); else d.error.clear();   // lastError() takes the source's mutex: only ask when there is an error
    if (got && g_cap.on && g_cap.dev == d.id) CaptureFrame(d, now);
  }
  if (g_cap.on) {   // no frame for 5 s: the sensor is silent, do not wait forever
    if (g_cap.lastFrame < 0) g_cap.lastFrame = now;
    if (now - g_cap.lastFrame > 5.0) { g_cap.on = false; A.notify("No frames from the sensor"); }
  }
  if (fire) FireTouchRoutes();
}

void SensorsShutdown() { g_live.clear(); g_cap.on = false; }

mikmap::ISensorSource* DeviceSource(const Device& d) {
  auto it = g_live.find(d.id);
  return it == g_live.end() ? nullptr : it->second.src.get();
}

// Latest raw scan of any lidar source (Hokuyo today, plugin in Task 10). Kept in the app, not the engine,
// so the radar view asks "is this a ScanSource?" instead of "is this a Hokuyo?".
const mikmap::LaserScan* DeviceScan(const Device& d) {
  auto* scan = dynamic_cast<mikmap::ScanSource*>(DeviceSource(d));
  if (!scan || scan->scansReceived() == 0) return nullptr;
  scan->scans().consume();
  return &scan->scans().readSlot();
}

void StartCapture(int step) {
  Device* d = A.curDevice();
  if (!d || d->kind == DevKind::Sim || step < 1 || step > 4) return;
  if (!DeviceSource(*d)) { A.notify("Connect the sensor first"); return; }
  g_cap = Capture{true, d->id, step, 0, 0, 0, -1};
}

// ── --roundtrip: a real OscSource over loopback UDP ──
static void SendUdp(int port, const uint8_t* data, size_t n) {
#if defined(_WIN32)
  WSADATA w{}; WSAStartup(MAKEWORD(2, 2), &w);
  SOCKET s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s == INVALID_SOCKET) return;
#else
  int s = ::socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  if (s < 0) return;
#endif
  sockaddr_in a{}; a.sin_family = AF_INET; a.sin_port = htons((uint16_t)port); inet_pton(AF_INET, "127.0.0.1", &a.sin_addr);
  ::sendto(s, (const char*)data, (int)n, 0, (sockaddr*)&a, sizeof a);
#if defined(_WIN32)
  closesocket(s);
#else
  close(s);
#endif
}

bool SensorSelfTest(std::string& why) {
  NewProject();
  A.routes.clear();   // a touch.down cord triggers a clip (selection, active column): a real project change, not the live state under test
  const int port = 39517;
  // `/mikmap/touch 16 0.5 0.25 0.0` — byte for byte the vector tests/test_osc.cpp and the pose_bridge test share
  static const uint8_t pkt[40] = {0x2f,0x6d,0x69,0x6b,0x6d,0x61,0x70,0x2f,0x74,0x6f,0x75,0x63,0x68,0,0,0, 0x2c,0x69,0x66,0x66,0x66,0,0,0,
                                  0,0,0,0x10, 0x3f,0,0,0, 0x3e,0x80,0,0, 0,0,0,0};
  auto now = [] { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); };
  auto dev = [](const char* id) -> Device& { for (auto& d : A.devices) if (d.id == id) return d; return A.devices.back(); };
  auto fail = [&](const std::string& w) { why = w; SensorsShutdown(); return false; };

  Device a = MakeDevice(DevKind::Skeleton, "selftest-a"); a.port = port; a.enabled = true;
  A.devices.push_back(a);
  MarkSaved();
  SensorsTick(now());
  SendUdp(port, pkt, sizeof pkt);
  for (double t0 = now(); now() - t0 < 2.0 && dev("selftest-a").touches.empty();) { SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
  Device& da = dev("selftest-a");
  if (da.touches.size() != 1 || da.touches[0].id != kCalibJoint) return fail("sensor: a UDP touch did not reach the Skeleton device");
  if (da.status != (int)mikmap::SourceStatus::Running || da.packets < 1) return fail("sensor: a running source must report Running and its packet count");
  if (ProjectDirty()) return fail("sensor: live touches made the project dirty");

  // Capture: 30 new frames of joint 16 average into calib[0] and the wizard moves on
  A.selDevice = (int)(&da - A.devices.data()); A.wizardStep = 1; StartCapture(1);
  for (double t0 = now(); now() - t0 < 3.0 && A.wizardStep == 1;) { SendUdp(port, pkt, sizeof pkt); SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
  if (A.wizardStep != 2 || std::fabs(da.calib[0].mx - 0.5f) > 1e-3f || std::fabs(da.calib[0].my - 0.25f) > 1e-3f) return fail("sensor: Capture did not average the wrist into the calibration point");
  // ...and a frame without exactly one candidate (the wrist let go) cancels it with a message
  StartCapture(2); A.toast.clear();
  for (double t0 = now(); now() - t0 < 1.5 && A.toast.empty();) { SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
  if (A.toast.rfind("Need exactly one point", 0) != 0) return fail("sensor: Capture without a candidate point must say so");
  A.wizardStep = 0;

  Device b = MakeDevice(DevKind::Skeleton, "selftest-b"); b.port = port; b.enabled = true;   // the port is taken by the first one
  A.devices.push_back(b);
  SensorsTick(now());
  if (dev("selftest-b").status != (int)mikmap::SourceStatus::Error || dev("selftest-b").error.empty()) return fail("sensor: a second source on a taken port must report Error with a message");
  if (DeviceSource(dev("selftest-b"))) return fail("sensor: a source that failed to start must not be reachable");

  // Hokuyo over a fake SCIP server: one blob at 1000 mm, then 30 frames averaged into calibration point 1
  mikmap::test::FakeScipServer hokuyo;
  if (!hokuyo.error().empty()) return fail("sensor: the fake SCIP server did not start");
  Device h = MakeDevice(DevKind::Hokuyo, "selftest-h");
  h.host = "127.0.0.1"; h.port = hokuyo.port(); h.enabled = true;
  A.devices.push_back(h);
  for (double t0 = now(); now() - t0 < 2.0 && !(DeviceScan(dev("selftest-h")) && dev("selftest-h").touches.size() == 1);) { SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
  Device& dh = dev("selftest-h");
  if (!DeviceScan(dh) || dh.touches.size() != 1) return fail("sensor: a Hokuyo scan did not produce exactly one blob");
  A.selDevice = (int)(&dh - A.devices.data()); A.wizardStep = 1; StartCapture(1);
  for (double t0 = now(); now() - t0 < 3.0 && A.wizardStep == 1;) { SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
  if (A.wizardStep != 2 || std::fabs(dh.calib[0].mx - 1000.f) > 25.f) return fail("sensor: Hokuyo Capture did not average the blob at 1000 mm");
  A.wizardStep = 0;

#ifdef MIKMAP_FAKE_PLUGIN_FILE
  // Plugin ABI through the app: the same fake scan as Hokuyo, produced by the DLL this build wrote into plugins/sensors.
  const mikmap::SensorPlugin* fp = nullptr;
  for (auto& p : SensorPlugins()) if (FileName(p.path) == MIKMAP_FAKE_PLUGIN_FILE) { fp = &p; break; }
  if (!fp) return fail("sensor: the fake plugin was not found in " + SensorPluginDir());
  Device q = MakeDevice(DevKind::Plugin, "selftest-p");
  q.name = fp->name; q.plugin = MIKMAP_FAKE_PLUGIN_FILE; q.config = "{\"fail_after\":0}"; q.enabled = true;
  A.devices.push_back(q);
  for (double t0 = now(); now() - t0 < 2.0 && !(DeviceScan(dev("selftest-p")) && dev("selftest-p").touches.size() == 1);) { SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
  if (!DeviceScan(dev("selftest-p")) || dev("selftest-p").touches.size() != 1) return fail("sensor: a plugin scan did not produce exactly one blob");

  // plugin + config are project data: save -> load must bring back the file name and the JSON unchanged
  std::error_code tec;
  const auto tmpdir = std::filesystem::temp_directory_path(tec);
  if (tec) return fail("sensor: no temp directory for the plugin save/load check");
  const std::string ppath = (tmpdir / "mikmap_selftest_plugin.mikmap").string();
  std::string perr;
  if (!SaveProject(ppath, perr)) return fail("sensor: cannot save a plugin device: " + perr);
  if (!LoadProject(ppath, perr)) return fail("sensor: cannot load a plugin device: " + perr);
  std::filesystem::remove(ppath, tec);
  Device& ql = dev("selftest-p");
  if (ql.plugin != MIKMAP_FAKE_PLUGIN_FILE || ql.config != "{\"fail_after\":0}") return fail("sensor: plugin file/config did not survive save/load");

  // a missing DLL keeps the device (with its config) and says exactly where it looked
  ql.plugin = "no_such_plugin.dll"; ql.enabled = false; SensorsTick(now()); ql.enabled = true; SensorsTick(now());
  if (dev("selftest-p").status != (int)mikmap::SourceStatus::Error || dev("selftest-p").error.find("plugin not found") == std::string::npos)
    return fail("sensor: a missing plugin must keep the device and say where it looked");
#endif

  dev("selftest-a").enabled = false; dev("selftest-b").enabled = false;
  SensorsTick(now());
  if (DeviceSource(dev("selftest-a")) || DeviceSource(dev("selftest-b"))) return fail("sensor: a disabled device must have no source");
  SensorsShutdown();
  NewProject();
  return true;
}

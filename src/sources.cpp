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
#include "io/net/NetScan.h"
#include "io/net/TouchSender.h"
#include "io/plugin/PluginLoader.h"
#include "io/sources/HokuyoSource.h"
#include "io/sources/OscSource.h"
#include "io/sources/PluginSource.h"
#include "io/sources/ScanSource.h"
#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
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

// Send out: one sender thread for every device, started on the first device that sends. Only SensorsTick pushes (its single producer).
mikmap::TouchSender g_out;

// A new frame of a calibrated device, through its H_s, as 0..1 of the 1920x1080 stage (not clamped). No allocation: render thread.
void SendOut(const Device& d, const mikmap::SensorFrame& f) {
  float H[9];
  if (!d.outOn || !DeviceH(d, H)) return;
  if (!g_out.running()) g_out.start();
  mikmap::OutPacket p;
  if (d.outHost.size() < sizeof p.host) std::memcpy(p.host, d.outHost.c_str(), d.outHost.size() + 1);   // too long = empty: the sender says "Host is empty"
  p.port = d.outPort >= 1 && d.outPort <= 65535 ? (uint16_t)d.outPort : 0;   // 0: the sender reports the bad port
  p.proto = (uint8_t)(d.outProto == 1);
  mikmap::setOutDevice(p, d.name.c_str());
  p.count = (uint8_t)std::min<int>(f.count, mikmap::kMaxTouchPoints);
  for (int i = 0; i < p.count; ++i) {
    float ox, oy; ApplyH(H, f.points[i].x, f.points[i].y, ox, oy);
    p.ids[i] = f.points[i].id; p.xy[2 * i] = ox / 1920.f; p.xy[2 * i + 1] = oy / 1080.f;
  }
  g_out.push(p);
}

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

// Wizard IP scan: one worker thread; it only touches `result` and publishes it with done (release). Never ImGui, never A.
struct IpScan { std::thread t; std::atomic<bool> done{false}; std::vector<std::string> result; } g_scan;
void JoinIpScan() { if (g_scan.t.joinable()) g_scan.t.join(); }

}  // namespace

void StartIpScan(const std::string& address, int prefix, uint16_t port) {
  JoinIpScan();   // a previous scan finishes first (at most one 300 ms batch per 256 hosts)
  g_scan.result.clear(); g_scan.done.store(false, std::memory_order_relaxed);
  g_scan.t = std::thread([address, prefix, port] {
    g_scan.result = mikmap::scanTcpPort(mikmap::hostsInSubnet(address, prefix), port, 300);
    g_scan.done.store(true, std::memory_order_release);
  });
}

bool IpScanDone(std::vector<std::string>& out) {
  if (!g_scan.t.joinable() || !g_scan.done.load(std::memory_order_acquire)) return false;
  JoinIpScan(); out = std::move(g_scan.result); g_scan.result.clear();
  return true;
}

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
    ApplyPlacement(d);   // a placed lidar follows its pose and its screen's mm size wherever they were edited (Mapping, undo)
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
      p.trackBlobs = d.blobOn; p.minBlobMm = d.blobMinMm; p.maxBlobMm = d.blobMaxMm; p.maxGapMm = d.blobGapMm; p.maxRangeMm = d.rangeMm;
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
      SendOut(d, f);
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

void SensorsShutdown() { g_live.clear(); g_cap.on = false; JoinIpScan(); g_out.stop(); }
void SendOutStats(uint64_t& sent, uint64_t& dropped, std::string& error) {
  sent = g_out.sent(); dropped = g_out.dropped();
  error = g_out.lastError();   // lock-free (error code -> text)
}
void RestartSource(const std::string& deviceId) { g_live.erase(deviceId); }

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
  constexpr double kWait = 10.0;   // every wait loop exits on success; the long deadline only matters on a slow or loaded machine
  int port = 0;   // a free UDP port the OS picked (a probe binds port 0, reads it back, lets it go); a fixed one may be taken
  { mikmap::OscConfig pc; pc.port = 0; mikmap::OscSource probe(pc); if (probe.start()) port = probe.boundPort(); }
  if (port == 0) { why = "sensor: cannot get a free UDP port"; return false; }
  // `/mikmap/touch 16 0.5 0.25 0.0` — byte for byte the vector tests/test_osc.cpp and the pose_bridge test share
  static const uint8_t pkt[40] = {0x2f,0x6d,0x69,0x6b,0x6d,0x61,0x70,0x2f,0x74,0x6f,0x75,0x63,0x68,0,0,0, 0x2c,0x69,0x66,0x66,0x66,0,0,0,
                                  0,0,0,0x10, 0x3f,0,0,0, 0x3e,0x80,0,0, 0,0,0,0};
  auto now = [] { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); };
  auto dev = [](const char* id) -> Device& { for (auto& d : A.devices) if (d.id == id) return d; return A.devices.back(); };
  auto fail = [&](const std::string& w) { why = w; SensorsShutdown(); return false; };

  // Tree: a sensor is a child of its screen in Mapping. (The default Hokuyo is devices[2].)
  {
    A.devices[2].screenId = A.screens[1].id;
    auto count = [](const std::vector<Node>& v, Node::Kind k, const std::string& sc) { int n = 0; for (auto& x : v) n += x.kind == k && (sc.empty() || x.sc == sc); return n; };
    const auto mt = BuildTree(false, "");
    if (count(mt, Node::SensorN, A.screens[1].id) != 1 || count(mt, Node::SensorN, "") != (int)A.devices.size())
      return fail("tree: Mapping must list the Hokuyo exactly once, under the screen it belongs to");
    A.devices[2].screenId = A.screens[0].id;
  }

  // One Mapping page: a sensor is selection kind 3; while it is selected, Mapping's slice shortcuts touch nothing.
  {
    A.screen = 0; A.mpage = 1;
    SelectDevice(A.devices[2].id);
    const Screen* ds = DeviceScreen(A.devices[2]);
    if (A.MapKind() != 3 || A.screen != 1 || A.curDevice() != &A.devices[2] || !ds || A.selSc != ds->id || MappingEditsSlices())
      return fail("select: a sensor must be selection kind 3 on the Mapping page, with its screen selected and slice shortcuts off");
    // the slice still "selected" underneath, with a picked corner: Delete / arrows / Ctrl+D must leave it alone
    Slice* hs = A.curSlice();
    if (!hs) return fail("select: the default project has no slice under the sensor's screen");
    const std::string hsId = hs->id; const size_t nsl = A.curScreen()->slices.size(); const ImVec2 q0 = hs->q[0];
    A.mapPts = {{hsId, 0}};
    MappingShortcut(ImGuiKey_RightArrow, false, true); MappingShortcut(ImGuiKey_DownArrow, false, false);
    MappingShortcut(ImGuiKey_Delete, false, false); MappingShortcut(ImGuiKey_D, true, false); MappingShortcut(ImGuiKey_X, true, false);
    hs = A.curSlice();
    if (A.curScreen()->slices.size() != nsl || !hs || hs->id != hsId || hs->q[0].x != q0.x || hs->q[0].y != q0.y)
      return fail("select: Mapping's Delete / nudge / duplicate / cut changed a slice while a sensor was selected");
    A.mapPts.clear();
    // the tree's own click path back to a slice brings slice editing back
    Node sn; sn.kind = Node::SliceN; sn.sc = A.screens[0].id; sn.sl = A.screens[0].slices[0].id;
    SelectTreeNode(sn);
    if (A.MapKind() != 1 || !MappingEditsSlices()) return fail("select: clicking a slice in the tree must make it the selection again");
    // a sensor whose screen is gone is shown on the fallback screen
    A.devices[2].screenId = "no-such-screen";
    SelectDevice(A.devices[2].id);
    bool fb = false; DeviceScreen(A.devices[2], &fb);
    if (A.selSc != A.screens[0].id || !fb || A.MapKind() != 3) return fail("select: a sensor whose screen was deleted must select the fallback screen");
    A.devices[2].screenId = A.screens[0].id;
    // Remove (the tree menu's own path) on the selected sensor: back to its screen, never a stale index
    Device rm = MakeDevice(DevKind::Sim, "selftest-rm"); rm.screenId = A.screens[1].id; A.devices.push_back(rm);
    SelectDevice("selftest-rm");
    const std::string rmSc = A.selSc;
    RemoveDevice("selftest-rm");
    if (A.MapKind() != 0 || A.selSc != rmSc || rmSc != A.screens[1].id || (A.curDevice() && A.curDevice()->id == "selftest-rm"))
      return fail("select: removing the selected sensor must fall back to its screen");
    // removing a sensor BEFORE the selected one keeps the same sensor selected
    rm.id = "selftest-rm2"; A.devices.insert(A.devices.begin(), rm);
    SelectDevice(A.devices[3].id); const std::string keepId = A.devices[3].id;
    RemoveDevice("selftest-rm2");
    if (A.MapKind() != 3 || !A.curDevice() || A.curDevice()->id != keepId) return fail("select: removing another sensor must keep the selected one");
    // --screen 2 (the old Sensor page): the Mapping page with a sensor selected
    A.screen = 0; A.selKind = -1;
    ApplyStartScreen(2);
    if (A.screen != 1 || A.MapKind() != 3) return fail("select: --screen 2 must open Mapping with a sensor selected");
    A.screen = 0; A.selKind = -1;
  }

  Device a = MakeDevice(DevKind::Skeleton, "selftest-a"); a.port = port; a.enabled = true;
  A.devices.push_back(a);
  MarkSaved();
  SensorsTick(now());
  SendUdp(port, pkt, sizeof pkt);
  for (double t0 = now(); now() - t0 < kWait && dev("selftest-a").touches.empty();) { SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
  Device& da = dev("selftest-a");
  if (da.touches.size() != 1 || da.touches[0].id != kCalibJoint) return fail("sensor: a UDP touch did not reach the Skeleton device");
  if (da.status != (int)mikmap::SourceStatus::Running || da.packets < 1) return fail("sensor: a running source must report Running and its packet count");
  if (ProjectDirty()) return fail("sensor: live touches made the project dirty");

  // Capture: 30 new frames of joint 16 average into calib[0] and the wizard moves on
  A.selDevice = (int)(&da - A.devices.data()); A.wizardStep = 1; StartCapture(1);
  for (double t0 = now(); now() - t0 < kWait && A.wizardStep == 1;) { SendUdp(port, pkt, sizeof pkt); SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
  if (A.wizardStep != 2 || std::fabs(da.calib[0].mx - 0.5f) > 1e-3f || std::fabs(da.calib[0].my - 0.25f) > 1e-3f) return fail("sensor: Capture did not average the wrist into the calibration point");
  // ...and a frame without exactly one candidate (the wrist let go) cancels it with a message
  StartCapture(2); A.toast.clear();
  for (double t0 = now(); now() - t0 < kWait && A.toast.empty();) { SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
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
  for (double t0 = now(); now() - t0 < kWait && !(DeviceScan(dev("selftest-h")) && dev("selftest-h").touches.size() == 1);) { SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
  Device& dh = dev("selftest-h");
  if (!DeviceScan(dh) || dh.touches.size() != 1) return fail("sensor: a Hokuyo scan did not produce exactly one blob");
  A.selDevice = (int)(&dh - A.devices.data()); A.wizardStep = 1; StartCapture(1);
  for (double t0 = now(); now() - t0 < kWait && A.wizardStep == 1;) { SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
  if (A.wizardStep != 2 || std::fabs(dh.calib[0].mx - 1000.f) > 25.f) return fail("sensor: Hokuyo Capture did not average the blob at 1000 mm");
  A.wizardStep = 0;

  // A placed lidar derives its calibration from its pose; a hand-tuned quad (placed = false) is never overwritten.
  {
    Screen& hs = *DeviceScreen(dh);
    hs.realW = 4000; hs.realH = 2250;
    dh.placed = true; dh.pose = mikmap::LidarPose{2000, 2250, -90, false};   // bottom middle, looking up the wall
    float H[9], ox = 0, oy = 0;
    if (!ApplyPlacement(dh) || !dh.calibrated || !DeviceH(dh, H)) return fail("sensor: a placed lidar on a sized screen must be calibrated");
    ApplyH(H, 1000, 0, ox, oy);
    if (std::fabs(ox - 960) > 2 || std::fabs(oy - 600) > 2) return fail("sensor: a blob 1 m in front of a bottom-middle lidar must land at (960, 600)");
    {   // Send out: that blob, as real OSC over loopback, arrives at (960/1920, 600/1080) of the screen
      mikmap::OutPacket nm; mikmap::setOutDevice(nm, dh.name.c_str());
      mikmap::OscConfig oc; oc.port = 0; oc.addressPrefix = std::string("/mikmap/") + nm.device + "/touch";
      mikmap::OscSource rx(oc);
      if (!rx.start()) return fail("send out: cannot listen on UDP: " + rx.lastError());
      dh.outOn = true; dh.outProto = 0; dh.outHost = "127.0.0.1"; dh.outPort = rx.boundPort();
      bool got = false;
      for (double t0 = now(); now() - t0 < kWait && !got;) {
        SensorsTick(now());
        if (rx.frames().consume() && rx.frames().readSlot().count == 1) got = true; else std::this_thread::sleep_for(std::chrono::milliseconds(5));
      }
      if (!got) return fail("send out: no OSC touch from the placed Hokuyo within 10 s");
      const mikmap::TouchPoint& tp = rx.frames().readSlot().points[0];
      if (std::fabs(tp.x - 0.5f) > 0.01f || std::fabs(tp.y - 0.556f) > 0.01f) return fail("send out: the touch must arrive normalised, about (0.5, 0.556)");
      dh.outOn = false; rx.stop();
    }
    {   // a Transform click that moves nothing (~1e-4 px round-trip) is no edit; a real move makes it Custom (4-point)
      ImVec2 q[4]; for (int i = 0; i < 4; ++i) q[i] = ImVec2(dh.calib[i].tx + 1e-3f, dh.calib[i].ty - 1e-3f);
      const float tx0 = dh.calib[0].tx;
      if (QuadEdit(dh, q) || !dh.placed || dh.calib[0].tx != tx0) return fail("sensor: a quad edit under 0.01 px must not flip a placed lidar to Custom");
      Device hand = dh; q[2].x += 0.5f;
      if (!QuadEdit(hand, q) || hand.placed || hand.calib[2].tx != q[2].x) return fail("sensor: moving a quad point by hand must make the lidar Custom (4-point)");
    }
    {   // a placed lidar whose screen is deleted keeps its calibration: it is never re-derived on the fallback screen
      Screen ns; ns.id = A.uid("screen"); ns.name = "Gone"; ns.realW = 3000; ns.realH = 1500;
      A.screens.push_back(ns);   // invalidates hs: not used below
      dh.screenId = ns.id;
      if (!ApplyPlacement(dh)) return fail("sensor: a placed lidar on the extra screen must be calibrated");
      const std::vector<Calib> kept = dh.calib;
      A.deleteScreen(ns.id);
      for (int k = 0; k < 5; ++k) SensorsTick(now());
      bool same = dh.placed && dh.calibrated;
      for (int i = 0; i < 4; ++i) same = same && dh.calib[i].tx == kept[i].tx && dh.calib[i].ty == kept[i].ty && dh.calib[i].mx == kept[i].mx && dh.calib[i].my == kept[i].my;
      if (!same) return fail("sensor: a placed lidar whose screen was deleted must keep its calibration, not re-derive it on the fallback screen");
      dh.screenId.clear(); ApplyPlacement(dh);   // back on screen 0 for the checks below
    }
    dh.placed = false; dh.calib[0].tx += 50; const float hand = dh.calib[0].tx; dh.pose.x = 2500;
    ApplyPlacement(dh); SensorsTick(now());
    if (dh.calib[0].tx != hand) return fail("sensor: moving the lidar overwrote a hand-tuned (Custom 4-point) calibration");
    if (!RoiToScreen(dh)) return fail("sensor: ROI = screen refused on a sized screen");
    const mikmap::Vec2 tl = mikmap::surfaceToLidar(dh.pose, {0, 0});
    if (std::fabs(dh.roi[0].x - (float)tl.x) > 0.5f || std::fabs(dh.roi[0].y - (float)tl.y) > 0.5f) return fail("sensor: ROI = screen must put roi[0] on the screen's top-left corner");
    using P = PropSection;
    const std::vector<P> lidarIn{P::Connection, P::ScreenSize, P::Placement, P::Blobs}, out{P::Calibration, P::SendOut, P::Triggers}, camIn{P::Connection};
    for (DevKind k : {DevKind::Hokuyo, DevKind::Plugin})
      if (SensorPropSections(k, 0) != lidarIn || SensorPropSections(k, 1) != out) return fail("sensor: lidar Properties sections are wrong");
    if (SensorPropSections(DevKind::Skeleton, 0) != camIn || SensorPropSections(DevKind::Skeleton, 1) != out) return fail("sensor: Skeleton Properties sections are wrong");
    RestartSource(dh.id);
    if (DeviceSource(dh)) return fail("sensor: Reconnect must drop the live source");
    SensorsTick(now());
    if (!DeviceSource(dh)) return fail("sensor: after Reconnect the next tick must start the source again");
  }

  // Add-sensor wizard: the scan thread finds a fake lidar, then step 1 Hokuyo -> step 2 typed 127.0.0.1:<port> -> step 3,
  // one enabled device on the chosen screen that reaches Running within 10 s.
  {
    mikmap::test::FakeScipServer wz;
    if (!wz.error().empty()) return fail("wizard: the fake SCIP server did not start");
    StartIpScan("127.0.0.2", 30, wz.port());   // 127.0.0.0/30 without itself = just 127.0.0.1
    std::vector<std::string> found; bool got = false;
    for (double t0 = now(); now() - t0 < kWait && !(got = IpScanDone(found));) std::this_thread::sleep_for(std::chrono::milliseconds(5));
    if (!got || found != std::vector<std::string>{"127.0.0.1"}) return fail("wizard: the IP scan thread did not report the listening fake lidar");

    const size_t before = A.devices.size();
    A.screen = 0; A.wizardStep = 3; OpenSensorWizard(A.screens[1].id);
    if (A.screen != 0) return fail("wizard: opening it must not change the page (it is a popup)");
    if (A.wizardStep != 0 || A.wizard.step != 1) return fail("wizard: opening it must close the 4-point calibration wizard and start at step 1");
    A.wizard.kind = DevKind::Hokuyo; SensorWizardNext();
    if (A.wizard.step != 2) return fail("wizard: a Hokuyo goes to the connect step");
    A.wizard.manualHost = " 127.0.0.1:" + std::to_string(wz.port()) + " "; SensorWizardAddManual();
    if (A.wizard.found.size() != 1 || !A.wizard.pick[0]) return fail("wizard: a typed IP must join the list ticked");
    SensorWizardNext();
    if (A.wizard.step != 3 || A.devices.size() != before + 1 || A.wizard.added.size() != 1) return fail("wizard: one ticked lidar must make exactly one device");
    const std::string nid = A.wizard.added[0];
    Device& nw = dev(nid.c_str());
    if (nw.screenId != A.screens[1].id || !nw.enabled || nw.host != "127.0.0.1" || nw.port != wz.port() || nw.name != "Hokuyo 127.0.0.1:" + std::to_string(wz.port()))
      return fail("wizard: the new Hokuyo must be enabled, on the chosen screen, at the typed address");
    for (double t0 = now(); now() - t0 < kWait && dev(nid.c_str()).status != (int)mikmap::SourceStatus::Running;) { SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
    if (dev(nid.c_str()).status != (int)mikmap::SourceStatus::Running) return fail("wizard: the new Hokuyo did not reach Running within 10 s");
    A.mpage = 1; SensorWizardDone();   // A.mpage = 0 after Done is intended: the new sensor opens on INPUT
    if (A.wizard.step != 0 || !A.curDevice() || A.curDevice()->id != nid) return fail("wizard: Done must close it and select the new device");
    if (A.MapKind() != 3 || A.screen != 1 || A.mpage != 0) return fail("wizard: Done must leave the new sensor selected on the Mapping page, INPUT tab");
    dev(nid.c_str()).enabled = false; SensorsTick(now());   // stop it before the fake server goes away

    // an address that is already a device (here disabled, so it would still answer a scan) is listed unticked and never
    // created twice; Next adds exactly the ticked rows, even with more rows than fit on screen (the list scrolls)
    {
      OpenSensorWizard(A.screens[1].id); A.wizard.kind = DevKind::Hokuyo; SensorWizardNext();
      const std::string taken = "127.0.0.1:" + std::to_string(wz.port());
      std::vector<std::string> res{taken};
      for (int k = 0; k < 20; ++k) res.push_back("192.0.2." + std::to_string(10 + k));   // TEST-NET, never connected here
      SensorWizardMergeScan(res);
      A.wizard.manualHost = taken; SensorWizardAddManual();
      if (A.wizard.found.size() != 21 || A.wizard.pick[0] || !WizardHostAdded(taken)) return fail("wizard: a lidar that is already a device must be listed once, unticked");
      int ticked = 0; for (char c : A.wizard.pick) ticked += c;
      A.wizard.pick[0] = 1;   // a stale tick must still not create a second device for it
      const size_t pre = A.devices.size();
      SensorWizardNext();
      if (ticked != 20 || A.devices.size() - pre != (size_t)ticked || A.wizard.added.size() != (size_t)ticked) return fail("wizard: Next must add exactly the ticked rows");
      int same = 0; for (auto& e : A.devices) same += e.kind == DevKind::Hokuyo && e.host == "127.0.0.1" && e.port == wz.port();
      if (same != 1) return fail("wizard: a known lidar address was added a second time");
      const std::vector<std::string> made = A.wizard.added;   // drop them again: the checks below count from `before`
      A.devices.erase(std::remove_if(A.devices.begin(), A.devices.end(), [&](const Device& e) { return std::find(made.begin(), made.end(), e.id) != made.end(); }), A.devices.end());
      A.wizard.step = 0;
    }

    // modality: nothing on the Composition page takes input under the popup; a menu open at frame start blocks the popup for
    // that frame and owns Esc (the popup stays); with no menu open Esc closes the popup
    {
      A.wizard.step = 0; if (!DeckUiFree()) return fail("wizard: the deck must be free with no popup and no overlay");
      OpenSensorWizard(A.screens[0].id);
      if (DeckUiFree()) return fail("wizard: the deck (preview handles, wheel, pan) must not take input under the popup");
      CaptureOverlayState();
      if (A.overlayAtStart || !WizardEscCloses(false) || WizardEscCloses(true)) return fail("wizard: with no menu open Esc must close the popup (unless a text field is active)");
      A.ctx.open = true; CaptureOverlayState(); A.ctx.open = false;   // the menu item click closed it during DrawOverlays, after the capture
      if (!A.overlayAtStart) return fail("wizard: a menu open at frame start must block the popup that frame");
      if (WizardEscCloses(false)) return fail("wizard: Esc with the card menu open must close only the menu, not the popup");
      A.wizard.step = 0; CaptureOverlayState();
    }
    // Close: steps 1-2 cancel without a device; step 3 closes and keeps what was made, without changing the selection
    {
      const size_t n0 = A.devices.size();
      OpenSensorWizard(A.screens[0].id); CloseSensorWizard();
      if (A.wizard.step != 0 || A.devices.size() != n0) return fail("wizard: closing step 1 must make no device");
      OpenSensorWizard(A.screens[0].id); A.wizard.kind = DevKind::Skeleton; SensorWizardNext(); CloseSensorWizard();
      if (A.wizard.step != 0 || A.devices.size() != n0) return fail("wizard: closing step 2 must make no device");
      OpenSensorWizard(A.screens[0].id); A.wizard.kind = DevKind::Sim; SensorWizardNext();
      if (A.wizard.step != 3 || A.devices.size() != n0 + 1) return fail("wizard: Simulated must reach step 3 with one device");
      const std::string kept = A.wizard.added[0]; A.screen = 0;
      CloseSensorWizard();
      bool still = false; for (auto& e : A.devices) still |= e.id == kept;
      if (A.wizard.step != 0 || !still || A.devices.size() != n0 + 1) return fail("wizard: closing step 3 must keep the devices already made");
      if (A.screen != 0) return fail("wizard: closing must not change the page");
      A.devices.erase(std::remove_if(A.devices.begin(), A.devices.end(), [&](const Device& e) { return e.id == kept; }), A.devices.end());
    }
    OpenSensorWizard(A.screens[0].id); A.wizard.kind = DevKind::Skeleton; SensorWizardNext();
    for (auto& e : A.devices) if (e.port == A.wizard.port) return fail("wizard: a new camera must get a port no device uses");
    OpenSensorWizard(A.screens[0].id); A.wizard.kind = DevKind::Sim; SensorWizardNext();
    if (A.wizard.step != 3 || A.devices.size() != before + 2 || A.devices.back().kind != DevKind::Sim) return fail("wizard: Simulated skips the connect step");
    A.wizard.step = 0;
  }

#ifdef MIKMAP_FAKE_PLUGIN_FILE
  // Plugin ABI through the app: the same fake scan as Hokuyo, produced by the DLL this build wrote into plugins/sensors.
  const mikmap::SensorPlugin* fp = nullptr;
  for (auto& p : SensorPlugins()) if (FileName(p.path) == MIKMAP_FAKE_PLUGIN_FILE) { fp = &p; break; }
  if (!fp) return fail("sensor: the fake plugin was not found in " + SensorPluginDir());
  Device q = MakeDevice(DevKind::Plugin, "selftest-p");
  q.name = fp->name; q.plugin = MIKMAP_FAKE_PLUGIN_FILE; q.config = "{\"fail_after\":0}"; q.enabled = true;
  A.devices.push_back(q);
  for (double t0 = now(); now() - t0 < kWait && !(DeviceScan(dev("selftest-p")) && dev("selftest-p").touches.size() == 1);) { SensorsTick(now()); std::this_thread::sleep_for(std::chrono::milliseconds(5)); }
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

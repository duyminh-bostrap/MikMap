// Sensor I/O screen: device manager + calibration · radar · parameter routing.
#include "app.h"
#include "io/ISensorSource.h"
#include "io/plugin/PluginLoader.h"
#include "io/sources/ScanSource.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

using namespace ui;

Device* App::curDevice() {
  if (devices.empty()) return nullptr;
  selDevice = std::clamp(selDevice, 0, (int)devices.size() - 1);
  return &devices[selDevice];
}

void FitAffine(const std::vector<Calib>& c, float out[9], float* rms) {
  float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f, tnx = 1e9f, txx = -1e9f, tny = 1e9f, txy = -1e9f;
  float smx = 0, smy = 0, stx = 0, sty = 0;
  for (auto& p : c) {
    mnx = std::min(mnx, p.mx); mxx = std::max(mxx, p.mx); mny = std::min(mny, p.my); mxy = std::max(mxy, p.my);
    tnx = std::min(tnx, p.tx); txx = std::max(txx, p.tx); tny = std::min(tny, p.ty); txy = std::max(txy, p.ty);
    smx += p.mx; smy += p.my; stx += p.tx; sty += p.ty;
  }
  auto span = [](float a, float b) { float s = b - a; return s == 0 ? 1.f : s; };
  float sx = span(tnx, txx) / span(mnx, mxx), sy = span(tny, txy) / span(mny, mxy);
  float ox = stx / 4 - sx * (smx / 4), oy = sty / 4 - sy * (smy / 4);
  float err = 0;
  for (auto& p : c) {
    float dx = p.mx * sx + ox - p.tx, dy = p.my * sy + oy - p.ty;
    err += std::sqrt(dx * dx + dy * dy);
  }
  float m[9] = {sx, 0, ox, 0, sy, oy, 0, 0, 1};
  for (int i = 0; i < 9; ++i) out[i] = m[i];
  if (rms) *rms = err / 4;
}

static std::vector<std::string> Wrap(FontId f, float sz, const std::string& text, float width) {
  std::vector<std::string> lines; std::string cur, word;
  auto flush = [&]() {
    if (word.empty()) return;
    std::string t = cur.empty() ? word : cur + " " + word;
    if (!cur.empty() && TextW(f, sz, t.c_str(), 0.01f) > width) { lines.push_back(cur); cur = word; } else cur = t;
    word.clear();
  };
  for (char ch : text) { if (ch == ' ') flush(); else word += ch; }
  flush();
  if (!cur.empty()) lines.push_back(cur);
  return lines;
}

static std::string Endpoint(const Device& d) {
  if (d.kind == DevKind::Sim) return "simulated";
  if (d.kind == DevKind::Plugin) return d.plugin.empty() ? "plugin" : d.plugin;   // the file inside plugins/sensors, not a path
  return (d.kind == DevKind::Skeleton ? "OSC :" : d.host + ":") + std::to_string(d.port);
}

static void Devices(ImRect r) {
  Fill(r, K(pal::g12));
  VLine(r.Max.x - 1, r.Min.y, r.Max.y, K(pal::g2a));
  PanelHeader(Rc(r.Min.x, r.Min.y, r.GetWidth() - 1, 24), "Device manager", pal::t88);
  A.curDevice();   // clamps selDevice
  int on = 0; for (auto& d : A.devices) on += d.enabled;
  char cnt[32]; snprintf(cnt, sizeof cnt, "%d/%d enabled", on, (int)A.devices.size());
  TextR(r.Max.x - 7, r.Min.y + 11.5f, MONO_M, 10, K(pal::t66), cnt);
  static ScrollArea sa;
  sa.Begin("##devices", ImRect(r.Min.x, r.Min.y + 24, r.Max.x - 1, r.Max.y));
  float ox = sa.origin.x, oy = sa.origin.y, W = r.GetWidth() - 1 - 8, x = ox + 6, w = W - 12, y = 6;
  for (int di = 0; di < (int)A.devices.size(); ++di) {
    Device& d = A.devices[di];
    const int st = d.status;
    const bool fields = di == A.selDevice && d.kind != DevKind::Sim;   // the selected card edits its address in place
    const bool hint = d.enabled && d.kind == DevKind::Skeleton && st == (int)mikmap::SourceStatus::Running && d.packets == 0;
    const bool errLine = d.enabled && !d.error.empty();
    const float fh = fields ? 18.f : 0.f;
    bool fb = false; const Screen* dsc = DeviceScreen(d, &fb);
    float h = (d.enabled ? 76.f : 46.f) + fh + (hint || errLine || fb ? 16.f : 0.f);
    ImRect cr(x, oy + y, x + w, oy + y + h);
    float prev = g.alpha; if (!d.enabled) g.alpha = 0.65f;
    Box(cr, d.enabled ? K(pal::g18) : K(pal::g14), di == A.selDevice ? K(pal::coral) : d.enabled ? K(pal::g3a) : K(pal::g2a), 4);   // coral edge = the device the page edits
    float cy = cr.Min.y + 6 + 9;
    uint32_t dotHex = !d.enabled ? 0x555555 : st == (int)mikmap::SourceStatus::Error ? pal::red : st == (int)mikmap::SourceStatus::Connecting ? pal::yellow : pal::mint;
    Dot(ImVec2(cr.Min.x + 6 + 4 + 1, cy), 8, dotHex, d.enabled && st == (int)mikmap::SourceStatus::Running);
    const char* bl = d.enabled ? "DISCONNECT" : "CONNECT";
    float bw = TextW(MONO_B, 9, bl, 0.09f) + 16 + 2;
    ImRect br(cr.Max.x - 7 - bw, cy - 9, cr.Max.x - 7, cy + 9);
    Hit bh = HitR(br);
    uint32_t bx = d.enabled ? pal::red : pal::mint;
    Box(br, K(bx, 0.16f), K(bx, 0.45f), 3);
    Text(br.Min.x + 9, cy, MONO_B, 9, K(bx), bl, 0.09f);
    if (bh.hover) CursorHand();
    if (bh.click) { d.enabled = !d.enabled; }
    else if (!bh.hover && HitR(cr).click) { A.selDevice = di; A.wizardStep = 0; A.editRoi = false; }   // each device has its own calibration, so a half-done wizard does not carry over
    if (!bh.hover && HitR(cr).rclick) {
      MenuItem rm; rm.label = "Remove"; rm.icon = "trash-2"; rm.danger = true;
      rm.run = [id = d.id] {
        for (size_t i = 0; i < A.devices.size(); ++i) if (A.devices[i].id == id) { A.devices.erase(A.devices.begin() + i); A.wizardStep = 0; A.editRoi = false; break; }   // its source stops on the next SensorsTick
      };
      A.openCtx(ImGui::GetIO().MousePos, {rm});
    }
    TextEll(cr.Min.x + 6 + 8 + 6 + 1, cy, br.Min.x - 6 - (cr.Min.x + 21), UI_B, 11, K(pal::white), d.name.c_str());
    if (!fields) TextEll(cr.Min.x + 7, cy + 9 + 4 + 5, cr.GetWidth() - 14, MONO_R, 10, K(pal::t88), Endpoint(d).c_str());
    else if (d.kind == DevKind::Plugin) {   // JSON config, applied on blur: typing must not restart the source on every character
      // shortcut: config is raw JSON; with a second plugin, add mikmap_sensor_params() to describe each field, like TouchDesigner's setupParameters
      static std::string cfgEdit;
      ImGuiID cid = ImGui::GetID("##devconfig");
      if (ImGui::GetActiveID() != cid) cfgEdit = d.config;
      float fy = cy + 9 + 4, fx = cr.Min.x + 7, fr = cr.Max.x - 7;
      TextField("##devconfig", Rc(fx, fy, fr - fx, 18), cfgEdit, MONO_R, 10);
      if (ImGui::IsItemDeactivatedAfterEdit()) d.config = cfgEdit;
    }
    else {   // host (Hokuyo) and port, applied when the field loses focus so typing "9001" does not restart the source on every digit
      static std::string hostEdit; static int portEdit = 0;
      ImGuiID hid = ImGui::GetID("##devhost"), pid = ImGui::GetID("##devport");
      if (ImGui::GetActiveID() != hid) hostEdit = d.host;
      if (ImGui::GetActiveID() != pid) portEdit = d.port;
      float fy = cy + 9 + 4, fx = cr.Min.x + 7, fr = cr.Max.x - 7, pw = 64;
      if (d.kind == DevKind::Hokuyo) {
        TextField("##devhost", Rc(fx, fy, fr - pw - 6 - fx, 18), hostEdit, MONO_R, 10);
        if (ImGui::IsItemDeactivatedAfterEdit()) d.host = hostEdit;
      } else {
        Text(fx, fy + 9, MONO_R, 9, K(pal::t66), "OSC UDP port");
      }
      IntField("##devport", Rc(fr - pw, fy, pw, 18), portEdit);
      if (ImGui::IsItemDeactivatedAfterEdit()) d.port = std::clamp(portEdit, 0, 65535);
    }
    if (d.enabled) {
      float sy = cy + 9 + 4 + 10 + 4 + fh;
      HLine(cr.Min.x + 7, cr.Max.x - 7, sy, K(pal::g2a));
      float cw = (cr.GetWidth() - 14 - 8) / 3.f;
      char v0[16], v1[16]; snprintf(v0, sizeof v0, "%d", d.fps); snprintf(v1, sizeof v1, "%.0fms", d.latencyMs);
      const char* labs[3] = {"FPS", "LATENCY", "PACKETS"}; std::string vs[3] = {v0, v1, std::to_string(d.packets)};
      if (d.kind == DevKind::Sim) vs[0] = vs[1] = vs[2] = "\xE2\x80\x94";   // the simulated radar has no source to measure
      for (int i = 0; i < 3; ++i) {
        float sx = cr.Min.x + 7 + i * (cw + 4);
        Text(sx, sy + 4 + 5, MONO_R, 9, K(pal::t66), labs[i]);
        Text(sx, sy + 4 + 9 + 1 + 5, MONO_B, 10, K(i == 0 ? pal::mint : pal::tcc), vs[i].c_str());
      }
      if (errLine) TextEll(cr.Min.x + 7, cr.Max.y - 12, cr.GetWidth() - 14, MONO_R, 9, K(pal::red), d.error.c_str());
      else if (hint) {
        std::string cmd = "python tools/pose_bridge/pose_bridge.py --port " + std::to_string(d.port);   // nothing has arrived yet: show how to start the sender
        TextEll(cr.Min.x + 7, cr.Max.y - 12, cr.GetWidth() - 14, MONO_R, 9, K(pal::t66), cmd.c_str());
      }
    }
    if (fb && dsc && !errLine && !hint) TextEll(cr.Min.x + 7, cr.Max.y - 12, cr.GetWidth() - 14, MONO_R, 9, K(pal::yellow), ("Screen not found \xE2\x80\x94 using " + dsc->name).c_str());
    g.alpha = prev;
    y += h + 6;
  }
  {
    ImRect ab(x, oy + y, x + w, oy + y + 26);
    Hit ah = HitR(ab);
    Box(ab, ah.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
    float tw = TextW(UI_B, 10, "Add device"), acx = (ab.Min.x + ab.Max.x) * 0.5f, acy = (ab.Min.y + ab.Max.y) * 0.5f;
    Icon("plus", ImVec2(acx - tw * 0.5f - 9, acy), 12, K(pal::t88));
    Text(acx - tw * 0.5f + 2, acy, UI_B, 10, K(pal::t88), "Add device");
    if (ah.hover) CursorHand();
    if (ah.click) {
      std::vector<MenuItem> mi;
      for (DevKind k : {DevKind::Skeleton, DevKind::Hokuyo, DevKind::Sim}) {
        MenuItem it; it.label = k == DevKind::Skeleton ? "Camera skeleton" : k == DevKind::Hokuyo ? "Hokuyo 2D lidar" : "Simulated radar"; it.icon = "plus";
        it.run = [k] {
          int n = 1; auto taken = [&](const std::string& id) { for (auto& e : A.devices) if (e.id == id) return true; return false; };
          while (taken("dev" + std::to_string(n))) ++n;
          Device nd = MakeDevice(k, "dev" + std::to_string(n));
          int same = 0; for (auto& e : A.devices) same += e.kind == k;
          if (same) nd.name += " " + std::to_string(same + 1);
          if (!A.screens.empty()) nd.screenId = A.screens[0].id;
          A.devices.push_back(nd); A.selDevice = (int)A.devices.size() - 1; A.wizardStep = 0; A.editRoi = false;
        };
        mi.push_back(it);
      }
      for (const auto& p : SensorPlugins()) {   // one entry per DLL that reported API 1 (+ Add device, Task 10)
        const size_t slash = p.path.find_last_of("/\\");
        const std::string file = slash == std::string::npos ? p.path : p.path.substr(slash + 1);
        MenuItem it; it.label = p.name.empty() ? file : p.name; it.icon = "plus";
        it.run = [name = it.label, file, cfg = p.defaultConfig] {
          int n = 1; auto taken = [&](const std::string& id) { for (auto& e : A.devices) if (e.id == id) return true; return false; };
          while (taken("dev" + std::to_string(n))) ++n;
          Device nd = MakeDevice(DevKind::Plugin, "dev" + std::to_string(n));
          nd.name = name; nd.plugin = file; nd.config = cfg;
          int same = 0; for (auto& e : A.devices) same += e.kind == DevKind::Plugin && e.plugin == file;
          if (same) nd.name += " " + std::to_string(same + 1);
          if (!A.screens.empty()) nd.screenId = A.screens[0].id;
          A.devices.push_back(nd); A.selDevice = (int)A.devices.size() - 1; A.wizardStep = 0; A.editRoi = false;
        };
        mi.push_back(it);
      }
      A.openCtx(ImVec2(ab.Min.x, ab.Max.y + 4), mi);
    }
    y += 26 + 6;
  }
  Device* cd = A.curDevice();
  if (!cd) { sa.End(W, y); return; }
  // calibration wizard (for the selected device)
  HLine(ox + 6, ox + 6 + w, oy + y, K(pal::g2a));
  y += 1 + 6;
  Icon("wand-2", ImVec2(x + 6, oy + y + 6), 12, K(pal::yellow));
  Text(x + 12 + 4, oy + y + 6, UI_B, 9, K(pal::yellow), "CALIBRATION WIZARD", 0.14f);
  float m[9], rms; if (!FitHomography(cd->calib, m, &rms)) FitAffine(cd->calib, m, &rms);
  char rb[32]; if (cd->calibrated) snprintf(rb, sizeof rb, "RMS: %.2fpx", rms); else snprintf(rb, sizeof rb, "NOT CALIBRATED");
  TextR(x + w, oy + y + 6, MONO_R, 9, K(cd->calibrated ? pal::mint : pal::coral), rb);
  y += 12 + 6;
  auto lines = Wrap(UI_S, 10, "Maps sensor coordinates to projector coordinates via 3\xC3\x97" "3 homography (DLT).", w);
  for (auto& l : lines) { Text(x, oy + y + 7.5f, UI_S, 10, K(pal::t88), l.c_str(), 0.01f); y += 15; }
  y += 6;
  if (A.wizardStep == 0) {
    ImRect br(x, oy + y, x + w, oy + y + 30);
    Hit h = HitR(br);
    Box(br, K(pal::coral, 0.18f), K(pal::coral, 0.5f), 3);
    float tw = TextW(UI_B, 10, "Start wizard");
    float cx = (br.Min.x + br.Max.x) * 0.5f, cy2 = (br.Min.y + br.Max.y) * 0.5f;
    Icon("crosshair", ImVec2(cx - tw * 0.5f - 10, cy2), 12, K(pal::coral));
    Text(cx - tw * 0.5f + 2, cy2, UI_B, 10, K(pal::coral), "Start wizard");
    if (h.hover) CursorHand();
    if (h.click) A.wizardStep = 1;
    y += 30 + 6;
  } else {
    ImRect br(x, oy + y, x + w, oy + y + 6 + 13 + 4 + 15 + 4 + 24 + 6);
    Box(br, K(pal::g18), K(pal::yellow, 0.5f), 3);
    char st[24]; snprintf(st, sizeof st, "STEP %d OF 4", A.wizardStep);
    static const char* CN[4] = {"TOP-LEFT", "TOP-RIGHT", "BOTTOM-RIGHT", "BOTTOM-LEFT"};
    Text(br.Min.x + 7, br.Min.y + 6 + 6.5f, MONO_B, 11, K(pal::yellow), st);
    TextR(br.Max.x - 7, br.Min.y + 6 + 6.5f, MONO_R, 10, K(pal::t88), CN[A.wizardStep - 1]);
    bool live = cd->kind != DevKind::Sim;   // a real sensor reports where the target stands: Capture averages 30 frames of it
    Text(br.Min.x + 7, br.Min.y + 6 + 13 + 4 + 7.5f, UI_S, 10, K(pal::te0), live ? (cd->kind == DevKind::Skeleton ? "Hold your right wrist on the target, then Capture" : "Stand alone on the target, then Capture") : "Touch the physical crosshair, or click the target", 0.0f);
    ImRect cr(br.Min.x + 7, br.Max.y - 6 - 24, br.Max.x - 7, br.Max.y - 6);
    if (live) {
      float half = (cr.GetWidth() - 6) * 0.5f;
      ImRect cap(cr.Min.x, cr.Min.y, cr.Min.x + half, cr.Max.y);
      cr.Min.x += half + 6;
      Hit ch = HitR(cap);
      Box(cap, K(pal::coral, 0.18f), K(pal::coral, 0.5f), 3);
      TextC((cap.Min.x + cap.Max.x) * 0.5f, (cap.Min.y + cap.Max.y) * 0.5f, UI_B, 10, K(pal::coral), "Capture");
      if (ch.hover) CursorHand();
      if (ch.click) StartCapture(A.wizardStep);
    }
    Hit h = HitR(cr);
    Box(cr, K(pal::g050), K(pal::g22), 3);
    TextC((cr.Min.x + cr.Max.x) * 0.5f, (cr.Min.y + cr.Max.y) * 0.5f, UI_S, 10, K(pal::t88), live ? "Cancel" : "Cancel wizard");
    if (h.hover) CursorHand();
    if (h.click) A.wizardStep = 0;
    y += br.GetHeight() + 6;
  }
  // matrix
  ImRect mr(x, oy + y, x + w, oy + y + 6 + 9 + 4 + (6 + 3 * 13 + 2 * 4 + 6) + 6);
  Box(mr, K(pal::g050), K(pal::g2a), 3);
  Text(mr.Min.x + 7, mr.Min.y + 6 + 4.5f, MONO_B, 9, K(pal::coral), "MATRIX H_s", 0.09f);
  ImRect gr(mr.Min.x + 7, mr.Min.y + 6 + 9 + 4, mr.Max.x - 7, mr.Max.y - 7);
  Fill(gr, K(0x000000), 3);
  float cw = (gr.GetWidth() - 12 - 8) / 3.f;
  for (int i = 0; i < 9; ++i) {
    char b[16]; snprintf(b, sizeof b, "%.2f", m[i]);
    TextC(gr.Min.x + 6 + (i % 3) * (cw + 4) + cw * 0.5f, gr.Min.y + 6 + (i / 3) * 17 + 6.5f, MONO_R, 10, K(pal::tcc), b);
  }
  y += mr.GetHeight() + 8;
  // G8: save/load the 4 calibration points + ROI/noise as their own file, independent of the project.
  {
    float gap = 6, bw = (w - gap) * 0.5f;
    ImRect sb(x, oy + y, x + bw, oy + y + 26), lb(x + bw + gap, oy + y, x + w, oy + y + 26);
    Hit sh = HitR(sb), lh = HitR(lb);
    Box(sb, K(pal::g1c), K(pal::g22), 3);
    Icon("save", ImVec2(sb.Min.x + 12, (sb.Min.y + sb.Max.y) * 0.5f), 12, K(pal::t88));
    Text(sb.Min.x + 24, (sb.Min.y + sb.Max.y) * 0.5f, UI_S, 10, K(pal::t88), "Save profile");
    if (sh.hover) CursorHand();
    if (sh.click) A.notify(DoSaveCalibProfile());
    Box(lb, K(pal::g1c), K(pal::g22), 3);
    Icon("folder-open", ImVec2(lb.Min.x + 12, (lb.Min.y + lb.Max.y) * 0.5f), 12, K(pal::t88));
    Text(lb.Min.x + 24, (lb.Min.y + lb.Max.y) * 0.5f, UI_S, 10, K(pal::t88), "Load profile");
    if (lh.hover) CursorHand();
    if (lh.click) {
      std::vector<MenuItem> mi;
      auto profiles = ListCalibProfiles();
      if (profiles.empty()) { MenuItem it; it.label = "No saved profiles"; it.disabled = true; mi.push_back(it); }
      for (auto& pf : profiles) {
        MenuItem it; it.label = pf.name; it.icon = "folder-open";
        it.run = [path = pf.path, name = pf.name] {
          Device* cur = A.curDevice(); if (!cur) return;
          std::string err;
          if (LoadCalibProfile(path, *cur, A.noise, A.blobSize, err)) { A.wizardStep = 0; A.notify("Loaded profile: " + name); }
          else A.notify("Load failed: " + err);
        };
        mi.push_back(it);
      }
      A.openCtx(ImVec2(lb.Min.x, lb.Max.y + 4), mi);
    }
    y += 26 + 8;
  }
  sa.End(W, y);
}

// G17: a "touch.down" patch cord that is switched on fires the clip named in its target ("<layer> · <clip>").
void FireTouchRoutes() {
  for (auto& r : A.routes) {
    if (!r.active || r.source != "touch.down") continue;
    size_t sep = r.target.find(" \xC2\xB7 ");
    if (sep == std::string::npos) continue;
    std::string ln = r.target.substr(0, sep), cn = r.target.substr(sep + 4);
    for (int li = 0; li < (int)A.layers.size(); ++li) {
      if (A.layers[li].name != ln) continue;
      for (int ci = 0; ci < (int)A.layers[li].clips.size(); ++ci)
        if (A.layers[li].clips[ci].st != Clip::Empty && A.layers[li].clips[ci].name == cn) { A.trigger(li, ci); A.notify("Route fired: " + r.target, 2.0); return; }
    }
  }
}

// Task 4: camera skeleton. MediaPipe sends landmark ids as slot*100 + landmark, so a bone only
// connects two joints of the same person. Coordinates stay raw ([0,1] image space, y down) and are
// mapped straight onto the 4:3 frame; H_s is applied later, where the touch is used (principle #5).
static void SkeletonView(ImRect r, const Device& d) {
  Fill(r, K(pal::g050));
  ImRect tb(r.Min.x, r.Min.y, r.Max.x, r.Min.y + 44);
  Fill(tb, K(pal::g12)); HLine(tb.Min.x, tb.Max.x, tb.Max.y - 1, K(pal::g2a));
  float cy = r.Min.y + 21.5f;
  Icon("activity", ImVec2(tb.Min.x + 8 + 6.5f, cy), 13, K(pal::mint));
  Text(tb.Min.x + 8 + 13 + 6, cy, UI_B, 11, K(pal::white), "CAMERA VIEW", 0.09f);
  // Output overlay is a debug aid for every device, so it stays; Edit ROI has no meaning here.
  const char* ol = "Output overlay";
  float ow = TextW(UI_B, 10, ol) + 10 + 6 + 12 + 10;
  ImRect ob(tb.Max.x - 8 - ow, cy - 12, tb.Max.x - 8, cy + 12);
  Hit oh = HitR(ob);
  Box(ob, A.sensorOverlay ? K(pal::mint, 0.18f) : K(pal::g1c), A.sensorOverlay ? K(pal::mint) : K(pal::g22), 3);
  ImU32 ofg = K(A.sensorOverlay ? pal::mint : pal::t88);
  Icon("activity", ImVec2(ob.Min.x + 10 + 6, cy), 12, ofg);
  Text(ob.Min.x + 10 + 12 + 6, cy, UI_B, 10, ofg, ol);
  if (oh.hover) CursorHand();
  if (oh.click) A.sensorOverlay = !A.sensorOverlay;
  int slots[64], nsl = 0;
  for (auto& t : d.touches) {
    int s = t.id / 100; bool seen = false;
    for (int i = 0; i < nsl; ++i) if (slots[i] == s) { seen = true; break; }
    if (!seen && nsl < 64) slots[nsl++] = s;
  }
  char cnt[64]; snprintf(cnt, sizeof cnt, "%d fps \xC2\xB7 %d %s", d.fps, nsl, nsl == 1 ? "person" : "people");
  TextR(ob.Min.x - 8, cy, MONO_R, 10, K(pal::t88), cnt);

  ImRect area(r.Min.x, r.Min.y + 44, r.Max.x, r.Max.y);
  float avW = area.GetWidth() - 48, avH = area.GetHeight() - 48;
  if (avW < 40 || avH < 30) return;
  // shortcut: 4:3 matches the bridge's default 640x480 capture; if the bridge changes resolution,
  // it has to send the size in /mikmap/meta.
  float w = std::min(avW, avH * 4.f / 3.f), h = w * 3.f / 4.f;
  float cx = (area.Min.x + area.Max.x) * 0.5f, cyc = (area.Min.y + area.Max.y) * 0.5f;
  ImRect fr(cx - w * 0.5f, cyc - h * 0.5f, cx + w * 0.5f, cyc + h * 0.5f);
  float k = w / 640.f;
  Box(fr, K(0x070707), K(pal::g22), 4);
  auto at = [&](int id, ImVec2& p) {
    for (auto& t : d.touches) if (t.id == id) { p = ImVec2(fr.Min.x + t.x * w, fr.Min.y + t.y * h); return true; }
    return false;
  };
  static const int kBones[35][2] = {{0,1},{1,2},{2,3},{3,7},{0,4},{4,5},{5,6},{6,8},{9,10},{11,12},{11,13},{13,15},{15,17},{15,19},{15,21},{17,19},{12,14},{14,16},{16,18},{16,20},{16,22},{18,20},{11,23},{12,24},{23,24},{23,25},{24,26},{25,27},{26,28},{27,29},{28,30},{29,31},{30,32},{27,31},{28,32}};
  for (int si = 0; si < nsl; ++si) {
    ImU32 col = Ca(K(RoleHex(slots[si] % 3)));
    for (auto& b : kBones) {
      ImVec2 p0, p1;
      if (at(slots[si] * 100 + b[0], p0) && at(slots[si] * 100 + b[1], p1)) g.dl->AddLine(p0, p1, col, std::max(1.f, 3.f * k));
    }
    for (auto& t : d.touches) {
      if (t.id / 100 != slots[si]) continue;
      ImVec2 p(fr.Min.x + t.x * w, fr.Min.y + t.y * h);
      g.dl->AddCircleFilled(p, std::max(2.f, 3.5f * k), col, 16);
      if (t.id == kCalibJoint && A.wizardStep >= 1 && A.wizardStep <= 4)
        g.dl->AddCircle(p, std::max(6.f, 10.f * k), Ca(K(pal::yellow)), 24, std::max(1.f, 2.f * k));
    }
  }
  if (d.touches.empty()) TextC(cx, (fr.Max.y + area.Max.y) * 0.5f, MONO_R, 10, K(pal::t66), "No camera data \xE2\x80\x94 run pose_bridge.py");
}

static void Radar(ImRect r) {
  Device* cd = A.curDevice();
  if (cd && cd->kind == DevKind::Skeleton) { A.editRoi = false; SkeletonView(r, *cd); return; }
  const bool sim = cd && cd->kind == DevKind::Sim;   // the radar disc speaks the Sim's normalised units
  const bool lidar = cd && dynamic_cast<mikmap::ScanSource*>(DeviceSource(*cd)) != nullptr;   // Hokuyo, or a plugin DLL
  const mikmap::LaserScan* scan = lidar ? DeviceScan(*cd) : nullptr;
  if (!sim && !lidar) A.editRoi = false;
  Fill(r, K(pal::g050));
  ImRect tb(r.Min.x, r.Min.y, r.Max.x, r.Min.y + 44);
  Fill(tb, K(pal::g12)); HLine(tb.Min.x, tb.Max.x, tb.Max.y - 1, K(pal::g2a));
  float cy = r.Min.y + 21.5f;
  const char* title = lidar ? "LIDAR VIEW" : "RADAR VIEW";
  Icon("activity", ImVec2(tb.Min.x + 8 + 6.5f, cy), 13, K(pal::mint));
  Text(tb.Min.x + 8 + 13 + 6, cy, UI_B, 11, K(pal::white), title, 0.09f);
  float lx = tb.Min.x + 8 + 13 + 6 + TextW(UI_B, 11, title, 0.09f) + 6;
  // ROI button
  {
    const char* lb = A.editRoi ? "Finish ROI" : "Edit ROI";
    float w = TextW(UI_B, 10, lb) + 10 + 6 + 12 + 10;
    ImRect br(tb.Max.x - 8 - w, cy - 12, tb.Max.x - 8, cy + 12);
    Hit h = HitR(br);
    Box(br, A.editRoi ? K(pal::yellow) : K(pal::g1c), A.editRoi ? K(pal::yellow) : K(pal::g22), 3);
    ImU32 fg = K(A.editRoi ? 0x0f0f0f : pal::t88);
    Icon("target", ImVec2(br.Min.x + 10 + 6, cy), 12, fg);
    Text(br.Min.x + 10 + 12 + 6, cy, UI_B, 10, fg, lb);
    if (h.hover) CursorHand();
    if (h.click && (sim || lidar)) A.editRoi = !A.editRoi;
    const char* ol = "Output overlay";
    float ow = TextW(UI_B, 10, ol) + 10 + 6 + 12 + 10;
    ImRect ob(br.Min.x - 6 - ow, cy - 12, br.Min.x - 6, cy + 12);
    Hit oh = HitR(ob);
    Box(ob, A.sensorOverlay ? K(pal::mint, 0.18f) : K(pal::g1c), A.sensorOverlay ? K(pal::mint) : K(pal::g22), 3);
    ImU32 ofg = K(A.sensorOverlay ? pal::mint : pal::t88);
    Icon("activity", ImVec2(ob.Min.x + 10 + 6, cy), 12, ofg);
    Text(ob.Min.x + 10 + 12 + 6, cy, UI_B, 10, ofg, ol);
    if (oh.hover) CursorHand();
    if (oh.click) A.sensorOverlay = !A.sensorOverlay;
    char info[48];
    if (lidar) snprintf(info, sizeof info, "%d Hz \xC2\xB7 %d %s", cd->fps, (int)cd->touches.size(), cd->touches.size() == 1 ? "blob" : "blobs");
    else snprintf(info, sizeof info, "40.0 Hz real-time sweep");
    TextEll(lx, cy, ob.Min.x - 8 - lx, MONO_R, 10, K(pal::t88), info);
  }
  ImRect area(r.Min.x, r.Min.y + 44, r.Max.x, r.Max.y);
  float D = std::min({area.GetWidth() - 32, 540.f, area.GetHeight() - 32 - 6 - 10});
  if (D < 60) return;
  float totalH = D + 6 + 10;
  float cx = (area.Min.x + area.Max.x) * 0.5f, top = (area.Min.y + area.Max.y) * 0.5f - totalH * 0.5f;
  ImVec2 c(cx, top + D * 0.5f);
  float k = D / 640.f, maxR = (320.f - 25.f) * k;
  ImDrawList* dl = g.dl;
  dl->AddCircleFilled(c, D * 0.5f, Ca(K(0x080808)), 96);
  const float steps[4] = {0.25f, 0.5f, 0.75f, 1.f};
  for (int i = 0; i < 4; ++i) {
    float rr = maxR * steps[i];
    dl->AddCircle(c, rr, Ca(K(0x1a1a1a)), 96, std::max(1.f, k));
    char b[8]; snprintf(b, sizeof b, "%d.0m", i + 1);
    Text(c.x + 5 * k, c.y - rr + 7 * k, MONO_R, 11 * k, K(0x444444), b);
  }
  for (int i = 0; i < 8; ++i) {
    float a = i * 3.14159265f / 4.f;
    dl->AddLine(c, ImVec2(c.x + cosf(a) * maxR, c.y + sinf(a) * maxR), Ca(K(0x181818)), std::max(1.f, k));
  }
  // Lidar ↔ screen: front (0°) is up, +y is left. The disc is a fixed 4 m window.
  const float mmToPx = maxR / kRadarRangeMm;
  auto lidarToScreen = [&](float xmm, float ymm) { return ImVec2(c.x - ymm * mmToPx, c.y - xmm * mmToPx); };
  auto screenToLidar = [&](ImVec2 p) { return ImVec2(-(p.y - c.y) / mmToPx, -(p.x - c.x) / mmToPx); };

  // Raw scan: one 1 px dot per valid ray inside the disc (pal::t66, the same grey as the rest of the chrome).
  if (lidar && scan) {
    const ImU32 dot = Ca(K(pal::t66));
    for (int i = 0; i < scan->count; ++i) {
      const float rm = scan->rangeMm[i];
      if (rm <= 0.f || rm > kRadarRangeMm) continue;
      const float a = scan->angleRad[i];
      const ImVec2 p = lidarToScreen(rm * std::cos(a), rm * std::sin(a));
      dl->AddRectFilled(ImVec2(p.x - 0.5f, p.y - 0.5f), ImVec2(p.x + 0.5f, p.y + 0.5f), dot);
    }
  }

  // ROI: Sim in normalised units, lidar in raw millimetres.
  ImVec2 rp[4] = {};
  if (sim || lidar) for (int i = 0; i < 4; ++i)
    rp[i] = sim ? ImVec2(c.x + cd->roi[i].x * maxR, c.y + cd->roi[i].y * maxR) : lidarToScreen(cd->roi[i].x, cd->roi[i].y);
  if (sim || lidar) for (int i = 1; i + 1 < 4; ++i) dl->AddTriangleFilled(rp[0], rp[i], rp[i + 1], Ca(A.editRoi ? K(pal::yellow, 0.15f) : K(pal::cyan, 0.10f)));
  if (sim || lidar) dl->AddPolyline(rp, 4, Ca(K(A.editRoi ? pal::yellow : pal::cyan)), ImDrawFlags_Closed, 2 * k);
  if (A.editRoi && (sim || lidar)) for (int i = 0; i < 4; ++i) dl->AddCircleFilled(rp[i], 6 * k, Ca(K(pal::yellow)), 20);
  static int roiDrag = -1;   // while "Edit ROI" is on, the four yellow handles can be dragged (Sim clamps to the disc, lidar uses mm)
  if (!A.editRoi) roiDrag = -1;
  else {
    ImVec2 mm = ImGui::GetIO().MousePos;
    bool nearH = false;
    for (int i = 0; i < 4; ++i) if (std::hypot(mm.x - rp[i].x, mm.y - rp[i].y) <= 12 * k + 4) nearH = true;
    if (roiDrag < 0 && !g.blocked && ImGui::IsMouseClicked(0))
      for (int i = 0; i < 4; ++i) if (std::hypot(mm.x - rp[i].x, mm.y - rp[i].y) <= 12 * k + 4) { roiDrag = i; break; }
    if (roiDrag >= 0) {
      if (ImGui::IsMouseDown(0)) {
        if (sim) {
          float nx = (mm.x - c.x) / maxR, ny = (mm.y - c.y) / maxR, len = std::hypot(nx, ny);
          if (len > 1.f) { nx /= len; ny /= len; }
          cd->roi[roiDrag] = ImVec2(std::round(nx * 1000) / 1000, std::round(ny * 1000) / 1000);
        } else {   // millimetres, deliberately NOT clamped to the disc: a real lidar sees beyond 4 m
          const ImVec2 p = screenToLidar(mm);
          cd->roi[roiDrag] = ImVec2(std::round(p.x), std::round(p.y));
        }
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      } else roiDrag = -1;
    } else if (nearH) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  // sweep
  float a = A.sweep;
  if (sim) {
    RadialFan(c, maxR, K(pal::mint, 0.30f), K(pal::mint, 0.f), a - 0.35f, a, 24);
    dl->AddLine(c, ImVec2(c.x + cosf(a) * maxR, c.y + sinf(a) * maxR), Ca(K(pal::mint)), 1.5f * k);
  }
  if (lidar) for (auto& t : cd->touches) {   // touches are blob centroids in raw millimetres
    const ImVec2 b = lidarToScreen(t.x, t.y);
    dl->AddCircleFilled(b, 7 * k, Ca(K(pal::coral)), 24);
    dl->AddCircle(b, 12 * k, Ca(K(pal::white)), 24, 1.5f * k);
    char lb[64]; snprintf(lb, sizeof lb, "ID:%d [%d,%d mm]", t.id, (int)std::lround(t.x), (int)std::lround(t.y));
    Text(b.x + 14 * k, b.y + 4 * k - 1, MONO_B, 11 * k, K(pal::white), lb);
  }
  if (sim) for (auto& t : cd->touches) {
    ImVec2 b(c.x + t.x * maxR, c.y + t.y * maxR);
    dl->AddCircleFilled(b, 7 * k, Ca(K(pal::coral)), 24);
    dl->AddCircle(b, 12 * k, Ca(K(pal::white)), 24, 1.5f * k);
    char lb[48]; snprintf(lb, sizeof lb, "ID:%d [%dmm]", t.id, (int)std::round(t.x * 2000));
    Text(b.x + 14 * k, b.y + 4 * k - 1, MONO_B, 11 * k, K(pal::white), lb);
  }
  if ((sim || lidar) && A.wizardStep >= 1 && A.wizardStep <= 4) {
    const Calib& cb = cd->calib[A.wizardStep - 1];
    const bool known = !lidar || cb.mx != 0 || cb.my != 0;   // a lidar target only exists after Capture
    if (known) {
      ImVec2 t = sim ? ImVec2(c.x + cb.mx * maxR, c.y + cb.my * maxR) : lidarToScreen(cb.mx, cb.my);
      dl->AddCircle(t, 18 * k, Ca(K(pal::yellow)), 32, 2 * k);
      char lb[24]; snprintf(lb, sizeof lb, "TARGET %d/4", A.wizardStep);
      Text(t.x + 22 * k, t.y - 6 * k - 1, MONO_B, 12 * k, K(pal::yellow), lb);
    }
  }
  dl->AddCircle(c, D * 0.5f - 0.5f, Ca(K(pal::g2a)), 96, 1.f);
  TextC(cx, top + D + 6 + 5, MONO_R, 10, K(pal::t66), A.editRoi ? "Drag the yellow handles to reshape the region of interest" : sim ? "Click the radar to inject a simulated touch point" : lidar ? "" : "Live view appears when this device is running");
  // click
  ImVec2 m = ImGui::GetIO().MousePos;
  float dist = std::hypot(m.x - c.x, m.y - c.y);
  if (sim && !A.editRoi && Hover(ImRect(c.x - D * 0.5f, c.y - D * 0.5f, c.x + D * 0.5f, c.y + D * 0.5f)) && dist <= D * 0.5f) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsMouseClicked(0)) {
      float nx = (m.x - c.x) / maxR, ny = (m.y - c.y) / maxR;
      if (std::hypot(nx, ny) <= 1.05f) {
        if (A.wizardStep >= 1 && A.wizardStep <= 4) {
          cd->calib[A.wizardStep - 1].mx = std::round(nx * 1000) / 1000; cd->calib[A.wizardStep - 1].my = std::round(ny * 1000) / 1000;
          A.wizardStep = A.wizardStep == 4 ? 0 : A.wizardStep + 1;
          if (A.wizardStep == 0) cd->calibrated = true;
        } else {
          cd->touches.assign(1, Touch{100 + rand() % 900, nx, ny});
          FireTouchRoutes();
        }
      }
    }
  }
}

static void Routing(ImRect r) {
  Fill(r, K(pal::g12));
  VLine(r.Min.x, r.Min.y, r.Max.y, K(pal::g2a));
  PanelHeader(Rc(r.Min.x + 1, r.Min.y, r.GetWidth() - 1, 24), "Parameter routing", pal::t88);
  char cc[24]; snprintf(cc, sizeof cc, "%d cords", (int)A.routes.size());
  TextR(r.Max.x - 7, r.Min.y + 11.5f, MONO_M, 10, K(pal::t66), cc);
  static ScrollArea sa;
  sa.Begin("##routing", ImRect(r.Min.x + 1, r.Min.y + 24, r.Max.x, r.Max.y));
  float ox = sa.origin.x, oy = sa.origin.y, W = r.GetWidth() - 1 - 8, x = ox + 8, w = W - 16, y = 8;
  Text(x, oy + y + 5, UI_B, 9, K(pal::t88), "BLOB TRACKING", 0.14f); y += 9 + 8;
  {
    Text(x, oy + y + 5, UI_S, 10, K(pal::t88), "Noise threshold");
    char b[16]; snprintf(b, sizeof b, "%.1f", A.noise);
    TextR(x + w, oy + y + 5, MONO_B, 10, K(pal::mint), b);
    y += 10 + 4;
    float v = (A.noise - 0.2f) / 3.8f * 100.f;
    if (Slider(0x4001, Rc(x, oy + y + 4, w, 6), v, pal::mint)) A.noise = std::round((0.2f + v / 100.f * 3.8f) * 10) / 10;
    y += 14 + 8;
    Text(x, oy + y + 5, UI_S, 10, K(pal::t88), "Min blob size");
    snprintf(b, sizeof b, "%dmm", (int)A.blobSize);
    TextR(x + w, oy + y + 5, MONO_B, 10, K(pal::mint), b);
    y += 10 + 4;
    float v2 = std::clamp((A.blobSize - 5.f) / 295.f * 100.f, 0.f, 100.f);   // lidar blobs: 5-300 mm
    if (Slider(0x4002, Rc(x, oy + y + 4, w, 6), v2, pal::mint)) A.blobSize = std::round(5 + v2 / 100.f * 295.f);
    y += 14 + 8;
  }
  HLine(x, x + w, oy + y, K(pal::g2a)); y += 1 + 6;
  int act = 0; for (auto& rt : A.routes) act += rt.active;
  Text(x, oy + y + 5, UI_B, 9, K(pal::t88), "PATCH CORDS", 0.14f);
  char ab[24]; snprintf(ab, sizeof ab, "%d ACTIVE", act);
  TextR(x + w, oy + y + 5, MONO_R, 9, K(pal::coral), ab);
  y += 10 + 8;
  for (auto& rt : A.routes) {
    ImRect cr(x, oy + y, x + w, oy + y + 6 + 14 + 4 + 12 + 6 + 2);
    float prev = g.alpha; if (!rt.active) g.alpha = 0.6f;
    Box(cr, rt.active ? K(pal::g18) : K(pal::g14), rt.active ? K(pal::coral, 0.4f) : K(pal::g2a), 4);
    Hit h = HitR(ImRect(cr.Min.x, cr.Min.y, cr.Max.x, cr.Min.y + 26));
    float cy = cr.Min.y + 1 + 6 + 7;
    ImRect cb(cr.Max.x - 7 - 14, cy - 7, cr.Max.x - 7, cy + 7);
    Box(cb, rt.active ? K(pal::coral) : K(pal::g050), rt.active ? K(pal::coral) : K(pal::g22), 2);
    if (rt.active) Check(ImVec2((cb.Min.x + cb.Max.x) * 0.5f, (cb.Min.y + cb.Max.y) * 0.5f), 12, K(0x0f0f0f));
    TextEll(cr.Min.x + 7, cy, cb.Min.x - 6 - (cr.Min.x + 7), UI_B, 11, K(pal::white), rt.source.c_str());
    float cy2 = cy + 7 + 4 + 6;
    Text(cr.Min.x + 7, cy2, MONO_R, 10, K(pal::coral), "\xE2\x86\x92");
    TextEll(cr.Min.x + 7 + 16, cy2, cr.GetWidth() - 14 - 16, MONO_R, 10, K(pal::mint), rt.target.c_str());
    g.alpha = prev;
    if (h.hover) CursorHand();
    if (h.click) rt.active = !rt.active;
    y += cr.GetHeight() + 8;
  }
  {
    ImRect br(x, oy + y, x + w, oy + y + 30);
    Hit h = HitR(br);
    Box(br, h.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
    float tw = TextW(UI_B, 10, "Simulate particle trigger"), cx = (br.Min.x + br.Max.x) * 0.5f, cy = (br.Min.y + br.Max.y) * 0.5f;
    Icon("zap", ImVec2(cx - tw * 0.5f - 11, cy), 13, K(pal::coral));
    Text(cx - tw * 0.5f + 2, cy, UI_B, 10, K(pal::coral), "Simulate particle trigger");
    if (h.hover) CursorHand();
    if (h.click) {
      for (auto& d : A.devices) if (d.kind == DevKind::Sim) {   // normalised units: only the simulated radar understands them
        d.touches.assign(1, Touch{100 + rand() % 900, ((rand() % 1000) / 1000.f - 0.5f) * 1.4f, ((rand() % 1000) / 1000.f - 0.5f) * 1.4f});
        break;
      }
    }
    y += 30 + 8;
  }
  sa.End(W, y);
}

void DrawSensor(ImRect body) {
  Fill(body, K(pal::g0f));
  Devices(ImRect(body.Min.x, body.Min.y, body.Min.x + 320, body.Max.y));
  Routing(ImRect(body.Max.x - 300, body.Min.y, body.Max.x, body.Max.y));
  Radar(ImRect(body.Min.x + 320, body.Min.y, body.Max.x - 300, body.Max.y));
}


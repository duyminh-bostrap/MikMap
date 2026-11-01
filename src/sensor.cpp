// A sensor selected on the Mapping page (selKind 3): what its stage shows under Mapping's INPUT / OUTPUT bar, and its Properties.
// Input = what the sensor sees (a lidar placed in mm on its screen; the Sim radar; the camera skeleton).
// Output = where its points land on the screen (1920x1080): the 4 calibration targets, edited with Mapping's own quad tools.
#include "app.h"
#include "io/ISensorSource.h"
#include "io/net/NetScan.h"
#include "io/plugin/PluginLoader.h"
#include "io/sources/ScanSource.h"
#include "io/sources/CameraSource.h"
#include "io/camera/MfCamera.h"
#include <GLFW/glfw3.h>
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

std::vector<PropSection> SensorPropSections(DevKind k, int page) {
  using P = PropSection;
  if (page != 0) return {P::Calibration, P::SendOut, P::Triggers};
  if (k == DevKind::Hokuyo || k == DevKind::Plugin) return {P::Connection, P::ScreenSize, P::Placement, P::Blobs};   // a plugin is a 2D lidar today
  return {P::Connection};
}

uint32_t SensorSelTone(int page) { return page == 0 ? pal::cyan : pal::coral; }
static bool IsLidar(const Device& d) { return d.kind == DevKind::Hokuyo || d.kind == DevKind::Plugin; }

int SensorProgress(const Device& d, double now) {
  if (!PackAvailableForDevice(d)) return 0;
  const bool connected = d.enabled && (d.kind == DevKind::Sim || d.status == (int)mikmap::SourceStatus::Running) && d.error.empty();
  const bool placed = !IsLidar(d) || d.placed || d.calibrated;   // calibrated by hand (Custom 4-point) counts as placed
  const bool sending = d.outOn && d.lastSentAt >= 0 && now - d.lastSentAt <= 2.0;
  const bool reached[4] = {connected, placed, d.calibrated, sending};
  int n = 0; while (n < 4 && reached[n]) ++n;
  return n;
}

void GoToSensorStep(const std::string& id, int step) {
  SelectDevice(id);
  const Device* d = A.curDevice(); if (!d || d->id != id || step < 1 || step > 4) return;
  A.mpage = step >= 3 ? 1 : 0;
  if (step != 3) A.sensorSecOpen.try_emplace((int)d->kind, 0xffffffffu).first->second |= 1u << (int)(step == 1 ? PropSection::Connection : step == 2 ? PropSection::Placement : PropSection::SendOut);
}

static const char* CameraModeName(int mode) {
  static const char* names[] = {"Off", "Body", "Hand", "Face", "Full"};
  return names[mode >= 0 && mode <= 4 ? mode : 0];
}
static uint32_t GroupColor(uint8_t group) {
  static const uint32_t colors[] = {pal::cyan, pal::mint, pal::coral, pal::violet, pal::yellow};
  return colors[group < 5 ? group : 0];
}
static std::string Endpoint(const Device& d) {
  if (d.kind == DevKind::Camera2D) return CameraModeName(d.camMode);
  if (d.kind == DevKind::Sim) return "simulated";
  if (d.kind == DevKind::Plugin) return d.plugin.empty() ? "plugin" : d.plugin;   // the file inside plugins/sensors, not a path
  return (d.kind == DevKind::Skeleton ? "OSC :" : d.host + ":") + std::to_string(d.port);
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

static bool OverStage(ImRect area) {   // the pointer may grab something on this stage (not over a panel, popup or another window)
  return area.Contains(ImGui::GetIO().MousePos) && !g.blocked && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
}
static float StageBase(ImRect area, float SW, float SH) {   // space width in px at 100%, as Mapping's stage
  return std::min(std::min(area.GetWidth() - 20, 960.f), (area.GetHeight() - 20) * SW / SH);
}

// ───────────────────────── Input: camera skeleton ─────────────────────────
// MediaPipe sends landmark ids as slot*100 + landmark, so a bone only connects two joints of the same person. Coordinates stay raw
// ([0,1] image space, y down) and are mapped straight onto the 4:3 frame; H_s is applied later, where the touch is used (principle #5).
static int People(const Device& d, int slots[64]) {
  int n = 0;
  for (auto& t : d.touches) {
    int s = t.id / (t.group == mikmap::kGroupFace ? 1000 : 100); bool seen = false;
    for (int i = 0; i < n; ++i) if (slots[i] == s) { seen = true; break; }
    if (!seen && n < 64) slots[n++] = s;
  }
  return n;
}
static void SkeletonView(ImRect area, const Device& d) {
  const bool camera = d.kind == DevKind::Camera2D;
  const auto* preview = camera ? DevicePreview(d) : nullptr;
  const auto* points = camera ? DevicePreviewPoints(d) : nullptr;
  auto each = [&](auto visit) {
    if (camera) { if (points) for (int i = 0; i < points->count; ++i) visit(Touch{int(points->points[i].id), points->points[i].x, points->points[i].y, points->points[i].group}); }
    else for (const auto& t : d.touches) visit(t);
  };
  int slots[64], nsl = 0;
  each([&](const Touch& t) { int slot = t.id / (t.group == mikmap::kGroupFace ? 1000 : 100); if (nsl < 64 && std::find(slots, slots + nsl, slot) == slots + nsl) slots[nsl++] = slot; });
  float avW = area.GetWidth() - 48, avH = area.GetHeight() - 48;
  if (avW < 40 || avH < 30) return;
  // shortcut: 4:3 matches the bridge's default 640x480 capture; if the bridge changes resolution,
  // it has to send the size in /mikmap/meta.
  const float aspect = preview ? float(preview->width) / float(preview->height) : camera ? float(d.camW) / float(d.camH) : 4.f / 3.f;
  float w = std::min(avW, avH * aspect), h = w / aspect;
  float cx = (area.Min.x + area.Max.x) * 0.5f, cyc = (area.Min.y + area.Max.y) * 0.5f;
  ImRect fr(cx - w * 0.5f, cyc - h * 0.5f, cx + w * 0.5f, cyc + h * 0.5f);
  float k = w / 640.f;
  Box(fr, K(0x070707), K(SensorSelTone(0), 0.6f), 4);
  if (preview) {
    static GLuint texture = 0; static int tw = 0, th = 0; static int64_t timestamp = 0; static std::string deviceId;
    if (!texture) { glGenTextures(1, &texture); glBindTexture(GL_TEXTURE_2D, texture); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR); }
    glBindTexture(GL_TEXTURE_2D, texture); glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    if (tw != preview->width || th != preview->height) {
      tw = preview->width; th = preview->height;
      glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, tw, th, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr); timestamp = 0;
    }
    if (timestamp != preview->tCaptureNs || deviceId != d.id) {
      glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, tw, th, GL_RGB, GL_UNSIGNED_BYTE, preview->rgb.data()); timestamp = preview->tCaptureNs; deviceId = d.id;
    }
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4); glBindTexture(GL_TEXTURE_2D, 0);
    g.dl->AddImage((ImTextureID)(intptr_t)texture, fr.Min, fr.Max);
  }
  auto at = [&](uint8_t group, int id, ImVec2& p) {
    bool found = false;
    each([&](const Touch& t) { if (t.group == group && t.id == id) { p = ImVec2(fr.Min.x + t.x * w, fr.Min.y + t.y * h); found = true; } });
    return found;
  };
  static const int kBones[35][2] = {{0,1},{1,2},{2,3},{3,7},{0,4},{4,5},{5,6},{6,8},{9,10},{11,12},{11,13},{13,15},{15,17},{15,19},{15,21},{17,19},{12,14},{14,16},{16,18},{16,20},{16,22},{18,20},{11,23},{12,24},{23,24},{23,25},{24,26},{25,27},{26,28},{27,29},{28,30},{29,31},{30,32},{27,31},{28,32}};
  static const int handBones[][2] = {{0,1},{1,2},{2,3},{3,4},{0,5},{5,6},{6,7},{7,8},{0,9},{9,10},{10,11},{11,12},{0,13},{13,14},{14,15},{15,16},{0,17},{17,18},{18,19},{19,20}};
  for (int si = 0; si < nsl; ++si) {
    auto bones = [&](uint8_t group, const auto& pairs, int offset) {
      for (auto& b : pairs) {
        ImVec2 p0, p1;
        if (at(group, slots[si] * 100 + offset + b[0], p0) && at(group, slots[si] * 100 + offset + b[1], p1))
          g.dl->AddLine(p0, p1, Ca(K(GroupColor(group))), std::max(1.f, 3.f * k));
      }
    };
    bones(mikmap::kGroupBody, kBones, 0);
    bones(mikmap::kGroupHandL, handBones, 0);
    bones(mikmap::kGroupHandR, handBones, 50);
  }
  each([&](const Touch& t) {
    ImVec2 p(fr.Min.x + t.x * w, fr.Min.y + t.y * h);
    const float radius = t.group == mikmap::kGroupFace ? 1.6f : 3.5f;
    g.dl->AddCircleFilled(p, std::max(1.f, radius * k), Ca(K(GroupColor(t.group))), 12);
    if (A.wizardStep >= 1 && A.wizardStep <= 4 && IsCalibrationPoint(d, t))
      g.dl->AddCircle(p, std::max(6.f, 10.f * k), Ca(K(pal::yellow)), 24, std::max(1.f, 2.f * k));
  });
  if (camera && !preview) TextC(cx, (fr.Max.y + area.Max.y) * .5f, MONO_R, 10, K(pal::t66), "No camera preview");
  if (!camera && d.touches.empty()) TextC(cx, (fr.Max.y + area.Max.y) * 0.5f, MONO_R, 10, K(pal::t66), "No camera data \xE2\x80\x94 run pose_bridge.py");
}

// ───────────────────────── Input: simulated radar ─────────────────────────
// The Sim device speaks normalised units on a fixed disc; click injects a touch, or sets the wizard's point.
static void Radar(ImRect area, Device& cd) {
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
  ImVec2 rp[4];
  for (int i = 0; i < 4; ++i) rp[i] = ImVec2(c.x + cd.roi[i].x * maxR, c.y + cd.roi[i].y * maxR);
  for (int i = 1; i + 1 < 4; ++i) dl->AddTriangleFilled(rp[0], rp[i], rp[i + 1], Ca(A.editRoi ? K(pal::yellow, 0.15f) : K(pal::cyan, 0.10f)));
  dl->AddPolyline(rp, 4, Ca(K(A.editRoi ? pal::yellow : pal::cyan)), ImDrawFlags_Closed, 2 * k);
  if (A.editRoi) for (int i = 0; i < 4; ++i) dl->AddCircleFilled(rp[i], 6 * k, Ca(K(pal::yellow)), 20);
  static int roiDrag = -1;   // while "Edit ROI" is on, the four yellow handles can be dragged (clamped to the disc)
  if (!A.editRoi) roiDrag = -1;
  else {
    ImVec2 mm = ImGui::GetIO().MousePos;
    bool nearH = false;
    for (int i = 0; i < 4; ++i) if (std::hypot(mm.x - rp[i].x, mm.y - rp[i].y) <= 12 * k + 4) nearH = true;
    if (roiDrag < 0 && !g.blocked && ImGui::IsMouseClicked(0))
      for (int i = 0; i < 4; ++i) if (std::hypot(mm.x - rp[i].x, mm.y - rp[i].y) <= 12 * k + 4) { roiDrag = i; break; }
    if (roiDrag >= 0) {
      if (ImGui::IsMouseDown(0)) {
        float nx = (mm.x - c.x) / maxR, ny = (mm.y - c.y) / maxR, len = std::hypot(nx, ny);
        if (len > 1.f) { nx /= len; ny /= len; }
        cd.roi[roiDrag] = ImVec2(std::round(nx * 1000) / 1000, std::round(ny * 1000) / 1000);
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      } else roiDrag = -1;
    } else if (nearH) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  float a = A.sweep;
  const uint32_t tone = SensorSelTone(0);
  RadialFan(c, maxR, K(tone, 0.30f), K(tone, 0.f), a - 0.35f, a, 24);
  dl->AddLine(c, ImVec2(c.x + cosf(a) * maxR, c.y + sinf(a) * maxR), Ca(K(tone)), 1.5f * k);
  for (auto& t : cd.touches) {
    ImVec2 b(c.x + t.x * maxR, c.y + t.y * maxR);
    dl->AddCircleFilled(b, 7 * k, Ca(K(pal::coral)), 24);
    dl->AddCircle(b, 12 * k, Ca(K(pal::white)), 24, 1.5f * k);
    char lb[48]; snprintf(lb, sizeof lb, "ID:%d [%dmm]", t.id, (int)std::round(t.x * 2000));
    Text(b.x + 14 * k, b.y + 4 * k - 1, MONO_B, 11 * k, K(pal::white), lb);
  }
  if (A.wizardStep >= 1 && A.wizardStep <= 4) {
    const Calib& cb = cd.calib[A.wizardStep - 1];
    ImVec2 t(c.x + cb.mx * maxR, c.y + cb.my * maxR);
    dl->AddCircle(t, 18 * k, Ca(K(pal::yellow)), 32, 2 * k);
    char lb[24]; snprintf(lb, sizeof lb, "TARGET %d/4", A.wizardStep);
    Text(t.x + 22 * k, t.y - 6 * k - 1, MONO_B, 12 * k, K(pal::yellow), lb);
  }
  dl->AddCircle(c, D * 0.5f - 0.5f, Ca(K(pal::g2a)), 96, 1.f);
  TextC(cx, top + D + 6 + 5, MONO_R, 10, K(pal::t66), A.editRoi ? "Drag the yellow handles to reshape the region of interest" : "Click the radar to inject a simulated touch point");
  ImVec2 m = ImGui::GetIO().MousePos;
  float dist = std::hypot(m.x - c.x, m.y - c.y);
  if (!A.editRoi && Hover(ImRect(c.x - D * 0.5f, c.y - D * 0.5f, c.x + D * 0.5f, c.y + D * 0.5f)) && dist <= D * 0.5f) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsMouseClicked(0)) {
      float nx = (m.x - c.x) / maxR, ny = (m.y - c.y) / maxR;
      if (std::hypot(nx, ny) <= 1.05f) {
        if (A.wizardStep >= 1 && A.wizardStep <= 4) {
          cd.calib[A.wizardStep - 1].mx = std::round(nx * 1000) / 1000; cd.calib[A.wizardStep - 1].my = std::round(ny * 1000) / 1000;
          A.wizardStep = A.wizardStep == 4 ? 0 : A.wizardStep + 1;
          if (A.wizardStep == 0) cd.calibrated = true;
        } else {
          cd.touches.assign(1, Touch{100 + rand() % 900, nx, ny});
          FireTouchRoutes();
        }
      }
    }
  }
}

// ───────────────────────── Input: a lidar placed on its screen, in mm ─────────────────────────
// No real size yet: two fields in the middle of the stage, written straight into the Screen (the same numbers as Mapping's).
static void ScreenSizeEntry(ImRect area, Screen& sc) {
  const float w = 320, h = 112, cx = (area.Min.x + area.Max.x) * 0.5f, cy = (area.Min.y + area.Max.y) * 0.5f;
  ImRect r(cx - w * 0.5f, cy - h * 0.5f, cx + w * 0.5f, cy + h * 0.5f);
  Box(r, K(pal::g14), K(pal::coral, 0.5f), 4);
  Icon("maximize", ImVec2(r.Min.x + 16, r.Min.y + 18), 12, K(pal::coral));
  TextEll(r.Min.x + 28, r.Min.y + 18, w - 40, UI_B, 11, K(pal::white), ("Real size of " + sc.name).c_str());
  TextEll(r.Min.x + 12, r.Min.y + 40, w - 24, UI_S, 10, K(pal::t88), "The lidar is placed in millimetres on this surface.");
  static int ew = 0, eh = 0;   // applied when a field loses focus, so typing "4000" does not jump to the stage after the first digit
  ImGuiID wid = ImGui::GetID("##realw"), hid = ImGui::GetID("##realh");
  if (ImGui::GetActiveID() != wid) ew = (int)sc.realW;
  if (ImGui::GetActiveID() != hid) eh = (int)sc.realH;
  const float fw = (w - 24 - 12) * 0.5f, fy = r.Min.y + 58;
  Text(r.Min.x + 12, fy + 5, MONO_R, 9, K(pal::t66), "WIDTH (MM)");
  Text(r.Min.x + 12 + fw + 12, fy + 5, MONO_R, 9, K(pal::t66), "HEIGHT (MM)");
  IntField("##realw", Rc(r.Min.x + 12, fy + 14, fw, 26), ew);
  if (ImGui::IsItemDeactivatedAfterEdit()) { sc.realW = (float)std::clamp(ew, 0, 100000); A.pushHist(); }
  IntField("##realh", Rc(r.Min.x + 12 + fw + 12, fy + 14, fw, 26), eh);
  if (ImGui::IsItemDeactivatedAfterEdit()) { sc.realH = (float)std::clamp(eh, 0, 100000); A.pushHist(); }
}

// A sensor stage's background: what the Deck's Live Output monitor shows (the composition, or the test card), stretched to
// the stage box; nothing over the dark box during blackout, as the monitor shows OUTPUT MUTED.
static void CompBackdrop(ImRect cv, float alpha) {
  Box(cv, K(0x0d0d0d), 0, 3);
  if (A.blackout) return;
  g.dl->PushClipRect(cv.Min, cv.Max, true);
  DrawComposite(cv, (float)g.time * 1.2f, alpha);   // same time base as the Live Output monitor
  g.dl->PopClipRect();
}

// The other sensors on this screen, grey and dim, not clickable; drawn before the selected one so it stays on top.
template <class F> static void EachOtherSensor(const Device& d, const Screen* sc, F f) {
  for (auto& o : A.devices) if (o.id != d.id && o.enabled && DeviceScreen(o) == sc) f(o);   // a disabled one sends nothing: no ghost
}
static void GhostLidar(const StageXf& X, const Device& o) {
  const ImU32 col = Ca(K(pal::t66, 0.45f));
  const mikmap::LidarPose& P = o.pose;
  if (const mikmap::LaserScan* scan = DeviceScan(o))
    for (int i = 0; i < scan->count; ++i) {
      const float rm = scan->rangeMm[i];
      if (rm <= 0.f) continue;
      const mikmap::Vec2 s = mikmap::lidarToSurface(P, {rm * std::cos(scan->angleRad[i]), rm * std::sin(scan->angleRad[i])});
      const ImVec2 p = X.toPx(ImVec2((float)s.x, (float)s.y));
      g.dl->AddRectFilled(ImVec2(p.x - 0.5f, p.y - 0.5f), ImVec2(p.x + 0.5f, p.y + 0.5f), col);
    }
  const ImVec2 c = X.toPx(ImVec2((float)P.x, (float)P.y));
  const float a = (float)P.angleDeg * kDegToRad;
  const ImVec2 fw(std::cos(a), std::sin(a));
  g.dl->AddLine(c, ImVec2(c.x + fw.x * 26, c.y + fw.y * 26), col, 1.5f);
  g.dl->AddCircle(c, 9, col, 24, 1.5f);
  TextC(c.x, c.y + 20, MONO_R, 9, K(pal::t66, 0.45f), o.name.c_str());
}

// A sensor may hang anywhere (a lidar is often mounted far beside or behind a small screen); this only keeps garbage out of the pose.
float SanePoseMm(float v, float keep) { return std::isfinite(v) ? std::clamp(v, -1e6f, 1e6f) : keep; }
// Largest 1-2-5 value (..., 1, 2, 5, 10, 20, 50, ...) that is <= max(realW, realH) / 50.
float SensorSnapStepMm(float realW, float realH) {
  const double t = std::max(realW, realH) / 50.0;
  if (!(t > 0)) return 1.f;
  const double base = std::pow(10.0, std::floor(std::log10(t) + 1e-9)), m = t / base;
  return (float)(base * (m >= 5 - 1e-9 ? 5 : m >= 2 - 1e-9 ? 2 : 1));
}
// Pointer (window px) -> sensor position (mm): exact through the stage transform; snapMm > 0 rounds to that grid; shift keeps the axis
// the pointer moved less on at its start. mo0 = the pointer at grab (mm), off = pose - mo0.
ImVec2 SensorDragToMm(const StageXf& X, ImVec2 mouse, ImVec2 off, ImVec2 mo0, bool shift, float snapMm) {
  const ImVec2 mo = X.fromPx(mouse);
  float nx = mo.x + off.x, ny = mo.y + off.y;
  if (shift) { if (std::fabs(mo.x - mo0.x) >= std::fabs(mo.y - mo0.y)) ny = mo0.y + off.y; else nx = mo0.x + off.x; }
  if (snapMm > 0) { nx = std::round(nx / snapMm) * snapMm; ny = std::round(ny / snapMm) * snapMm; }
  return ImVec2(SanePoseMm(nx, mo0.x + off.x), SanePoseMm(ny, mo0.y + off.y));
}

static void LidarPlacementView(ImRect area, Device& d) {
  Screen* sc = DeviceScreen(d);
  if (!sc) { TextC((area.Min.x + area.Max.x) * 0.5f, (area.Min.y + area.Max.y) * 0.5f, MONO_R, 10, K(pal::t66), "No screen to place the sensor on"); return; }
  if (sc->realW <= 0 || sc->realH <= 0) { ScreenSizeEntry(area, *sc); return; }
  const float SW = sc->realW, SH = sc->realH;
  static int drag = 0; static std::string dragDev; static ImVec2 off, mo0;   // 1 move the sensor, 2 turn it
  if (drag && (!ImGui::IsMouseDown(0) || dragDev != d.id)) {   // the drag ended: that pose places a lidar nobody aligned yet
    for (auto& o : A.devices) if (o.id == dragDev) CommitPose(o);
    drag = 0; A.pushHist();
  }
  bool rClick = false;
  const StageXf X = StageViewUpdate(A.sensorView[0], area, SW, SH, StageBase(area, SW, SH), false, false, &rClick);   // no edge-scroll: the view must not run away under a sensor being dragged
  const mikmap::LidarPose& P = d.pose;
  auto surf = [&](float xmm, float ymm) { const mikmap::Vec2 s = mikmap::lidarToSurface(P, {xmm, ymm}); return X.toPx(ImVec2((float)s.x, (float)s.y)); };
  ImDrawList* dl = g.dl;
  dl->PushClipRect(area.Min, area.Max, true);
  CompBackdrop(X.cv, 1.f);   // the live composition stretched over the screen's real surface (realW x realH mm), with a faint 1 m grid to place by
  EachOtherSensor(d, sc, [&](const Device& o) { if (IsLidar(o) && (o.placed || o.calibrated)) GhostLidar(X, o); });   // a never-placed pose is a default, not where it hangs
  for (float gx = 1000; gx < SW; gx += 1000) dl->AddLine(X.toPx({gx, 0}), X.toPx({gx, SH}), Ca(K(pal::g1c)), 1.f);
  for (float gy = 1000; gy < SH; gy += 1000) dl->AddLine(X.toPx({0, gy}), X.toPx({SW, gy}), Ca(K(pal::g1c)), 1.f);
  Border(X.cv, K(pal::coral, 0.7f), 2, 1.5f);
  char sz[96]; snprintf(sz, sizeof sz, "%s \xC2\xB7 %d \xC3\x97 %d mm", sc->name.c_str(), (int)SW, (int)SH);
  Text(X.cv.Min.x, X.cv.Min.y - 10, MONO_B, 10, K(pal::coral), sz);
  // raw scan, 1 px per ray; rays past the range limit are dimmed (the source drops them)
  if (const mikmap::LaserScan* scan = DeviceScan(d)) {
    const ImU32 dot = Ca(K(pal::t66)), dim = Ca(K(pal::t66, 0.3f));
    for (int i = 0; i < scan->count; ++i) {
      const float rm = scan->rangeMm[i];
      if (rm <= 0.f) continue;
      const ImVec2 p = surf(rm * std::cos(scan->angleRad[i]), rm * std::sin(scan->angleRad[i]));
      dl->AddRectFilled(ImVec2(p.x - 0.5f, p.y - 0.5f), ImVec2(p.x + 0.5f, p.y + 0.5f), d.rangeMm > 0 && rm > d.rangeMm ? dim : dot);
    }
  }
  ImVec2 rp[4]; for (int i = 0; i < 4; ++i) rp[i] = surf(d.roi[i].x, d.roi[i].y);
  dl->AddPolyline(rp, 4, Ca(K(pal::cyan, 0.6f)), ImDrawFlags_Closed, 1.f);   // ROI (raw mm; Count only: Screen in Properties makes it the screen)
  for (auto& t : d.touches) {   // blob centroids, raw mm
    const ImVec2 b = surf(t.x, t.y);
    dl->AddCircleFilled(b, 6, Ca(K(pal::coral)), 20);
    dl->AddCircle(b, 10, Ca(K(pal::white)), 20, 1.2f);
    char lb[16]; snprintf(lb, sizeof lb, "ID:%d", t.id);
    Text(b.x + 13, b.y, MONO_B, 10, K(pal::white), lb);
  }
  if (A.wizardStep >= 1 && A.wizardStep <= 4 && A.curDevice() == &d) {   // the point Capture stored for this step
    const Calib& cb = d.calib[A.wizardStep - 1];
    if (cb.mx != 0 || cb.my != 0) dl->AddCircle(surf(cb.mx, cb.my), 16, Ca(K(pal::yellow)), 32, 2);
  }
  // the sensor: body, facing arrow, range ring, and the turn dot 60 px ahead
  const ImVec2 c = X.toPx(ImVec2((float)P.x, (float)P.y));
  const float a = (float)P.angleDeg * kDegToRad;
  const ImVec2 fw(std::cos(a), std::sin(a)), hd(c.x + fw.x * 60, c.y + fw.y * 60);
  const ImVec2 m = ImGui::GetIO().MousePos;
  int hot = 0;
  if (!drag && OverStage(area)) {
    if (std::hypot(m.x - hd.x, m.y - hd.y) <= 9) hot = 2; else if (std::hypot(m.x - c.x, m.y - c.y) <= 13) hot = 1;
  }
  if (hot && ImGui::IsMouseClicked(0)) {
    drag = hot; dragDev = d.id; mo0 = X.fromPx(m); off = ImVec2((float)P.x - mo0.x, (float)P.y - mo0.y);
  }
  if (drag == 1) {   // move: follows the pointer exactly; Ctrl: snap to the screen-size grid; Shift: along the axis the pointer has moved further on
    const ImGuiIO& io = ImGui::GetIO();
    const ImVec2 np = SensorDragToMm(X, m, off, mo0, io.KeyShift, io.KeyCtrl ? SensorSnapStepMm(SW, SH) : 0.f);
    d.pose.x = np.x; d.pose.y = np.y;
  } else if (drag == 2) {   // turn about the sensor; Shift: 15 degree steps
    float deg = std::atan2(m.y - c.y, m.x - c.x) / kDegToRad;
    deg = ImGui::GetIO().KeyShift ? std::round(deg / 15.f) * 15.f : std::round(deg * 10.f) / 10.f;
    d.pose.angleDeg = deg;
  }
  if (hot || drag) ImGui::SetMouseCursor(hot == 2 || drag == 2 ? ImGuiMouseCursor_ResizeAll : ImGuiMouseCursor_Hand);
  const uint32_t tone = SensorSelTone(0);
  if (d.rangeMm > 0) dl->AddCircle(c, d.rangeMm * X.s, Ca(K(tone, 0.35f)), 96, 1.f);
  dl->AddLine(c, hd, Ca(K(tone, 0.7f)), 1.5f);
  const ImVec2 tip(c.x + fw.x * 26, c.y + fw.y * 26), nrm(-fw.y, fw.x);
  dl->AddTriangleFilled(tip, ImVec2(c.x + fw.x * 17 + nrm.x * 5, c.y + fw.y * 17 + nrm.y * 5), ImVec2(c.x + fw.x * 17 - nrm.x * 5, c.y + fw.y * 17 - nrm.y * 5), Ca(K(tone)));
  dl->AddCircleFilled(c, 9, Ca(K(tone, hot == 1 || drag == 1 ? 0.45f : 0.25f)), 24);
  dl->AddCircle(c, 9, Ca(K(hot == 1 || drag == 1 ? pal::white : tone)), 24, 1.8f);
  dl->AddCircleFilled(hd, 5, Ca(K(hot == 2 || drag == 2 ? pal::white : tone)), 16);
  char pl[64]; snprintf(pl, sizeof pl, "%d, %d mm \xC2\xB7 %.1f\xC2\xB0", (int)P.x, (int)P.y, P.angleDeg);
  TextC(c.x, c.y + 20, MONO_R, 9, K(tone), pl);
  dl->PopClipRect();
  const char* hint = d.placed ? "Drag the sensor to move it, the dot to turn it \xC2\xB7 Shift: one axis / 15\xC2\xB0 \xC2\xB7 Ctrl: snap"
                   : d.calibrated ? "Custom (4-point) calibration: moving the sensor does not change it \xE2\x80\x94 Re-apply placement on Output"
                                  : "Drag the sensor to where it really is \xE2\x80\x94 that places it";
  TextC((area.Min.x + area.Max.x) * 0.5f, area.Max.y - 12, MONO_R, 10, K(d.placed ? pal::t66 : pal::yellow), hint);
}

// ───────────────────────── Output: the 4 calibration targets on the screen ─────────────────────────
static void SensorOutputView(ImRect area, Device& d) {
  Screen* sc = DeviceScreen(d);
  if (!sc || d.calib.size() != 4) return;
  bool rClick = false;
  // shortcut: no edge-scroll while dragging a quad point (EditQuad does not say a drag is in flight); add it if points hide under the panels
  const StageXf X = StageViewUpdate(A.sensorView[1], area, 1920, 1080, StageBase(area, 1920, 1080), false, false, &rClick);
  g.dl->PushClipRect(area.Min, area.Max, true);
  CompBackdrop(X.cv, 0.6f);   // dimmed: the quad is what this page edits
  Border(X.cv, K(pal::coral, 0.6f), 2, 1.f);
  EachOtherSensor(d, sc, [&](const Device& o) {   // their calibration quads and touches, grey
    const ImU32 col = Ca(K(pal::t66, 0.45f));
    if (o.calib.size() == 4) {
      ImVec2 oq[4]; for (int i = 0; i < 4; ++i) oq[i] = X.toPx(ImVec2(o.calib[i].tx, o.calib[i].ty));
      g.dl->AddPolyline(oq, 4, col, ImDrawFlags_Closed, 1.5f);
    }
    float oH[9];
    if (DeviceH(o, oH)) for (auto& t : o.touches) {
      float ox, oy; ApplyH(oH, t.x, t.y, ox, oy);
      g.dl->AddCircleFilled(X.toPx(ImVec2(ox, oy)), 5, col, 20);
    }
  });
  const uint32_t tone = SensorSelTone(1);
  ImVec2 q[4]; for (int i = 0; i < 4; ++i) q[i] = ImVec2(d.calib[i].tx, d.calib[i].ty);
  const std::string id = "##sensorquad" + d.id;
  const bool released = EditQuad(id.c_str(), X, q, A.outTool, tone, OverStage(area));
  QuadEdit(d, q);
  if (released) A.pushHist();
  static const char* kCorner[4] = {"1", "2", "3", "4"};
  const ImVec2 qc = X.toPx(ImVec2((q[0].x + q[1].x + q[2].x + q[3].x) * 0.25f, (q[0].y + q[1].y + q[2].y + q[3].y) * 0.25f));
  for (int i = 0; i < 4; ++i) {   // numbered inside the quad, so a corner at the stage edge keeps its label
    const ImVec2 p = X.toPx(q[i]); const float dx = qc.x - p.x, dy = qc.y - p.y, l = std::max(1.f, std::hypot(dx, dy));
    TextC(p.x + dx / l * 26, p.y + dy / l * 26, MONO_B, 10, K(tone), kCorner[i]);
  }
  if (A.wizardStep >= 1 && A.wizardStep <= 4 && A.curDevice() == &d) g.dl->AddCircle(X.toPx(q[A.wizardStep - 1]), 18, Ca(K(pal::yellow)), 32, 2);
  float H[9];
  if (DeviceH(d, H)) for (auto& t : d.touches) {   // what the device sends, through its H_s
    float ox, oy; ApplyH(H, t.x, t.y, ox, oy);
    const ImVec2 p = X.toPx(ImVec2(ox, oy));
    g.dl->AddCircleFilled(p, 6, Ca(K(pal::coral)), 20);
    g.dl->AddCircle(p, 10, Ca(K(pal::white)), 20, 1.2f);
  }
  g.dl->PopClipRect();
  TextC((area.Min.x + area.Max.x) * 0.5f, area.Max.y - 12, MONO_R, 10, K(pal::t66),
        A.outTool == 0 ? "Drag a corner to fine-tune where the sensor lands \xC2\xB7 Shift: one axis" : "Move, scale or turn the whole quad \xC2\xB7 Shift: axis / aspect / 45\xC2\xB0");
}

// The Sim radar and the camera keep their own fixed Input views (no pan/zoom).
static bool FixedView(const Device& d) { return A.mpage == 0 && !IsLidar(d); }

// Right of Mapping's bar while a sensor is selected: − % + (a stage that pans), Output overlay, Edit ROI (Sim radar), and a live
// readout in the free middle from lx.
void SensorBarTools(ImRect tb, float lx, Device& d) {
  const float cy = tb.Min.y + 21.5f, kBtnW = 28.f, kBtnH = 28.f, kGrpH = 17.f;
  float xr = tb.Max.x - 6;
  if (!FixedView(d)) {   // − % + ; the readout goes back to 100 % centred
    StageView& v = A.sensorView[A.mpage == 0 ? 0 : 1];
    const float pctW = 46.f, gw = 3 + kBtnW + pctW + kBtnW + 3;
    ImRect zg(xr - gw, cy - kGrpH, xr, cy + kGrpH);
    Box(zg, K(pal::g1c), K(pal::g22), 3);
    float zx = zg.Min.x + 3;
    auto iconBtn = [&](float x, const char* ic) {
      ImRect br(x, cy - kBtnH * 0.5f, x + kBtnW, cy + kBtnH * 0.5f);
      Hit h = HitR(br);
      if (h.hover) { Fill(br, K(pal::g22), 3); CursorHand(); }
      Icon(ic, ImVec2((br.Min.x + br.Max.x) * 0.5f, cy), 16, K(h.hover ? pal::white : pal::tcc));
      return h.click;
    };
    if (iconBtn(zx, "zoom-out")) v.setZoom(v.z / 1.4f);
    zx += kBtnW;
    ImRect pr(zx, cy - kBtnH * 0.5f, zx + pctW, cy + kBtnH * 0.5f);
    Hit ph = HitR(pr);
    if (ph.hover) { Fill(pr, K(pal::g22), 3); CursorHand(); }
    char zl[16]; snprintf(zl, sizeof zl, "%d%%", (int)std::round(v.z * 100));
    TextC((pr.Min.x + pr.Max.x) * 0.5f, cy, MONO_B, 11, K(ph.hover ? pal::white : pal::tcc), zl);
    if (ph.click) v.setZoom(1, 0.5f, 0.5f);
    zx += pctW;
    if (iconBtn(zx, "zoom-in")) v.setZoom(v.z * 1.4f);
    xr = zg.Min.x - 6;
  }
  {   // G13: draw live touches on the projector window (a debug aid for every device)
    const char* ol = "Output overlay";
    const float ow = TextW(UI_B, 10, ol) + 10 + 6 + 12 + 10;
    ImRect ob(xr - ow, cy - 12, xr, cy + 12);
    Hit oh = HitR(ob);
    Box(ob, A.sensorOverlay ? K(pal::mint, 0.18f) : K(pal::g1c), A.sensorOverlay ? K(pal::mint) : K(pal::g22), 3);
    const ImU32 ofg = K(A.sensorOverlay ? pal::mint : pal::t88);
    Icon("activity", ImVec2(ob.Min.x + 10 + 6, cy), 12, ofg);
    Text(ob.Min.x + 10 + 12 + 6, cy, UI_B, 10, ofg, ol);
    if (oh.hover) CursorHand();
    if (oh.click) A.sensorOverlay = !A.sensorOverlay;
    xr = ob.Min.x - 8;
  }
  char info[64] = "";
  if (A.mpage == 0 && d.kind == DevKind::Sim) {   // Edit ROI: the Sim radar's handles
    const char* lb = A.editRoi ? "Finish ROI" : "Edit ROI";
    const float w = TextW(UI_B, 10, lb) + 10 + 6 + 12 + 10;
    ImRect br(xr - w, cy - 12, xr, cy + 12);
    Hit h = HitR(br);
    Box(br, A.editRoi ? K(pal::yellow) : K(pal::g1c), A.editRoi ? K(pal::yellow) : K(pal::g22), 3);
    const ImU32 fg = K(A.editRoi ? 0x0f0f0f : pal::t88);
    Icon("target", ImVec2(br.Min.x + 10 + 6, cy), 12, fg);
    Text(br.Min.x + 10 + 12 + 6, cy, UI_B, 10, fg, lb);
    if (h.hover) CursorHand();
    if (h.click) A.editRoi = !A.editRoi;
    xr = br.Min.x - 8;
    snprintf(info, sizeof info, "40.0 Hz real-time sweep");
  } else if (A.mpage == 0 && d.kind == DevKind::Skeleton) {
    int slots[64]; const int n = People(d, slots);
    snprintf(info, sizeof info, "%d fps \xC2\xB7 %d %s", d.fps, n, n == 1 ? "person" : "people");
  } else if (IsLidar(d)) {
    snprintf(info, sizeof info, "%d Hz \xC2\xB7 %d %s", d.fps, (int)d.touches.size(), d.touches.size() == 1 ? "blob" : "blobs");
  }
  if (xr - lx > 40) TextEll(lx, cy, xr - lx, MONO_R, 10, K(pal::t88), info);
}

void SensorStageBody(ImRect area, Device& d) {
  if (A.mpage != 0 || d.kind != DevKind::Sim) A.editRoi = false;
  if (A.mpage != 0) SensorOutputView(area, d);
  else if (IsLidar(d)) LidarPlacementView(area, d);
  else if (d.kind == DevKind::Skeleton || d.kind == DevKind::Camera2D) SkeletonView(area, d);
  else Radar(area, d);
}


// ───────────────────────── Properties ─────────────────────────
static void PLabel(float x, float y, const char* t) { Text(x, y + 4.5f, MONO_R, 9, K(pal::t88), Upper(t).c_str(), 0.09f); }
// a 24 px row: label on the left, a number field on the right
static bool NumRow(float x, float y, float w, const char* label, const char* id, float& v, int decimals = 0) {
  Text(x, y + 12, MONO_R, 9, K(pal::t66), label);
  return FloatField(id, Rc(x + 104, y, w - 104, 24), v, decimals);
}
static bool CheckRow(float x, float y, const char* label, bool& v) {
  ImRect cb(x, y + 5, x + 14, y + 19);
  Hit h = HitR(ImRect(x - 2, y, x + 20 + TextW(UI_S, 10, label), y + 24));
  Box(cb, v ? K(pal::mint) : K(pal::g050), v ? K(pal::mint) : K(pal::g22), 2);
  if (v) Check(ImVec2((cb.Min.x + cb.Max.x) * 0.5f, (cb.Min.y + cb.Max.y) * 0.5f), 12, K(0x0f0f0f));
  Text(x + 20, y + 12, UI_S, 10, K(pal::tcc), label);
  if (h.hover) CursorHand();
  if (h.click) { v = !v; A.pushHist(); }
  return h.click;
}
static bool WideButton(float x, float y, float w, const char* label, const char* icon, uint32_t hex) {
  ImRect br(x, y, x + w, y + 26);
  Hit h = HitR(br);
  Box(br, h.hover ? K(hex, 0.25f) : K(hex, 0.14f), K(hex, 0.45f), 3);
  const float tw = TextW(UI_B, 10, label), cx = (br.Min.x + br.Max.x) * 0.5f, cy = (br.Min.y + br.Max.y) * 0.5f;
  if (icon) Icon(icon, ImVec2(cx - tw * 0.5f - 10, cy), 12, K(hex));
  Text(cx - tw * 0.5f + (icon ? 2 : 0), cy, UI_B, 10, K(hex), label);
  if (h.hover) CursorHand();
  return h.click;
}

// a collapsible section header; open state per device kind (bit = PropSection), kept while the app runs
static bool Section(float ox, float W, float x, float w, float oy, float& y, const char* title, DevKind k, PropSection s, const char* note = nullptr) {
  HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 3;
  uint32_t& bits = A.sensorSecOpen.try_emplace((int)k, 0xffffffffu).first->second;   // absent = every section open
  const uint32_t bit = 1u << (int)s;
  ImRect hr(x, oy + y, x + w, oy + y + 20);
  Hit hh = HitR(hr);
  if (hh.hover) { Fill(ImRect(hr.Min.x - 4, hr.Min.y, hr.Max.x, hr.Max.y), K(pal::g1c), 2); CursorHand(); }
  if (hh.click) bits ^= bit;
  const bool open = (bits & bit) != 0;
  Icon(open ? "chevron-down" : "chevron-right", ImVec2(x + 6, oy + y + 10), 11, K(hh.hover ? pal::white : pal::t88));
  Text(x + 18, oy + y + 10, MONO_R, 9, K(pal::t88), Upper(title).c_str(), 0.09f);
  if (note) TextR(x + w, oy + y + 10, MONO_R, 8, K(pal::t66), note);
  y += 20 + 4;
  return open;
}

static void CameraChoice(Device& d, float x, float w, float oy, float& y, const char* label,
                         const std::string& value, const std::vector<std::string>& choices, std::function<void(Device&, int)> apply, int enabled = 100) {
  Text(x, oy + y + 12, MONO_R, 9, K(pal::t66), label);
  ImRect r = Rc(x + 104, oy + y, w - 104, 24); Hit hit = HitR(r);
  Box(r, K(pal::g1c), K(pal::g22), 3); TextEll(r.Min.x + 6, r.Min.y + 12, r.GetWidth() - 20, MONO_R, 9, K(pal::tcc), value.c_str());
  Icon("chevron-down", ImVec2(r.Max.x - 8, r.Min.y + 12), 10, K(pal::t88));
  if (hit.click) {
    std::vector<MenuItem> items;
    for (int i = 0; i < (int)choices.size(); ++i) {
      MenuItem item; item.label = choices[i]; item.disabled = i >= enabled;
      item.run = [id = d.id, i, apply] { for (auto& device : A.devices) if (device.id == id) { apply(device, i); A.pushHist(); break; } };
      items.push_back(std::move(item));
    }
    A.openCtx(ImVec2(r.Min.x, r.Max.y + 3), items);
  }
  y += 28;
}
static void CameraSection(Device& d, float x, float w, float oy, float& y) {
  static std::vector<mikmap::CameraInfo> cameras; static std::string error; static bool listed = false;
  if (!listed) { cameras = mikmap::listCameras(error); listed = true; }
  std::vector<std::string> names{"Default camera"}, ids{""}; std::string name = d.camId.empty() ? names[0] : d.camId;
  for (const auto& c : cameras) { names.push_back(c.name); ids.push_back(c.id); if (d.camId == c.id) name = c.name; }
  CameraChoice(d,x,w,oy,y,"Camera",name,names,[ids](Device& v,int i){v.camId=ids[i];});
  if (WideButton(x,oy+y,w,"Refresh cameras","rotate-ccw",pal::mint)) { cameras = mikmap::listCameras(error); }
  y += 32;
  if (!error.empty()) { TextEll(x,oy+y+6,w,MONO_R,9,K(pal::yellow),error.c_str()); y += 18; }
  CameraChoice(d,x,w,oy,y,"Resolution",d.camW == 1280 ? "1280 x 720" : "640 x 480",{"640 x 480","1280 x 720"},[](Device& v,int i){v.camW=i?1280:640;v.camH=i?720:480;});
  CheckRow(x,oy+y,"Flip horizontally",d.camFlip); y += 28;
  CameraChoice(d,x,w,oy,y,"Mode",CameraModeName(d.camMode),{"Off","Body","Hand","Face","Full","Gesture (coming soon)"},[](Device& v,int i){SetCamMode(v,i);},5);
  if (!d.camMode) return;
  if (d.camMode == 1) CameraChoice(d,x,w,oy,y,"Model",d.camModel?"RTMO m":"RTMO s",{"RTMO s","RTMO m"},[](Device& v,int i){v.camModel=i;});
  else {
    const char* model = d.camMode == 2 ? "RTM Hand (21 points)" : d.camMode == 3 ? "RTM Face (106 points)" : "RTMW (133 points)";
    Text(x,oy+y+12,MONO_R,9,K(pal::t66),"Model"); TextEll(x+104,oy+y+12,w-104,MONO_R,9,K(pal::tcc),model); y += 28;
  }
  if (d.camMode == 4) {
    static const char* groups[] = {"Body", "Left hand", "Right hand", "Face", "Feet"};
    for (int i=0;i<5;++i) {
      bool on = (d.camGroups & (1u << i)) != 0;
      if (CheckRow(x,oy+y,groups[i],on)) d.camGroups = uint8_t(on ? d.camGroups | (1u << i) : d.camGroups & ~(1u << i));
      Dot(ImVec2(x+w-8,oy+y+12),5,GroupColor(uint8_t(i)),false); y += 24;
    }
  }
  const std::vector<std::string> devices{"Auto (CUDA / CPU)","CUDA only","CPU"};
  CameraChoice(d,x,w,oy,y,"Compute",devices[d.camDevice],devices,[](Device& v,int i){v.camDevice=i;});
  const int cap = mikmap::modeSpec(static_cast<mikmap::CamMode>(d.camMode)).objectsCap;
  float objects = float(std::min(d.maxPeople, cap));
  if (NumRow(x,oy+y,w-55,"Max objects","##campeople",objects)) d.maxPeople=std::clamp(int(objects),1,cap);
  TextR(x+w,oy+y+12,MONO_R,9,K(pal::t66),("cap " + std::to_string(cap)).c_str()); y += 28;
  if (NumRow(x,oy+y,w,"Person threshold","##camperson",d.personThr,2)) d.personThr=std::clamp(d.personThr,0.f,1.f); y += 28;
  if (NumRow(x,oy+y,w,"Joint threshold","##camjoint",d.jointThr,2)) d.jointThr=std::clamp(d.jointThr,0.f,1.f); y += 28;
  const std::vector<std::string> smooth{"Off","Light","Medium"};
  CameraChoice(d,x,w,oy,y,"Smoothing",smooth[d.smoothing],smooth,[](Device& v,int i){v.smoothing=i;});
}

static void ConnectionSection(Device& d, float x, float w, float oy, float& y) {
  if (d.kind == DevKind::Camera2D) CameraSection(d, x, w, oy, y);
  const std::string missing = d.kind == DevKind::Camera2D ? MissingCameraPack(d) : d.needsPack;
  if (!PackAvailableForDevice(d)) {
    const PackState pst = PackStateFor(missing);
    const char* hint = SensorPackHint(d, pst);
    PackRow pr{missing, missing, "1.0.0", "", "", 0, pst, false, false, false, 0.f};
    for (const auto& r : PackRows()) {
      if (r.id == missing) { pr = r; break; }
    }
    ImRect br(x, oy + y, x + w, oy + y + 54);
    Box(br, K(pal::coral, 0.12f), K(pal::coral, 0.65f), 3);
    Dot(ImVec2(br.Min.x + 10, br.Min.y + 13), 6, pal::coral, false);
    TextEll(br.Min.x + 19, br.Min.y + 13, w - 25, UI_B, 9.5f, K(pal::coral),
            hint ? hint : ("Needs pack: " + d.needsPack).c_str());

    const char* actLab = PackActionLabel(pr);
    const bool actEn = PackActionEnabled(pr);
    const char* dispLab = (actLab && actLab[0]) ? actLab : "Install";
    const float halfW = (w - 18.f) * 0.5f;
    ImRect actB(br.Min.x + 6, br.Min.y + 26, br.Min.x + 6 + halfW, br.Max.y - 5);
    ImRect setB(actB.Max.x + 6, br.Min.y + 26, br.Max.x - 6, br.Max.y - 5);

    Hit ah = HitR(actB);
    Box(actB, !actEn ? K(pal::g16) : (ah.hover ? K(pal::coral, 0.28f) : K(pal::coral, 0.18f)),
        !actEn ? K(pal::g22) : K(pal::coral, 0.75f), 3);
    TextC((actB.Min.x + actB.Max.x) * 0.5f, (actB.Min.y + actB.Max.y) * 0.5f, UI_B, 9,
          K(actEn ? pal::coral : pal::t66), Upper(dispLab).c_str(), 0.08f);
    if (ah.hover && actEn) CursorHand();
    if (ah.click && actEn) {
      if (pst == PackState::Downloading) PackCancel();
      else PackInstall(missing);
    }

    Hit sh = HitR(setB);
    Box(setB, sh.hover ? K(pal::g22) : K(pal::g1c), K(pal::g2a), 3);
    TextC((setB.Min.x + setB.Max.x) * 0.5f, (setB.Min.y + setB.Max.y) * 0.5f, UI_B, 9,
          K(sh.hover ? pal::white : pal::tcc), "SENSOR PACKS", 0.08f);
    if (sh.hover) CursorHand();
    if (sh.click) { A.settingsOpen = true; A.setTab = 5; }

    y += 54 + 8;
  }
  if (d.kind == DevKind::Camera2D) return;
  if (d.kind == DevKind::Sim) { PropertyRow(Rc(x - 6, oy + y, w + 12, 24), "Source", "simulated"); y += 24 + 6; return; }
  if (d.kind == DevKind::Hokuyo) {   // applied when the field loses focus so typing does not restart the source on every key
    static std::string hostEdit;
    if (ImGui::GetActiveID() != ImGui::GetID("##devhost")) hostEdit = d.host;
    Text(x, oy + y + 12, MONO_R, 9, K(pal::t66), "Host");
    TextField("##devhost", Rc(x + 104, oy + y, w - 104, 24), hostEdit, MONO_R, 10);
    if (ImGui::IsItemDeactivatedAfterEdit()) { d.host = hostEdit; A.pushHist(); }
    y += 24 + 4;
  }
  if (d.kind == DevKind::Plugin) {
    PropertyRow(Rc(x - 6, oy + y, w + 12, 24), "Plugin", d.plugin.empty() ? "\xE2\x80\x94" : d.plugin.c_str()); y += 24 + 4;
    // shortcut: config is raw JSON; with a second plugin, add mikmap_sensor_params() to describe each field, like TouchDesigner's setupParameters
    static std::string cfgEdit;
    if (ImGui::GetActiveID() != ImGui::GetID("##devconfig")) cfgEdit = d.config;
    Text(x, oy + y + 12, MONO_R, 9, K(pal::t66), "Config");
    TextField("##devconfig", Rc(x + 104, oy + y, w - 104, 24), cfgEdit, MONO_R, 10);
    if (ImGui::IsItemDeactivatedAfterEdit()) { d.config = cfgEdit; A.pushHist(); }
    y += 24 + 4;
  } else {
    static int portEdit = 0;
    if (ImGui::GetActiveID() != ImGui::GetID("##devport")) portEdit = d.port;
    Text(x, oy + y + 12, MONO_R, 9, K(pal::t66), d.kind == DevKind::Skeleton ? "OSC UDP port" : "Port");
    IntField("##devport", Rc(x + 104, oy + y, w - 104, 24), portEdit);
    if (ImGui::IsItemDeactivatedAfterEdit()) { d.port = std::clamp(portEdit, 0, 65535); A.pushHist(); }
    y += 24 + 4;
  }
  if (d.kind == DevKind::Skeleton) {   // how to start the sender that feeds this port
    std::string cmd = "python tools/pose_bridge/pose_bridge.py --port " + std::to_string(d.port);
    TextEll(x, oy + y + 6, w, MONO_R, 9, K(pal::t88), cmd.c_str()); y += 12 + 4;
  }
  PropertyRow(Rc(x - 6, oy + y, w + 12, 24), "Packets", std::to_string(d.packets).c_str()); y += 24 + 6;
  if (WideButton(x, oy + y, w, "Reconnect", "rotate-ccw", pal::mint)) { RestartSource(d.id); A.notify("Reconnecting " + d.name); }
  y += 26 + 8;
  if (d.kind == DevKind::Hokuyo) {   // look for more lidars on the network: the add wizard at its scan step, on this sensor's screen
    if (WideButton(x, oy + y, w, "Scan again", "radar", pal::mint)) { OpenSensorWizard(DeviceScreen(d)->id); A.wizard.kind = DevKind::Hokuyo; SensorWizardNext(); }
    y += 26 + 8;
  }
}

static void ScreenSizeSection(Device& d, float x, float w, float oy, float& y) {
  Screen* sc = DeviceScreen(d);
  if (!sc) return;
  NumRow(x, oy + y, w, "Real width (mm)", "##prealw", sc->realW); y += 24 + 4;
  NumRow(x, oy + y, w, "Real height (mm)", "##prealh", sc->realH); y += 24 + 4;
  sc->realW = std::clamp(sc->realW, 0.f, 100000.f); sc->realH = std::clamp(sc->realH, 0.f, 100000.f);
  if (sc->realW <= 0 || sc->realH <= 0) { TextEll(x, oy + y + 6, w, MONO_R, 9, K(pal::yellow), "Enter both to place the lidar"); y += 12 + 4; }
  y += 4;
}

static void PlacementSection(Device& d, float x, float w, float oy, float& y) {
  const Screen* sc = DeviceScreen(d);
  const float realH = sc ? sc->realH : 0.f;   // Origin Bottom-left: the Y field counts up from the bottom edge (needs a sized screen); d.pose itself stays y-down
  float px = (float)d.pose.x, py = PoseYToUi((float)d.pose.y, realH, d.originBottom), pa = (float)d.pose.angleDeg;
  const bool smallScreen = sc && std::max(sc->realW, sc->realH) < 2000.f;   // a small screen: sub-mm positions matter
  auto typed = [&](bool isX) {   // a typed pose places a lidar nobody aligned yet; only the edited axis is written (the other keeps its exact value), no screen-relative limit
    if (isX) d.pose.x = SanePoseMm(px, (float)d.pose.x); else d.pose.y = SanePoseMm(UiToPoseY(py, realH, d.originBottom), (float)d.pose.y);
    CommitPose(d);
  };
  if (NumRow(x, oy + y, w, "X (mm)", "##posex", px, smallScreen ? 1 : 0)) typed(true);
  y += 24 + 4;
  if (NumRow(x, oy + y, w, d.originBottom && realH > 0 ? "Y (mm, from bottom)" : "Y (mm)", "##posey", py, smallScreen ? 1 : 0)) typed(false);
  y += 24 + 4;
  if (NumRow(x, oy + y, w, "Angle (\xC2\xB0)", "##posea", pa, 1)) { d.pose.angleDeg = std::clamp(pa, -360.f, 360.f); CommitPose(d); }
  y += 24 + 4;
  if (CheckRow(x, oy + y, "Mirror (mounted upside down)", d.pose.mirror)) CommitPose(d);
  y += 24 + 4;
  if (NumRow(x, oy + y, w, "Range limit (mm)", "##range", d.rangeMm)) d.rangeMm = std::clamp(d.rangeMm, 0.f, 100000.f);
  y += 24 + 2;
  TextEll(x, oy + y + 5, w, MONO_R, 8, K(pal::t66), "0 = unlimited"); y += 10 + 6;
  // Count only: Screen keeps the ROI on the screen's 4 corners (ApplyRoiAuto, every tick); Zone (custom shapes) arrives with sensor masks
  Text(x, oy + y + 12, MONO_R, 9, K(pal::t66), "Count only");
  const float sx = x + 104, sw = (w - 104) * 0.5f;
  ImRect scr(sx, oy + y, sx + sw, oy + y + 24), zon(sx + sw, oy + y, x + w, oy + y + 24);
  Hit sh = HitR(scr);
  Box(scr, d.roiAuto ? K(pal::cyan, 0.25f) : sh.hover ? K(pal::g1c, 1.f) : K(pal::g050), K(pal::cyan, d.roiAuto ? 0.7f : 0.3f), 3);
  TextC((scr.Min.x + scr.Max.x) * 0.5f, (scr.Min.y + scr.Max.y) * 0.5f, UI_B, 9, K(pal::cyan, d.roiAuto ? 1.f : 0.7f), "Screen");
  Box(zon, K(pal::g050), K(pal::g22), 3);   // not clickable yet
  TextC((zon.Min.x + zon.Max.x) * 0.5f - 8, (zon.Min.y + zon.Max.y) * 0.5f, UI_B, 9, K(pal::t66, 0.6f), "Zone");
  TextR(zon.Max.x - 5, (zon.Min.y + zon.Max.y) * 0.5f, MONO_R, 7, K(pal::t66, 0.6f), "soon");
  if (sh.hover) CursorHand();
  if (sh.click && !d.roiAuto) {
    d.roiAuto = true;
    if (!ApplyRoiAuto(d)) A.notify("Enter the screen's real size first");
    A.pushHist();
  }
  y += 24 + 4;
  const char* hint = d.roiAuto ? RoiHint(d, DeviceScreen(d)) : "Using the ROI saved in this project";
  if (hint) { TextEll(x, oy + y + 5, w, MONO_R, 8, K(pal::t66), hint); y += 10 + 4; }
  y += 6;
}

const char* RoiHint(const Device& d, const Screen* sc) {
  if (!d.roiAuto || (d.kind != DevKind::Hokuyo && d.kind != DevKind::Plugin)) return nullptr;
  if (!sc || sc->realW <= 0 || sc->realH <= 0) return "Enter the screen's real size to use it as the ROI";
  if (!d.placed && !d.calibrated) return "The ROI follows the screen from the sensor's pose \xE2\x80\x94 drag the sensor to where it hangs";
  return nullptr;
}

static void BlobSection(Device& d, float x, float w, float oy, float& y) {
  CheckRow(x, oy + y, "Blob tracking", d.blobOn); y += 24 + 4;
  const float prev = g.alpha; if (!d.blobOn) g.alpha *= 0.5f;
  if (NumRow(x, oy + y, w, "Min (mm)", "##blobmin", d.blobMinMm)) d.blobMinMm = std::clamp(d.blobMinMm, 0.f, 10000.f);
  y += 24 + 4;
  if (NumRow(x, oy + y, w, "Max (mm)", "##blobmax", d.blobMaxMm)) d.blobMaxMm = std::clamp(d.blobMaxMm, 0.f, 10000.f);
  y += 24 + 4;
  if (NumRow(x, oy + y, w, "Gap (mm)", "##blobgap", d.blobGapMm)) d.blobGapMm = std::clamp(d.blobGapMm, 0.f, 10000.f);
  y += 24 + 4;
  g.alpha = prev;
  PropertyRow(Rc(x - 6, oy + y, w + 12, 24), "Blobs", std::to_string(d.touches.size()).c_str(), nullptr, pal::mint); y += 24 + 8;
}

static void CalibrationSection(Device& d, float x, float w, float oy, float& y) {
  if (IsLidar(d)) {   // the badge says whether the quad follows the placement or is held by hand; Re-apply sits next to it
    const char* bl = d.placed ? "Placed" : "Custom (4-point)";
    Badge(x + BadgeW(bl, true), oy + y + 12, bl, d.placed ? T_AUDIO : T_STANDBY, true);
    const char* rl = "Re-apply placement";
    const float rw = TextW(UI_B, 9, rl) + 16;
    ImRect rb(x + w - rw, oy + y, x + w, oy + y + 24);
    Hit rh = HitR(rb);
    Box(rb, rh.hover ? K(pal::mint, 0.25f) : K(pal::g1c), K(pal::mint, 0.45f), 3);
    TextC((rb.Min.x + rb.Max.x) * 0.5f, (rb.Min.y + rb.Max.y) * 0.5f, UI_B, 9, K(pal::mint), rl);
    if (rh.hover) CursorHand();
    if (rh.click) {
      const bool was = d.placed; d.placed = true;
      if (ApplyPlacement(d)) A.pushHist(); else { d.placed = was; A.notify("Enter the screen's real size first"); }
    }
    y += 24 + 8;
  }
  static const char* CN[4] = {"1 \xC2\xB7 TL", "2 \xC2\xB7 TR", "3 \xC2\xB7 BR", "4 \xC2\xB7 BL"};
  const float fw = (w - 52 - 6) * 0.5f;
  for (int i = 0; i < 4 && i < (int)d.calib.size(); ++i) {   // the targets in output px; typing one is a hand edit (Custom)
    Text(x, oy + y + 12, MONO_R, 9, K(pal::t66), CN[i]);
    char ix[16], iy[16]; snprintf(ix, sizeof ix, "##ctx%d", i); snprintf(iy, sizeof iy, "##cty%d", i);
    if (FloatField(ix, Rc(x + 52, oy + y, fw, 24), d.calib[i].tx, 0)) d.placed = false;
    if (FloatField(iy, Rc(x + 52 + fw + 6, oy + y, fw, 24), d.calib[i].ty, 0)) d.placed = false;
    y += 24 + 4;
  }
  float m[9], rms; if (!FitHomography(d.calib, m, &rms)) FitAffine(d.calib, m, &rms);
  char rb[32]; if (d.calibrated) snprintf(rb, sizeof rb, "%.2f px", rms); else snprintf(rb, sizeof rb, "not calibrated");
  PropertyRow(Rc(x - 6, oy + y, w + 12, 24), "RMS", rb, nullptr, d.calibrated ? pal::mint : pal::coral); y += 24 + 6;
  // the 4-point wizard: touch / stand on the physical crosshair, Capture (a live sensor) or click the target (Sim)
  if (A.wizardStep == 0) {
    if (WideButton(x, oy + y, w, "Start 4-point wizard", "crosshair", pal::coral)) A.wizardStep = 1;
    y += 26 + 6;
  } else {
    ImRect br(x, oy + y, x + w, oy + y + 6 + 13 + 4 + 15 + 4 + 24 + 6);
    Box(br, K(pal::g18), K(pal::yellow, 0.5f), 3);
    char st[24]; snprintf(st, sizeof st, "STEP %d OF 4", A.wizardStep);
    static const char* WN[4] = {"TOP-LEFT", "TOP-RIGHT", "BOTTOM-RIGHT", "BOTTOM-LEFT"};
    Text(br.Min.x + 7, br.Min.y + 6 + 6.5f, MONO_B, 11, K(pal::yellow), st);
    TextR(br.Max.x - 7, br.Min.y + 6 + 6.5f, MONO_R, 10, K(pal::t88), WN[A.wizardStep - 1]);
    const bool live = d.kind != DevKind::Sim;   // a real sensor reports where the target stands: Capture averages 30 frames of it
    TextEll(br.Min.x + 7, br.Min.y + 6 + 13 + 4 + 7.5f, w - 14, UI_S, 10, K(pal::te0), live ? (d.kind == DevKind::Camera2D ? (CameraCaptureUnavailable(d) ? CameraCaptureUnavailable(d) : CalibHintForMode(d.camMode)) : d.kind == DevKind::Skeleton ? CalibHintForMode(1) : "Stand alone on the target, then Capture") : "Click the target on the Input radar");
    ImRect cr(br.Min.x + 7, br.Max.y - 6 - 24, br.Max.x - 7, br.Max.y - 6);
    if (live) {
      const float half = (cr.GetWidth() - 6) * 0.5f;
      ImRect cap(cr.Min.x, cr.Min.y, cr.Min.x + half, cr.Max.y);
      cr.Min.x += half + 6;
      Hit ch = HitR(cap);
      Box(cap, K(pal::coral, 0.18f), K(pal::coral, 0.5f), 3);
      TextC((cap.Min.x + cap.Max.x) * 0.5f, (cap.Min.y + cap.Max.y) * 0.5f, UI_B, 10, K(CameraCaptureUnavailable(d) ? pal::t66 : pal::coral), "Capture");
      if (ch.hover && !CameraCaptureUnavailable(d)) CursorHand();
      if (ch.click && !CameraCaptureUnavailable(d)) { d.placed = false; StartCapture(A.wizardStep); }   // captured points are a hand calibration, not the placement
    }
    Hit h = HitR(cr);
    Box(cr, K(pal::g050), K(pal::g22), 3);
    TextC((cr.Min.x + cr.Max.x) * 0.5f, (cr.Min.y + cr.Max.y) * 0.5f, UI_S, 10, K(pal::t88), "Cancel");
    if (h.hover) CursorHand();
    if (h.click) A.wizardStep = 0;
    y += br.GetHeight() + 6;
  }
  // G8: save/load the 4 calibration points + ROI/noise as their own file, independent of the project
  const float gap = 6, bw = (w - gap) * 0.5f;
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
        if (LoadCalibProfile(path, *cur, A.noise, A.blobSize, err)) { cur->placed = false; A.wizardStep = 0; A.notify("Loaded profile: " + name); }
        else A.notify("Load failed: " + err);
      };
      mi.push_back(it);
    }
    A.openCtx(ImVec2(lb.Min.x, lb.Max.y + 4), mi);
  }
  y += 26 + 8;
}

// Mapped touches go out from their own thread (sources.cpp) as 0..1 of the screen; the counters are the one shared sender's.
static void SendOutSection(Device& d, float x, float w, float oy, float& y) {
  CheckRow(x, oy + y, "Send touches out", d.outOn); y += 24 + 4;
  const float prev = g.alpha; if (!d.outOn) g.alpha *= 0.5f;
  auto segmented = [&](const char* label, const char* const opt[2], int sel) {   // a two-button choice; returns the index clicked, or -1
    Text(x, oy + y + 12, MONO_R, 9, K(pal::t66), label);
    ImRect grp(x + 104, oy + y, x + w, oy + y + 24);
    Box(grp, K(0x000000), K(pal::g2a), 3);
    const float bw = (grp.GetWidth() - 6 - 2) * 0.5f;
    int clicked = -1;
    for (int i = 0; i < 2; ++i) {
      const float bx = grp.Min.x + 3 + i * (bw + 2);
      ImRect br(bx, grp.Min.y + 3, bx + bw, grp.Max.y - 3);
      Hit h = HitR(br);
      const bool on = sel == i;
      if (on) Fill(br, K(pal::coral), 2); else if (h.hover) Fill(br, K(pal::g1c), 2);
      TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, UI_B, 9, K(on ? pal::white : pal::t88), opt[i]);
      if (h.hover) CursorHand();
      if (h.click && !on) clicked = i;
    }
    y += 24 + 4;
    return clicked;
  };
  static const char* const fmt[2] = {"OSC", "UDP text"}, * const org[2] = {"Bottom-left", "Top-left"};
  if (int c = segmented("Format", fmt, d.outProto); c >= 0) { d.outProto = c; A.pushHist(); }
  if (int c = segmented("Origin", org, d.originBottom ? 0 : 1); c >= 0) { d.originBottom = c == 0; A.pushHist(); }
  {   // applied when the field loses focus, like Connection's Host / Port
    static std::string hostEdit; static int portEdit = 0;
    if (ImGui::GetActiveID() != ImGui::GetID("##outhost")) hostEdit = d.outHost;
    Text(x, oy + y + 12, MONO_R, 9, K(pal::t66), "Host");
    TextField("##outhost", Rc(x + 104, oy + y, w - 104, 24), hostEdit, MONO_R, 10);
    if (ImGui::IsItemDeactivatedAfterEdit()) { d.outHost = hostEdit; A.pushHist(); }
    y += 24 + 4;
    if (ImGui::GetActiveID() != ImGui::GetID("##outport")) portEdit = d.outPort;
    Text(x, oy + y + 12, MONO_R, 9, K(pal::t66), "Port");
    IntField("##outport", Rc(x + 104, oy + y, w - 104, 24), portEdit);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
      if (portEdit >= 1 && portEdit <= 65535) { d.outPort = portEdit; A.pushHist(); } else A.notify("Port must be 1-65535");
    }
    y += 24 + 4;
  }
  uint64_t sent = 0, dropped = 0; std::string err; SendOutStats(sent, dropped, err);
  char sb[64]; snprintf(sb, sizeof sb, "%llu sent \xC2\xB7 %llu dropped", (unsigned long long)sent, (unsigned long long)dropped);
  PropertyRow(Rc(x - 6, oy + y, w + 12, 24), "Packets", sb); y += 24 + 2;
  if (!err.empty()) { TextEll(x, oy + y + 6, w, MONO_R, 9, K(pal::red), err.c_str()); y += 12 + 4; }
  const std::string hint = std::string(d.outProto == 1 ? "\"<name> <id> <x> <y>\" \xC2\xB7 x, y 0..1" : "/mikmap/<name>/touch id x y \xC2\xB7 x, y 0..1") + (d.originBottom ? " \xC2\xB7 y=0 bottom" : " \xC2\xB7 y=0 top");
  TextEll(x, oy + y + 5, w, MONO_R, 8, K(pal::t66), hint.c_str()); y += 10 + 8;
  g.alpha = prev;
}

static void TriggersSection(float x, float w, float oy, float& y) {   // G17 patch cords (the old Parameter routing panel)
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
  if (WideButton(x, oy + y, w, "Simulate particle trigger", "zap", pal::coral)) {
    for (auto& d : A.devices) if (d.kind == DevKind::Sim) {   // normalised units: only the simulated radar understands them
      d.touches.assign(1, Touch{100 + rand() % 900, ((rand() % 1000) / 1000.f - 0.5f) * 1.4f, ((rand() % 1000) / 1000.f - 0.5f) * 1.4f});
      break;
    }
  }
  y += 26 + 8;
}

// Like Mapping's Properties: a block that is always there (name, status, Active, screen), then the sections of the open tab.
// Input = what the sensor sees and how it is filtered; Output = where its points land on the screen and where they go next.
void SensorProps(ImRect r, Device* d) {
  Fill(r, K(pal::g12));
  VLine(r.Min.x, r.Min.y, r.Max.y, K(pal::g2a));
  PanelHeader(Rc(r.Min.x + 1, r.Min.y, r.GetWidth() - 1, 24), "Sensor properties", pal::t88);
  if (!d) return;
  Badge(r.Max.x - 6, r.Min.y + 11.5f, DevKindName(d->kind), T_AUDIO, true);
  static ScrollArea sa;
  sa.Begin("##sensorprops", ImRect(r.Min.x + 1, r.Min.y + 24, r.Max.x, r.Max.y));
  const float ox = sa.origin.x, oy = sa.origin.y, W = r.GetWidth() - 1 - 8, x = ox + 8, w = W - 16;
  float y = 8;
  PLabel(x, oy + y, "Sensor name"); y += 9 + 4;
  if (TextField("##sensorname", Rc(x, oy + y, w, 28), d->name)) A.pushHist();
  y += 28 + 8;
  {   // status · fps · latency, and the Active switch
    const int st = d->status;
    const uint32_t dot = !d->enabled ? 0x555555 : st == (int)mikmap::SourceStatus::Error ? pal::red : st == (int)mikmap::SourceStatus::Connecting ? pal::yellow : pal::mint;
    Dot(ImVec2(x + 4, oy + y + 12), 8, dot, d->enabled && st == (int)mikmap::SourceStatus::Running);
    char sl[64];
    if (!d->enabled) snprintf(sl, sizeof sl, "Inactive");
    else if (d->kind == DevKind::Sim) snprintf(sl, sizeof sl, "Simulated");
    else if (st == (int)mikmap::SourceStatus::Running) snprintf(sl, sizeof sl, "%d fps \xC2\xB7 %.0f ms", d->fps, d->latencyMs);
    else snprintf(sl, sizeof sl, st == (int)mikmap::SourceStatus::Connecting ? "Connecting\xE2\x80\xA6" : st == (int)mikmap::SourceStatus::Error ? "Error" : "Stopped");
    Text(x + 14, oy + y + 12, MONO_B, 10, K(d->enabled ? pal::tcc : pal::t66), sl);
    const float aw = 20 + TextW(UI_S, 10, "Active");
    CheckRow(x + w - aw, oy + y, "Active", d->enabled);
    y += 24 + 2;
    if (d->enabled && !d->error.empty()) { TextEll(x, oy + y + 6, w, MONO_R, 9, K(pal::red), d->error.c_str()); y += 12 + 4; }
    if (d->kind == DevKind::Camera2D) {
      const auto stc = DeviceCameraStats(*d); char line[128];
      snprintf(line,sizeof line,"Camera %.1f fps / %s %.1f fps",stc.camFps,CameraModeName(d->camMode),stc.inferFps);
      TextEll(x,oy+y+6,w,MONO_R,9,K(pal::tcc),line); y += 18;
      snprintf(line,sizeof line,"Infer %.1f ms / latency %.1f ms / %d objects",stc.inferMs,stc.latencyMs,stc.people);
      TextEll(x,oy+y+6,w,MONO_R,9,K(pal::tcc),line); y += 18;
      if (st == (int)mikmap::SourceStatus::Running && d->camMode) {
        const char* compute = !d->camReplay.empty() ? "Replay fixture (no inference)" : stc.onCpu ? "CPU - inference may be slow" : "CUDA";
        TextEll(x,oy+y+6,w,MONO_R,9,K(stc.onCpu ? pal::yellow : pal::t88),compute); y += 18;
      }
    }
    y += 4;
  }
  {   // the screen is changed from the tree's right-click menu (Move to screen); here only the fallback warning
    bool fb = false; const Screen* cur = DeviceScreen(*d, &fb);
    if (fb && cur) { TextEll(x, oy + y + 6, w, MONO_R, 9, K(pal::yellow), ("Screen not found \xE2\x80\x94 using " + cur->name).c_str()); y += 12 + 4; }
  }
  for (PropSection s : SensorPropSections(d->kind, A.mpage)) {
    switch (s) {
      case PropSection::Connection: if (Section(ox, W, x, w, oy, y, "Connection", d->kind, s, d->kind == DevKind::Sim ? nullptr : Endpoint(*d).c_str())) ConnectionSection(*d, x, w, oy, y); break;
      case PropSection::ScreenSize: if (Section(ox, W, x, w, oy, y, "Screen size", d->kind, s)) ScreenSizeSection(*d, x, w, oy, y); break;
      case PropSection::Placement: if (Section(ox, W, x, w, oy, y, "Placement", d->kind, s)) PlacementSection(*d, x, w, oy, y); break;
      case PropSection::Blobs: if (Section(ox, W, x, w, oy, y, "Blob tracking", d->kind, s)) BlobSection(*d, x, w, oy, y); break;
      case PropSection::Calibration: if (Section(ox, W, x, w, oy, y, "Calibration", d->kind, s)) CalibrationSection(*d, x, w, oy, y); break;
      case PropSection::SendOut: if (Section(ox, W, x, w, oy, y, "Send out", d->kind, s, !d->outOn ? "off" : d->outProto == 1 ? "UDP text" : "OSC")) SendOutSection(*d, x, w, oy, y); break;
      case PropSection::Triggers: {
        int act = 0; for (auto& rt : A.routes) act += rt.active;
        char nb[24]; snprintf(nb, sizeof nb, "%d active", act);
        if (Section(ox, W, x, w, oy, y, "Triggers", d->kind, s, nb)) TriggersSection(x, w, oy, y);
        break;
      }
    }
  }
  sa.End(W, y + 8);
}

// ───────────────────────── Add-sensor wizard ─────────────────────────
// The only way to add a device: 1 pick a type · 2 connect (Hokuyo: scan the subnet) · 3 watch the new devices come up.
constexpr uint16_t kHokuyoPort = 10940;
static std::vector<mikmap::Ipv4Iface> gIfs; static bool gIfsFresh = false;   // network cards, read once per visit to step 2

static std::string PluginFile(const mikmap::SensorPlugin& p) {
  const size_t k = p.path.find_last_of("/\\");
  return k == std::string::npos ? p.path : p.path.substr(k + 1);
}
static std::string NewDeviceId() {
  for (int n = 1;; ++n) {
    const std::string id = "dev" + std::to_string(n);
    bool taken = false; for (auto& e : A.devices) taken |= e.id == id;
    if (!taken) return id;
  }
}
static int FreeOscPort() {   // the lowest port >= 9000 no device uses yet
  for (int p = 9000; p < 65536; ++p) {
    bool used = false; for (auto& e : A.devices) used |= e.port == p;
    if (!used) return p;
  }
  return 9000;
}
static std::string Numbered(const std::string& base) {   // "Simulated radar", "Simulated radar 2", ...
  int same = 0; for (auto& e : A.devices) same += e.name == base || e.name.rfind(base + " ", 0) == 0;
  return same ? base + " " + std::to_string(same + 1) : base;
}

// "ip" or "ip:port" -> host + port (10940 when absent; a bad port is kept: the device reports "Port must be 1-65535")
static void SplitHost(const std::string& e, std::string& host, int& port) {
  const size_t c = e.rfind(':');
  host = e.substr(0, c); port = c == std::string::npos ? kHokuyoPort : std::atoi(e.c_str() + c + 1);
}
// a Hokuyo device at this address already exists: a UST serves one client, so a second device would fight it for the link
bool WizardHostAdded(const std::string& e) {
  std::string host; int port = 0; SplitHost(e, host, port);
  for (auto& d : A.devices) if (d.kind == DevKind::Hokuyo && d.host == host && d.port == port) return true;
  return false;
}
// one list entry: an already-added address is listed (so the user sees why) but never ticked
static void WizardOffer(const std::string& h) {
  SensorWizard& w = A.wizard;
  w.pick.resize(w.found.size(), 0);
  const char tick = WizardHostAdded(h) ? 0 : 1;
  for (size_t i = 0; i < w.found.size(); ++i) if (w.found[i] == h) { w.pick[i] = tick; return; }
  w.found.push_back(h); w.pick.push_back(tick);
}

void SensorWizardAddManual() {
  SensorWizard& w = A.wizard;
  std::string h = w.manualHost;
  h.erase(0, h.find_first_not_of(" \t")); h.erase(h.find_last_not_of(" \t") + 1);
  if (h.empty()) return;
  if (WizardHostAdded(h)) A.notify(h + " is already added");
  WizardOffer(h); w.manualHost.clear();
}

void SensorWizardMergeScan(const std::vector<std::string>& res) { for (auto& h : res) WizardOffer(h); }

void SensorWizardNext() {
  SensorWizard& w = A.wizard;
  if (w.step == 1 && w.kind != DevKind::Sim && w.kind != DevKind::Camera2D) {
    w.step = 2; w.found.clear(); w.pick.clear(); w.added.clear(); w.scanning = false;
    if (w.kind == DevKind::Skeleton) w.port = FreeOscPort();
    return;
  }
  if (w.step != 1 && w.step != 2) return;
  std::string sid = w.screenId;
  if (sid.empty() && !A.screens.empty()) sid = A.screens[0].id;
  auto add = [&](DevKind k, const std::string& name) -> Device& {
    Device nd = MakeDevice(k, NewDeviceId());
    nd.name = name; nd.enabled = true; nd.screenId = sid;
    A.devices.push_back(nd); w.added.push_back(nd.id);
    return A.devices.back();
  };
  w.added.clear();
  if (w.kind == DevKind::Hokuyo) {
    w.pick.resize(w.found.size(), 0);
    for (size_t i = 0; i < w.found.size(); ++i) {
      if (!w.pick[i] || WizardHostAdded(w.found[i])) continue;   // also catches "ip" and "ip:10940" both ticked: the first one wins
      const std::string& e = w.found[i];
      std::string host; int port = 0; SplitHost(e, host, port);
      Device& d = add(DevKind::Hokuyo, "Hokuyo " + e);
      d.host = host; d.port = port;
    }
    if (w.added.empty()) { A.notify("Tick at least one lidar, or type its IP and Add"); return; }
  } else if (w.kind == DevKind::Camera2D) {
    add(DevKind::Camera2D, Numbered("Camera 2D"));
  } else if (w.kind == DevKind::Skeleton) {
    add(DevKind::Skeleton, "Camera :" + std::to_string(w.port)).port = w.port;
  } else if (w.kind == DevKind::Plugin) {
    std::string name = w.plugin;
    for (auto& p : SensorPlugins()) if (PluginFile(p) == w.plugin && !p.name.empty()) name = p.name;
    Device& d = add(DevKind::Plugin, Numbered(name));
    d.plugin = w.plugin; d.config = w.config; d.needsPack = w.needsPack;
  } else {
    add(DevKind::Sim, Numbered("Simulated radar"));
  }
  A.pushHist();
  w.step = 3;
}

void SensorWizardDone() {
  SensorWizard& w = A.wizard;
  w.step = 0; A.mpage = 0;   // intended: the new sensor opens on INPUT (A.mpage is shared with slices)
  if (!w.added.empty()) SelectDevice(w.added[0]);
}

void CaptureOverlayState() {
  A.overlayAtStart = A.pop.open || A.blendDD.open || A.ctx.open || A.layerMenu.open || A.colMenu.open || A.deckMenu.open || A.settingsOpen || A.rename.open || A.openDialog || A.helpOpen;
}
bool WizardEscCloses(bool wantTextInput) { return A.wizard.step > 0 && !A.overlayAtStart && !wantTextInput; }

void CloseSensorWizard() { A.wizard.step = 0; }   // a running IP scan is left to finish and be joined later, as before; devices made stay

// one choice on step 1; a dimmed card only says it is planned
static bool WizCard(ImRect r, const char* icon, const char* title, const char* sub, bool dim) {
  Hit h = HitR(r);
  const float prev = g.alpha; if (dim) g.alpha *= 0.45f;
  Box(r, h.hover && !dim ? K(pal::mint, 0.10f) : K(pal::g14), h.hover && !dim ? K(pal::mint, 0.6f) : K(pal::g2a), 4);
  const float cy = (r.Min.y + r.Max.y) * 0.5f;
  Icon(icon, ImVec2(r.Min.x + 26, cy), 20, K(dim ? pal::t66 : pal::mint));
  TextEll(r.Min.x + 50, cy - 8, r.GetWidth() - 60, UI_B, 11, K(pal::white), title);
  TextEll(r.Min.x + 50, cy + 9, r.GetWidth() - 60, MONO_R, 9, K(pal::t88), sub);
  g.alpha = prev;
  if (h.hover) CursorHand();
  return h.click;
}

static void TickRow(ImRect r, const char* label, char& on, bool taken) {   // taken: dimmed "already added", cannot be ticked
  Hit h = HitR(r);
  if (h.hover && !taken) Fill(r, K(pal::g1c), 2);
  const float prev = g.alpha; if (taken) { g.alpha *= 0.45f; on = 0; }
  const float cy = (r.Min.y + r.Max.y) * 0.5f;
  ImRect cb(r.Min.x + 6, cy - 7, r.Min.x + 20, cy + 7);
  Box(cb, on ? K(pal::mint) : K(pal::g050), on ? K(pal::mint) : K(pal::g22), 2);
  if (on) Check(ImVec2((cb.Min.x + cb.Max.x) * 0.5f, cy), 12, K(0x0f0f0f));
  Text(r.Min.x + 28, cy, MONO_R, 10, K(pal::tcc), label);
  if (taken) TextR(r.Max.x - 8, cy, MONO_R, 9, K(pal::t88), "already added");
  g.alpha = prev;
  if (taken) return;
  if (h.hover) CursorHand();
  if (h.click) on = !on;
}

void SensorWizardView(ImRect r) {
  SensorWizard& w = A.wizard;
  if (w.step != 2) gIfsFresh = false;
  // header: title, the three steps, the target screen, close
  ImRect tb(r.Min.x + 1, r.Min.y + 1, r.Max.x - 1, r.Min.y + 44);
  g.dl->AddRectFilled(tb.Min, tb.Max, Ca(K(pal::g12)), 5, ImDrawFlags_RoundCornersTop); HLine(tb.Min.x, tb.Max.x, tb.Max.y - 1, K(pal::g2a));
  const float cy = tb.Min.y + 21.5f;
  Icon("plus", ImVec2(tb.Min.x + 18, cy), 14, K(pal::mint));
  Text(tb.Min.x + 32, cy, UI_B, 11, K(pal::white), "ADD SENSOR", 0.09f);
  static const char* kSteps[3] = {"1 Type", "2 Connect", "3 Check"};
  float sx = tb.Min.x + 32 + TextW(UI_B, 11, "ADD SENSOR", 0.09f) + 18;
  for (int i = 0; i < 3; ++i) {
    const bool on = w.step == i + 1, past = w.step > i + 1;
    Text(sx, cy, MONO_B, 10, K(on ? pal::mint : past ? pal::t88 : pal::t66), kSteps[i]);
    sx += TextW(MONO_B, 10, kSteps[i]) + 14;
  }
  ImRect xb(tb.Max.x - 34, cy - 13, tb.Max.x - 8, cy + 13);
  Hit xh = HitR(xb);
  if (xh.hover) { Fill(xb, K(pal::g22), 3); CursorHand(); }
  Icon("x", ImVec2((xb.Min.x + xb.Max.x) * 0.5f, cy), 14, K(xh.hover ? pal::white : pal::t88));
  if (xh.click) { CloseSensorWizard(); return; }
  {
    const Screen* sc = nullptr; for (auto& s : A.screens) if (s.id == w.screenId) sc = &s;
    if (!sc && !A.screens.empty()) sc = &A.screens[0];
    if (sc) TextR(xb.Min.x - 10, cy, MONO_R, 10, K(pal::coral), ("on " + sc->name).c_str());
  }

  const float colW = std::max(60.f, std::min(640.f, r.GetWidth() - 48)), x = (r.Min.x + r.Max.x - colW) * 0.5f;
  float y = tb.Max.y + 24;

  if (w.step == 1) {
    Text(x, y + 6, UI_B, 11, K(pal::tcc), "What are you connecting?"); y += 26;
    struct C { DevKind k; std::string plugin, title, sub, packId; const char* icon; bool dim; };
    std::vector<C> cards = {{DevKind::Camera2D, "", "Camera 2D", "USB camera - Body / Hand / Face / Full", "", "camera", false},
                            {DevKind::Skeleton, "", "Skeleton (OSC)", "External pose_bridge fallback", "", "camera", false},
                            {DevKind::Hokuyo, "", "Lidar 2D \xC2\xB7 Hokuyo", "UST-10LX / 20LX over Ethernet", "", "radar", false}};
    for (auto& p : SensorPlugins()) cards.push_back({DevKind::Plugin, PluginFile(p), "Lidar 2D \xC2\xB7 " + (p.name.empty() ? PluginFile(p) : p.name), PluginFile(p), "", "radar", false});
    cards.push_back({DevKind::Plugin, "mikmap_orbbec_plugin.dll", "Depth camera \xC2\xB7 Orbbec", "Femto Bolt / Mega over Orbbec SDK", "orbbec-femto", "camera", false});
    cards.push_back({DevKind::Sim, "", "Simulated", "Click a radar to make touches", "", "activity", false});
    cards.push_back({DevKind::Sim, "", "Lidar 3D", "Planned", "", "layers", true});

    const auto packRows = PackRows();
    auto findPackRow = [&](const std::string& pid) -> PackRow {
      for (const auto& pr : packRows) if (pr.id == pid) return pr;
      return PackRow{pid, pid, "1.0.0", "", "", 18ull * 1024 * 1024, PackStateFor(pid), false, false, false, 0.f};
    };

    const float cw = (colW - 10) * 0.5f, ch = 64;
    for (size_t i = 0; i < cards.size(); ++i) {
      const C& c = cards[i];
      ImRect cr = Rc(x + (i % 2) * (cw + 10), y + (i / 2) * (ch + 10), cw, ch);
      if (!c.packId.empty()) {
        const PackRow pr = findPackRow(c.packId);
        const bool installed = (pr.state == PackState::Installed || pr.state == PackState::UpdateAvailable) && !pr.pendingRemoval;
        if (!installed) {
          const char* actLab = PackActionLabel(pr);
          const bool actEn = PackActionEnabled(pr);
          const char* btnText = (actLab && actLab[0]) ? actLab : "Packs\xE2\x80\xA6";
          const float bw = std::max(58.f, ButtonW(btnText, 1) + 6.f);
          const float ccy = (cr.Min.y + cr.Max.y) * 0.5f;
          ImRect btnR(cr.Max.x - 8 - bw, ccy - 11, cr.Max.x - 8, ccy + 11);
          Hit bh = HitR(btnR);
          Hit chHit = HitR(cr);

          uint32_t bdHex = pr.state == PackState::Downloading ? pal::cyan : (pr.state == PackState::Failed ? pal::red : pal::g2a);
          Box(cr, K(pal::g14), K(bdHex, bdHex == pal::g2a ? 1.f : 0.65f), 4);
          Icon(c.icon, ImVec2(cr.Min.x + 24, ccy), 20, K(pr.state == PackState::Downloading ? pal::cyan : pal::yellow));
          const float textMaxW = btnR.Min.x - 8 - (cr.Min.x + 46);
          TextEll(cr.Min.x + 46, ccy - 9, textMaxW, UI_B, 11, K(pal::white), c.title.c_str());

          std::string statusLine;
          uint32_t statusHex = pal::yellow;
          if (pr.state == PackState::Downloading) {
            statusLine = "Installing " + std::to_string((int)std::lround(pr.progress * 100.f)) + "% \xC2\xB7 " + FormatBytes(pr.sizeBytes);
            statusHex = pal::cyan;
          } else if (pr.state == PackState::Failed) {
            statusLine = "Install failed \xC2\xB7 " + FormatBytes(pr.sizeBytes);
            statusHex = pal::red;
          } else {
            statusLine = "Not installed \xC2\xB7 " + FormatBytes(pr.sizeBytes);
            statusHex = pal::yellow;
          }
          TextEll(cr.Min.x + 46, ccy + 8, textMaxW, MONO_R, 9, K(statusHex), statusLine.c_str());

          if (pr.state == PackState::Downloading) {
            ImRect pb(cr.Min.x + 46, cr.Max.y - 8, btnR.Min.x - 10, cr.Max.y - 4);
            Box(pb, K(pal::g050), K(pal::g22), 2);
            float fw = std::max(0.f, (pb.GetWidth() - 2.f) * std::clamp(pr.progress, 0.f, 1.f));
            if (fw > 0.5f) Fill(ImRect(pb.Min.x + 1, pb.Min.y + 1, pb.Min.x + 1 + fw, pb.Max.y - 1), K(pal::cyan), 2);
          }

          uint32_t btnHex = pr.state == PackState::Downloading ? pal::yellow : (pr.state == PackState::Failed ? pal::red : pal::cyan);
          Box(btnR, bh.hover ? K(btnHex, 0.25f) : K(btnHex, 0.14f), K(btnHex, 0.75f), 3);
          TextC((btnR.Min.x + btnR.Max.x) * 0.5f, (btnR.Min.y + btnR.Max.y) * 0.5f, UI_B, 9, K(btnHex), Upper(btnText).c_str(), 0.08f);
          if (bh.hover || chHit.hover) CursorHand();
          if (bh.click) {
            if (pr.state == PackState::Downloading) PackCancel();
            else if (actEn) PackInstall(c.packId);
            else { A.settingsOpen = true; A.setTab = 5; }
            return;
          }
          if (chHit.click && !bh.hover) {
            A.notify("Install the sensor pack first (" + c.title + ")");
          }
          continue;
        }
      }
      if (!WizCard(cr, c.icon, c.title.c_str(), c.sub.c_str(), c.dim)) continue;
      if (c.dim) { A.notify("Not supported yet \xE2\x80\x94 planned"); continue; }
      w.kind = c.k; w.plugin = c.plugin; w.needsPack = c.packId; w.config.clear();
      if (c.k == DevKind::Plugin) for (auto& p : SensorPlugins()) if (PluginFile(p) == c.plugin) w.config = p.defaultConfig;
      SensorWizardNext();
      return;
    }
    return;
  }

  if (w.step == 2) {
    float fy = r.Max.y - 24 - 26;   // footer: Back · Next
    const float bw = 110;
    if (WideButton(x, fy, bw, "Back", "chevron-left", pal::t88)) { w.step = 1; return; }
    if (w.kind == DevKind::Hokuyo) {
      if (!gIfsFresh) { gIfs = mikmap::localIpv4Interfaces(); gIfsFresh = true; }   // a card plugged in later shows after Back
      auto& ifs = gIfs;
      if (w.iface >= (int)ifs.size()) w.iface = 0;
      PLabel(x, y, "Network card"); y += 9 + 4;
      ImRect ir(x, y, x + colW, y + 28);
      Hit ih = HitR(ir);
      Box(ir, ih.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
      std::string cur = ifs.empty() ? std::string("No network card is up \xE2\x80\x94 type the IP below")
                                     : ifs[w.iface].name + "  \xC2\xB7  " + ifs[w.iface].address + "/" + std::to_string(ifs[w.iface].prefix);
      TextEll(ir.Min.x + 10, (ir.Min.y + ir.Max.y) * 0.5f, colW - 40, MONO_R, 10, K(pal::tf3), cur.c_str());
      Icon("chevron-down", ImVec2(ir.Max.x - 12, (ir.Min.y + ir.Max.y) * 0.5f), 10, K(pal::t66));
      if (ih.hover) CursorHand();
      if (ih.click && !ifs.empty()) {
        std::vector<MenuItem> mi;
        for (int i = 0; i < (int)ifs.size(); ++i) {
          MenuItem it; it.label = ifs[i].name + "  " + ifs[i].address + "/" + std::to_string(ifs[i].prefix); it.icon = "activity";
          it.toneHex = i == w.iface ? pal::mint : 0;
          it.run = [i] { A.wizard.iface = i; };
          mi.push_back(it);
        }
        A.openCtx(ImVec2(ir.Min.x, ir.Max.y + 4), mi);
      }
      y += 28 + 8;
      const bool tooBig = !ifs.empty() && ifs[w.iface].prefix < 22;
      const bool canScan = !ifs.empty() && !w.scanning && !tooBig;
      {
        const float prev = g.alpha; if (!canScan) g.alpha *= 0.5f;
        const bool click = WideButton(x, y, 200, w.scanning ? "Scanning\xE2\x80\xA6" : "Scan port 10940", "radar", pal::mint);
        g.alpha = prev;
        if (click && canScan) {
          if (mikmap::hostsInSubnet(ifs[w.iface].address, ifs[w.iface].prefix).empty()) A.notify("Nothing to scan on " + ifs[w.iface].address);
          else { w.scanning = true; StartIpScan(ifs[w.iface].address, ifs[w.iface].prefix, kHokuyoPort); }
        }
      }
      if (tooBig) {
        char tb2[96]; snprintf(tb2, sizeof tb2, "Subnet too large to scan (/%d) \xE2\x80\x94 type the IP", ifs[w.iface].prefix);
        TextEll(x + 212, y + 13, colW - 212, MONO_R, 10, K(pal::yellow), tb2);
      }
      std::vector<std::string> res;
      if (w.scanning && IpScanDone(res)) {
        w.scanning = false;
        SensorWizardMergeScan(res);   // new lidars join ticked; one that is already a device is listed but not ticked
        A.notify(res.empty() ? "No lidar answered on port 10940" : "Found " + std::to_string(res.size()) + (res.size() == 1 ? " lidar" : " lidars"));
      }
      y += 26 + 14;
      PLabel(x, y, "Lidars"); y += 9 + 4;
      w.pick.resize(w.found.size(), 0);
      if (w.found.empty()) { Text(x, y + 12, MONO_R, 10, K(pal::t66), w.scanning ? "Scanning\xE2\x80\xA6" : "Nothing yet \xE2\x80\x94 Scan, or type an IP below"); y += 24; }
      if (!w.found.empty()) {   // every row Next would add is reachable: the list scrolls when it is taller than the space left
        const float contentH = w.found.size() * 26.f, listH = std::max(26.f, std::min(contentH, fy - 14 - 49 - y));
        const float rowW = colW - (contentH > listH ? 12 : 0);   // room for the scrollbar
        static ScrollArea la;
        la.Begin("##wizlidars", Rc(x, y, colW, listH));
        for (size_t i = 0; i < w.found.size(); ++i)
          TickRow(Rc(la.origin.x, la.origin.y + i * 26.f, rowW, 24), w.found[i].c_str(), w.pick[i], WizardHostAdded(w.found[i]));
        la.End(rowW, contentH);
        y += listH;
      }
      y += 10;
      PLabel(x, y, "Add by IP (ip or ip:port)"); y += 9 + 4;
      TextField("##wizhost", Rc(x, y, colW - 90, 26), w.manualHost, MONO_R, 10);
      if (WideButton(x + colW - 80, y, 80, "Add", "plus", pal::mint)) SensorWizardAddManual();
      int n = 0; for (size_t i = 0; i < w.found.size(); ++i) n += w.pick[i] && !WizardHostAdded(w.found[i]);
      char nl[48]; snprintf(nl, sizeof nl, n == 1 ? "Add 1 lidar" : "Add %d lidars", n);
      const float prev = g.alpha; if (n == 0) g.alpha *= 0.5f;
      const bool next = WideButton(x + colW - 180, fy, 180, nl, nullptr, pal::mint);
      g.alpha = prev;
      if (next) SensorWizardNext();
    } else if (w.kind == DevKind::Skeleton) {
      PLabel(x, y, "OSC UDP port"); y += 9 + 4;
      IntField("##wizport", Rc(x, y, 160, 26), w.port);
      w.port = std::clamp(w.port, 1, 65535);
      y += 26 + 12;
      PLabel(x, y, "Start the camera bridge"); y += 9 + 4;
      const std::string cmd = "python tools/pose_bridge/pose_bridge.py --port " + std::to_string(w.port);
      Box(Rc(x, y, colW, 28), K(0x000000), K(pal::g22), 3);
      TextEll(x + 10, y + 14, colW - 20, MONO_R, 10, K(pal::mint), cmd.c_str());
      if (WideButton(x + colW - 180, fy, 180, "Add camera", nullptr, pal::mint)) SensorWizardNext();
    } else {   // Plugin
      PLabel(x, y, ("Config for " + w.plugin + " (JSON)").c_str()); y += 9 + 4;
      TextField("##wizcfg", Rc(x, y, colW, 26), w.config, MONO_R, 10);
      if (WideButton(x + colW - 180, fy, 180, "Add lidar", nullptr, pal::mint)) SensorWizardNext();
    }
    return;
  }

  // step 3: the new devices, live
  Text(x, y + 6, UI_B, 11, K(pal::tcc), w.added.size() == 1 ? "Connecting the new sensor" : "Connecting the new sensors"); y += 26;
  for (auto& id : w.added) {
    const Device* d = nullptr; for (auto& e : A.devices) if (e.id == id) d = &e;
    if (!d) continue;   // removed meanwhile (undo)
    ImRect rr = Rc(x, y, colW, 40);
    Box(rr, K(pal::g14), K(pal::g2a), 4);
    const int st = d->status;
    const bool run = d->kind == DevKind::Sim || st == (int)mikmap::SourceStatus::Running;
    const uint32_t dot = st == (int)mikmap::SourceStatus::Error ? pal::red : run ? pal::mint : pal::yellow;
    Dot(ImVec2(rr.Min.x + 14, rr.Min.y + 14), 8, dot, run);
    TextEll(rr.Min.x + 26, rr.Min.y + 14, colW * 0.45f, UI_B, 11, K(pal::white), d->name.c_str());
    char sl[64];
    if (d->kind == DevKind::Sim) snprintf(sl, sizeof sl, "Simulated");
    else if (run) snprintf(sl, sizeof sl, "%d fps", d->fps);
    else snprintf(sl, sizeof sl, st == (int)mikmap::SourceStatus::Error ? "Error" : "Connecting\xE2\x80\xA6");
    TextR(rr.Max.x - 10, rr.Min.y + 14, MONO_B, 10, K(run ? pal::mint : st == (int)mikmap::SourceStatus::Error ? pal::red : pal::yellow), sl);
    const std::string sub = d->error.empty() ? Endpoint(*d) : d->error;
    TextEll(rr.Min.x + 26, rr.Min.y + 29, colW - 36, MONO_R, 9, K(d->error.empty() ? pal::t88 : pal::red), sub.c_str());
    y += 40 + 6;
  }
  TextEll(x, y + 10, colW, MONO_R, 9, K(pal::t66), "A sensor that does not connect stays in the list \xE2\x80\x94 fix its address in Properties");
  if (WideButton(x + colW - 140, r.Max.y - 24 - 26, 140, "Done", nullptr, pal::mint)) SensorWizardDone();
}


// The popup: its own full-window ImGui window drawn after the page, so it sits above everything (scroll children included)
// and the page's IsWindowHovered-based hit tests fail behind it; main.cpp also sets g.blocked at frame start.
void DrawSensorWizardPopup() {
  if (A.wizard.step == 0) return;
  ImGuiIO& io = ImGui::GetIO();
  const ImVec2 disp = io.DisplaySize;
  ImDrawList* prevDl = g.dl; const float prevAlpha = g.alpha; const bool prevBlocked = g.blocked;
  static bool wasOpen = false;
  if (!wasOpen) ImGui::SetNextWindowFocus();
  wasOpen = true;
  ImGui::SetNextWindowPos(ImVec2(0, 0)); ImGui::SetNextWindowSize(disp);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::Begin("##addsensor", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                           ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse | ImGuiWindowFlags_NoNavInputs);
  ImGui::PopStyleVar();
  g.dl = ImGui::GetWindowDrawList(); g.alpha = 1.f;
  g.blocked = A.overlayAtStart;   // frame-start state: the click that picked a menu item already closed the menu in DrawOverlays, it must not also hit what is under it
  Fill(ImRect(0, 0, disp.x, disp.y), K(0x000000, 0.6f));
  const float w = std::max(120.f, std::min(720.f, disp.x - 80.f)), h = std::max(120.f, std::min(520.f, disp.y - 80.f));
  const ImRect r((disp.x - w) * 0.5f, (disp.y - h) * 0.5f, (disp.x + w) * 0.5f, (disp.y + h) * 0.5f);
  Box(r, K(pal::g14), K(pal::g2a), 6);
  g.dl->PushClipRect(r.Min, r.Max, true);
  SensorWizardView(r);
  g.dl->PopClipRect();
  const bool esc = ImGui::IsKeyPressed(ImGuiKey_Escape, false) && WizardEscCloses(io.WantTextInput);
  if (esc) CloseSensorWizard();
  if (A.wizard.step == 0) wasOpen = false;
  ImGui::End();
  g.dl = prevDl; g.alpha = prevAlpha; g.blocked = prevBlocked;
}

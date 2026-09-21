// Sensor I/O screen: device manager + calibration · radar · parameter routing.
#include "app.h"
#include <cmath>
#include <cstdlib>
#include <algorithm>

using namespace ui;

void FitAffine(float out[9], float* rms) {
  const auto& c = A.calib;
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

static void Devices(ImRect r) {
  Fill(r, K(pal::g12));
  VLine(r.Max.x - 1, r.Min.y, r.Max.y, K(pal::g2a));
  PanelHeader(Rc(r.Min.x, r.Min.y, r.GetWidth() - 1, 24), "Device manager", pal::t88);
  int on = 0; for (auto& d : A.devices) on += d.connected;
  char cnt[32]; snprintf(cnt, sizeof cnt, "%d/%d online", on, (int)A.devices.size());
  TextR(r.Max.x - 7, r.Min.y + 11.5f, MONO_M, 10, K(pal::t66), cnt);
  static ScrollArea sa;
  sa.Begin("##devices", ImRect(r.Min.x, r.Min.y + 24, r.Max.x - 1, r.Max.y));
  float ox = sa.origin.x, oy = sa.origin.y, W = r.GetWidth() - 1 - 8, x = ox + 6, w = W - 12, y = 6;
  for (auto& d : A.devices) {
    float h = d.connected ? 76.f : 46.f;
    ImRect cr(x, oy + y, x + w, oy + y + h);
    float prev = g.alpha; if (!d.connected) g.alpha = 0.65f;
    Box(cr, d.connected ? K(pal::g18) : K(pal::g14), d.connected ? K(pal::g3a) : K(pal::g2a), 4);
    float cy = cr.Min.y + 6 + 9;
    Dot(ImVec2(cr.Min.x + 6 + 4 + 1, cy), 8, d.connected ? pal::mint : 0x555555, d.connected);
    const char* bl = d.connected ? "DISCONNECT" : "CONNECT";
    float bw = TextW(MONO_B, 9, bl, 0.09f) + 16 + 2;
    ImRect br(cr.Max.x - 7 - bw, cy - 9, cr.Max.x - 7, cy + 9);
    Hit bh = HitR(br);
    uint32_t bx = d.connected ? pal::red : pal::mint;
    Box(br, K(bx, 0.16f), K(bx, 0.45f), 3);
    Text(br.Min.x + 9, cy, MONO_B, 9, K(bx), bl, 0.09f);
    if (bh.hover) CursorHand();
    if (bh.click) { d.connected = !d.connected; }
    TextEll(cr.Min.x + 6 + 8 + 6 + 1, cy, br.Min.x - 6 - (cr.Min.x + 21), UI_B, 11, K(pal::white), d.name.c_str());
    TextEll(cr.Min.x + 7, cy + 9 + 4 + 5, cr.GetWidth() - 14, MONO_R, 10, K(pal::t88), d.endpoint.c_str());
    if (d.connected) {
      float sy = cy + 9 + 4 + 10 + 4;
      HLine(cr.Min.x + 7, cr.Max.x - 7, sy, K(pal::g2a));
      float cw = (cr.GetWidth() - 14 - 8) / 3.f;
      char v0[16], v1[16]; snprintf(v0, sizeof v0, "%d", d.fps); snprintf(v1, sizeof v1, "%dms", d.latency);
      const char* labs[3] = {"FPS", "LATENCY", "PACKETS"}; std::string vs[3] = {v0, v1, d.packets};
      for (int i = 0; i < 3; ++i) {
        float sx = cr.Min.x + 7 + i * (cw + 4);
        Text(sx, sy + 4 + 5, MONO_R, 9, K(pal::t66), labs[i]);
        Text(sx, sy + 4 + 9 + 1 + 5, MONO_B, 10, K(i == 0 ? pal::mint : pal::tcc), vs[i].c_str());
      }
    }
    g.alpha = prev;
    y += h + 6;
  }
  // calibration wizard
  HLine(ox + 6, ox + 6 + w, oy + y, K(pal::g2a));
  y += 1 + 6;
  Icon("wand-2", ImVec2(x + 6, oy + y + 6), 12, K(pal::yellow));
  Text(x + 12 + 4, oy + y + 6, UI_B, 9, K(pal::yellow), "CALIBRATION WIZARD", 0.14f);
  float m[9], rms; if (!FitHomography(A.calib, m, &rms)) FitAffine(m, &rms);
  char rb[32]; snprintf(rb, sizeof rb, "RMS: %.2fpx", rms);
  TextR(x + w, oy + y + 6, MONO_R, 9, K(pal::mint), rb);
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
    Text(br.Min.x + 7, br.Min.y + 6 + 13 + 4 + 7.5f, UI_S, 10, K(pal::te0), "Touch the physical crosshair, or click the target", 0.0f);
    ImRect cr(br.Min.x + 7, br.Max.y - 6 - 24, br.Max.x - 7, br.Max.y - 6);
    Hit h = HitR(cr);
    Box(cr, K(pal::g050), K(pal::g22), 3);
    TextC((cr.Min.x + cr.Max.x) * 0.5f, (cr.Min.y + cr.Max.y) * 0.5f, UI_S, 10, K(pal::t88), "Cancel wizard");
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
  sa.End(W, y);
}

// G17: a "touch.down" patch cord that is switched on fires the clip named in its target ("<layer> · <clip>").
static void FireTouchRoutes() {
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

static void Radar(ImRect r) {
  Fill(r, K(pal::g050));
  ImRect tb(r.Min.x, r.Min.y, r.Max.x, r.Min.y + 44);
  Fill(tb, K(pal::g12)); HLine(tb.Min.x, tb.Max.x, tb.Max.y - 1, K(pal::g2a));
  float cy = r.Min.y + 21.5f;
  Icon("activity", ImVec2(tb.Min.x + 8 + 6.5f, cy), 13, K(pal::mint));
  Text(tb.Min.x + 8 + 13 + 6, cy, UI_B, 11, K(pal::white), "RADAR VIEW", 0.09f);
  float lx = tb.Min.x + 8 + 13 + 6 + TextW(UI_B, 11, "RADAR VIEW", 0.09f) + 6;
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
    if (h.click) A.editRoi = !A.editRoi;
    TextEll(lx, cy, br.Min.x - 8 - lx, MONO_R, 10, K(pal::t88), "40.0 Hz real-time sweep");
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
  // ROI
  ImVec2 rp[4];
  for (int i = 0; i < 4; ++i) rp[i] = ImVec2(c.x + A.roi[i].x * maxR, c.y + A.roi[i].y * maxR);
  for (int i = 1; i + 1 < 4; ++i) dl->AddTriangleFilled(rp[0], rp[i], rp[i + 1], Ca(A.editRoi ? K(pal::yellow, 0.15f) : K(pal::cyan, 0.10f)));
  dl->AddPolyline(rp, 4, Ca(K(A.editRoi ? pal::yellow : pal::cyan)), ImDrawFlags_Closed, 2 * k);
  if (A.editRoi) for (int i = 0; i < 4; ++i) dl->AddCircleFilled(rp[i], 6 * k, Ca(K(pal::yellow)), 20);
  static int roiDrag = -1;   // S4: while "Edit ROI" is on, the four yellow handles can be dragged (clamped to the radar disc)
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
        A.roi[roiDrag] = ImVec2(std::round(nx * 1000) / 1000, std::round(ny * 1000) / 1000);
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      } else roiDrag = -1;
    } else if (nearH) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  }
  // sweep
  float a = A.sweep;
  RadialFan(c, maxR, K(pal::mint, 0.30f), K(pal::mint, 0.f), a - 0.35f, a, 24);
  dl->AddLine(c, ImVec2(c.x + cosf(a) * maxR, c.y + sinf(a) * maxR), Ca(K(pal::mint)), 1.5f * k);
  for (auto& t : A.touchPts) {
    ImVec2 b(c.x + t.x * maxR, c.y + t.y * maxR);
    dl->AddCircleFilled(b, 7 * k, Ca(K(pal::coral)), 24);
    dl->AddCircle(b, 12 * k, Ca(K(pal::white)), 24, 1.5f * k);
    char lb[48]; snprintf(lb, sizeof lb, "ID:%d [%dmm]", t.id, (int)std::round(t.x * 2000));
    Text(b.x + 14 * k, b.y + 4 * k - 1, MONO_B, 11 * k, K(pal::white), lb);
  }
  if (A.wizardStep >= 1 && A.wizardStep <= 4) {
    const Calib& cb = A.calib[A.wizardStep - 1];
    ImVec2 t(c.x + cb.mx * maxR, c.y + cb.my * maxR);
    dl->AddCircle(t, 18 * k, Ca(K(pal::yellow)), 32, 2 * k);
    char lb[24]; snprintf(lb, sizeof lb, "TARGET %d/4", A.wizardStep);
    Text(t.x + 22 * k, t.y - 6 * k - 1, MONO_B, 12 * k, K(pal::yellow), lb);
  }
  dl->AddCircle(c, D * 0.5f - 0.5f, Ca(K(pal::g2a)), 96, 1.f);
  TextC(cx, top + D + 6 + 5, MONO_R, 10, K(pal::t66), A.editRoi ? "Drag the yellow handles to reshape the region of interest" : "Click the radar to inject a simulated touch point");
  // click
  ImVec2 m = ImGui::GetIO().MousePos;
  float dist = std::hypot(m.x - c.x, m.y - c.y);
  if (!A.editRoi && Hover(ImRect(c.x - D * 0.5f, c.y - D * 0.5f, c.x + D * 0.5f, c.y + D * 0.5f)) && dist <= D * 0.5f) {
    ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (ImGui::IsMouseClicked(0)) {
      float nx = (m.x - c.x) / maxR, ny = (m.y - c.y) / maxR;
      if (std::hypot(nx, ny) <= 1.05f) {
        if (A.wizardStep >= 1 && A.wizardStep <= 4) {
          A.calib[A.wizardStep - 1].mx = std::round(nx * 1000) / 1000; A.calib[A.wizardStep - 1].my = std::round(ny * 1000) / 1000;
          A.wizardStep = A.wizardStep == 4 ? 0 : A.wizardStep + 1;
        } else {
          A.touchPts.clear(); A.touchPts.push_back({100 + rand() % 900, nx, ny});
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
    float v2 = (A.blobSize - 5) / 45.f * 100.f;
    if (Slider(0x4002, Rc(x, oy + y + 4, w, 6), v2, pal::mint)) A.blobSize = std::round(5 + v2 / 100.f * 45.f);
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
      A.touchPts.clear();
      A.touchPts.push_back({100 + rand() % 900, ((rand() % 1000) / 1000.f - 0.5f) * 1.4f, ((rand() % 1000) / 1000.f - 0.5f) * 1.4f});
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


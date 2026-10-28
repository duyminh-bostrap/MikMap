// Stage pieces shared by Advanced Mapping and the Sensor page: box math (Output Transform), and later the pan/zoom view,
// the screen backdrop and quad editing. Moved out of mapping.cpp unchanged so both pages run the same code.
#include "app.h"
#include <cmath>
#include <algorithm>

using namespace ui;

// ── Output Transformation > Transform (Resolume): a box around the points as the audience sees them, turned by rotDeg ──
RectXf BoxOfPoints(const ImVec2* p, int n, float rotDeg) {
  const float r = rotDeg * kDegToRad, co = std::cos(r), si = std::sin(r);
  float x0 = 1e9f, y0 = 1e9f, x1 = -1e9f, y1 = -1e9f;
  for (int i = 0; i < n; ++i) {
    float lx = p[i].x * co + p[i].y * si, ly = -p[i].x * si + p[i].y * co;   // into the box's own axes
    x0 = std::min(x0, lx); x1 = std::max(x1, lx); y0 = std::min(y0, ly); y1 = std::max(y1, ly);
  }
  float lcx = (x0 + x1) * 0.5f, lcy = (y0 + y1) * 0.5f, w = std::max(1.f, x1 - x0), h = std::max(1.f, y1 - y0);
  float cx = lcx * co - lcy * si, cy = lcx * si + lcy * co;
  return {cx - w * 0.5f, cy - h * 0.5f, w, h, rotDeg};
}
void MapBoxPoints(ImVec2* p, int n, const RectXf& from, const RectXf& to) {
  const float fr = from.rot * kDegToRad, tr = to.rot * kDegToRad, fc = std::cos(fr), fs = std::sin(fr), tc = std::cos(tr), ts = std::sin(tr);
  const float fcx = from.x + from.w * 0.5f, fcy = from.y + from.h * 0.5f, tcx = to.x + to.w * 0.5f, tcy = to.y + to.h * 0.5f;
  const float kx = to.w / std::max(1e-3f, from.w), ky = to.h / std::max(1e-3f, from.h);
  for (int i = 0; i < n; ++i) {
    ImVec2& q = p[i];
    float dx = q.x - fcx, dy = q.y - fcy, lx = (dx * fc + dy * fs) * kx, ly = (-dx * fs + dy * fc) * ky;
    q = ImVec2(std::clamp(tcx + lx * tc - ly * ts, -4000.f, 8000.f), std::clamp(tcy + lx * ts + ly * tc, -4000.f, 8000.f));
  }
}

// ── stage view: free pan/zoom over the space ──
StageXf StageViewUpdate(StageView& v, ImRect area, float SW, float SH, float baseW, bool handTool, bool edgeScroll, bool* rClickOut) {
  ImVec2 m = ImGui::GetIO().MousePos;
  // View: the canvas centre sits at the stage centre, shifted by a free pan (scrollX/Y, px). Nothing clamps the view
  // to the canvas box — points outside it must stay visible and grabbable — only the view centre is kept inside the
  // -4000..8000 range the points themselves are limited to, so you can't pan off into nothing.
  ImVec2 ac((area.Min.x + area.Max.x) * 0.5f, (area.Min.y + area.Max.y) * 0.5f);
  if (v.req.valid) {   // centre the requested output point
    v.z = std::clamp(v.req.z, kMinZoom, kMaxZoom);
    float sn = baseW * v.z / SW;
    v.scrollX = (v.req.cx * SW - SW * 0.5f) * sn; v.scrollY = (v.req.cy * SH - SH * 0.5f) * sn;
    v.req.valid = false;
  }
  bool inArea = area.Contains(m) && !g.blocked;
  ImGuiIO& io = ImGui::GetIO();
  float s = baseW * v.z / SW;   // px per space px
  // Mouse wheel = zoom about the cursor (the point under it stays put); Shift+wheel scrolls vertically; a sideways wheel / trackpad pans
  // horizontally. Panning otherwise: the hand tool, or right-drag.
  if (inArea && io.MouseWheelH != 0.f) v.scrollX -= io.MouseWheelH * 48.f;
  if (inArea && io.MouseWheel != 0.f && io.KeyShift) v.scrollY -= io.MouseWheel * 48.f;
  if (inArea && io.MouseWheel != 0.f && !io.KeyShift) {
    ImVec2 p((m.x - ac.x + v.scrollX) / s + SW * 0.5f, (m.y - ac.y + v.scrollY) / s + SH * 0.5f);
    v.z = std::clamp(v.z * std::pow(1.15f, std::clamp(io.MouseWheel, -3.f, 3.f)), kMinZoom, kMaxZoom);   // a wheel notch = one 15% step; trackpad fractions are smooth
    s = baseW * v.z / SW;
    v.scrollX = (p.x - SW * 0.5f) * s + (ac.x - m.x); v.scrollY = (p.y - SH * 0.5f) * s + (ac.y - m.y);
  }
  // shortcut: one pan in flight for the whole app — fine while only one stage is drawn per frame; give StageView its own if two ever are
  static int panBtn = -1; static ImVec2 panM; static float panX, panY;   // right-drag pan, or left-drag while the hand tool is on
  bool rClick = false;   // right button released without dragging = context menu, not a pan
  if (panBtn < 0 && inArea) {
    if (ImGui::IsMouseClicked(1)) panBtn = 1; else if (handTool && ImGui::IsMouseClicked(0)) panBtn = 0;
    if (panBtn >= 0) { panM = m; panX = v.scrollX; panY = v.scrollY; }
  }
  if (panBtn >= 0) {
    if (ImGui::IsMouseDown(panBtn)) { v.scrollX = panX - (m.x - panM.x); v.scrollY = panY - (m.y - panM.y); ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll); }
    else { rClick = panBtn == 1 && std::hypot(m.x - panM.x, m.y - panM.y) < 4.f; panBtn = -1; }
  } else if (handTool && inArea) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
  // dragging a point to (or past) the stage edge scrolls the view, so the point never slides under the side panels
  if (edgeScroll && ImGui::IsMouseDown(0)) {
    const float edge = 28.f;
    auto push = [&](float val, float lo, float hi) {
      if (val < lo + edge) return -std::min(24.f, (lo + edge - val) * 0.35f);
      if (val > hi - edge) return std::min(24.f, (val - (hi - edge)) * 0.35f);
      return 0.f;
    };
    v.scrollX += push(m.x, area.Min.x, area.Max.x); v.scrollY += push(m.y, area.Min.y, area.Max.y);
  }
  v.scrollX = std::clamp(v.scrollX, (-4000.f - SW * 0.5f) * s, (8000.f - SW * 0.5f) * s);
  v.scrollY = std::clamp(v.scrollY, (-4000.f - SH * 0.5f) * s, (8000.f - SH * 0.5f) * s);
  v.cx = (SW * 0.5f + v.scrollX / s) / SW; v.cy = (SH * 0.5f + v.scrollY / s) / SH;
  float cw = SW * s, ch = SH * s;
  StageXf x;
  x.cv = ImRect(ac.x - cw * 0.5f - v.scrollX, ac.y - ch * 0.5f - v.scrollY, ac.x + cw * 0.5f - v.scrollX, ac.y + ch * 0.5f - v.scrollY);
  x.s = s;
  if (rClickOut) *rClickOut = rClick;
  return x;
}

// ── what the screen sends out: its box, and every visible slice's thumbnail after input rect, warp, masks, opacity / colour ──
void DrawScreenBackdrop(const Screen& sc, const StageXf& x, float t, float alpha) {
  const float prevA = g.alpha; g.alpha *= alpha;
  Box(x.cv, K(0x0d0d0d), 0, 3);
  bool anySolo = false;
  for (auto& S : sc.slices) if (S.solo && S.visible) anySolo = true;
  g.dl->PushClipRect(x.cv.Min, x.cv.Max, true);   // the projector only shows the 1920x1080 box
  for (auto& S : sc.slices) if (S.visible && !(anySolo && !S.solo)) DrawSliceOutput(sc, S, x.cv.Min.x, x.cv.Min.y, x.s, x.s, t);
  g.dl->PopClipRect();
  g.alpha = prevA;
}

// ── one free quad (4 points in space px) edited on the stage, with Mapping's two Output tools ──
// Mapping keeps its own slice editor (it shares one frame between input rect, mask and Transform box, with the magnet and the
// picked points); this is the same look and the same box math (BoxOfPoints / MapBoxPoints) for a plain quad.
// The quad keeps no turn of its own: at rest its box is turned like its top edge (q[0] -> q[1]), so a turned quad gets a snug box.
static void BoxCorners(const RectXf& R, ImVec2 c[4]) {   // tl, tr, br, bl of the turned box
  const float co = std::cos(R.rot * kDegToRad), si = std::sin(R.rot * kDegToRad), hx = R.w * 0.5f, hy = R.h * 0.5f, cx = R.x + hx, cy = R.y + hy;
  const float lx[4] = {-hx, hx, hx, -hx}, ly[4] = {-hy, -hy, hy, hy};
  for (int i = 0; i < 4; ++i) c[i] = ImVec2(cx + lx[i] * co - ly[i] * si, cy + lx[i] * si + ly[i] * co);
}
bool EditQuad(const char* id, const StageXf& x, ImVec2 q[4], int tool, uint32_t tone) {
  // one drag at a time across every EditQuad; kind: 1 corner (Edit Points), 2 resize, 5 move, 6 turn (Transform)
  static ImGuiID active = 0; static int kind = 0, idx = 0; static bool changed = false;
  static ImVec2 q0[4], off, mo0, anchor; static RectXf R0, Rnow; static float aspect = 1, ang0 = 0;
  const ImGuiID me = ImGui::GetID(id);
  const ImVec2 m = ImGui::GetIO().MousePos, mo = x.fromPx(m);
  const bool shift = ImGui::GetIO().KeyShift;
  auto dist = [](ImVec2 a, ImVec2 b) { return std::hypot(a.x - b.x, a.y - b.y); };
  bool released = false;
  if (active == me && !ImGui::IsMouseDown(0)) { active = 0; released = changed; }
  // ---- drag (before the box is taken, so the frame drawn is this frame's) ----
  if (active == me && kind == 1) {
    ImVec2 p(mo.x + off.x, mo.y + off.y);
    if (shift) { if (std::fabs(mo.x - mo0.x) >= std::fabs(mo.y - mo0.y)) p.y = q0[idx].y; else p.x = q0[idx].x; }   // one axis
    p = ImVec2(std::clamp(std::round(p.x), -4000.f, 8000.f), std::clamp(std::round(p.y), -4000.f, 8000.f));
    if (p.x != q[idx].x || p.y != q[idx].y) { q[idx] = p; changed = true; }
  } else if (active == me) {
    RectXf N = R0;
    if (kind == 2) {   // resize: the opposite corner / edge middle stays put
      const float co = std::cos(N.rot * kDegToRad), si = std::sin(N.rot * kDegToRad), dx = mo.x - anchor.x, dy = mo.y - anchor.y;
      const float lx = dx * co + dy * si, ly = -dx * si + dy * co;   // pointer relative to the anchor, in box axes
      float w = N.w, h = N.h, hx = 0, hy = 0; const int k = idx % 10;
      if (idx >= 10) {
        if (k == 0) { h = std::max(8.f, -ly); hy = -h * 0.5f; } else if (k == 1) { w = std::max(8.f, lx); hx = w * 0.5f; }
        else if (k == 2) { h = std::max(8.f, ly); hy = h * 0.5f; } else { w = std::max(8.f, -lx); hx = -w * 0.5f; }
      } else {
        const float sx = (k == 1 || k == 2) ? 1.f : -1.f, sy = (k == 2 || k == 3) ? 1.f : -1.f;
        w = std::max(8.f, sx * lx); h = std::max(8.f, sy * ly);
        if (shift && aspect > 0.f) { if (w / h > aspect) h = w / aspect; else w = h * aspect; }   // same aspect
        hx = sx * w * 0.5f; hy = sy * h * 0.5f;
      }
      const float cx = anchor.x + hx * co - hy * si, cy = anchor.y + hx * si + hy * co;
      N.w = w; N.h = h; N.x = cx - w * 0.5f; N.y = cy - h * 0.5f;
    } else if (kind == 5) {   // move; Shift: along the axis the pointer has moved further on
      float cx = mo.x + off.x, cy = mo.y + off.y;
      if (shift) { if (std::fabs(mo.x - mo0.x) >= std::fabs(mo.y - mo0.y)) cy = mo0.y + off.y; else cx = mo0.x + off.x; }
      N.x = cx - N.w * 0.5f; N.y = cy - N.h * 0.5f;
    } else {   // turn about the centre; Shift: 45 degree steps
      const float ccx = N.x + N.w * 0.5f, ccy = N.y + N.h * 0.5f;
      float r = R0.rot + (std::atan2(mo.y - ccy, mo.x - ccx) - ang0) / kDegToRad;
      if (shift) r = std::round(r / 45.f) * 45.f;
      r -= 360.f * std::floor((r + 180.f) / 360.f);   // keep within -180..180
      N.rot = std::fabs(r) < 0.05f ? 0.f : r;
    }
    ImVec2 nq[4] = {q0[0], q0[1], q0[2], q0[3]};
    MapBoxPoints(nq, 4, R0, N);
    for (int i = 0; i < 4; ++i) if (nq[i].x != q[i].x || nq[i].y != q[i].y) { q[i] = nq[i]; changed = true; }
    Rnow = N;
  }
  // ---- the box, handles, hit-test, grab ----
  const float topRot = std::atan2(q[1].y - q[0].y, q[1].x - q[0].x) / kDegToRad;
  const RectXf R = active == me && kind != 1 ? Rnow : BoxOfPoints(q, 4, std::fabs(topRot) < 0.05f ? 0.f : topRot);
  ImVec2 cp[4], mc[4], cpx[4], mpx[4], qpx[4];
  BoxCorners(R, cp);
  for (int i = 0; i < 4; ++i) {
    mc[i] = ImVec2((cp[i].x + cp[(i + 1) % 4].x) * 0.5f, (cp[i].y + cp[(i + 1) % 4].y) * 0.5f);
    cpx[i] = x.toPx(cp[i]); mpx[i] = x.toPx(mc[i]); qpx[i] = x.toPx(q[i]);
  }
  const ImVec2 ctr(R.x + R.w * 0.5f, R.y + R.h * 0.5f);
  const bool idle = active == 0 && !g.blocked, click = idle && ImGui::IsMouseClicked(0);
  int hot = 0, hotIdx = 0;
  if (idle && tool == 0) {
    for (int i = 0; i < 4 && !hot; ++i) if (dist(m, qpx[i]) <= 10.f + 4.f) { hot = 1; hotIdx = i; }
  } else if (idle) {   // Mapping's frame: squares resize, rings turn, inside moves
    for (int i = 0; i < 4 && !hot; ++i) if (dist(m, cpx[i]) <= 8.f) { hot = 2; hotIdx = i; }
    for (int i = 0; i < 4 && !hot; ++i) if (dist(m, mpx[i]) <= 8.f) { hot = 2; hotIdx = 10 + i; }
    for (int i = 0; i < 4 && !hot; ++i) if (dist(m, cpx[i]) <= 16.f) { hot = 6; hotIdx = i; }
    if (!hot) {
      bool in = false;
      for (int i = 0, j = 3; i < 4; j = i++)
        if (((cpx[i].y > m.y) != (cpx[j].y > m.y)) && (m.x < (cpx[j].x - cpx[i].x) * (m.y - cpx[i].y) / (cpx[j].y - cpx[i].y) + cpx[i].x)) in = !in;
      if (in) hot = 5;
    }
    if (hot) ImGui::SetMouseCursor(hot == 5 ? ImGuiMouseCursor_Hand : ImGuiMouseCursor_ResizeAll);
  }
  if (hot && click) {
    active = me; kind = hot; idx = hotIdx; changed = false; mo0 = mo; R0 = Rnow = R; aspect = R.w / std::max(1e-3f, R.h);
    for (int j = 0; j < 4; ++j) q0[j] = q[j];
    if (hot == 1) off = ImVec2(q[hotIdx].x - mo.x, q[hotIdx].y - mo.y);
    else if (hot == 2) anchor = hotIdx >= 10 ? mc[(hotIdx % 10 + 2) % 4] : cp[(hotIdx + 2) % 4];   // opposite corner / edge middle
    else if (hot == 5) off = ImVec2(ctr.x - mo.x, ctr.y - mo.y);
    else ang0 = std::atan2(mo.y - ctr.y, mo.x - ctr.x);
  }
  // ---- draw ----
  const bool dragging = active == me;
  g.dl->AddPolyline(qpx, 4, Ca(K(tone)), ImDrawFlags_Closed, 2.f);
  if (tool == 0) {
    for (int i = 0; i < 4; ++i) {   // big squares, like Mapping's perspective corners
      const bool h = (dragging && idx == i) || (hot == 1 && hotIdx == i);
      const float hh = 10.f + (h ? 1.5f : 0.f);
      ImRect br(qpx[i].x - hh, qpx[i].y - hh, qpx[i].x + hh, qpx[i].y + hh);
      g.dl->AddRectFilled(br.Min, br.Max, Ca(K(tone, h ? 0.35f : 0.18f)), 2.f);
      g.dl->AddRect(br.Min, br.Max, Ca(K(h ? pal::white : tone)), 2.f, 0, 1.8f);
      if (h) CursorHand();
    }
  } else {
    g.dl->AddPolyline(cpx, 4, Ca(K(tone, 0.8f)), ImDrawFlags_Closed, 1.5f);   // the Transform box: a thin line around the quad
    const bool turning = (dragging && kind == 6) || hot == 6;
    for (int i = 0; i < 4; ++i) g.dl->AddCircle(cpx[i], 11.f, Ca(K(turning && (dragging || hotIdx == i) ? pal::white : tone)), 24, 1.5f);
    for (const ImVec2* set : {cpx, mpx}) for (int i = 0; i < 4; ++i) {
      ImRect hr(set[i].x - 5, set[i].y - 5, set[i].x + 5, set[i].y + 5);
      Box(hr, K(pal::white), K(tone), 2);
      Border(hr, K(tone), 2, 2);
    }
  }
  return released;
}

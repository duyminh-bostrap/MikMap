// Advanced Mapping screen: mapping tree · stage canvas · slice/mask/screen properties.
#include "app.h"
#include <cmath>
#include <cstring>
#include <algorithm>

using namespace ui;

// ───────────────────────── model helpers ─────────────────────────
Screen* App::curScreen() {
  for (auto& s : screens) if (s.id == selSc) return &s;
  return screens.empty() ? nullptr : &screens[0];
}
Slice* App::curSlice() {
  Screen* sc = curScreen();
  if (!sc) return nullptr;
  for (auto& s : sc->slices) if (s.id == selSl) return &s;
  return sc->slices.empty() ? nullptr : &sc->slices[0];
}
Mask* App::curMask() {
  Slice* sl = curSlice();
  if (!sl || selMk.empty()) return nullptr;
  for (auto& m : sl->masks) if (m.id == selMk) return &m;
  return nullptr;
}

static void QuadOf(Slice& s, float x, float y, float w, float h) {
  s.q[0] = {x, y}; s.q[1] = {x + w, y}; s.q[2] = {x + w, y + h}; s.q[3] = {x, y + h};
}

// ── input rect geometry ──
// The input rect is ix..ih rotated by irot about its centre. These convert it to/from a 4-point shape so it can be
// exchanged with the output quad ("Match output shape" / "Swap"): the input lives in canvas px, the quad in the
// screen's 1920x1080 output px, so the two spaces are scaled per axis.
static constexpr float kDegToRad = 3.14159265f / 180.f;
static void InputCorners(const Slice& s, ImVec2 c[4]) {   // canvas px: tl, tr, br, bl
  float cx = s.ix + s.iw * 0.5f, cy = s.iy + s.ih * 0.5f, hw = s.iw * 0.5f, hh = s.ih * 0.5f;
  float co = std::cos(s.irot * kDegToRad), si = std::sin(s.irot * kDegToRad);
  static const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
  for (int i = 0; i < 4; ++i) { float lx = sx[i] * hw, ly = sy[i] * hh; c[i] = ImVec2(cx + lx * co - ly * si, cy + lx * si + ly * co); }
}
static void InputAsOutputQuad(const Slice& s, ImVec2 q[4]) {
  ImVec2 c[4]; InputCorners(s, c);
  float kx = 1920.f / std::max(1, A.canvasW), ky = 1080.f / std::max(1, A.canvasH);
  for (int i = 0; i < 4; ++i) q[i] = ImVec2(c[i].x * kx, c[i].y * ky);
}
static void SetInputFromQuad(Slice& s, const ImVec2 q[4]) {   // best-fit rotated rectangle through a (possibly keystoned) quad
  float kx = (float)std::max(1, A.canvasW) / 1920.f, ky = (float)std::max(1, A.canvasH) / 1080.f;
  ImVec2 p[4]; for (int i = 0; i < 4; ++i) p[i] = ImVec2(q[i].x * kx, q[i].y * ky);
  auto len = [](ImVec2 a, ImVec2 b) { return std::hypot(b.x - a.x, b.y - a.y); };
  float cx = (p[0].x + p[1].x + p[2].x + p[3].x) * 0.25f, cy = (p[0].y + p[1].y + p[2].y + p[3].y) * 0.25f;
  float w = (len(p[0], p[1]) + len(p[3], p[2])) * 0.5f, h = (len(p[0], p[3]) + len(p[1], p[2])) * 0.5f;
  float ex = (p[1].x - p[0].x) + (p[2].x - p[3].x), ey = (p[1].y - p[0].y) + (p[2].y - p[3].y);   // top + bottom edge
  float rot = std::atan2(ey, ex) / kDegToRad;
  s.iw = std::max(20, (int)std::lround(w)); s.ih = std::max(20, (int)std::lround(h));
  s.ix = (int)std::lround(cx - s.iw * 0.5f); s.iy = (int)std::lround(cy - s.ih * 0.5f);
  s.irot = std::fabs(rot) < 0.01f ? 0.f : rot;
}

// ── keystone: unit square → the 4 corner pins ──
// A true perspective map (closed-form square-to-quad homography, Heckbert), same model as engine WarpCornerPin and
// Resolume's perspective corners: straight lines stay straight and spacing foreshortens the way a tilted projector's
// does. Cheap enough to evaluate per vertex. A concave or bow-tie quad has no sane projective map (content would fold
// through infinity), so it falls back to bilinear — dragging a corner through a bad shape degrades instead of exploding.
namespace {
ImVec2 Vadd(ImVec2 p, ImVec2 q) { return ImVec2(p.x + q.x, p.y + q.y); }
ImVec2 Vsub(ImVec2 p, ImVec2 q) { return ImVec2(p.x - q.x, p.y - q.y); }
ImVec2 Vmul(ImVec2 p, float k) { return ImVec2(p.x * k, p.y * k); }
ImVec2 Bilerp(ImVec2 p00, ImVec2 p10, ImVec2 p11, ImVec2 p01, float u, float v) {
  return ImVec2((1 - u) * (1 - v) * p00.x + u * (1 - v) * p10.x + u * v * p11.x + (1 - u) * v * p01.x,
                (1 - u) * (1 - v) * p00.y + u * (1 - v) * p10.y + u * v * p11.y + (1 - u) * v * p01.y);
}

struct Keystone {
  const ImVec2* q;
  bool proj = false;
  float a = 0, b = 0, c = 0, d = 0, e = 0, f = 0, g = 0, h = 0;   // x = (a u + b v + c) / (g u + h v + 1), y likewise
  explicit Keystone(const ImVec2 quad[4]) : q(quad) {
    int pos = 0, neg = 0;
    for (int i = 0; i < 4; ++i) {
      ImVec2 e0 = Vsub(q[(i + 1) % 4], q[i]), e1 = Vsub(q[(i + 2) % 4], q[(i + 1) % 4]);
      float cr = e0.x * e1.y - e0.y * e1.x;
      if (cr > 1e-3f) ++pos; else if (cr < -1e-3f) ++neg;
    }
    if (pos != 4 && neg != 4) return;
    float sx = q[0].x - q[1].x + q[2].x - q[3].x, sy = q[0].y - q[1].y + q[2].y - q[3].y;
    if (std::fabs(sx) < 1e-4f && std::fabs(sy) < 1e-4f) {   // parallelogram: plain affine
      a = q[1].x - q[0].x; b = q[2].x - q[1].x; c = q[0].x; d = q[1].y - q[0].y; e = q[2].y - q[1].y; f = q[0].y;
    } else {
      float dx1 = q[1].x - q[2].x, dx2 = q[3].x - q[2].x, dy1 = q[1].y - q[2].y, dy2 = q[3].y - q[2].y;
      float den = dx1 * dy2 - dx2 * dy1;
      if (std::fabs(den) < 1e-6f) return;
      g = (sx * dy2 - dx2 * sy) / den; h = (dx1 * sy - sx * dy1) / den;
      a = q[1].x - q[0].x + g * q[1].x; b = q[3].x - q[0].x + h * q[3].x; c = q[0].x;
      d = q[1].y - q[0].y + g * q[1].y; e = q[3].y - q[0].y + h * q[3].y; f = q[0].y;
    }
    proj = true;
  }
  ImVec2 Fwd(ImVec2 l) const {
    if (proj) { float w = g * l.x + h * l.y + 1; return ImVec2((a * l.x + b * l.y + c) / w, (d * l.x + e * l.y + f) / w); }
    return Bilerp(q[0], q[1], q[2], q[3], l.x, l.y);
  }
  // output point → keystone-local point; works outside the unit square too (mesh points may sit beyond the quad)
  bool Inv(ImVec2 p, ImVec2& l) const {
    if (proj) {
      float A = e - f * h, B = c * h - b, C = b * f - c * e, D = f * g - d, E = a - c * g, F = c * d - a * f;
      float G = d * h - e * g, H = b * g - a * h, I = a * e - b * d;
      float w = G * p.x + H * p.y + I;
      if (std::fabs(w) < 1e-9f) return false;
      l = ImVec2((A * p.x + B * p.y + C) / w, (D * p.x + E * p.y + F) / w);
      return g * l.x + h * l.y + 1 > 1e-4f;   // beyond the horizon line: no point in front maps there
    }
    ImVec2 t(0.5f, 0.5f);   // bilinear fallback: Newton on P(u,v) = p
    for (int it = 0; it < 24; ++it) {
      ImVec2 r = Vsub(Fwd(t), p);
      if (r.x * r.x + r.y * r.y < 1e-6f) { l = t; return true; }
      ImVec2 du = Vadd(Vmul(Vsub(q[1], q[0]), 1 - t.y), Vmul(Vsub(q[2], q[3]), t.y));
      ImVec2 dv = Vadd(Vmul(Vsub(q[3], q[0]), 1 - t.x), Vmul(Vsub(q[2], q[1]), t.x));
      float det = du.x * dv.y - du.y * dv.x;
      if (std::fabs(det) < 1e-9f) return false;
      t.x -= (r.x * dv.y - r.y * dv.x) / det; t.y -= (du.x * r.y - du.y * r.x) / det;
    }
    return false;
  }
};
}  // namespace

static std::vector<float> Uni(int n) { std::vector<float> v; for (int i = 1; i < std::max(2, n); ++i) v.push_back((float)i / n); return v; }
static void MeshUV(const Slice& s, std::vector<float>& us, std::vector<float>& vs) {
  us = s.meshU.empty() ? Uni(s.meshCols) : s.meshU; vs = s.meshV.empty() ? Uni(s.meshRows) : s.meshV;
  std::sort(us.begin(), us.end()); std::sort(vs.begin(), vs.end());
}
// grid line positions including both borders: {0, splits..., 1}
static void MeshLines(const Slice& s, std::vector<float>& us, std::vector<float>& vs) {
  std::vector<float> uu, vv; MeshUV(s, uu, vv);
  us = {0}; us.insert(us.end(), uu.begin(), uu.end()); us.push_back(1);
  vs = {0}; vs.insert(vs.end(), vv.begin(), vv.end()); vs.push_back(1);
}
static bool LocalSized(const Slice& s, size_t rows, size_t cols) { return s.meshLocal.size() == rows && !s.meshLocal.empty() && s.meshLocal[0].size() == cols; }
// mesh vertices in keystone-local space (the stored grid, or the undeformed one if none matches the current splits)
static std::vector<std::vector<ImVec2>> LocalGrid(const Slice& s) {
  std::vector<float> us, vs; MeshLines(s, us, vs);
  if (LocalSized(s, vs.size(), us.size())) return s.meshLocal;
  std::vector<std::vector<ImVec2>> g;
  for (float v : vs) { std::vector<ImVec2> row; for (float u : us) row.push_back(ImVec2(u, v)); g.push_back(row); }
  return g;
}
// the same vertices in output pixels — what the stage draws and hit-tests
static std::vector<std::vector<ImVec2>> MeshGrid(const Slice& s) {
  Keystone k(s.q);
  auto g = LocalGrid(s);
  for (auto& row : g) for (auto& p : row) p = k.Fwd(p);
  return g;
}
// output-space outline of the slice as the audience sees it (quad, or the mesh border once warped)
static std::vector<ImVec2> SliceOutline(const Slice& s) {
  if (s.warp == 0) return {s.q[0], s.q[1], s.q[2], s.q[3]};
  auto g = MeshGrid(s);
  int R = (int)g.size(), C = (int)g[0].size();
  std::vector<ImVec2> o;
  for (int c = 0; c < C; ++c) o.push_back(g[0][c]);
  for (int r = 1; r < R; ++r) o.push_back(g[r][C - 1]);
  for (int c = C - 2; c >= 0; --c) o.push_back(g[R - 1][c]);
  for (int r = R - 2; r > 0; --r) o.push_back(g[r][0]);
  return o;
}

// unit square (u,v) → a point in the screen's 1920x1080 output space: mesh warp (if any) in keystone space, then keystone
ImVec2 SliceMapUV(const Slice& s, float u, float v) {
  Keystone k(s.q);
  if (s.warp == 0) return k.Fwd(ImVec2(u, v));
  std::vector<float> us, vs; MeshLines(s, us, vs);
  bool stored = LocalSized(s, vs.size(), us.size());
  auto at = [&](int r, int c) { return stored ? s.meshLocal[r][c] : ImVec2(us[c], vs[r]); };
  int ci = 0, ri = 0;
  while (ci + 2 < (int)us.size() && u >= us[ci + 1]) ++ci;
  while (ri + 2 < (int)vs.size() && v >= vs[ri + 1]) ++ri;
  float lu = (u - us[ci]) / std::max(1e-6f, us[ci + 1] - us[ci]);
  float lv = (v - vs[ri]) / std::max(1e-6f, vs[ri + 1] - vs[ri]);
  return k.Fwd(Bilerp(at(ri, ci), at(ri, ci + 1), at(ri + 1, ci + 1), at(ri + 1, ci), lu, lv));
}

void SliceOutputBounds(const Slice& s, ImVec2& mn, ImVec2& mx) {
  mn = ImVec2(1e9f, 1e9f); mx = ImVec2(-1e9f, -1e9f);
  auto add = [&](ImVec2 p) { mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); };
  for (int i = 0; i < 4; ++i) add(s.q[i]);
  if (s.warp != 0) for (auto& row : MeshGrid(s)) for (auto& p : row) add(p);   // interior points may bulge past the quad
}

void MigrateAbsoluteMesh(Slice& s, const std::vector<std::vector<ImVec2>>& abs) {
  s.meshLocal.clear();
  std::vector<float> us, vs; MeshLines(s, us, vs);
  if (abs.size() != vs.size() || abs.empty() || abs[0].size() != us.size()) return;   // old code ignored mismatched grids too
  size_t R = abs.size(), C = abs[0].size();
  // In mesh mode the old renderer drew the grid and ignored q, so its corners ARE what the audience saw: adopt them as
  // the keystone, then express every other vertex relative to it. In corner-pin mode q was what showed; keep it.
  if (s.warp != 0) { s.q[0] = abs[0][0]; s.q[1] = abs[0][C - 1]; s.q[2] = abs[R - 1][C - 1]; s.q[3] = abs[R - 1][0]; }
  Keystone k(s.q);
  std::vector<std::vector<ImVec2>> loc(R, std::vector<ImVec2>(C));
  for (size_t r = 0; r < R; ++r) for (size_t c = 0; c < C; ++c) {
    bool corner = (r == 0 || r == R - 1) && (c == 0 || c == C - 1);
    if (corner) loc[r][c] = ImVec2(us[c], vs[r]);
    else if (!k.Inv(abs[r][c], loc[r][c])) return;   // unreachable point: fall back to an undeformed grid
  }
  s.meshLocal = loc;
}

ImVec2 WarpMap::Map(float canvasX, float canvasY) const {
  if (!slice) return ImVec2(ox + canvasX * sx, oy + canvasY * sy);
  // canvas pixels → the slice's input rect in unit coordinates (undo the rect's rotation about its centre, then mirror)
  float rw = (float)std::max(1, slice->iw), rh = (float)std::max(1, slice->ih);
  float dx = canvasX - (slice->ix + rw * 0.5f), dy = canvasY - (slice->iy + rh * 0.5f);
  if (slice->irot != 0.f) {
    float r = -slice->irot * 3.14159265f / 180.f, co = std::cos(r), si = std::sin(r);
    float x = dx * co - dy * si; dy = dx * si + dy * co; dx = x;
  }
  float u = dx / rw + 0.5f, v = dy / rh + 0.5f;
  if (slice->iflipX) u = 1.f - u;
  if (slice->iflipY) v = 1.f - v;
  ImVec2 o = SliceMapUV(*slice, u, v);
  return ImVec2(ox + o.x * sx, oy + o.y * sy);
}

void App::addScreen() {
  pushHist();
  int n = (int)screens.size() + 1;
  Screen sc; sc.id = uid("screen"); sc.name = "Screen " + std::to_string(n); sc.outDev = "Projector / Display " + std::to_string(n);
  sc.role = 2;
  Slice sl; sl.id = uid("slice"); sl.name = "Slice 1"; QuadOf(sl, 100, 100, 1720, 880);
  sc.slices.push_back(sl);
  selSc = sc.id; selSl = sl.id; selMk.clear(); selKind = -1;
  screens.push_back(sc);
}
void App::addSlice() { pushHist();
  Screen* sc = curScreen(); if (!sc) return;
  Slice sl; sl.id = uid("slice"); sl.name = "Slice " + std::to_string(sc->slices.size() + 1);
  // input rect defaults to the whole canvas (matches Slice's own struct defaults) — a new slice should show
  // everything until the operator deliberately crops it, not an arbitrary pre-cropped corner
  QuadOf(sl, 200, 100, 800, 600);
  selSl = sl.id; selMk.clear(); selKind = -1;
  sc->slices.push_back(sl);
}
void App::addMask() { pushHist();
  Slice* sl = curSlice(); if (!sl) return;
  Mask m; m.id = uid("mask"); m.name = "Mask " + std::to_string(sl->masks.size() + 1); m.inverted = true; m.feather = 4;
  m.pts[0] = {620, 380}; m.pts[1] = {1100, 380}; m.pts[2] = {1020, 720}; m.pts[3] = {700, 720};
  selSl = sl->id; selMk = m.id;
  sl->masks.push_back(m);
}
void App::deleteMask() { pushHist();
  Slice* sl = curSlice(); Mask* mk = curMask(); if (!sl || !mk) return;
  std::string id = mk->id;
  sl->masks.erase(std::remove_if(sl->masks.begin(), sl->masks.end(), [&](const Mask& m) { return m.id == id; }), sl->masks.end());
  selMk.clear(); selKind = -1;
}
void App::deleteSlice() { pushHist();
  Screen* sc = curScreen(); Slice* sl = curSlice();
  if (!sc || !sl || sc->slices.size() <= 1) return;
  std::string id = sl->id;
  sc->slices.erase(std::remove_if(sc->slices.begin(), sc->slices.end(), [&](const Slice& s) { return s.id == id; }), sc->slices.end());
  selSl = sc->slices[0].id; selMk.clear(); selKind = -1;
}
void App::resetWarp() { pushHist(); if (Slice* s = curSlice()) QuadOf(*s, 0, 0, 1920, 1080); }
void App::resetMeshWarp() { pushHist(); if (Slice* s = curSlice()) s->meshLocal.clear(); }
void App::resetAllWarping() {
  pushHist(); if (Slice* s = curSlice()) {
    QuadOf(*s, 0, 0, 1920, 1080);
    s->meshLocal.clear(); s->meshU.clear(); s->meshV.clear();
    s->meshCols = 4; s->meshRows = 3;
  }
}
// Resolume-style explicit match (the old resetWarp behaviour): output quad takes the input rect's shape/position.
// Kept as its own action so "reset" always means "back to fullscreen default", never "copy the crop".
void App::matchOutputToInput() {
  pushHist();
  if (Slice* s = curSlice()) { ImVec2 q[4]; InputAsOutputQuad(*s, q); for (int i = 0; i < 4; ++i) s->q[i] = q[i]; }   // keeps the input's rotation
}
// Resolume calls this "Whole area": the input rect snaps back to covering the entire composition canvas —
// matches a brand-new slice's own default (Slice struct defaults / NewBlankProject), so an operator who cropped
// by mistake gets back to exactly what they started with, not some other arbitrary rectangle.
void App::resetInputRect() {
  pushHist(); if (Slice* s = curSlice()) {
    int cw = canvasW > 0 ? canvasW : 1920, ch = canvasH > 0 ? canvasH : 1080;
    s->ix = 0; s->iy = 0; s->iw = cw; s->ih = ch; s->irot = 0;   // a rotated rect could not cover the whole canvas
  }
}
// ── slice clipboard / stacking order ──
static void FreshIds(Slice& c) { c.id = A.uid("slice"); for (auto& m : c.masks) m.id = A.uid("mask"); }
static int SliceIndex(const Screen& sc, const std::string& id) { for (int i = 0; i < (int)sc.slices.size(); ++i) if (sc.slices[i].id == id) return i; return -1; }
void App::duplicateSlice() {
  Screen* sc = curScreen(); Slice* sl = curSlice(); if (!sc || !sl) return;
  pushHist();
  int at = SliceIndex(*sc, sl->id); Slice c = *sl; FreshIds(c); c.name += " copy";
  sc->slices.insert(sc->slices.begin() + at + 1, c);   // right above the original
  selSl = c.id; selMk.clear(); selKind = 1;
}
void App::copySlice() { if (Slice* sl = curSlice()) { sliceClip = *sl; hasSliceClip = true; } }
void App::cutSlice() { Screen* sc = curScreen(); if (!sc || sc->slices.size() <= 1) return; copySlice(); deleteSlice(); }   // the last slice can't be removed
void App::pasteSlice() {
  Screen* sc = curScreen(); if (!sc || !hasSliceClip) return;
  pushHist();
  Slice c = sliceClip; FreshIds(c);
  for (auto& o : sc->slices) if (o.name == c.name) { c.name += " copy"; break; }
  Slice* cur = curSlice(); int at = cur ? SliceIndex(*sc, cur->id) + 1 : (int)sc->slices.size();
  sc->slices.insert(sc->slices.begin() + std::clamp(at, 0, (int)sc->slices.size()), c);
  selSl = c.id; selMk.clear(); selKind = 1;
}
void App::moveSliceZ(int delta) {
  Screen* sc = curScreen(); Slice* sl = curSlice(); if (!sc || !sl) return;
  int i = SliceIndex(*sc, sl->id), j = i + delta;
  if (i < 0 || j < 0 || j >= (int)sc->slices.size()) return;
  pushHist(); std::swap(sc->slices[i], sc->slices[j]);
}
// A new canvas resolution rescales every slice's input rect (per axis) so it keeps covering the same part of the
// canvas — a slice that took "the whole area" of 1920x1080 still takes the whole area of 3840x2160. Only the INPUT
// side is content-space; output quads/meshes/masks live in the screen's own pixels and are left alone.
void App::setCanvasSize(int w, int h) {
  w = std::clamp(w, 64, 16384); h = std::clamp(h, 64, 16384);
  if (w == canvasW && h == canvasH) return;
  double fx = (double)w / std::max(1, canvasW), fy = (double)h / std::max(1, canvasH);
  for (auto& sc : screens) for (auto& s : sc.slices) {
    int x1 = (int)std::lround((s.ix + s.iw) * fx), y1 = (int)std::lround((s.iy + s.ih) * fy);
    s.ix = (int)std::lround(s.ix * fx); s.iy = (int)std::lround(s.iy * fy);
    s.iw = std::max(20, x1 - s.ix); s.ih = std::max(20, y1 - s.iy);
  }
  canvasW = w; canvasH = h;
}
void App::deleteScreen(const std::string& id) { pushHist();
  if (screens.size() <= 1) return;
  screens.erase(std::remove_if(screens.begin(), screens.end(), [&](const Screen& s) { return s.id == id; }), screens.end());
  if (selSc == id) { selSc = screens[0].id; selSl = screens[0].slices.empty() ? "" : screens[0].slices[0].id; selMk.clear(); selKind = -1; }
}
void App::removeSlice(const std::string& scId, const std::string& slId) {
  for (auto& sc : screens) if (sc.id == scId && sc.slices.size() > 1) {
    sc.slices.erase(std::remove_if(sc.slices.begin(), sc.slices.end(), [&](const Slice& s) { return s.id == slId; }), sc.slices.end());
    if (selSl == slId) { selSc = scId; selSl = sc.slices[0].id; selMk.clear(); selKind = -1; }
  }
}
void App::dupScreen(const std::string& id) { pushHist();
  for (size_t i = 0; i < screens.size(); ++i) if (screens[i].id == id) {
    Screen c = screens[i]; c.id = uid("screen"); c.name += " copy";
    for (auto& s : c.slices) { s.id = uid("slice"); for (auto& m : s.masks) m.id = uid("mask"); }
    selSc = c.id; selSl = c.slices.empty() ? "" : c.slices[0].id; selMk.clear(); selKind = -1;
    screens.insert(screens.begin() + i + 1, c);
    return;
  }
}
void App::dupSlice(const std::string& scId, const std::string& slId) { pushHist();
  for (auto& sc : screens) if (sc.id == scId) for (size_t i = 0; i < sc.slices.size(); ++i) if (sc.slices[i].id == slId) {
    Slice c = sc.slices[i]; c.id = uid("slice"); c.name += " copy";
    for (auto& m : c.masks) m.id = uid("mask");
    selSc = scId; selSl = c.id; selMk.clear(); selKind = -1;
    sc.slices.insert(sc.slices.begin() + i + 1, c);
    return;
  }
}
void App::moveScreen(const std::string& id, int dir) {
  for (int i = 0; i < (int)screens.size(); ++i) if (screens[i].id == id) {
    int j = i + dir; if (j < 0 || j >= (int)screens.size()) return;
    std::swap(screens[i], screens[j]); return;
  }
}
void App::moveSlice(const std::string& scId, const std::string& slId, int dir) {
  for (auto& sc : screens) if (sc.id == scId) for (int i = 0; i < (int)sc.slices.size(); ++i) if (sc.slices[i].id == slId) {
    int j = i + dir; if (j < 0 || j >= (int)sc.slices.size()) return;
    std::swap(sc.slices[i], sc.slices[j]); return;
  }
}

std::vector<MenuItem> App::screenMenu(const std::string& scId) {
  int i = 0; Screen* sc = nullptr;
  for (int k = 0; k < (int)screens.size(); ++k) if (screens[k].id == scId) { i = k; sc = &screens[k]; }
  std::vector<MenuItem> m;
  if (!sc) return m;
  bool vis = sc->visible;
  auto mk = [](const char* l, const char* ic, bool dis, bool danger, std::function<void()> f) { MenuItem x; x.label = l; x.icon = ic; x.disabled = dis; x.danger = danger; x.run = f; return x; };
  m.push_back(mk(vis ? "Hide from output" : "Show on output", vis ? "eye-off" : "eye", false, false, [this, scId] { for (auto& s : screens) if (s.id == scId) s.visible = !s.visible; }));
  m.push_back(mk("Move up", "arrow-up", i <= 0, false, [this, scId] { moveScreen(scId, -1); }));
  m.push_back(mk("Move down", "arrow-down", i >= (int)screens.size() - 1, false, [this, scId] { moveScreen(scId, 1); }));
  m.push_back(mk("Duplicate screen", "copy", false, false, [this, scId] { dupScreen(scId); }));
  m.push_back(mk("Add slice", "plus", false, false, [this, scId] {
    for (auto& s : screens) if (s.id == scId) { selSc = scId; selSl = s.slices.empty() ? "" : s.slices[0].id; selMk.clear(); selKind = -1; }
    addSlice(); }));
  m.push_back(mk("Delete screen", "trash-2", screens.size() <= 1, true, [this, scId] { deleteScreen(scId); }));
  return m;
}
std::vector<MenuItem> App::sliceMenu(const std::string& scId, const std::string& slId) {
  std::vector<MenuItem> m;
  for (auto& sc : screens) if (sc.id == scId) for (int i = 0; i < (int)sc.slices.size(); ++i) if (sc.slices[i].id == slId) {
    bool vis = sc.slices[i].visible; int n = (int)sc.slices.size();
    auto mk = [](const char* l, const char* ic, bool dis, bool danger, std::function<void()> f) { MenuItem x; x.label = l; x.icon = ic; x.disabled = dis; x.danger = danger; x.run = f; return x; };
    auto sel = [this, scId, slId] { selSc = scId; selSl = slId; selMk.clear(); selKind = -1; };
    m.push_back(mk(vis ? "Hide from output" : "Show on output", vis ? "eye-off" : "eye", false, false, [this, scId, slId] {
      for (auto& s : screens) if (s.id == scId) for (auto& l : s.slices) if (l.id == slId) l.visible = !l.visible; }));
    m.push_back(mk(sc.slices[i].solo ? "Unsolo" : "Solo", "target", false, false, [this, scId, slId] {
      for (auto& s : screens) if (s.id == scId) for (auto& l : s.slices) if (l.id == slId) l.solo = !l.solo; }));
    m.push_back(mk("Move up", "arrow-up", i <= 0, false, [this, scId, slId] { moveSlice(scId, slId, -1); }));
    m.push_back(mk("Move down", "arrow-down", i >= n - 1, false, [this, scId, slId] { moveSlice(scId, slId, 1); }));
    m.push_back(mk("Duplicate slice", "copy", false, false, [this, scId, slId] { dupSlice(scId, slId); }));
    m.push_back(mk("Whole area (input)", "maximize", false, false, [this, sel] { sel(); resetInputRect(); }));
    m.push_back(mk("Match output to input", "frame", false, false, [this, sel] { sel(); matchOutputToInput(); }));
    m.push_back(mk("Reset warp", "rotate-ccw", false, false, [this, sel] { sel(); resetWarp(); }));
    m.push_back(mk("Reset mesh warp", "grid-3x3", false, false, [this, sel] { sel(); resetMeshWarp(); }));
    m.push_back(mk("Reset all warping", "rotate-ccw", false, false, [this, sel] { sel(); resetAllWarping(); }));
    m.push_back(mk("Add mask", "scissors", false, false, [this, sel] { sel(); addMask(); }));
    m.push_back(mk("Delete slice", "trash-2", n <= 1, true, [this, scId, slId] { removeSlice(scId, slId); }));
  }
  return m;
}
std::vector<MenuItem> App::maskMenu(const std::string& scId, const std::string& slId, const std::string& mkId) {
  std::vector<MenuItem> m;
  auto mk = [](const char* l, const char* ic, bool danger, std::function<void()> f) { MenuItem x; x.label = l; x.icon = ic; x.danger = danger; x.run = f; return x; };
  bool inv = true;
  for (auto& sc : screens) if (sc.id == scId) for (auto& sl : sc.slices) if (sl.id == slId) for (auto& k : sl.masks) if (k.id == mkId) inv = k.inverted;
  m.push_back(mk(inv ? "Make additive" : "Invert (cut hole)", "scissors", false, [this, scId, slId, mkId] {
    selSc = scId; selSl = slId; selMk = mkId; if (Mask* k = curMask()) k->inverted = !k->inverted; }));
  m.push_back(mk("Duplicate mask", "copy", false, [this, scId, slId, mkId] {
    for (auto& sc : screens) if (sc.id == scId) for (auto& sl : sc.slices) if (sl.id == slId) for (auto k : sl.masks) if (k.id == mkId) {
      k.id = uid("mask"); k.name += " copy"; sl.masks.push_back(k); return; } }));
  m.push_back(mk("Delete mask", "trash-2", true, [this, scId, slId, mkId] {
    for (auto& sc : screens) if (sc.id == scId) for (auto& sl : sc.slices) if (sl.id == slId)
      sl.masks.erase(std::remove_if(sl.masks.begin(), sl.masks.end(), [&](const Mask& k) { return k.id == mkId; }), sl.masks.end()); }));
  return m;
}

// ───────────────────────── tree ─────────────────────────
struct Node {
  enum Kind { ScreenN, SliceN, MaskN } kind;
  std::string sc, sl, mk, name, meta;
  float h, pad;
  bool open = true, canHide = true, vis = true;
  uint32_t color = pal::coral;
};

static void TreeRow(ImRect r, const Node& n, bool railMode) {
  Screen* cs = A.curScreen(); Slice* csl = A.curSlice(); Mask* cmk = A.curMask();
  bool scOn = cs && n.sc == cs->id;
  bool on;
  uint32_t hexOn;
  if (n.kind == Node::ScreenN) { on = scOn; hexOn = n.color; }
  else if (n.kind == Node::SliceN) { on = scOn && csl && n.sl == csl->id && !cmk; hexOn = pal::coral; }
  else { on = cmk && cmk->id == n.mk; hexOn = pal::yellow; }
  Hit h = HitR(r);
  float prev = g.alpha;
  if (!n.vis) g.alpha = 0.5f;
  float mixA = n.kind == Node::ScreenN ? 0.12f : 0.15f, borA = n.kind == Node::ScreenN ? 0.55f : 0.40f;
  if (on) Box(r, K(hexOn, mixA), n.kind == Node::MaskN ? 0 : K(hexOn, borA), 3);
  float cy = (r.Min.y + r.Max.y) * 0.5f;
  float x = r.Min.x + n.pad;
  bool chevHit = false;
  if (n.kind == Node::ScreenN && !railMode) {
    ImRect cr(x, cy - 8, x + 12, cy + 8);
    Icon(n.open ? "chevron-down" : "chevron-right", ImVec2(x + 6, cy), 12, K(pal::t88));
    if (HitR(cr).click) { A.collapsedScreens[n.sc] = n.open; chevHit = true; }
    x += 12 + 6;
  } else if (!railMode) x += 12 + 6;
  const char* ico = n.kind == Node::ScreenN ? "monitor" : n.kind == Node::SliceN ? "maximize" : "scissors";
  float isz = n.kind == Node::MaskN ? 10.f : n.kind == Node::ScreenN ? 12.f : 11.f;
  uint32_t icol = n.kind == Node::ScreenN ? (scOn ? n.color : pal::t88) : n.kind == Node::SliceN ? pal::cyan : pal::yellow;
  Icon(ico, ImVec2(x + isz * 0.5f, cy), isz, K(icol));
  x += isz + 6;
  uint32_t fg = on ? hexOn : n.kind == Node::ScreenN ? pal::tcc : n.kind == Node::SliceN ? pal::t88 : pal::t77;
  float right = r.Max.x - 6;
  bool visHit = false;
  if (n.canHide) {
    ImRect er(right - 11, cy - 8, right, cy + 8);
    Icon(n.vis ? "eye" : "eye-off", ImVec2(right - 5.5f, cy), 11, K(n.vis ? pal::t88 : pal::t66));
    if (HitR(er).click) {
      visHit = true;
      for (auto& s : A.screens) if (s.id == n.sc) {
        if (n.kind == Node::ScreenN) s.visible = !s.visible;
        else for (auto& l : s.slices) if (l.id == n.sl) l.visible = !l.visible;
      }
    }
    right -= 11 + 6;
  }
  if (!n.meta.empty()) { TextR(right, cy, MONO_R, 9, K(pal::t66), n.meta.c_str()); right -= TextW(MONO_R, 9, n.meta.c_str()) + 6; }
  TextEll(x, cy, right - x, n.kind == Node::ScreenN ? UI_B : UI_S, 10, K(fg), n.name.c_str());
  g.alpha = prev;
  if (h.hover) CursorHand();
  if (h.click && !chevHit && !visHit) {
    if (n.kind == Node::ScreenN) {
      Screen* sc = nullptr; for (auto& s : A.screens) if (s.id == n.sc) sc = &s;
      bool keep = false; if (sc) for (auto& sl : sc->slices) if (sl.id == A.selSl) keep = true;
      A.selSc = n.sc; A.selSl = keep ? A.selSl : (sc && !sc->slices.empty() ? sc->slices[0].id : ""); A.selMk.clear(); A.selKind = 0;
    } else { A.selSc = n.sc; A.selSl = n.sl; A.selMk = n.kind == Node::MaskN ? n.mk : ""; A.selKind = n.kind == Node::MaskN ? 2 : 1; }
  }
  if (h.rclick) {
    if (n.kind == Node::ScreenN) A.openCtx(ImGui::GetIO().MousePos, A.screenMenu(n.sc));
    else if (n.kind == Node::SliceN) A.openCtx(ImGui::GetIO().MousePos, A.sliceMenu(n.sc, n.sl));
    else A.openCtx(ImGui::GetIO().MousePos, A.maskMenu(n.sc, n.sl, n.mk));
  }
}

static std::vector<Node> BuildTree(bool onlyScreen, const std::string& only) {
  std::vector<Node> out;
  for (auto& sc : A.screens) {
    if (onlyScreen && sc.id != only) continue;
    bool open = !(A.collapsedScreens.count(sc.id) && A.collapsedScreens[sc.id]);
    if (!onlyScreen) {
      Node n; n.kind = Node::ScreenN; n.sc = sc.id; n.name = sc.name; n.meta = std::to_string(sc.w) + "x" + std::to_string(sc.h);
      n.h = 28; n.pad = 6; n.open = open; n.vis = sc.visible; n.color = RoleHex(sc.role); out.push_back(n);
      if (!open) continue;
    }
    for (auto& sl : sc.slices) {
      Node s; s.kind = Node::SliceN; s.sc = sc.id; s.sl = sl.id; s.name = sl.name; s.meta = sl.solo ? "SOLO" : ""; s.h = onlyScreen ? 24 : 26; s.pad = onlyScreen ? 6 : 16; s.vis = sl.visible; out.push_back(s);
      for (auto& m : sl.masks) {
        Node k; k.kind = Node::MaskN; k.sc = sc.id; k.sl = sl.id; k.mk = m.id; k.name = m.name; k.h = 24; k.pad = onlyScreen ? 20 : 28; k.canHide = false; out.push_back(k);
      }
    }
  }
  return out;
}

static void TreePanel(ImRect r) {
  Fill(r, K(pal::g12));
  VLine(r.Max.x - 1, r.Min.y, r.Max.y, K(pal::g2a));
  PanelHeader(Rc(r.Min.x, r.Min.y, r.GetWidth() - 1, 24), "Mapping tree", pal::t88);
  {
    float cy = r.Min.y + 11.5f, xr = r.Max.x - 1 - 6;
    Icon("panel-left-close", ImVec2(xr - 6, cy), 12, K(pal::t88));
    if (HitR(ImRect(xr - 14, cy - 9, xr + 2, cy + 9)).click) { A.treeCollapsed = true; A.railScreen = A.selSc; }
    xr -= 12 + 8;
    std::string c = std::to_string(A.screens.size()) + " screens";
    TextR(xr, cy, MONO_M, 10, K(pal::t66), c.c_str());
  }
  float footH = 46;
  static ScrollArea sa;
  sa.Begin("##tree", ImRect(r.Min.x, r.Min.y + 24, r.Max.x - 1, r.Max.y - footH));
  float ox = sa.origin.x, oy = sa.origin.y, y = 4;
  auto nodes = BuildTree(false, "");
  for (auto& n : nodes) {
    ImRect rr(ox + 6, oy + y, ox + r.GetWidth() - 1 - 6, oy + y + n.h);
    TreeRow(rr, n, false);
    y += n.h + 2;
  }
  sa.End(r.GetWidth() - 1, y + 4);
  // footer actions
  ImRect fr(r.Min.x, r.Max.y - footH, r.Max.x - 1, r.Max.y);
  Fill(fr, K(pal::g14));
  HLine(fr.Min.x, fr.Max.x, fr.Min.y, K(pal::g2a));
  float bw = (fr.GetWidth() - 12 - 12) / 3.f;
  struct Ac { const char* l; const char* ic; uint32_t fg; bool primary; } acts[3] = {{"Screen", "monitor", pal::coral, true}, {"Slice", "plus", pal::white, false}, {"Mask", "scissors", pal::yellow, false}};
  for (int i = 0; i < 3; ++i) {
    ImRect br(fr.Min.x + 6 + i * (bw + 6), fr.Min.y + 9, fr.Min.x + 6 + i * (bw + 6) + bw, fr.Min.y + 9 + 28);
    Hit h = HitR(br);
    Box(br, acts[i].primary ? K(pal::coral, 0.15f) : h.hover ? K(pal::ctrlHover) : K(pal::g1c), acts[i].primary ? K(pal::coral, 0.4f) : K(pal::g22), 3);
    float tw = TextW(UI_B, 10, acts[i].l), cx = (br.Min.x + br.Max.x) * 0.5f, cy = (br.Min.y + br.Max.y) * 0.5f;
    Icon(acts[i].ic, ImVec2(cx - tw * 0.5f - 8, cy), 11, K(acts[i].fg));
    Text(cx - tw * 0.5f + 4, cy, UI_B, 10, K(acts[i].fg), acts[i].l);
    if (h.hover) CursorHand();
    if (h.click) { if (i == 0) A.addScreen(); else if (i == 1) A.addSlice(); else A.addMask(); }
  }
}

static void RailPanel(ImRect r) {
  Fill(r, K(pal::g12));
  VLine(r.Max.x - 1, r.Min.y, r.Max.y, K(pal::g2a));
  float cx = (r.Min.x + r.Max.x - 1) * 0.5f, y = r.Min.y + 6;
  ImRect eb(cx - 14, y, cx + 14, y + 26);
  Hit eh = HitR(eb);
  Box(eb, K(pal::g1c), K(pal::g22), 3);
  Icon("panel-left-open", ImVec2(cx, y + 13), 13, K(pal::t88));
  if (eh.hover) CursorHand();
  if (eh.click) { A.treeCollapsed = false; A.railScreen.clear(); }
  y += 26 + 6;
  Fill(Rc(cx - 11, y, 22, 1), K(pal::g2a));
  y += 1 + 6;
  Screen* cs = A.curScreen();
  float railTop = 0;
  for (size_t i = 0; i < A.screens.size(); ++i) {
    Screen& sc = A.screens[i];
    ImRect br(cx - 14, y, cx + 14, y + 30);
    Hit h = HitR(br);
    bool on = cs && sc.id == cs->id;
    Box(br, on ? K(pal::coral, 0.18f) : K(pal::g1c), on ? K(pal::coral, 0.55f) : K(pal::g22), 3);
    ImU32 fg = K(on ? pal::coral : pal::t88);
    Icon("monitor", ImVec2(cx, y + 11), 12, fg);
    TextC(cx, y + 23, MONO_B, 8, fg, std::to_string(i + 1).c_str());
    if (A.railScreen == sc.id) railTop = y;
    if (h.hover) CursorHand();
    if (h.click) { A.selSc = sc.id; A.selSl = sc.slices.empty() ? "" : sc.slices[0].id; A.selMk.clear(); A.selKind = 0; A.railScreen = A.railScreen == sc.id ? "" : sc.id; }
    if (h.rclick) A.openCtx(ImGui::GetIO().MousePos, A.screenMenu(sc.id));
    y += 30 + 6;
  }
  (void)railTop;
}

static void RailPopover(ImRect rail) {
  if (A.railScreen.empty()) return;
  int idx = 0; Screen* sc = nullptr;
  for (int i = 0; i < (int)A.screens.size(); ++i) if (A.screens[i].id == A.railScreen) { idx = i; sc = &A.screens[i]; }
  if (!sc) return;
  auto nodes = BuildTree(true, sc->id);
  float listH = std::min(260.f, (float)nodes.size() * 26 + 8);
  float w = 212, h = 24 + listH + 42;
  ImRect r(rail.Max.x + 6, rail.Min.y + 58 + idx * 36, rail.Max.x + 6 + w, rail.Min.y + 58 + idx * 36 + h);
  ImDrawList* dl = g.dl;
  dl->AddRectFilled(ImVec2(r.Min.x - 1, r.Min.y - 1), ImVec2(r.Max.x + 1, r.Max.y + 1), Ca(K(0x000000, 0.4f)), 5);
  Box(r, K(pal::g16), K(pal::g3a), 4);
  ImRect hd(r.Min.x + 1, r.Min.y + 1, r.Max.x - 1, r.Min.y + 25);
  Fill(hd, K(pal::g18)); HLine(hd.Min.x, hd.Max.x, hd.Max.y - 1, K(pal::g2a));
  std::string t = Upper(sc->name);
  TextEll(hd.Min.x + 6, (hd.Min.y + hd.Max.y) * 0.5f, w - 40, UI_B, 9, K(pal::tcc), t.c_str(), 0.09f);
  ImRect xb(hd.Max.x - 22, hd.Min.y, hd.Max.x, hd.Max.y);
  Icon("x", ImVec2(hd.Max.x - 12, (hd.Min.y + hd.Max.y) * 0.5f), 12, K(pal::t66));
  if (HitR(xb).click) A.railScreen.clear();
  float y = r.Min.y + 24 + 4;
  for (auto& n : nodes) {
    ImRect rr(r.Min.x + 5, y, r.Max.x - 5, y + 24);
    TreeRow(rr, n, true);
    y += 26;
  }
  ImRect fr(r.Min.x + 1, r.Max.y - 43, r.Max.x - 1, r.Max.y - 1);
  Fill(fr, K(pal::g14)); HLine(fr.Min.x, fr.Max.x, fr.Min.y, K(pal::g2a));
  for (int i = 0; i < 2; ++i) {
    float bw = (fr.GetWidth() - 18) / 2.f;
    ImRect br(fr.Min.x + 6 + i * (bw + 6), fr.Min.y + 9, fr.Min.x + 6 + i * (bw + 6) + bw, fr.Min.y + 9 + 24);
    Hit h = HitR(br);
    Box(br, h.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
    TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, UI_B, 10, K(i ? pal::yellow : pal::white), i ? "Mask" : "Slice");
    if (h.hover) CursorHand();
    if (h.click) { if (i) A.addMask(); else A.addSlice(); }
  }
}

// ───────────────────────── stage ─────────────────────────
static int dragKind = 0, dragIdx = 0;  // 1 corner, 2 input resize (0..3 corners, 10..13 edge middles), 3 mask point, 4 mesh point (row*100+col), 5 input move, 6 input rotate
static ImVec2 dragOff;                  // grabbed point minus cursor (output px), so grabbing off-centre doesn't jump
static ImVec2 dragAnchor;               // input rect drags: the fixed corner/edge-middle (resize) in canvas px
static float dragAng0 = 0.f, dragRot0 = 0.f;   // input rect rotate: pointer angle and rect rotation when the drag began

// ── stage view ──
// Corner pins and mesh points can sit far outside the 1920x1080 output (-4000..8000), so the stage is a free pan/zoom
// over output space — zoom below 100% to reach them — rather than a scroll clamped to the canvas box.
constexpr float kMinZoom = 0.2f, kMaxZoom = 6.f;
static ImRect StageArea(ImRect r) { return ImRect(r.Min.x, r.Min.y + 44, r.Max.x, r.Max.y); }
// The stage draws ONE space at a time: page 0 (Input selection) is the composition canvas, page 1 (Output routing)
// is the screen's output pixels (1920x1080). Every "1920"/"1080" the stage math used to hard-code is this space now.
static void StageSpace(float& w, float& h) {
  if (A.mpage == 0) { w = (float)std::max(1, A.canvasW); h = (float)std::max(1, A.canvasH); }
  else { w = 1920.f; h = 1080.f; }
}
static float StageBaseW(ImRect area) {   // space width in px at 100%
  float SW, SH; StageSpace(SW, SH);
  float innerW = area.GetWidth() - 20, innerH = area.GetHeight() - 20;
  return std::min(A.mapFocus ? innerW : std::min(innerW, 960.f), innerH * SW / SH);
}
static void Grow(ImVec2& mn, ImVec2& mx, ImVec2 p) { mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); }
// the output box plus every visible slice/mask point on this screen, however far outside the box it sits
static void StageContentBox(const Screen& sc, ImVec2& mn, ImVec2& mx) {
  float SW, SH; StageSpace(SW, SH);
  mn = ImVec2(0, 0); mx = ImVec2(SW, SH);
  if (A.mpage == 0) return;   // the input page only ever shows the canvas box; slice output geometry is a different space
  for (auto& s : sc.slices) {
    if (!s.visible) continue;
    ImVec2 a, b; SliceOutputBounds(s, a, b); Grow(mn, mx, a); Grow(mn, mx, b);
    for (auto& m : s.masks) for (auto& p : m.pts) Grow(mn, mx, p);
  }
}
// zoom + centre so the output-space box [mn,mx] fills `fill` of the stage
static void FrameBox(ImRect area, ImVec2 mn, ImVec2 mx, float fill, float zHi) {
  float bw = StageBaseW(area);
  if (bw <= 0) return;
  float SW, SH; StageSpace(SW, SH);
  float z = std::min((area.GetWidth() - 40) * fill / (std::max(1.f, mx.x - mn.x) * bw / SW),
                     (area.GetHeight() - 40) * fill / (std::max(1.f, mx.y - mn.y) * bw / SW));
  A.setZoom(std::clamp(z, kMinZoom, zHi), (mn.x + mx.x) * 0.5f / SW, (mn.y + mx.y) * 0.5f / SH);
}
static void FitAll(ImRect area, const Screen& sc) {
  ImVec2 mn, mx; StageContentBox(sc, mn, mx);
  float SW, SH; StageSpace(SW, SH);
  if (mn.x >= 0 && mn.y >= 0 && mx.x <= SW && mx.y <= SH) A.setZoom(1, 0.5f, 0.5f);   // all inside: plain 100%
  else FrameBox(area, mn, mx, 1.f, 1.f);
}
static void ZoomToSlice(ImRect area, const Slice& s) { ImVec2 mn, mx; SliceOutputBounds(s, mn, mx); FrameBox(area, mn, mx, 0.85f, kMaxZoom); }

static bool PointInPoly(ImVec2 p, const ImVec2* v, int n) {
  bool in = false;
  for (int i = 0, j = n - 1; i < n; j = i++)
    if (((v[i].y > p.y) != (v[j].y > p.y)) && (p.x < (v[j].x - v[i].x) * (p.y - v[i].y) / (v[j].y - v[i].y) + v[i].x)) in = !in;
  return in;
}

static void FillPoly(const ImVec2* p, int n, ImU32 c) {
  // fan triangulation (quads are convex; masks are user-edited quads)
  for (int i = 1; i + 1 < n; ++i) g.dl->AddTriangleFilled(p[0], p[i], p[i + 1], Ca(c));
}

// Right-click on the input rect: quick placement (centre / mirror / halves / whole), exchange shape with the output quad,
// stacking order, and the slice clipboard — the same list Resolume offers on its input selection.
static void InputRectMenu(ImVec2 at) {
  Screen* sc = A.curScreen(); Slice* sl = A.curSlice(); if (!sc || !sl) return;
  int n = (int)sc->slices.size(), idx = 0; for (int i = 0; i < n; ++i) if (sc->slices[i].id == sl->id) idx = i;
  std::vector<MenuItem> mi;
  auto add = [&](const char* label, std::function<void()> fn, bool divider = false, bool disabled = false) {
    if (divider) { MenuItem d; d.label = ""; d.disabled = true; d.divider = true; mi.push_back(d); }
    MenuItem it; it.label = label; it.disabled = disabled; it.run = std::move(fn); mi.push_back(it);
  };
  auto edit = [](std::function<void(Slice&)> f) { return [f] { A.pushHist(); if (Slice* s = A.curSlice()) f(*s); }; };
  auto setRect = [](Slice& s, int x, int y, int w, int h) { s.ix = x; s.iy = y; s.iw = std::max(20, w); s.ih = std::max(20, h); s.irot = 0; };
  add("Center X", edit([](Slice& s) { s.ix = (A.canvasW - s.iw) / 2; }));
  add("Center Y", edit([](Slice& s) { s.iy = (A.canvasH - s.ih) / 2; }));
  add("Mirror X", edit([](Slice& s) { s.iflipX = !s.iflipX; }));
  add("Mirror Y", edit([](Slice& s) { s.iflipY = !s.iflipY; }));
  add("Left Half", edit([setRect](Slice& s) { setRect(s, 0, 0, A.canvasW / 2, A.canvasH); }), true);
  add("Top Half", edit([setRect](Slice& s) { setRect(s, 0, 0, A.canvasW, A.canvasH / 2); }));
  add("Right Half", edit([setRect](Slice& s) { setRect(s, A.canvasW / 2, 0, A.canvasW - A.canvasW / 2, A.canvasH); }));
  add("Bottom Half", edit([setRect](Slice& s) { setRect(s, 0, A.canvasH / 2, A.canvasW, A.canvasH - A.canvasH / 2); }));
  add("Whole Area", [] { A.resetInputRect(); });
  add("Match Output Shape", edit([](Slice& s) { SetInputFromQuad(s, s.q); }), true);
  add("Swap Input Output Shape", edit([](Slice& s) {
    ImVec2 outQ[4], inQ[4]; for (int i = 0; i < 4; ++i) outQ[i] = s.q[i];
    InputAsOutputQuad(s, inQ);
    for (int i = 0; i < 4; ++i) s.q[i] = inQ[i];
    SetInputFromQuad(s, outQ);
  }));
  add("Bring Forward", [] { A.moveSliceZ(1); }, true, idx >= n - 1);
  add("Send Backwards", [] { A.moveSliceZ(-1); }, false, idx <= 0);
  add("Duplicate", [] { A.duplicateSlice(); }, true);
  add("Copy", [] { A.copySlice(); });
  add("Cut", [] { A.cutSlice(); }, false, n <= 1);
  add("Paste", [] { A.pasteSlice(); }, false, !A.hasSliceClip);
  A.openCtx(at, mi);
}

static void Stage(ImRect r) {
  Fill(r, K(pal::g050));
  Screen* sc = A.curScreen(); Slice* sl = A.curSlice(); Mask* mk = A.curMask();
  ImVec2 m = ImGui::GetIO().MousePos;
  // toolbar
  ImRect tb(r.Min.x, r.Min.y, r.Max.x, r.Min.y + 44);
  Fill(tb, K(pal::g12)); HLine(tb.Min.x, tb.Max.x, tb.Max.y - 1, K(pal::g2a));
  float cy = r.Min.y + 21.5f;
  const char* pages[2] = {"Input selection", "Output routing"};
  uint32_t pcol[2] = {pal::cyan, pal::coral};
  float pw[2]; float tot = 4;
  for (int i = 0; i < 2; ++i) { pw[i] = TextW(UI_B, 10, Upper(pages[i]).c_str(), 0.09f) + 18; tot += pw[i] + (i ? 0 : 2); }
  ImRect grp(tb.Min.x + 6, cy - 14, tb.Min.x + 6 + tot + 2, cy + 14);
  Box(grp, K(0x000000), K(pal::g2a), 4);
  float px = grp.Min.x + 3;
  for (int i = 0; i < 2; ++i) {
    ImRect pr(px, cy - 11, px + pw[i], cy + 11);
    Hit h = HitR(pr);
    bool on = A.mpage == i;
    if (on) Fill(pr, K(pcol[i]), 3);
    TextC((pr.Min.x + pr.Max.x) * 0.5f, cy, UI_B, 10, K(on ? pal::white : pal::t77), Upper(pages[i]).c_str(), 0.09f);
    if (h.hover) CursorHand();
    if (h.click) A.mpage = i;
    px += pw[i] + 2;
  }
  float leftEnd = grp.Max.x;
  if (sl && A.mpage != 0) {
    ImRect wg(grp.Max.x + 4, cy - 14, grp.Max.x + 4 + 4 + 21 * 2 + 2 + 2, cy + 14);
    Box(wg, K(0x000000), K(pal::g2a), 4);
    const char* wi[2] = {"frame", "grid-3x3"};
    for (int i = 0; i < 2; ++i) {
      ImRect br(wg.Min.x + 3 + i * 23, cy - 11, wg.Min.x + 3 + i * 23 + 21, cy + 11);
      bool on = (i == 0) == (sl->warp == 0);
      Hit h = HitR(br);
      Box(br, on ? K(pal::coral, 0.2f) : 0, on ? K(pal::coral, 0.5f) : 0, 3);
      Icon(wi[i], ImVec2((br.Min.x + br.Max.x) * 0.5f, cy), 11, K(on ? pal::coral : pal::t77));
      if (h.hover) CursorHand();
      if (h.click && !on) { A.pushHist(); sl->warp = i; }
    }
    leftEnd = wg.Max.x;
  }
  // right side: zoom group, history/reset group, readout
  float xr = tb.Max.x - 6;
  {
    const char* zi[5] = {"zoom-in", "zoom-out", "scan-search", "maximize", A.mapFocus ? "minimize-2" : "expand"};
    char zl[16]; snprintf(zl, sizeof zl, "%d%%", (int)std::round(A.mapZ * 100));
    float gw = 2 + 5 * 20 + 26 + 4 + 2;
    ImRect zg(xr - gw, cy - 13, xr, cy + 13);
    Box(zg, K(pal::g1c), K(pal::g22), 3);
    float zx = zg.Min.x + 3;
    for (int i = 0; i < 5; ++i) {
      ImRect br(zx, cy - 10, zx + 18, cy + 10);
      Hit h = HitR(br);
      if (h.hover) Fill(br, K(pal::g1c), 2);
      uint32_t hex = i == 2 ? pal::coral : (i == 4 && A.mapFocus) ? pal::coral : h.hover ? pal::white : pal::tcc;
      Icon(zi[i], ImVec2((br.Min.x + br.Max.x) * 0.5f, cy), 11, K(hex));
      if (h.hover) CursorHand();
      if (h.click) {
        if (i == 0) A.setZoom(A.mapZ * 1.4f); else if (i == 1) A.setZoom(A.mapZ / 1.4f);
        else if (i == 2 && sl) ZoomToSlice(StageArea(r), *sl);
        else if (i == 3 && sc) FitAll(StageArea(r), *sc);   // frames points dragged outside the output too
        else if (i == 4) A.mapFocus = !A.mapFocus;
      }
      zx += 20;
    }
    TextC(zx + 11, cy, MONO_B, 9, K(pal::tcc), zl);
    xr = zg.Min.x - 4;
    float hw = 2 + 20 * 2 + 2 + 1 + 2 + 20 + 2 + 2;
    ImRect hg(xr - hw, cy - 13, xr, cy + 13);
    Box(hg, K(pal::g1c), K(pal::g22), 3);
    float hx = hg.Min.x + 3;
    for (int i = 0; i < 2; ++i) {
      ImRect br(hx, cy - 10, hx + 20, cy + 10);
      bool can = i == 0 ? CanUndo() : CanRedo();
      Hit h = HitR(br);
      if (h.hover && can) Fill(br, K(pal::g1c), 2);
      float prev = g.alpha; if (!can) g.alpha *= 0.4f;
      Icon(i == 0 ? "undo-2" : "redo-2", ImVec2((br.Min.x + br.Max.x) * 0.5f, cy), 11, K(can ? (h.hover ? pal::white : pal::tcc) : pal::t66));
      g.alpha = prev;
      if (h.hover && can) CursorHand();
      if (h.click) { if (i == 0) A.undoMap(); else A.redoMap(); }
      hx += 22;
    }
    VLine(hx, cy - 7, cy + 7, K(pal::g2a)); hx += 3;
    ImRect rb(hx, cy - 10, hx + 20, cy + 10);
    Hit rh = HitR(rb);
    if (rh.hover) Fill(rb, K(pal::g1c), 2);
    Icon("rotate-ccw", ImVec2((rb.Min.x + rb.Max.x) * 0.5f, cy), 12, K(rh.hover ? pal::white : pal::t88));
    if (rh.hover) CursorHand();
    if (rh.click) {
      std::vector<MenuItem> mi;
      auto mk = [](const char* l, const char* ic, bool dis, std::function<void()> f) { MenuItem x; x.label = l; x.icon = ic; x.disabled = dis; x.run = f; return x; };
      if (A.mpage == 0) {
        // Input Selection has nothing to do with corner pins/mesh (that's the OUTPUT warp) — offer the one reset
        // that actually applies here instead of showing output actions out of context.
        mi.push_back(mk("Whole area", "maximize", false, [] { A.resetInputRect(); }));
      } else {
        mi.push_back(mk("Reset 4 corner pins", "frame", false, [] { A.resetWarp(); }));
        bool noMesh = !A.curSlice() || A.curSlice()->warp == 0;
        mi.push_back(mk("Match output to input", "frame", false, [] { A.matchOutputToInput(); }));
        mi.push_back(mk("Reset mesh warp", "grid-3x3", noMesh, [] { A.resetMeshWarp(); }));
        mi.push_back(mk("Reset all warping", "rotate-ccw", false, [] { A.resetAllWarping(); }));
      }
      A.openCtx(ImVec2(rb.Min.x - 60, rb.Max.y + 4), mi);
    }
    xr = hg.Min.x - 6;
    std::string ro = A.mpage == 0 ? "Source Content: " + (sl ? SliceSourceName(*sl) + " \xC2\xB7 " : std::string()) + std::to_string(A.canvasW) + "x" + std::to_string(A.canvasH) : (sc ? sc->name + " (" + sc->outDev + ")" : "");
    float lim = leftEnd + 8;
    float rw = std::min(TextW(MONO_R, 10, ro.c_str()), std::max(0.f, xr - lim));
    TextEll(xr - rw, cy, rw, MONO_R, 10, K(pal::t88), ro.c_str());
  }  // canvas viewport
  ImRect area = StageArea(r);
  if (area.GetWidth() - 20 < 60 || !sc) return;
  // View: the canvas centre sits at the stage centre, shifted by a free pan (mapScrollX/Y, px). Nothing clamps the view
  // to the canvas box — points outside it must stay visible and grabbable — only the view centre is kept inside the
  // -4000..8000 range the points themselves are limited to, so you can't pan off into nothing.
  float baseW = StageBaseW(area);
  float SW, SH; StageSpace(SW, SH);   // the space this page draws (canvas px on the input page, 1920x1080 output px on the output page)
  ImVec2 ac((area.Min.x + area.Max.x) * 0.5f, (area.Min.y + area.Max.y) * 0.5f);
  if (A.mapReq.valid) {   // centre the requested output point
    A.mapZ = std::clamp(A.mapReq.z, kMinZoom, kMaxZoom);
    float sn = baseW * A.mapZ / SW;
    A.mapScrollX = (A.mapReq.cx * SW - SW * 0.5f) * sn; A.mapScrollY = (A.mapReq.cy * SH - SH * 0.5f) * sn;
    A.mapReq.valid = false;
  }
  bool inArea = area.Contains(m) && !g.blocked;
  ImGuiIO& io = ImGui::GetIO();
  float s = baseW * A.mapZ / SW;   // px per space px
  if (inArea && !io.KeyAlt && io.MouseWheel != 0.f) A.mapScrollY -= io.MouseWheel * 48.f;
  if (inArea && !io.KeyAlt && io.MouseWheelH != 0.f) A.mapScrollX -= io.MouseWheelH * 48.f;
  if (inArea && io.KeyAlt && io.MouseWheel != 0.f) {   // alt+wheel: zoom about the cursor (the point under it stays put)
    ImVec2 p((m.x - ac.x + A.mapScrollX) / s + SW * 0.5f, (m.y - ac.y + A.mapScrollY) / s + SH * 0.5f);
    A.mapZ = std::clamp(A.mapZ * (io.MouseWheel > 0 ? 1.15f : 1.f / 1.15f), kMinZoom, kMaxZoom);
    s = baseW * A.mapZ / SW;
    A.mapScrollX = (p.x - SW * 0.5f) * s + (ac.x - m.x); A.mapScrollY = (p.y - SH * 0.5f) * s + (ac.y - m.y);
  }
  static bool panning = false; static ImVec2 panM; static float panX, panY;   // right-drag pan
  bool rClick = false;   // right button released without dragging = context menu, not a pan
  if (inArea && ImGui::IsMouseClicked(1) && !panning) { panning = true; panM = m; panX = A.mapScrollX; panY = A.mapScrollY; }
  if (panning) {
    if (ImGui::IsMouseDown(1)) { A.mapScrollX = panX - (m.x - panM.x); A.mapScrollY = panY - (m.y - panM.y); }
    else { rClick = std::hypot(m.x - panM.x, m.y - panM.y) < 4.f; panning = false; }
  }
  // dragging a point to (or past) the stage edge scrolls the view, so the point never slides under the side panels
  if ((dragKind == 1 || dragKind == 3 || dragKind == 4) && ImGui::IsMouseDown(0)) {
    const float edge = 28.f;
    auto push = [&](float v, float lo, float hi) {
      if (v < lo + edge) return -std::min(24.f, (lo + edge - v) * 0.35f);
      if (v > hi - edge) return std::min(24.f, (v - (hi - edge)) * 0.35f);
      return 0.f;
    };
    A.mapScrollX += push(m.x, area.Min.x, area.Max.x); A.mapScrollY += push(m.y, area.Min.y, area.Max.y);
  }
  A.mapScrollX = std::clamp(A.mapScrollX, (-4000.f - SW * 0.5f) * s, (8000.f - SW * 0.5f) * s);
  A.mapScrollY = std::clamp(A.mapScrollY, (-4000.f - SH * 0.5f) * s, (8000.f - SH * 0.5f) * s);
  A.mapCx = (SW * 0.5f + A.mapScrollX / s) / SW; A.mapCy = (SH * 0.5f + A.mapScrollY / s) / SH;
  float cw = SW * s, ch = SH * s;
  ImRect cv(ac.x - cw * 0.5f - A.mapScrollX, ac.y - ch * 0.5f - A.mapScrollY, ac.x + cw * 0.5f - A.mapScrollX, ac.y + ch * 0.5f - A.mapScrollY);
  auto toPx = [&](ImVec2 p) { return ImVec2(cv.Min.x + p.x * s, cv.Min.y + p.y * s); };
  ImVec2 mo((m.x - cv.Min.x) / s, (m.y - cv.Min.y) / s);   // cursor in output px, unclamped
  // The input rect (page 0) and masks pick from canvas CONTENT and stay inside it; corner pins and mesh points are output
  // geometry and reach as far as the numeric corner-pin fields allow (-4000..8000).
  auto inCanvas = [SW, SH](ImVec2 p) { return ImVec2(std::clamp(std::round(p.x), 0.f, SW), std::clamp(std::round(p.y), 0.f, SH)); };
  auto inOutput = [](ImVec2 p) { return ImVec2(std::clamp(std::round(p.x), -4000.f, 8000.f), std::clamp(std::round(p.y), -4000.f, 8000.f)); };
  ImVec2 mu = inCanvas(mo);

  g.dl->PushClipRect(area.Min, area.Max, true);
  Box(cv, K(0x0d0d0d), 0, 3);
  g.dl->PushClipRect(cv.Min, cv.Max, true);
  if (A.mpage == 0) {
    // Input selection shows what the selected slice actually takes: the live content of its source (the whole composition,
    // or the one layer / group it is routed to). With no slice selected there is nothing to route, so show the composition.
    Slice comp;
    DrawSliceSource(sl ? *sl : comp, cv, (float)g.time * 1.2f, 1.f);   // same time base as the Live Output monitor
  }
  g.dl->PopClipRect();   // everything below draws across the whole stage: a point outside the output box stays visible

  // ---- drag processing ----
  if (dragKind && !ImGui::IsMouseDown(0)) dragKind = 0;
  if (dragKind && sl) {
    ImVec2 to = Vadd(mo, dragOff);
    if (dragKind == 4) {   // the mesh lives in keystone space: store where the cursor lands in it
      int rr = dragIdx / 100, cc = dragIdx % 100;
      ImVec2 loc;
      if (Keystone(sl->q).Inv(inOutput(to), loc)) {
        auto lg = LocalGrid(*sl);
        if (rr < (int)lg.size() && cc < (int)lg[rr].size()) { lg[rr][cc] = loc; sl->meshLocal = lg; }
      }
    }
    else if (dragKind == 1) sl->q[dragIdx] = inOutput(to);   // the mesh follows on its own — it is keystone-relative
    else if (dragKind == 3 && mk) mk->pts[dragIdx] = inCanvas(to);
    else if (dragKind == 2) {   // resize in the rect's own (rotated) frame; the opposite corner / edge middle stays put
      float co = std::cos(sl->irot * kDegToRad), si = std::sin(sl->irot * kDegToRad);
      ImVec2 P = sl->irot == 0.f ? mu : ImVec2(std::round(mo.x), std::round(mo.y));   // upright rects stay inside the canvas; rotated ones may reach past it
      float dx = P.x - dragAnchor.x, dy = P.y - dragAnchor.y;
      float lx = dx * co + dy * si, ly = -dx * si + dy * co;   // pointer relative to the anchor, in rect-local axes
      float w = (float)sl->iw, h = (float)sl->ih, hx = 0, hy = 0;   // hx/hy: new centre offset from the anchor, local axes
      int k = dragIdx % 10;
      if (dragIdx >= 10) {
        if (k == 0) { h = std::max(20.f, -ly); hy = -h * 0.5f; }
        else if (k == 1) { w = std::max(20.f, lx); hx = w * 0.5f; }
        else if (k == 2) { h = std::max(20.f, ly); hy = h * 0.5f; }
        else { w = std::max(20.f, -lx); hx = -w * 0.5f; }
      } else {
        float sx = (k == 1 || k == 2) ? 1.f : -1.f, sy = (k == 2 || k == 3) ? 1.f : -1.f;
        w = std::max(20.f, sx * lx); h = std::max(20.f, sy * ly); hx = sx * w * 0.5f; hy = sy * h * 0.5f;
      }
      float cx = dragAnchor.x + hx * co - hy * si, cy = dragAnchor.y + hx * si + hy * co;
      sl->iw = (int)std::lround(w); sl->ih = (int)std::lround(h);
      sl->ix = (int)std::lround(cx - sl->iw * 0.5f); sl->iy = (int)std::lround(cy - sl->ih * 0.5f);
    }
    else if (dragKind == 5) {   // move: an upright rect stays inside the canvas, a rotated one just keeps its centre on it
      float cx = to.x, cy = to.y;
      if (sl->irot == 0.f && sl->iw <= A.canvasW && sl->ih <= A.canvasH) {
        sl->ix = std::clamp((int)std::lround(cx - sl->iw * 0.5f), 0, A.canvasW - sl->iw);
        sl->iy = std::clamp((int)std::lround(cy - sl->ih * 0.5f), 0, A.canvasH - sl->ih);
      } else {
        sl->ix = (int)std::lround(std::clamp(cx, 0.f, (float)A.canvasW) - sl->iw * 0.5f);
        sl->iy = (int)std::lround(std::clamp(cy, 0.f, (float)A.canvasH) - sl->ih * 0.5f);
      }
    }
    else if (dragKind == 6) {   // rotate about the centre; Shift snaps to 15 degrees
      float ccx = sl->ix + sl->iw * 0.5f, ccy = sl->iy + sl->ih * 0.5f;
      float r = dragRot0 + (std::atan2(mo.y - ccy, mo.x - ccx) - dragAng0) / kDegToRad;
      if (ImGui::GetIO().KeyShift) r = std::round(r / 15.f) * 15.f;
      r -= 360.f * std::floor((r + 180.f) / 360.f);   // keep within -180..180
      sl->irot = std::fabs(r) < 0.05f ? 0.f : r;
    }
  }

  bool scVis = sc->visible;
  // anywhere on the stage, not just inside the output box — a handle dragged outside must be grabbable again
  bool overStage = inArea && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  bool clickPending = ImGui::IsMouseClicked(0) && overStage && !dragKind;

  if (A.mpage == 0) {
    // The other slices of this screen show only their input outline (no handles), so it is clear which parts of the source
    // are already taken while another slice is being edited. Drawn first so the selected slice's frame stays on top.
    bool frameTookClick = false;   // the selected slice's own frame (handles / rings / body) has first claim on a click
    if (scVis) for (auto& o : sc->slices) {
      if (!o.visible || (sl && o.id == sl->id)) continue;
      ImVec2 oc[4], opx[4]; InputCorners(o, oc);
      for (int i = 0; i < 4; ++i) opx[i] = toPx(oc[i]);
      DashedPoly(opx, 4, K(pal::cyan, 0.7f), 1.5f, 8, 5);   // dashed and unnamed: only the selected slice carries its name
    }
    if (sl && sl->visible && scVis) {
      // The input rect is edited like the Preview Cue transform frame: drag inside to move, the small squares (corners and
      // edge middles) to resize, the rings around the corners to rotate. Right-click for the quick placement menu.
      ImVec2 cp[4]; InputCorners(*sl, cp);
      ImVec2 cpx[4], mpx[4], mcv[4];
      for (int i = 0; i < 4; ++i) { cpx[i] = toPx(cp[i]); mcv[i] = ImVec2((cp[i].x + cp[(i + 1) % 4].x) * 0.5f, (cp[i].y + cp[(i + 1) % 4].y) * 0.5f); mpx[i] = toPx(mcv[i]); }
      ImVec2 ctrCv(sl->ix + sl->iw * 0.5f, sl->iy + sl->ih * 0.5f), ctrPx = toPx(ctrCv);
      const float kSqR = 8.f, kRingR = 16.f;
      auto dist = [&](ImVec2 a2, ImVec2 b2) { return std::hypot(a2.x - b2.x, a2.y - b2.y); };
      int hot = 0, hotIdx = 0;   // what the pointer is over: 2 resize, 6 rotate, 5 move
      if (inArea && dragKind == 0) {
        for (int i = 0; i < 4 && !hot; ++i) if (dist(m, cpx[i]) <= kSqR) { hot = 2; hotIdx = i; }
        for (int i = 0; i < 4 && !hot; ++i) if (dist(m, mpx[i]) <= kSqR) { hot = 2; hotIdx = 10 + i; }
        for (int i = 0; i < 4 && !hot; ++i) if (dist(m, cpx[i]) <= kRingR) { hot = 6; hotIdx = i; }
        if (!hot && PointInPoly(m, cpx, 4)) hot = 5;
      }
      if (hot) ImGui::SetMouseCursor(hot == 5 ? ImGuiMouseCursor_Hand : ImGuiMouseCursor_ResizeAll);
      if (hot && clickPending) {
        frameTookClick = true;
        A.pushHist(); dragKind = hot; dragIdx = hotIdx;
        if (hot == 2) dragAnchor = hotIdx >= 10 ? mcv[(hotIdx % 10 + 2) % 4] : cp[(hotIdx + 2) % 4];   // opposite corner / edge middle
        else if (hot == 5) dragOff = Vsub(ctrCv, mo);
        else { dragAng0 = std::atan2(mo.y - ctrCv.y, mo.x - ctrCv.x); dragRot0 = sl->irot; }
      }
      if (rClick && PointInPoly(m, cpx, 4)) InputRectMenu(m);
      g.dl->AddConvexPolyFilled(cpx, 4, Ca(K(pal::cyan, 0.05f)));   // faint on purpose: the source thumbnail underneath has to stay readable
      g.dl->AddPolyline(cpx, 4, Ca(K(pal::cyan)), ImDrawFlags_Closed, 2.f);
      char dm[64]; int len = snprintf(dm, sizeof dm, "%d \xC3\x97 %d", sl->iw, sl->ih);
      if (sl->irot != 0.f) len += snprintf(dm + len, sizeof dm - len, " \xC2\xB7 %.0f\xC2\xB0", sl->irot);
      if (sl->iflipX || sl->iflipY) snprintf(dm + len, sizeof dm - len, " \xC2\xB7 flip %s%s", sl->iflipX ? "X" : "", sl->iflipY ? "Y" : "");
      TextC(ctrPx.x, ctrPx.y - 7, MONO_B, 11, K(pal::cyan), sl->name.c_str());
      TextC(ctrPx.x, ctrPx.y + 7, MONO_R, 9, K(pal::tcc), dm);
      const bool rotating = dragKind == 6 || (dragKind == 0 && hot == 6);
      for (int i = 0; i < 4; ++i) {
        bool ringHot = rotating && (dragKind == 6 || hotIdx == i);
        g.dl->AddCircle(cpx[i], 11.f, Ca(K(ringHot ? pal::white : pal::cyan)), 24, 1.5f);
      }
      for (const ImVec2* set : {cpx, mpx}) for (int i = 0; i < 4; ++i) {
        ImRect hr(set[i].x - 5, set[i].y - 5, set[i].x + 5, set[i].y + 5);
        Box(hr, K(pal::white), K(pal::cyan), 2);
        Border(hr, K(pal::cyan), 2, 2);
      }
    }
    // Clicking inside another slice's dashed outline selects it (the topmost one when they overlap).
    if (scVis && inArea && dragKind == 0 && !frameTookClick) {
      for (auto it = sc->slices.rbegin(); it != sc->slices.rend(); ++it) {
        if (!it->visible || (sl && it->id == sl->id)) continue;
        ImVec2 oc[4], opx[4]; InputCorners(*it, oc);
        for (int i = 0; i < 4; ++i) opx[i] = toPx(oc[i]);
        if (!PointInPoly(m, opx, 4)) continue;
        CursorHand();
        if (clickPending) { A.selSc = sc->id; A.selSl = it->id; A.selMk.clear(); A.selKind = 1; }
        break;
      }
    }
  } else if (scVis) {
    bool consumed = false;
    auto polyPx = [&](const ImVec2* q, int n, ImVec2* out) { for (int i = 0; i < n; ++i) out[i] = toPx(q[i]); };
    auto outlinePx = [&](const Slice& S) { std::vector<ImVec2> o = SliceOutline(S); for (auto& p : o) p = toPx(p); return o; };
    // handles keep a fixed on-screen size at any zoom (they used to scale with it and vanish when zoomed out)
    const float kCornerR = 8.f, kMaskR = 7.f, kMeshR = 5.5f, kGrabPad = 4.f;
    auto nearPt = [&](ImVec2 outPt, float rad) { ImVec2 p = toPx(outPt); return std::hypot(m.x - p.x, m.y - p.y) <= rad; };
    auto grab = [&](int kind, int idx, ImVec2 at) { A.pushHist(); dragKind = kind; dragIdx = idx; dragOff = Vsub(at, mo); consumed = true; };
    // zoom-to-zone chip next to the selected slice label
    ImRect zoomBtn; bool haveZoom = false; ImVec2 labelAt; float labelW = 0;
    if (sl && sl->visible) {
      ImVec2 bmn, bmx; SliceOutputBounds(*sl, bmn, bmx);
      ImVec2 c = toPx(ImVec2((sl->q[0].x + sl->q[1].x + sl->q[2].x + sl->q[3].x) * 0.25f, (sl->q[0].y + sl->q[1].y + sl->q[2].y + sl->q[3].y) * 0.25f));
      labelW = std::min(TextW(MONO_B, 11, sl->name.c_str()), std::max(0.f, (bmx.x - bmn.x) * s - 32));   // stay inside the slice
      labelAt = ImVec2(c.x - (labelW + 20) * 0.5f, c.y);
      zoomBtn = ImRect(labelAt.x + labelW + 4, c.y - 8, labelAt.x + labelW + 20, c.y + 8);
      haveZoom = true;
    }
    if (clickPending && haveZoom && zoomBtn.Contains(m)) { ZoomToSlice(area, *sl); consumed = true; }
    if (sl && sl->visible && clickPending && !consumed)
      for (int i = 0; i < 4 && !consumed; ++i) if (nearPt(sl->q[i], kCornerR + kGrabPad)) grab(1, i, sl->q[i]);
    if (mk && clickPending && !consumed)
      for (int i = 0; i < 4 && !consumed; ++i) if (nearPt(mk->pts[i], kMaskR + kGrabPad)) grab(3, i, mk->pts[i]);
    if (clickPending && !consumed && sl && sl->visible && sl->warp != 0) {
      auto gr = MeshGrid(*sl);
      int R = (int)gr.size(), C = (int)gr[0].size();
      for (int rr = 0; rr < R && !consumed; ++rr) for (int cc = 0; cc < C && !consumed; ++cc) {
        bool corner = (rr == 0 || rr == R - 1) && (cc == 0 || cc == C - 1);   // the mesh's corners ARE the keystone corners
        if (!corner && nearPt(gr[rr][cc], kMeshR + kGrabPad)) grab(4, rr * 100 + cc, gr[rr][cc]);
      }
    }
    if (clickPending && !consumed) {
      ImVec2 pp[4];
      for (auto& S : sc->slices) if (S.visible) for (auto& M : S.masks) { polyPx(M.pts, 4, pp); if (!consumed && PointInPoly(m, pp, 4)) { A.selSc = sc->id; A.selSl = S.id; A.selMk = M.id; A.selKind = 2; consumed = true; } }
      for (auto it = sc->slices.rbegin(); it != sc->slices.rend() && !consumed; ++it) if (it->visible) {
        auto ol = outlinePx(*it);
        if (!PointInPoly(m, ol.data(), (int)ol.size())) continue;
        bool wasOn = sl && it->id == sl->id;
        A.selSc = sc->id; A.selSl = it->id; A.selMk.clear(); A.selKind = 1; consumed = true;
        ImVec2 l;   // where the click lands in keystone space = the split position for "+ add col / + add row"
        if (wasOn && it->warp != 0 && Keystone(it->q).Inv(mo, l)) {
          float u = std::clamp(l.x, 0.02f, 0.98f), v = std::clamp(l.y, 0.02f, 0.98f);
          if (A.meshArm) {
            std::vector<float> us, vs; MeshUV(*it, us, vs);
            std::vector<float>& lst = A.meshArm == 'u' ? us : vs; float p = A.meshArm == 'u' ? u : v;
            bool dup = false; for (float x : lst) if (std::fabs(x - p) < 0.01f) dup = true;
            if (!dup) { A.pushHist(); lst.push_back(p); std::sort(lst.begin(), lst.end()); it->meshU = us; it->meshV = vs; it->warp = 1; }
            A.meshArm = 0;
          }
          A.meshPickOn = true; A.meshPickU = u; A.meshPickV = v;
        }
      }
    }
    sl = A.curSlice(); mk = A.curMask();
    for (auto& S : sc->slices) {
      if (!S.visible) continue;
      bool on = sl && S.id == sl->id;
      ImU32 fill = on ? K(pal::coral, 0.22f) : K(0xffffff, 0.05f);
      if (S.warp == 0) { ImVec2 pp[4]; polyPx(S.q, 4, pp); FillPoly(pp, 4, fill); }
      else {   // a warped mesh may be concave overall — fill it cell by cell
        auto gr = MeshGrid(S);
        for (size_t rr = 0; rr + 1 < gr.size(); ++rr) for (size_t cc = 0; cc + 1 < gr[rr].size(); ++cc) {
          ImVec2 cell[4] = {toPx(gr[rr][cc]), toPx(gr[rr][cc + 1]), toPx(gr[rr + 1][cc + 1]), toPx(gr[rr + 1][cc])};
          FillPoly(cell, 4, fill);
        }
      }
      auto ol = outlinePx(S);
      if (on) g.dl->AddPolyline(ol.data(), (int)ol.size(), Ca(K(pal::coral)), ImDrawFlags_Closed, 2.5f);
      else DashedPoly(ol.data(), (int)ol.size(), K(0x444444), 1.5f, 9, 6);
      if (on && S.warp != 0) { ImVec2 pp[4]; polyPx(S.q, 4, pp); DashedPoly(pp, 4, K(pal::coral, 0.45f), 1.f, 5, 5); }   // the keystone frame the warp sits in
    }
    if (sl && sl->visible && sl->warp != 0) {
      auto gr = MeshGrid(*sl);
      int R = (int)gr.size(), C = (int)gr[0].size();
      auto seg = [&](ImVec2 a, ImVec2 b) { ImVec2 p2[2] = {toPx(a), toPx(b)}; DashedPoly(p2, 2, K(pal::coral, 0.55f), 1.5f, 8, 6); };
      for (int rr = 1; rr + 1 < R; ++rr) for (int cc = 0; cc + 1 < C; ++cc) seg(gr[rr][cc], gr[rr][cc + 1]);
      for (int cc = 1; cc + 1 < C; ++cc) for (int rr = 0; rr + 1 < R; ++rr) seg(gr[rr][cc], gr[rr + 1][cc]);
      for (int rr = 0; rr < R; ++rr) for (int cc = 0; cc < C; ++cc) {
        bool corner = (rr == 0 || rr == R - 1) && (cc == 0 || cc == C - 1);
        if (corner) continue;
        bool edge = rr == 0 || rr == R - 1 || cc == 0 || cc == C - 1;
        ImVec2 p = toPx(gr[rr][cc]);
        bool hot = (dragKind == 4 && dragIdx == rr * 100 + cc) || (!dragKind && overStage && nearPt(gr[rr][cc], kMeshR + kGrabPad));
        float rad = kMeshR - (edge ? 0.5f : 0.f) + (hot ? 1.5f : 0.f);
        g.dl->AddCircleFilled(p, rad, Ca(K(edge ? pal::yellow : pal::coral)), 20);
        g.dl->AddCircle(p, rad, Ca(K(hot ? pal::white : 0x050505)), 20, hot ? 1.5f : 2.f);
        if (hot) CursorHand();
      }
      if (A.meshPickOn) {
        ImVec2 mp = toPx(Keystone(sl->q).Fwd(ImVec2(A.meshPickU, A.meshPickV)));
        g.dl->AddCircleFilled(mp, 6.f, Ca(K(pal::yellow)), 20); g.dl->AddCircle(mp, 6.f, Ca(K(0x050505)), 20, 2.f);
      }
    }
    for (auto& S : sc->slices) {
      if (!S.visible) continue;
      for (auto& M : S.masks) {
        bool on = mk && mk->id == M.id;
        ImVec2 pp[4]; polyPx(M.pts, 4, pp);
        FillPoly(pp, 4, on ? K(pal::yellow, 0.30f) : K(pal::red, 0.20f));
        DashedPoly(pp, 4, on ? K(pal::yellow) : K(pal::red), 2.f, 9, 7);
      }
    }
    if (sl && sl->visible && haveZoom) {
      TextEll(labelAt.x, labelAt.y, labelW, MONO_B, 11, K(pal::coral), sl->name.c_str());
      Box(zoomBtn, K(pal::g1c, 0.8f), K(pal::g22), 2);
      Icon("scan-search", ImVec2((zoomBtn.Min.x + zoomBtn.Max.x) * 0.5f, labelAt.y), 10, K(pal::coral));
      if (inArea && zoomBtn.Contains(m)) CursorHand();
    }
    // G13: sensor touches projected through the calibration homography H_s into output space (first screen only —
    // the calibration targets are in that screen's 1920x1080 pixels). Sensor coordinates stay raw until here (principle #5).
    if (!A.touchPts.empty() && sc == &A.screens[0]) {
      float H[9], rms; bool ok = FitHomography(A.calib, H, &rms);
      if (!ok) FitAffine(H, &rms);
      for (auto& t : A.touchPts) {
        float ox, oy; ApplyH(H, t.x, t.y, ox, oy);
        ImVec2 p = toPx(ImVec2(ox, oy));
        float ph = std::fmod((float)g.time * 1.4f, 1.f);
        g.dl->AddCircle(p, (10 + 40 * ph) * s, Ca(K(pal::mint, 0.7f * (1.f - ph))), 32, 2.f);
        g.dl->AddCircleFilled(p, 6 * s + 2, Ca(K(pal::mint)), 20);
        g.dl->AddLine(ImVec2(p.x - 14 * s - 6, p.y), ImVec2(p.x + 14 * s + 6, p.y), Ca(K(pal::mint)), 1.f);
        g.dl->AddLine(ImVec2(p.x, p.y - 14 * s - 6), ImVec2(p.x, p.y + 14 * s + 6), Ca(K(pal::mint)), 1.f);
        char lb[48]; snprintf(lb, sizeof lb, "ID:%d  %d,%d px", t.id, (int)std::round(ox), (int)std::round(oy));
        Text(p.x - TextW(MONO_B, 10, lb) * 0.5f, p.y + 14 * s + 18, MONO_B, 10, K(pal::mint), lb);
      }
    }
    if (mk) for (int i = 0; i < 4; ++i) {
      ImVec2 p = toPx(mk->pts[i]);
      bool hot = (dragKind == 3 && dragIdx == i) || (!dragKind && overStage && nearPt(mk->pts[i], kMaskR + kGrabPad));
      g.dl->AddCircleFilled(p, kMaskR + (hot ? 1.5f : 0.f), Ca(K(pal::yellow)), 24); g.dl->AddCircle(p, kMaskR + (hot ? 1.5f : 0.f), Ca(K(pal::white)), 24, 2.f);
      if (hot) CursorHand();
    }
    if (sl && sl->visible) for (int i = 0; i < 4; ++i) {
      ImVec2 p = toPx(sl->q[i]);
      bool hot = (dragKind == 1 && dragIdx == i) || (!dragKind && overStage && nearPt(sl->q[i], kCornerR + kGrabPad));
      g.dl->AddCircleFilled(p, kCornerR + (hot ? 2.f : 0.f), Ca(K(pal::coral)), 24); g.dl->AddCircle(p, kCornerR + (hot ? 2.f : 0.f), Ca(K(pal::white)), 24, 2.5f);
      if (hot) CursorHand();
    }
    // live readout of the point being dragged — most useful exactly when it is outside the output box
    if (dragKind == 1 || dragKind == 3 || dragKind == 4) {
      ImVec2 at = dragKind == 1 ? sl->q[dragIdx] : dragKind == 3 && mk ? mk->pts[dragIdx] : ImVec2(0, 0);
      if (dragKind == 4) { auto gr = MeshGrid(*sl); int rr = dragIdx / 100, cc = dragIdx % 100; if (rr < (int)gr.size() && cc < (int)gr[rr].size()) at = gr[rr][cc]; }
      char rd[40]; snprintf(rd, sizeof rd, "%d, %d", (int)std::round(at.x), (int)std::round(at.y));
      float tw = TextW(MONO_B, 10, rd);
      ImRect tb2(m.x + 14, m.y + 12, m.x + 14 + tw + 12, m.y + 12 + 18);
      Box(tb2, K(0x000000, 0.8f), K(pal::coral, 0.6f), 3);
      Text(tb2.Min.x + 6, (tb2.Min.y + tb2.Max.y) * 0.5f, MONO_B, 10, K(pal::white), rd);
    }
  }
  Border(cv, K(pal::g2a), 3);
  // position indicators: a thumb per axis whenever part of the content (output box or any point) is outside the view
  {
    ImVec2 cmn(0, 0), cmx(1920, 1080);
    if (A.mpage != 0) StageContentBox(*sc, cmn, cmx);
    ImVec2 v0((area.Min.x - cv.Min.x) / s, (area.Min.y - cv.Min.y) / s), v1((area.Max.x - cv.Min.x) / s, (area.Max.y - cv.Min.y) / s);
    auto thumb = [&](float c0, float c1, float a0, float a1, bool horiz) {
      if (a0 <= c0 && a1 >= c1) return;
      float t0 = std::min(c0, a0), t1 = std::max(c1, a1), len = (horiz ? area.GetWidth() : area.GetHeight()) - 8;
      float p0 = (a0 - t0) / (t1 - t0) * len, p1 = std::max(p0 + 24.f, (a1 - t0) / (t1 - t0) * len);
      if (horiz) g.dl->AddRectFilled(ImVec2(area.Min.x + 4 + p0, area.Max.y - 9), ImVec2(area.Min.x + 4 + p1, area.Max.y - 1), Ca(K(pal::g33)), 8);
      else g.dl->AddRectFilled(ImVec2(area.Max.x - 9, area.Min.y + 4 + p0), ImVec2(area.Max.x - 1, area.Min.y + 4 + p1), Ca(K(pal::g33)), 8);
    };
    thumb(cmn.x, cmx.x, v0.x, v1.x, true);
    thumb(cmn.y, cmx.y, v0.y, v1.y, false);
  }
  g.dl->PopClipRect();
}
// ───────────────────────── properties ─────────────────────────
static void Label(float x, float y, const char* t, uint32_t hex = pal::t88) {
  Text(x, y + 4.5f, MONO_R, 9, K(hex), Upper(t).c_str(), 0.09f);
}

static void PropsPanel(ImRect r) {
  Fill(r, K(pal::g12));
  VLine(r.Min.x, r.Min.y, r.Max.y, K(pal::g2a));
  Screen* sc = A.curScreen(); Slice* sl = A.curSlice(); Mask* mk = A.curMask();
  int kind = A.MapKind();
  if (kind == 2 && !mk) kind = 1;
  if (kind == 1 && !sl) kind = 0;
  PanelHeader(Rc(r.Min.x + 1, r.Min.y, r.GetWidth() - 1, 24), kind == 0 ? "Screen properties" : kind == 2 ? "Mask properties" : "Slice properties", pal::t88);
  Badge(r.Max.x - 6, r.Min.y + 11.5f, kind == 0 ? "Screen" : kind == 2 ? "Mask" : "Slice", kind == 0 ? T_AUDIO : kind == 2 ? T_STANDBY : T_LIVE, true);
  static ScrollArea sa;
  sa.Begin("##mapprops", ImRect(r.Min.x + 1, r.Min.y + 24, r.Max.x, r.Max.y));
  float ox = sa.origin.x + 0, oy = sa.origin.y, W = r.GetWidth() - 1 - 8, x = ox + 8, w = W - 16, y = 8;
  bool output = A.mpage != 0;
  if (kind == 1) {
    Label(x, oy + y, "Slice name"); y += 9 + 4;
    TextField("##slicename", Rc(x, oy + y, w, 28), sl->name);
    y += 28 + 8;
    // F22: what this slice shows — the whole composition, or a single layer / group routed to it
    {
      Label(x, oy + y, "Input source"); y += 9 + 4;
      bool ok = SliceSourceValid(*sl);
      std::string cur = ok ? SliceSourceName(*sl) : std::string("Missing \xC2\xB7 showing Composition");
      ImRect sr(x, oy + y, x + w, oy + y + 28);
      Hit h = HitR(sr);
      Box(sr, h.hover ? K(pal::ctrlHover) : K(pal::g1c), ok ? K(pal::g22) : K(pal::red, 0.7f), 3);
      Icon(sl->srcKind == Slice::SrcLayer ? "layers" : sl->srcKind == Slice::SrcGroup ? "folder" : "monitor", ImVec2(sr.Min.x + 14, (sr.Min.y + sr.Max.y) * 0.5f), 11, K(ok ? pal::cyan : pal::red));
      TextEll(sr.Min.x + 28, (sr.Min.y + sr.Max.y) * 0.5f, w - 50, UI_S, 10, K(ok ? pal::tf3 : pal::red), cur.c_str());
      Icon("chevron-down", ImVec2(sr.Max.x - 12, (sr.Min.y + sr.Max.y) * 0.5f), 10, K(pal::t66));
      if (h.hover) CursorHand();
      if (h.click) {
        std::string slId = sl->id, scId = sc ? sc->id : std::string();
        auto set = [scId, slId](int kind, std::string ref) {
          for (auto& S : A.screens) if (S.id == scId) for (auto& L : S.slices) if (L.id == slId) { A.pushHist(); L.srcKind = kind; L.srcRef = std::move(ref); }
        };
        auto item = [](std::string label, const char* icon, bool on, std::function<void()> f) { MenuItem m; m.label = std::move(label); m.icon = icon; m.toneHex = on ? pal::cyan : 0; m.run = std::move(f); return m; };
        std::vector<MenuItem> mi;
        mi.push_back(item("Composition", "monitor", sl->srcKind == Slice::SrcComp, [set] { set(Slice::SrcComp, ""); }));
        for (auto& L : A.layers) mi.push_back(item("Layer \xC2\xB7 " + L.name, "layers", sl->srcKind == Slice::SrcLayer && sl->srcRef == L.id, [set, id = L.id] { set(Slice::SrcLayer, id); }));
        for (auto& G : A.groups) mi.push_back(item("Group \xC2\xB7 " + G.name, "folder", sl->srcKind == Slice::SrcGroup && sl->srcRef == G.id, [set, id = G.id] { set(Slice::SrcGroup, id); }));
        A.openCtx(ImVec2(sr.Min.x, sr.Max.y + 4), mi);
      }
      y += 28 + 8;
    }
    if (sl->warp != 0) {
      HLine(ox + 8, ox + 8 + w, oy + y, K(pal::g2a)); y += 1 + 6;
      std::vector<float> uu, vv; MeshUV(*sl, uu, vv);
      int ncol = (int)uu.size() + 1, nrow = (int)vv.size() + 1;
      bool custom = !sl->meshU.empty() || !sl->meshV.empty();
      Text(x, oy + y + 4.5f, MONO_R, 9, K(pal::t88), "OUTPUT MESH GRID", 0.09f);
      char ml[64]; snprintf(ml, sizeof ml, "%d \xC3\x97 %d patches%s", ncol, nrow, custom ? " \xC2\xB7 custom" : "");
      TextR(x + w, oy + y + 4.5f, MONO_B, 9, K(pal::coral), ml);
      y += 9 + 4;
      float fw = (w - 12) / 2.f;
      for (int i = 0; i < 2; ++i) {
        float fx = x + i * (fw + 12);
        Text(fx, oy + y + 4.5f, MONO_R, 9, K(pal::t66), i ? "ROWS" : "COLUMNS", 0.09f);
        int n = i ? nrow : ncol;
        ImRect mb(fx, oy + y + 11, fx + 22, oy + y + 11 + 26), pb(fx + fw - 22, oy + y + 11, fx + fw, oy + y + 11 + 26);
        ImRect vb(fx + 24, oy + y + 11, fx + fw - 24, oy + y + 11 + 26);
        int nv = n; bool chg = false;
        Hit hm = HitR(mb), hp = HitR(pb);
        Box(mb, K(pal::g1c), hm.hover ? K(pal::coral) : K(pal::g22), 3); TextC((mb.Min.x + mb.Max.x) * 0.5f, (mb.Min.y + mb.Max.y) * 0.5f, MONO_B, 11, K(hm.hover ? pal::coral : pal::tcc), "\xE2\x88\x92");
        Box(pb, K(pal::g1c), hp.hover ? K(pal::coral) : K(pal::g22), 3); TextC((pb.Min.x + pb.Max.x) * 0.5f, (pb.Min.y + pb.Max.y) * 0.5f, MONO_B, 11, K(hp.hover ? pal::coral : pal::tcc), "+");
        Box(vb, K(pal::g050), K(pal::g22), 3);
        char nb[8]; snprintf(nb, sizeof nb, "%d", n);
        TextC((vb.Min.x + vb.Max.x) * 0.5f, (vb.Min.y + vb.Max.y) * 0.5f, MONO_B, 11, K(pal::tf3), nb);
        if (hm.hover || hp.hover) CursorHand();
        if (hm.click) { nv = n - 1; chg = true; } if (hp.click) { nv = n + 1; chg = true; }
        if (chg) { A.pushHist(); nv = std::clamp(nv, 2, 16); if (i) { sl->meshRows = nv; sl->meshV.clear(); } else { sl->meshCols = nv; sl->meshU.clear(); } sl->meshLocal.clear(); }
      }
      y += 11 + 26 + 6;
      {
        const char* pl = A.meshArm ? (A.meshArm == 'u' ? "CLICK CANVAS TO PLACE COLUMN" : "CLICK CANVAS TO PLACE ROW") : A.meshPickOn ? "" : "PICK ADD COL / ADD ROW FIRST";
        char pk[64]; if (!A.meshArm && A.meshPickOn) snprintf(pk, sizeof pk, "LAST POINT \xC2\xB7 U %d%% \xC2\xB7 V %d%%", (int)std::round(A.meshPickU * 100), (int)std::round(A.meshPickV * 100)); else snprintf(pk, sizeof pk, "%s", pl);
        uint32_t ph = A.meshArm ? pal::yellow : A.meshPickOn ? pal::coral : pal::t66;
        float bw1 = TextW(MONO_B, 9, "FLATTEN", 0.09f) + 12 + 2, bw2 = TextW(MONO_B, 9, "UNIFORM", 0.09f) + 12 + 2;
        ImRect ub(x + w - bw2, oy + y, x + w, oy + y + 20), fb(x + w - bw2 - 4 - bw1, oy + y, x + w - bw2 - 4, oy + y + 20);
        TextEll(x, oy + y + 10, fb.Min.x - 4 - x, MONO_R, 9, K(ph), pk, 0.09f);
        Hit hf = HitR(fb), hu = HitR(ub);
        Box(fb, K(pal::g1c), hf.hover ? K(pal::coral) : K(pal::g22), 3); TextC((fb.Min.x + fb.Max.x) * 0.5f, oy + y + 10, MONO_B, 9, K(hf.hover ? pal::coral : pal::t88), "FLATTEN", 0.09f);
        Box(ub, K(pal::g1c), hu.hover ? K(pal::yellow) : K(pal::g22), 3); TextC((ub.Min.x + ub.Max.x) * 0.5f, oy + y + 10, MONO_B, 9, K(hu.hover ? pal::yellow : pal::t88), "UNIFORM", 0.09f);
        if (hf.hover || hu.hover) CursorHand();
        if (hf.click) { A.pushHist(); sl->meshLocal.clear(); }
        if (hu.click) { A.pushHist(); sl->meshU.clear(); sl->meshV.clear(); sl->meshLocal.clear(); }
        y += 20 + 6;
      }
      {
        float bw = (w - 6) / 2.f;
        for (int i = 0; i < 2; ++i) {
          ImRect br(x + i * (bw + 6), oy + y, x + i * (bw + 6) + bw, oy + y + 24);
          bool armed = A.meshArm == (i ? 'v' : 'u');
          Hit h = HitR(br);
          if (h.hover) Glow(br, pal::coral, 0.3f, 10, 3);
          Box(br, armed ? K(pal::yellow, 0.22f) : K(pal::coral, 0.15f), armed ? K(pal::yellow) : K(pal::coral, 0.45f), 3);
          TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, MONO_B, 9, K(armed ? pal::yellow : pal::coral), i ? "+ ADD ROW" : "+ ADD COL", 0.09f);
          if (h.hover) CursorHand();
          if (h.click) { char want = i ? 'v' : 'u'; A.meshArm = A.meshArm == want ? 0 : want; }
        }
        y += 24 + 8;
      }
    }
    if (output) {
      HLine(ox + 8, ox + 8 + w, oy + y, K(pal::g2a)); y += 1 + 6;
      Label(x, oy + y, "Corner pins"); y += 9 + 4;
      const char* ck[4] = {"TL", "TR", "BR", "BL"};
      for (int i = 0; i < 4; ++i) {
        ImRect cr(x, oy + y, x + w, oy + y + 24);
        Box(cr, K(pal::g18), K(pal::g2a), 3);
        Text(cr.Min.x + 8, cr.Min.y + 12, MONO_B, 10, K(pal::coral), ck[i]);
        // F15: type exact coordinates (output-space px); dragging on the stage still works and stays in sync
        float fw = (cr.GetWidth() - 34 - 6) / 2.f;
        int vx = (int)std::round(sl->q[i].x), vy = (int)std::round(sl->q[i].y);
        char idx[24], idy[24]; snprintf(idx, sizeof idx, "##cpx%d", i); snprintf(idy, sizeof idy, "##cpy%d", i);
        if (IntField(idx, Rc(cr.Min.x + 30, cr.Min.y, fw, 24), vx)) sl->q[i].x = (float)std::clamp(vx, -4000, 8000);
        if (IntField(idy, Rc(cr.Min.x + 30 + fw + 6, cr.Min.y, fw, 24), vy)) sl->q[i].y = (float)std::clamp(vy, -4000, 8000);
        y += 24 + 4;
      }
      y += 4;
    } else {
      HLine(ox + 8, ox + 8 + w, oy + y, K(pal::g2a)); y += 1 + 6;
      Label(x, oy + y, "Input rectangle (px)"); y += 9 + 4;
      const char* fl[4] = {"X", "Y", "Width", "Height"};
      int* vals[4] = {&sl->ix, &sl->iy, &sl->iw, &sl->ih};
      float fw = (w - 6) / 2.f;
      for (int i = 0; i < 4; ++i) {
        float fx = x + (i % 2) * (fw + 6), fy = y + (i / 2) * (35 + 6);
        Text(fx, oy + fy + 4.5f, MONO_R, 9, K(pal::t66), fl[i]);
        char id[32]; snprintf(id, sizeof id, "##rect%d", i);
        IntField(id, Rc(fx, oy + fy + 11, fw, 24), *vals[i]);
      }
      y += 35 * 2 + 6 + 8;
      // rotation about the rect's centre — the same value the rings on the stage edit (double-click resets)
      Text(x, oy + y + 5, UI_S, 10, K(pal::t88), "Rotation");
      char rb[16]; snprintf(rb, sizeof rb, "%.0f\xC2\xB0", sl->irot);
      Hit rh = HitR(ImRect(x, oy + y, x + w, oy + y + 10));
      TextR(x + w, oy + y + 5, MONO_B, 10, K(pal::cyan), rb);
      y += 10 + 6;
      float rv = sl->irot;
      if (Slider(0x3010, Rc(x, oy + y + 4, w, 6), rv, pal::cyan, -180.f, 180.f)) sl->irot = std::fabs(rv) < 0.5f ? 0.f : std::round(rv);
      if (rh.dbl) sl->irot = 0.f;
      y += 14 + 8;
    }
  }
  if (kind == 2) {
    Label(x, oy + y, "Mask name"); y += 9 + 6;
    TextField("##maskname", Rc(x, oy + y, w, 28), mk->name);
    y += 28 + 6;
    HLine(x, x + w, oy + y, K(pal::g2a)); y += 1 + 4;
    ImRect ir(x, oy + y, x + w, oy + y + 24);
    Hit ih = HitR(ir);
    Text(x, ir.Min.y + 12, UI_S, 10, K(pal::te0), "Invert mask (cut hole)");
    ImRect cb(ir.Max.x - 14, ir.Min.y + 5, ir.Max.x, ir.Min.y + 19);
    Box(cb, mk->inverted ? K(pal::yellow) : K(pal::g050), mk->inverted ? K(pal::yellow) : K(pal::g22), 2);
    if (mk->inverted) Check(ImVec2((cb.Min.x + cb.Max.x) * 0.5f, (cb.Min.y + cb.Max.y) * 0.5f), 12, K(0x0f0f0f));
    if (ih.hover) CursorHand();
    if (ih.click) mk->inverted = !mk->inverted;
    y += 24 + 6;
    Text(x, oy + y + 5, UI_S, 10, K(pal::t88), "Feather");
    char fb[16]; snprintf(fb, sizeof fb, "%dpx", mk->feather);
    TextR(x + w, oy + y + 5, MONO_B, 10, K(pal::yellow), fb);
    y += 10 + 6;
    float fv = mk->feather / 40.f * 100.f;
    if (Slider(0x3001, Rc(x, oy + y + 4, w, 6), fv, pal::yellow)) mk->feather = (int)std::round(fv / 100.f * 40.f);
    y += 14 + 6;
    if (output) {
      HLine(x, x + w, oy + y, K(pal::g2a)); y += 1 + 4;
      Label(x, oy + y, "Mask points (output)"); y += 9 + 4;
      for (int i = 0; i < 4; ++i) {
        ImRect cr(x, oy + y, x + w, oy + y + 22);
        Box(cr, K(pal::g18), K(pal::g2a), 3);
        char k[8]; snprintf(k, sizeof k, "P%d", i + 1);
        Text(cr.Min.x + 8, cr.Min.y + 11, MONO_B, 10, K(pal::yellow), k);
        char v[64]; snprintf(v, sizeof v, "X: %d  |  Y: %d", (int)mk->pts[i].x, (int)mk->pts[i].y);
        TextR(cr.Max.x - 8, cr.Min.y + 11, MONO_R, 10, K(pal::tcc), v);
        y += 22 + 6;
      }
    } else {
      HLine(x, x + w, oy + y, K(pal::g2a)); y += 1 + 4;
      for (auto& ln : std::vector<std::string>{"Mask geometry is edited in Output mode. Switch the", "stage to Output to move mask points."}) {
        Text(x, oy + y + 8, UI_S, 10, K(pal::t66), ln.c_str(), 0.01f); y += 16;
      }
      y += 2;
    }
    ImRect dr(x, oy + y, x + w, oy + y + 28);
    Hit dh = HitR(dr);
    Box(dr, K(pal::red, 0.14f), K(pal::red, 0.5f), 3);
    TextC((dr.Min.x + dr.Max.x) * 0.5f, dr.Min.y + 14, UI_B, 10, K(pal::red), "Delete mask");
    if (dh.hover) CursorHand();
    if (dh.click) A.deleteMask();
    y += 28 + 8;
    HLine(ox, ox + W, oy + y - 1, K(pal::g2a));
  }
  if (kind == 0 && sc) {
    HLine(ox, ox + W, oy + y, K(pal::g2a));
    y += 1 + 8;
    Text(x, oy + y + 5, MONO_R, 9, K(pal::t66), "OUTPUT TARGET", 0.09f);
    TextR(x + w, oy + y + 5, MONO_R, 9, K(pal::mint), sc->name.c_str());
    y += 10 + 6;
    float bx = y;
    ImRect box(x, oy + bx, x + w, oy + bx + 6 + 9 + 2 + 26 + 6 + 9 + 2 + 26 + 6 + 9 + 6 + 4 + 22 + 6);
    Box(box, K(pal::g18), K(pal::g2a), 3);
    float ix = x + 6, iw = w - 12, yy = bx + 6;
    Text(ix, oy + yy + 4.5f, MONO_R, 9, K(pal::t66), "SCREEN NAME", 0.09f); yy += 9 + 2;
    TextField("##scname", Rc(ix, oy + yy, iw, 26), sc->name); yy += 26 + 6;
    Text(ix, oy + yy + 4.5f, MONO_R, 9, K(pal::t66), "OUTPUT DEVICE", 0.09f); yy += 9 + 2;
    // F2/I1: the physical display this screen is sent to. This one dropdown IS the projector output choice — it
    // used to be a free-text field here plus a separate click-to-cycle "PROJECTOR OUTPUT" box below, two controls
    // for the same thing that could disagree with each other.
    {
      ImRect dr2(ix, oy + yy, ix + iw, oy + yy + 26);
      Hit dh2 = HitR(dr2);
      bool haveMon = MonitorCount() > 0;
      std::string cur = haveMon ? MonitorName(A.outMonitor) : sc->outDev;
      Box(dr2, dh2.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
      Icon("monitor", ImVec2(dr2.Min.x + 14, (dr2.Min.y + dr2.Max.y) * 0.5f), 11, K(pal::cyan));
      TextEll(dr2.Min.x + 28, (dr2.Min.y + dr2.Max.y) * 0.5f, iw - 50, UI_S, 10, K(pal::tf3), cur.c_str());
      Icon("chevron-down", ImVec2(dr2.Max.x - 12, (dr2.Min.y + dr2.Max.y) * 0.5f), 10, K(pal::t66));
      if (dh2.hover) CursorHand();
      if (dh2.click) {
        std::string scId = sc->id;
        std::vector<MenuItem> mi;
        for (int i = 0; i < MonitorCount(); ++i) {
          MenuItem it; it.label = MonitorName(i); it.icon = "monitor"; it.toneHex = (i == A.outMonitor) ? pal::cyan : 0;
          it.run = [i, scId] {
            A.pushHist();
            for (auto& S : A.screens) if (S.id == scId) S.outDev = MonitorName(i);
            A.outMonitor = i;
            if (OutputOpen()) OpenOutput(glfwWin(), A.outMonitor);
          };
          mi.push_back(it);
        }
        if (mi.empty()) { MenuItem it; it.label = "No display detected"; it.disabled = true; mi.push_back(it); }
        A.openCtx(ImVec2(dr2.Min.x, dr2.Max.y + 4), mi);
      }
      yy += 26 + 6;
    }
    char rl[64]; snprintf(rl, sizeof rl, "RES: %d x %d @ %dHz", sc->w, sc->h, sc->fps);
    Text(ix, oy + yy + 4.5f, MONO_R, 9, K(pal::tcc), rl); yy += 9 + 6;
    HLine(ix, ix + iw, oy + yy, K(pal::g2a)); yy += 1 + 4;
    ImRect er(ix, oy + yy, ix + iw, oy + yy + 22);
    Hit eh = HitR(er);
    Text(ix, er.Min.y + 11, UI_S, 10, K(pal::t88), "Edge blending");
    const char* el = sc->edgeBlend ? "ENABLED" : "DISABLED";
    float bw2 = TextW(MONO_B, 9, el, 0.09f) + 12 + 2;
    ImRect bb(er.Max.x - bw2, er.Min.y + 2, er.Max.x, er.Min.y + 20);
    Box(bb, sc->edgeBlend ? K(pal::mint, 0.2f) : K(pal::g1c), sc->edgeBlend ? K(pal::mint, 0.4f) : K(pal::g22), 3);
    Text(bb.Min.x + 7, (bb.Min.y + bb.Max.y) * 0.5f, MONO_B, 9, K(sc->edgeBlend ? pal::mint : pal::t66), el, 0.09f);
    if (eh.hover) CursorHand();
    if (eh.click) sc->edgeBlend = !sc->edgeBlend;
    y = bx + box.GetHeight() + 8;
    // ── F2/I1: open / close the output window on the display chosen in OUTPUT DEVICE above ──
    HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 8;
    {
      ImRect ob(x, oy + y, x + w, oy + y + 30);
      Hit oh = HitR(ob);
      bool on = OutputOpen();
      if (on) Glow(ob, pal::coral, 0.35f, 12, 3);
      Box(ob, on ? K(pal::coral, 0.2f) : K(pal::g1c), on ? K(pal::coral) : K(pal::g22), 3);
      const char* lb = on ? "\xC4\x90\xC3\x93NG OUTPUT (F11)" : "M\xE1\xBB\x9E OUTPUT (F11)";
      TextC((ob.Min.x + ob.Max.x) * 0.5f, (ob.Min.y + ob.Max.y) * 0.5f, UI_B, 10, K(on ? pal::coral : pal::tcc), lb, 0.09f);
      if (oh.hover) CursorHand();
      if (oh.click) ToggleOutput(glfwWin(), A.outMonitor);
      y += 30 + 8;
    }
    // F8: save/load this screen (device/resolution/slices/masks) as its own file, independent of the project.
    HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 8;
    Text(x, oy + y + 5, MONO_R, 9, K(pal::t88), "OUTPUT PRESET", 0.09f);
    y += 9 + 4;
    {
      float gap = 6, bw = (w - gap) * 0.5f;
      ImRect sb(x, oy + y, x + bw, oy + y + 26), lb(x + bw + gap, oy + y, x + w, oy + y + 26);
      Hit sh = HitR(sb), lh = HitR(lb);
      Box(sb, K(pal::g1c), K(pal::g22), 3);
      Icon("save", ImVec2(sb.Min.x + 12, (sb.Min.y + sb.Max.y) * 0.5f), 12, K(pal::t88));
      Text(sb.Min.x + 24, (sb.Min.y + sb.Max.y) * 0.5f, UI_S, 10, K(pal::t88), "Save preset");
      if (sh.hover) CursorHand();
      if (sh.click) A.notify(DoSaveOutputPreset(*sc));
      Box(lb, K(pal::g1c), K(pal::g22), 3);
      Icon("folder-open", ImVec2(lb.Min.x + 12, (lb.Min.y + lb.Max.y) * 0.5f), 12, K(pal::t88));
      Text(lb.Min.x + 24, (lb.Min.y + lb.Max.y) * 0.5f, UI_S, 10, K(pal::t88), "Load preset");
      if (lh.hover) CursorHand();
      if (lh.click) {
        std::vector<MenuItem> mi;
        auto presets = ListPresets();
        if (presets.empty()) { MenuItem it; it.label = "No saved presets"; it.disabled = true; mi.push_back(it); }
        for (auto& pf : presets) {
          MenuItem it; it.label = pf.name; it.icon = "folder-open";
          it.run = [path = pf.path, name = pf.name, scId = sc->id] {
            Screen loaded; std::string err;
            if (!LoadOutputPreset(path, loaded, err)) { A.notify("Load failed: " + err); return; }
            for (auto& s : A.screens) if (s.id == scId) {
              loaded.id = s.id;   // keep this screen's identity — selSc/selSl and any refs to it must stay valid
              s = loaded;
              A.selSl = s.slices.empty() ? "" : s.slices[0].id; A.selMk.clear(); A.selKind = -1;
            }
            A.notify("Loaded preset: " + name);
          };
          mi.push_back(it);
        }
        A.openCtx(ImVec2(lb.Min.x, lb.Max.y + 4), mi);
      }
      y += 26 + 8;
    }
  }
  sa.End(W, y + 8);
}

void DrawMapping(ImRect body) {
  float x0 = body.Min.x, x1 = body.Max.x;
  Fill(body, K(pal::g0f));
  float lw = A.treeCollapsed ? 46.f : (A.mapFocus ? 0.f : 236.f);
  float rw = A.mapFocus ? 0.f : 280.f;
  ImRect left(x0, body.Min.y, x0 + lw, body.Max.y);
  if (lw > 0) { if (A.treeCollapsed) RailPanel(left); else TreePanel(left); }
  if (rw > 0) PropsPanel(ImRect(x1 - rw, body.Min.y, x1, body.Max.y));
  Stage(ImRect(x0 + lw, body.Min.y, x1 - rw, body.Max.y));
  if (A.treeCollapsed) RailPopover(left);
}




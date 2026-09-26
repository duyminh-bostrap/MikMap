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
// A rotated rectangle in canvas px: what the slice's input rect and a mask both are, so one editor (the on-stage frame) serves both.
struct RectXf { float x, y, w, h, rot; };   // Left/Top of the unrotated rect, size, degrees
static RectXf RectOfSlice(const Slice& s) { return {(float)s.ix, (float)s.iy, (float)s.iw, (float)s.ih, s.irot}; }
static RectXf RectOfMask(const Mask& m) { return {m.x, m.y, m.w, m.h, m.rot}; }
// A 4-key slice is edited as a plain rectangle (like the input rect and the Preview Cue frame): its keystone quad read as centre,
// size and turn, and written back as the corners of that rectangle. Free corners belong to Mesh mode.
static RectXf RectOfQuad(const ImVec2 q[4]) {
  auto len = [](ImVec2 a, ImVec2 b) { return std::hypot(b.x - a.x, b.y - a.y); };
  float w = (len(q[0], q[1]) + len(q[3], q[2])) * 0.5f, h = (len(q[0], q[3]) + len(q[1], q[2])) * 0.5f;
  float cx = (q[0].x + q[1].x + q[2].x + q[3].x) * 0.25f, cy = (q[0].y + q[1].y + q[2].y + q[3].y) * 0.25f;
  float rot = std::atan2(q[1].y - q[0].y, q[1].x - q[0].x) / kDegToRad;
  return {cx - w * 0.5f, cy - h * 0.5f, w, h, std::fabs(rot) < 0.05f ? 0.f : rot};
}
static void RectCorners(const RectXf& r, ImVec2 c[4]) {   // canvas px: tl, tr, br, bl
  float cx = r.x + r.w * 0.5f, cy = r.y + r.h * 0.5f, hw = r.w * 0.5f, hh = r.h * 0.5f;
  float co = std::cos(r.rot * kDegToRad), si = std::sin(r.rot * kDegToRad);
  static const float sx[4] = {-1, 1, 1, -1}, sy[4] = {-1, -1, 1, 1};
  for (int i = 0; i < 4; ++i) { float lx = sx[i] * hw, ly = sy[i] * hh; c[i] = ImVec2(cx + lx * co - ly * si, cy + lx * si + ly * co); }
}
static void InputCorners(const Slice& s, ImVec2 c[4]) { RectCorners(RectOfSlice(s), c); }
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
std::vector<ImVec2> SliceOutline(const Slice& s) {
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
  if (slice->iflipX != ((slice->oflip & 1) != 0)) u = 1.f - u;   // the input rect's mirror and the output Flip cancel each other out
  if (slice->iflipY != ((slice->oflip & 2) != 0)) v = 1.f - v;
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
// Outline of a preset mask shape in the unit square (0..1), clockwise from the top-left region.
static std::vector<ImVec2> MaskUnitPts(int shape) {
  std::vector<ImVec2> p;
  const float kPi = 3.14159265f;
  switch (shape) {
    case App::MS_TRIANGLE: p = {{0.5f, 0.f}, {1.f, 1.f}, {0.f, 1.f}}; break;
    case App::MS_HEXAGON: for (int i = 0; i < 6; ++i) { float a = i * kPi / 3.f; p.push_back({0.5f + 0.5f * std::cos(a), 0.5f + 0.5f * std::sin(a)}); } break;
    case App::MS_CIRCLE: for (int i = 0; i < 32; ++i) { float a = i * 2.f * kPi / 32.f; p.push_back({0.5f + 0.5f * std::cos(a), 0.5f + 0.5f * std::sin(a)}); } break;
    case App::MS_HEART:   // the classic parametric heart, x in [-16,16], y in [-17,12] -> fitted to the unit square
      for (int i = 0; i < 36; ++i) {
        float t = i * 2.f * kPi / 36.f, x = 16.f * std::pow(std::sin(t), 3.f);
        float y = 13.f * std::cos(t) - 5.f * std::cos(2 * t) - 2.f * std::cos(3 * t) - std::cos(4 * t);
        p.push_back({0.5f + x / 32.f, 0.5f - (y + 2.5f) / 29.f});
      }
      break;
    default: p = {{0.f, 0.f}, {1.f, 0.f}, {1.f, 1.f}, {0.f, 1.f}}; break;
  }
  // Fit the outline to the unit square exactly, so every shape touches all four sides of its frame (a hexagon or a heart drawn from a
  // formula would otherwise stop short of the top and bottom).
  ImVec2 mn = p[0], mx = p[0];
  for (auto& q : p) { mn.x = std::min(mn.x, q.x); mn.y = std::min(mn.y, q.y); mx.x = std::max(mx.x, q.x); mx.y = std::max(mx.y, q.y); }
  for (auto& q : p) q = ImVec2((q.x - mn.x) / std::max(1e-6f, mx.x - mn.x), (q.y - mn.y) / std::max(1e-6f, mx.y - mn.y));
  return p;
}
// A mask's polygon = its unit outline placed by the rotated rectangle (the same placement the input rect uses).
void MaskRebuild(Mask& m) {
  std::vector<ImVec2> uu = m.shape >= 0 ? MaskUnitPts(m.shape) : m.u;
  if (uu.size() < 3) uu = MaskUnitPts(App::MS_SQUARE);
  float cx = m.x + m.w * 0.5f, cy = m.y + m.h * 0.5f, co = std::cos(m.rot * 3.14159265f / 180.f), si = std::sin(m.rot * 3.14159265f / 180.f);
  m.pts.resize(uu.size());
  for (size_t i = 0; i < uu.size(); ++i) {
    float lx = (uu[i].x - 0.5f) * m.w, ly = (uu[i].y - 0.5f) * m.h;
    m.pts[i] = ImVec2(cx + lx * co - ly * si, cy + lx * si + ly * co);
  }
}
void MaskFromPolygon(Mask& m, const std::vector<ImVec2>& poly) {
  if (poly.size() < 3) { m.shape = App::MS_SQUARE; MaskRebuild(m); return; }
  ImVec2 mn = poly[0], mx = poly[0];
  for (auto& p : poly) { mn.x = std::min(mn.x, p.x); mn.y = std::min(mn.y, p.y); mx.x = std::max(mx.x, p.x); mx.y = std::max(mx.y, p.y); }
  m.shape = -1; m.rot = 0; m.x = mn.x; m.y = mn.y; m.w = std::max(4.f, mx.x - mn.x); m.h = std::max(4.f, mx.y - mn.y);
  m.u.clear();
  for (auto& p : poly) m.u.push_back(ImVec2((p.x - m.x) / m.w, (p.y - m.y) / m.h));
  MaskRebuild(m);
}
// The shape buttons draw the same outline, fitted to a box centred on c.
static std::vector<ImVec2> MaskShapePts(int shape, ImVec2 c, float hw, float hh) {
  std::vector<ImVec2> p = MaskUnitPts(shape);
  for (auto& q : p) q = ImVec2(c.x + (q.x - 0.5f) * 2.f * hw, c.y + (q.y - 0.5f) * 2.f * hh);
  return p;
}
void App::addMask(int shape) { pushHist();
  Slice* sl = curSlice(); if (!sl) return;
  // A new mask is a half-size shape in the middle of the slice's INPUT rect (composition canvas px); it cuts the picture the slice takes.
  Mask m; m.id = uid("mask"); m.name = "Mask " + std::to_string(sl->masks.size() + 1); m.inverted = true; m.feather = 4;
  m.w = std::max(80.f, sl->iw * 0.5f); m.h = std::max(80.f, sl->ih * 0.5f);
  m.x = sl->ix + sl->iw * 0.5f - m.w * 0.5f; m.y = sl->iy + sl->ih * 0.5f - m.h * 0.5f; m.rot = 0; m.shape = shape;
  MaskRebuild(m);
  selSl = sl->id; selMk = m.id; selKind = 2;
  sl->masks.push_back(m);
  mpage = 0;   // masks are edited on the Input stage — show it
}
void App::setMaskShape(int shape) {   // Mask properties > Input Mask: the shape buttons re-shape the selected mask
  Mask* mk = curMask(); if (!mk) return;
  pushHist(); mk->shape = shape; MaskRebuild(*mk);
}
void App::startMaskPen(bool replaceSelected) {
  if (!curSlice()) return;
  penReplace = replaceSelected && curMask() != nullptr;
  maskPen = true; mapHand = false; penPts.clear(); mpage = 0;
  if (!penReplace) { selMk.clear(); selKind = 1; }
}
void App::cancelMaskPen() { maskPen = false; penReplace = false; penPts.clear(); }
void App::finishMaskPen() {
  Slice* sl = curSlice();
  if (sl && penPts.size() >= 3) {
    pushHist();
    if (penReplace && curMask()) MaskFromPolygon(*curMask(), penPts);   // redraw the selected mask's outline (rect resets to the drawing's box)
    else {
      Mask m; m.id = uid("mask"); m.name = "Mask " + std::to_string(sl->masks.size() + 1); m.inverted = true; m.feather = 4;
      MaskFromPolygon(m, penPts);
      selSl = sl->id; selMk = m.id; selKind = 2;
      sl->masks.push_back(m);
    }
  }
  cancelMaskPen();
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
static void FreshIds(Screen& c) { c.id = A.uid("screen"); for (auto& s : c.slices) FreshIds(s); }
static int SliceIndex(const Screen& sc, const std::string& id) { for (int i = 0; i < (int)sc.slices.size(); ++i) if (sc.slices[i].id == id) return i; return -1; }

// ── groups of output points (corner pins + mesh points), moved together ──
struct PtStart { std::string sl; int idx; ImVec2 out; };   // where a point was (output px) when the group move began
static Slice* SliceById(Screen& sc, const std::string& id) { for (auto& s : sc.slices) if (s.id == id) return &s; return nullptr; }
static bool PointPos(const Slice& s, int idx, ImVec2& out) {
  if (idx < 1000) { if (idx < 0 || idx > 3) return false; out = s.q[idx]; return true; }
  auto gr = MeshGrid(s); int rr = (idx - 1000) / 100, cc = (idx - 1000) % 100;
  if (s.warp == 0 || rr < 0 || rr >= (int)gr.size() || cc < 0 || cc >= (int)gr[rr].size()) return false;
  out = gr[rr][cc]; return true;
}
static std::vector<PtStart> CapturePoints(Screen& sc, const std::vector<App::PtRef>& refs) {
  std::vector<PtStart> out;
  for (auto& r : refs) if (Slice* s = SliceById(sc, r.sl)) { ImVec2 p; if (PointPos(*s, r.idx, p)) out.push_back({r.sl, r.idx, p}); }
  return out;
}
// Put every captured point at (its start + delta): corners first (they define the keystone), then mesh points through the new keystone.
static void MovePointsTo(Screen& sc, const std::vector<PtStart>& st, ImVec2 delta) {
  auto clampOut = [](ImVec2 p) { return ImVec2(std::clamp(std::round(p.x + 0.f), -4000.f, 8000.f), std::clamp(std::round(p.y + 0.f), -4000.f, 8000.f)); };
  for (auto& p : st) if (p.idx < 1000) if (Slice* s = SliceById(sc, p.sl)) s->q[p.idx] = clampOut(ImVec2(p.out.x + delta.x, p.out.y + delta.y));
  for (auto& s : sc.slices) {
    bool any = false; for (auto& p : st) if (p.idx >= 1000 && p.sl == s.id) any = true;
    if (!any || s.warp == 0) continue;
    Keystone k(s.q); auto lg = LocalGrid(s);
    for (auto& p : st) if (p.idx >= 1000 && p.sl == s.id) {
      int rr = (p.idx - 1000) / 100, cc = (p.idx - 1000) % 100; ImVec2 loc;
      if (rr < (int)lg.size() && cc < (int)lg[rr].size() && k.Inv(clampOut(ImVec2(p.out.x + delta.x, p.out.y + delta.y)), loc)) lg[rr][cc] = loc;
    }
    s.meshLocal = lg;
  }
}

// ── multi-selection ──
static bool RefIs(int kind, const App::MapRef& a, const App::MapRef& b) { return kind == 0 ? a.sc == b.sc : kind == 1 ? a.sl == b.sl : a.mk == b.mk; }
static bool RefExists(int kind, const App::MapRef& r) {
  for (auto& sc : A.screens) if (sc.id == r.sc) {
    if (kind == 0) return true;
    for (auto& sl : sc.slices) if (sl.id == r.sl) {
      if (kind == 1) return true;
      for (auto& m : sl.masks) if (m.id == r.mk) return true;
    }
  }
  return false;
}
void App::mapValidate() {
  if (mapMulti.empty()) return;
  int k = MapKind();
  MapRef prim{selSc, selSl, selMk};
  std::vector<MapRef> keep; bool primIn = false;
  for (auto& r : mapMulti) if (RefExists(k, r)) { keep.push_back(r); if (RefIs(k, r, prim)) primIn = true; }
  if (k != mapMultiKind || !primIn || keep.size() < 2) mapMulti.clear(); else mapMulti = keep;
}
std::vector<App::MapRef> App::mapSelection() {
  mapValidate();
  if (!mapMulti.empty()) return mapMulti;
  MapRef p{selSc, selSl, MapKind() == 2 ? selMk : std::string()};
  if (MapKind() == 0) p.sl.clear();
  if (RefExists(MapKind(), p)) return {p};
  return {};
}
bool App::mapIsSel(int kind, const std::string& sc, const std::string& sl, const std::string& mk) {
  if (MapKind() != kind) return false;
  MapRef q{sc, sl, mk};
  for (auto& r : mapSelection()) if (RefIs(kind, r, q)) return true;
  return false;
}
void App::mapSelectRefs(int kind, const std::vector<MapRef>& refs) {
  if (refs.empty()) return;
  const MapRef& p = refs.back();
  selSc = p.sc; selKind = kind;
  if (kind >= 1) selSl = p.sl; else { bool keep = false; for (auto& sc : screens) if (sc.id == p.sc) for (auto& sl : sc.slices) if (sl.id == selSl) keep = true;
    if (!keep) for (auto& sc : screens) if (sc.id == p.sc) selSl = sc.slices.empty() ? "" : sc.slices[0].id; }
  selMk = kind == 2 ? p.mk : std::string();
  if (refs.size() >= 2) { mapMulti = refs; mapMultiKind = kind; } else mapMulti.clear();
}
void App::mapToggle(int kind, const MapRef& r) {
  if (MapKind() != kind) return;   // screens, slices and masks are never selected together
  std::vector<MapRef> cur = mapSelection();
  int at = -1; for (int i = 0; i < (int)cur.size(); ++i) if (RefIs(kind, cur[i], r)) at = i;
  if (at >= 0) { if (cur.size() < 2) return; cur.erase(cur.begin() + at); }
  else cur.push_back(r);
  mapSelectRefs(kind, cur);
}

// ── clipboard ──
static std::string UniqueName(const std::vector<std::string>& taken, std::string base) {
  auto has = [&](const std::string& n) { return std::find(taken.begin(), taken.end(), n) != taken.end(); };
  while (has(base)) base += " copy";
  return base;
}
void App::copyKind(int kind) {
  std::vector<MapRef> sel = MapKind() == kind ? mapSelection() : std::vector<MapRef>();
  if (sel.empty()) { MapRef p{selSc, selSl, selMk}; if (kind == 0) p.sl.clear(); if (kind < 2) p.mk.clear(); if (RefExists(kind, p)) sel = {p}; }
  MapClip c; c.kind = kind;
  auto picked = [&](const MapRef& r) { for (auto& q : sel) if (RefIs(kind, q, r)) return true; return false; };
  for (auto& sc : screens) {   // in tree order, so pasting keeps the on-screen order
    if (kind == 0 && picked({sc.id, "", ""})) c.screens.push_back(sc);
    for (auto& sl : sc.slices) {
      if (kind == 1 && picked({sc.id, sl.id, ""})) c.slices.push_back(sl);
      for (auto& m : sl.masks) if (kind == 2 && picked({sc.id, sl.id, m.id})) c.masks.push_back(m);
    }
  }
  if (c.screens.empty() && c.slices.empty() && c.masks.empty()) return;
  mapClip = c;
}
void App::deleteKind(int kind) {
  std::vector<MapRef> sel = MapKind() == kind ? mapSelection() : std::vector<MapRef>();
  if (sel.empty()) return;
  pushHist();
  int gone = 0;
  if (kind == 0) {
    for (auto& r : sel) { if (screens.size() <= 1) break;   // there is always at least one screen
      auto it = std::remove_if(screens.begin(), screens.end(), [&](const Screen& s) { return s.id == r.sc; });
      if (it != screens.end()) { screens.erase(it, screens.end()); ++gone; } }
    if (gone) { selSc = screens[0].id; selSl = screens[0].slices.empty() ? "" : screens[0].slices[0].id; selMk.clear(); selKind = -1; }
  } else if (kind == 1) {
    std::string keepSc;
    for (auto& r : sel) for (auto& sc : screens) if (sc.id == r.sc && sc.slices.size() > 1) {   // a screen keeps at least one slice
      auto it = std::remove_if(sc.slices.begin(), sc.slices.end(), [&](const Slice& s) { return s.id == r.sl; });
      if (it != sc.slices.end()) { sc.slices.erase(it, sc.slices.end()); ++gone; keepSc = sc.id; }
    }
    if (gone) for (auto& sc : screens) if (sc.id == keepSc) { selSc = sc.id; selSl = sc.slices[0].id; selMk.clear(); selKind = -1; }
  } else {
    std::string keepSc, keepSl;
    for (auto& r : sel) for (auto& sc : screens) if (sc.id == r.sc) for (auto& sl : sc.slices) if (sl.id == r.sl) {
      auto it = std::remove_if(sl.masks.begin(), sl.masks.end(), [&](const Mask& m) { return m.id == r.mk; });
      if (it != sl.masks.end()) { sl.masks.erase(it, sl.masks.end()); ++gone; keepSc = sc.id; keepSl = sl.id; }
    }
    if (gone) { selSc = keepSc; selSl = keepSl; selMk.clear(); selKind = 1; }
  }
  mapMulti.clear();
  if (!gone) notify(kind == 0 ? "A project needs at least one screen" : kind == 1 ? "A screen needs at least one slice" : "Nothing to delete");
}
void App::duplicateKind(int kind) {
  std::vector<MapRef> sel = MapKind() == kind ? mapSelection() : std::vector<MapRef>();
  if (sel.empty()) return;
  pushHist();
  std::vector<MapRef> made;
  if (kind == 0) {
    for (auto& r : sel) for (size_t i = 0; i < screens.size(); ++i) if (screens[i].id == r.sc) {
      Screen c = screens[i]; FreshIds(c); c.name += " copy"; made.push_back({c.id, c.slices.empty() ? "" : c.slices[0].id, ""});
      screens.insert(screens.begin() + i + 1, c); break; }
  } else if (kind == 1) {
    for (auto& r : sel) for (auto& sc : screens) if (sc.id == r.sc) { int at = SliceIndex(sc, r.sl); if (at < 0) continue;
      Slice c = sc.slices[at]; FreshIds(c); c.name += " copy"; made.push_back({sc.id, c.id, ""});
      sc.slices.insert(sc.slices.begin() + at + 1, c); }
  } else {
    for (auto& r : sel) for (auto& sc : screens) if (sc.id == r.sc) for (auto& sl : sc.slices) if (sl.id == r.sl) for (size_t i = 0; i < sl.masks.size(); ++i) if (sl.masks[i].id == r.mk) {
      Mask c = sl.masks[i]; c.id = uid("mask"); c.name += " copy"; made.push_back({sc.id, sl.id, c.id});
      sl.masks.insert(sl.masks.begin() + i + 1, c); break; }
  }
  if (!made.empty()) { mapSelectRefs(kind, made); if (kind == 2) mpage = 0; }
}
void App::pasteClip() {
  if (!hasClip()) return;
  if (mapClip.kind == 0) {
    if (mapClip.screens.empty()) return;
    pushHist();
    int at = (int)screens.size();   // after the last selected screen (or the current one), else at the end
    if (Screen* cs = curScreen()) for (int i = 0; i < (int)screens.size(); ++i) if (screens[i].id == cs->id) at = i + 1;
    if (MapKind() == 0) for (auto& r : mapSelection()) for (int i = 0; i < (int)screens.size(); ++i) if (screens[i].id == r.sc) at = std::max(at, i + 1);
    std::vector<MapRef> made;
    for (auto c : mapClip.screens) {
      std::vector<std::string> names; for (auto& s : screens) names.push_back(s.name);
      FreshIds(c); c.name = UniqueName(names, c.name);
      made.push_back({c.id, c.slices.empty() ? "" : c.slices[0].id, ""});
      screens.insert(screens.begin() + std::min(at++, (int)screens.size()), c);
    }
    mapSelectRefs(0, made);
  } else if (mapClip.kind == 1) {
    Screen* sc = curScreen(); if (!sc || mapClip.slices.empty()) return;
    pushHist();
    int at = (int)sc->slices.size();
    if (Slice* cur = curSlice()) at = SliceIndex(*sc, cur->id) + 1;
    if (MapKind() == 1) for (auto& r : mapSelection()) if (r.sc == sc->id) at = std::max(at, SliceIndex(*sc, r.sl) + 1);
    std::string scId = sc->id; std::vector<MapRef> made;
    for (auto c : mapClip.slices) {
      std::vector<std::string> names; for (auto& s : sc->slices) names.push_back(s.name);
      FreshIds(c); c.name = UniqueName(names, c.name); made.push_back({scId, c.id, ""});
      sc->slices.insert(sc->slices.begin() + std::clamp(at++, 0, (int)sc->slices.size()), c);
    }
    mapSelectRefs(1, made);
  } else {
    Screen* sc = curScreen(); Slice* sl = curSlice();
    if (!sc || !sl || mapClip.masks.empty()) { notify("Select a slice to paste the mask into"); return; }
    pushHist();
    std::string scId = sc->id, slId = sl->id; std::vector<MapRef> made;
    for (auto c : mapClip.masks) {
      std::vector<std::string> names; for (auto& m : sl->masks) names.push_back(m.name);
      c.id = uid("mask"); c.name = UniqueName(names, c.name); made.push_back({scId, slId, c.id}); sl->masks.push_back(c);
    }
    mapSelectRefs(2, made); mpage = 0;
  }
}
void App::nudgeSelection(float dx, float dy) {
  std::vector<MapRef> sel = mapSelection();
  if ((sel.empty() && !(mpage == 1 && !mapPts.empty())) || maskPen) return;
  int kind = MapKind();
  auto slice = [&](const MapRef& r) -> Slice* { for (auto& sc : screens) if (sc.id == r.sc) for (auto& sl : sc.slices) if (sl.id == r.sl) return &sl; return nullptr; };
  auto clampQ = [](float v) { return std::clamp(v, -4000.f, 8000.f); };
  if (mpage == 0) {   // Input: the selected input rects / masks (canvas px)
    for (auto& r : sel) {
      if (kind == 2) { for (auto& sc : screens) if (sc.id == r.sc) for (auto& sl : sc.slices) if (sl.id == r.sl) for (auto& m : sl.masks) if (m.id == r.mk) { m.x += dx; m.y += dy; MaskRebuild(m); } }
      else if (kind == 1) if (Slice* s = slice(r)) { s->ix += (int)std::lround(dx); s->iy += (int)std::lround(dy); }
    }
  } else if (!mapPts.empty()) {   // Output with points picked: move exactly those
    if (Screen* sc = curScreen()) { auto st = CapturePoints(*sc, mapPts); MovePointsTo(*sc, st, ImVec2(dx, dy)); }
  } else {            // Output: the selected slices' quads, or every slice of the selected screens (the mesh rides along with the keystone)
    auto moveQ = [&](Slice& s) { for (auto& q : s.q) { q.x = clampQ(q.x + dx); q.y = clampQ(q.y + dy); } };
    for (auto& r : sel) {
      if (kind == 1) { if (Slice* s = slice(r)) moveQ(*s); }
      else if (kind == 0) for (auto& sc : screens) if (sc.id == r.sc) for (auto& sl : sc.slices) moveQ(sl);
    }
  }
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
// Ctrl / Cmd / Shift held: a click adds to / removes from the selection instead of replacing it.
static bool MultiMod() { const ImGuiIO& io = ImGui::GetIO(); return io.KeyCtrl || io.KeyShift || io.KeySuper; }

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
  const int mkind = A.MapKind(); const bool multi = A.mapSelCount() > 1;   // several of one kind are selected: mark exactly those
  if (n.kind == Node::ScreenN) { on = multi && mkind == 0 ? A.mapIsSel(0, n.sc, "", "") : scOn; hexOn = n.color; }
  else if (n.kind == Node::SliceN) { on = multi && mkind == 1 ? A.mapIsSel(1, n.sc, n.sl, "") : scOn && csl && n.sl == csl->id && !cmk; hexOn = pal::coral; }
  else { on = multi && mkind == 2 ? A.mapIsSel(2, n.sc, n.sl, n.mk) : cmk && cmk->id == n.mk; hexOn = pal::yellow; }
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
  if (h.click && !chevHit && !visHit && MultiMod()) {   // Ctrl / Cmd / Shift + click: add or remove (only within the same kind)
    A.mapToggle(n.kind == Node::ScreenN ? 0 : n.kind == Node::SliceN ? 1 : 2, {n.sc, n.sl, n.kind == Node::MaskN ? n.mk : std::string()});
  } else if (h.click && !chevHit && !visHit) {
    A.mapMulti.clear();
    if (n.kind == Node::ScreenN) {
      Screen* sc = nullptr; for (auto& s : A.screens) if (s.id == n.sc) sc = &s;
      bool keep = false; if (sc) for (auto& sl : sc->slices) if (sl.id == A.selSl) keep = true;
      A.selSc = n.sc; A.selSl = keep ? A.selSl : (sc && !sc->slices.empty() ? sc->slices[0].id : ""); A.selMk.clear(); A.selKind = 0;
    } else { A.selSc = n.sc; A.selSl = n.sl; A.selMk = n.kind == Node::MaskN ? n.mk : ""; A.selKind = n.kind == Node::MaskN ? 2 : 1; if (n.kind == Node::MaskN) A.mpage = 0; }   // masks are edited on the Input stage
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
  float bw = (fr.GetWidth() - 12 - 6) / 2.f;   // Mask moved to Slice Properties > Input Mask
  struct Ac { const char* l; const char* ic; uint32_t fg; bool primary; } acts[2] = {{"Screen", "monitor", pal::coral, true}, {"Slice", "plus", pal::white, false}};
  for (int i = 0; i < 2; ++i) {
    ImRect br(fr.Min.x + 6 + i * (bw + 6), fr.Min.y + 9, fr.Min.x + 6 + i * (bw + 6) + bw, fr.Min.y + 9 + 28);
    Hit h = HitR(br);
    Box(br, acts[i].primary ? K(pal::coral, 0.15f) : h.hover ? K(pal::ctrlHover) : K(pal::g1c), acts[i].primary ? K(pal::coral, 0.4f) : K(pal::g22), 3);
    float tw = TextW(UI_B, 10, acts[i].l), cx = (br.Min.x + br.Max.x) * 0.5f, cy = (br.Min.y + br.Max.y) * 0.5f;
    Icon(acts[i].ic, ImVec2(cx - tw * 0.5f - 8, cy), 11, K(acts[i].fg));
    Text(cx - tw * 0.5f + 4, cy, UI_B, 10, K(acts[i].fg), acts[i].l);
    if (h.hover) CursorHand();
    if (h.click) { if (i == 0) A.addScreen(); else A.addSlice(); }
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
    if (h.click) { A.mapMulti.clear(); A.selSc = sc.id; A.selSl = sc.slices.empty() ? "" : sc.slices[0].id; A.selMk.clear(); A.selKind = 0; A.railScreen = A.railScreen == sc.id ? "" : sc.id; }
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
  {
    ImRect br(fr.Min.x + 6, fr.Min.y + 9, fr.Max.x - 6, fr.Min.y + 9 + 24);
    Hit h = HitR(br);
    Box(br, h.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
    TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, UI_B, 10, K(pal::white), "Slice");
    if (h.hover) CursorHand();
    if (h.click) A.addSlice();
  }
}

// ───────────────────────── stage ─────────────────────────
static int dragKind = 0, dragIdx = 0;  // 1 corner, 2 input resize (0..3 corners, 10..13 edge middles), 3 mask point, 4 mesh point (row*100+col), 5 input move, 6 input rotate
static bool dragOnQuad = false;         // the resize / move / rotate drag edits the output quad of a 4-key slice, not an input rect or a mask
static ImVec2 dragOff;                  // grabbed point minus cursor (output px), so grabbing off-centre doesn't jump
static bool dragOnMask = false;          // the input-frame drags (2 resize / 5 move / 6 rotate) edit the selected mask instead of the slice
static std::vector<PtStart> gGroup;      // the points being dragged together (dragKind 7)
static ImVec2 gGrabStart;                // the grabbed point of that group, output px, at grab time
static int gMarquee = 0; static ImVec2 gMarqueeA;   // 0 idle, 1 pressed on empty stage, 2 dragging a selection rectangle
static ImVec2 dragAnchor;               // input rect drags: the fixed corner/edge-middle (resize) in canvas px
static float dragAng0 = 0.f, dragRot0 = 0.f;   // input rect rotate: pointer angle and rect rotation when the drag began

// ── magnet: drags snap to other points, to edges, and to the canvas / output box ──
struct SnapSet { std::vector<float> xs, ys; std::vector<ImVec2> pts; std::vector<std::pair<ImVec2, ImVec2>> segs; ImVec2 boxMin{0, 0}, boxMax{0, 0}; };
static std::vector<std::pair<ImVec2, ImVec2>> gGuideLines;   // stage-space lines/segments the last snap locked onto (drawn by Stage)
static std::vector<ImVec2> gGuideDots;
static void AddRectTargets(SnapSet& S, const RectXf& R) {
  ImVec2 c[4]; RectCorners(R, c);
  for (int i = 0; i < 4; ++i) { S.pts.push_back(c[i]); S.segs.push_back({c[i], c[(i + 1) % 4]}); }
}
static SnapSet BuildSnap(const Screen* sc, const Slice* sl, const Mask* mk, bool onMask, int skipQ) {
  SnapSet S;
  if (A.mpage == 1) {   // Output: the screen's 1920x1080 box, and every visible slice's corners and edges
    S.xs = {0.f, 960.f, 1920.f}; S.ys = {0.f, 540.f, 1080.f}; S.boxMax = ImVec2(1920.f, 1080.f);
    if (sc) for (auto& o : sc->slices) {
      if (!o.visible) continue;
      bool own = sl && o.id == sl->id;
      if (own && skipQ == -2) continue;   // a whole-quad edit: the slice's own (moving) edges are no targets
      for (int i = 0; i < 4; ++i) {
        if (own && i == skipQ) continue;   // never snap a corner onto itself
        S.pts.push_back(o.q[i]);
        if (!(own && (i == skipQ || (i + 1) % 4 == skipQ))) S.segs.push_back({o.q[i], o.q[(i + 1) % 4]});
      }
    }
  } else {              // Input: the composition canvas, other slices' input rects (and the slice's own rect when editing its mask)
    float W = (float)A.canvasW, H = (float)A.canvasH;
    S.xs = {0.f, W * 0.5f, W}; S.ys = {0.f, H * 0.5f, H}; S.boxMax = ImVec2(W, H);
    if (sc) for (auto& o : sc->slices) {
      if (!o.visible) continue;
      if (sl && o.id == sl->id && !onMask) continue;   // the rect being edited is not a target for itself
      AddRectTargets(S, RectOfSlice(o));
    }
    (void)mk;
  }
  return S;
}
// The magnet locked onto x (vertical) or y (horizontal) = v: light up only the edge it locked onto — a side of a box that has
// exactly that coordinate, or the centre line drawn across the frame — never a line running off to the edges of the stage.
static void GuideAxis(const SnapSet& S, bool vertical, float v, const ImVec2* src) {
  int n = 0;
  for (auto& sg : S.segs) {
    float a = vertical ? sg.first.x : sg.first.y, b = vertical ? sg.second.x : sg.second.y;
    if (std::fabs(a - v) < 0.5f && std::fabs(b - v) < 0.5f) { gGuideLines.push_back(sg); ++n; }
  }
  for (float c : vertical ? S.xs : S.ys) if (std::fabs(c - v) < 0.5f) {
    gGuideLines.push_back(vertical ? std::make_pair(ImVec2(v, S.boxMin.y), ImVec2(v, S.boxMax.y)) : std::make_pair(ImVec2(S.boxMin.x, v), ImVec2(S.boxMax.x, v)));
    ++n; break;
  }
  if (!n && src) gGuideDots.push_back(*src);   // a corner of a turned box: no straight side to light, mark the corner itself
}
// Snap a point: an existing point wins, then a shared x / y line, then the nearest spot on an edge. thr is in stage px.
static ImVec2 SnapPoint(ImVec2 p, const SnapSet& S, float thr) {
  float best = thr; const ImVec2* bp = nullptr;
  for (auto& q : S.pts) { float d = std::hypot(q.x - p.x, q.y - p.y); if (d < best) { best = d; bp = &q; } }
  if (bp) { gGuideDots.push_back(*bp); return *bp; }
  ImVec2 r = p; bool sx = false, sy = false; float bx = thr, by = thr;
  const ImVec2 *srcX = nullptr, *srcY = nullptr;
  auto tryX = [&](float x, const ImVec2* q) { float d = std::fabs(p.x - x); if (d < bx) { bx = d; r.x = x; sx = true; srcX = q; } };
  auto tryY = [&](float y, const ImVec2* q) { float d = std::fabs(p.y - y); if (d < by) { by = d; r.y = y; sy = true; srcY = q; } };
  for (float x : S.xs) tryX(x, nullptr); for (float y : S.ys) tryY(y, nullptr);
  for (auto& q : S.pts) { tryX(q.x, &q); tryY(q.y, &q); }
  if (sx) GuideAxis(S, true, r.x, srcX);
  if (sy) GuideAxis(S, false, r.y, srcY);
  if (!sx && !sy) {   // no shared line: slide onto the nearest edge
    float bd = thr; const std::pair<ImVec2, ImVec2>* bs = nullptr; ImVec2 bpnt = p;
    for (auto& sg : S.segs) {
      ImVec2 a = sg.first, b = sg.second, ab(b.x - a.x, b.y - a.y);
      float l2 = ab.x * ab.x + ab.y * ab.y; if (l2 < 1e-6f) continue;
      float t = std::clamp(((p.x - a.x) * ab.x + (p.y - a.y) * ab.y) / l2, 0.f, 1.f);
      ImVec2 pr(a.x + ab.x * t, a.y + ab.y * t); float d = std::hypot(pr.x - p.x, pr.y - p.y);
      if (d < bd) { bd = d; bs = &sg; bpnt = pr; }
    }
    if (bs) { r = bpnt; gGuideLines.push_back(*bs); }
  }
  return r;
}
// Snap a rect that is being moved: its edges / centre (of its bounding box) onto shared x / y lines.
static void SnapRectMove(RectXf& R, const SnapSet& S, float thr) {
  ImVec2 c[4]; RectCorners(R, c);
  float mnx = c[0].x, mxx = mnx, mny = c[0].y, mxy = mny;
  for (auto& q : c) { mnx = std::min(mnx, q.x); mxx = std::max(mxx, q.x); mny = std::min(mny, q.y); mxy = std::max(mxy, q.y); }
  const float cxs[3] = {mnx, (mnx + mxx) * 0.5f, mxx}, cys[3] = {mny, (mny + mxy) * 0.5f, mxy};
  float bdx = thr, bdy = thr, dx = 0, dy = 0, gx = 0, gy = 0; bool sx = false, sy = false;
  const ImVec2 *srcX = nullptr, *srcY = nullptr;
  auto tx = [&](float t, const ImVec2* q) { for (float cnd : cxs) { float d = t - cnd; if (std::fabs(d) < bdx) { bdx = std::fabs(d); dx = d; gx = t; sx = true; srcX = q; } } };
  auto ty = [&](float t, const ImVec2* q) { for (float cnd : cys) { float d = t - cnd; if (std::fabs(d) < bdy) { bdy = std::fabs(d); dy = d; gy = t; sy = true; srcY = q; } } };
  for (float x : S.xs) tx(x, nullptr); for (float y : S.ys) ty(y, nullptr);
  for (auto& q : S.pts) { tx(q.x, &q); ty(q.y, &q); }
  R.x += dx; R.y += dy;
  if (sx) GuideAxis(S, true, gx, srcX);
  if (sy) GuideAxis(S, false, gy, srcY);
}

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

// Right-click on the input rect: quick placement (centre / mirror / halves / whole), exchange shape with the output quad,
// stacking order, and the slice clipboard — the same list Resolume offers on its input selection.
static void InputRectMenu(ImVec2 at, bool output = false) {
  Screen* sc = A.curScreen(); Slice* sl = A.curSlice(); if (!sc || !sl) return;
  int n = (int)sc->slices.size(), idx = 0; for (int i = 0; i < n; ++i) if (sc->slices[i].id == sl->id) idx = i;
  std::vector<MenuItem> mi;
  auto add = [&](const char* label, std::function<void()> fn, bool divider = false, bool disabled = false) {
    if (divider) { MenuItem d; d.label = ""; d.disabled = true; d.divider = true; mi.push_back(d); }
    MenuItem it; it.label = label; it.disabled = disabled; it.run = std::move(fn); mi.push_back(it);
  };
  auto edit = [](std::function<void(Slice&)> f) { return [f] { A.pushHist(); if (Slice* s = A.curSlice()) f(*s); }; };
  auto setRect = [](Slice& s, int x, int y, int w, int h) { s.ix = x; s.iy = y; s.iw = std::max(20, w); s.ih = std::max(20, h); s.irot = 0; };
  if (output) {   // the same list, acting on the slice's quad in the 1920x1080 output (a mesh follows: it lives inside the quad)
    auto bbox = [](const Slice& s, float& x0, float& y0, float& x1, float& y1) { x0 = x1 = s.q[0].x; y0 = y1 = s.q[0].y; for (auto& p : s.q) { x0 = std::min(x0, p.x); x1 = std::max(x1, p.x); y0 = std::min(y0, p.y); y1 = std::max(y1, p.y); } };
    auto shift = [bbox](Slice& s, float dx, float dy) { for (auto& p : s.q) { p.x = std::round(p.x + dx); p.y = std::round(p.y + dy); } (void)bbox; };
    auto setQuad = [](Slice& s, float x, float y, float w, float h) { QuadOf(s, (int)x, (int)y, (int)w, (int)h); };
    add("Center X", edit([bbox, shift](Slice& s) { float a, b, c, d; bbox(s, a, b, c, d); shift(s, 960.f - (a + c) * 0.5f, 0.f); }));
    add("Center Y", edit([bbox, shift](Slice& s) { float a, b, c, d; bbox(s, a, b, c, d); shift(s, 0.f, 540.f - (b + d) * 0.5f); }));
    add("Mirror X", edit([](Slice& s) { std::swap(s.q[0], s.q[1]); std::swap(s.q[3], s.q[2]); }));
    add("Mirror Y", edit([](Slice& s) { std::swap(s.q[0], s.q[3]); std::swap(s.q[1], s.q[2]); }));
    add("Left Half", edit([setQuad](Slice& s) { setQuad(s, 0, 0, 960, 1080); }), true);
    add("Top Half", edit([setQuad](Slice& s) { setQuad(s, 0, 0, 1920, 540); }));
    add("Right Half", edit([setQuad](Slice& s) { setQuad(s, 960, 0, 960, 1080); }));
    add("Bottom Half", edit([setQuad](Slice& s) { setQuad(s, 0, 540, 1920, 540); }));
    add("Whole Area", edit([setQuad](Slice& s) { setQuad(s, 0, 0, 1920, 1080); }));
    add("Match Input Shape", [] { A.matchOutputToInput(); }, true);
  } else {
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
  }
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
  add("Paste", [] { A.pasteSlice(); }, false, !A.hasClip());
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
  const float kBtnW = 28.f, kBtnH = 28.f, kIco = 16.f, kGrpH = 17.f;   // toolbar buttons: big icons
  if (sl && A.mpage != 0) {
    ImRect wg(grp.Max.x + 4, cy - kGrpH, grp.Max.x + 4 + 3 + 2 * (kBtnW + 2) + 1, cy + kGrpH);
    Box(wg, K(0x000000), K(pal::g2a), 4);
    const char* wi[2] = {"frame", "grid-3x3"};
    for (int i = 0; i < 2; ++i) {
      ImRect br(wg.Min.x + 3 + i * (kBtnW + 2), cy - kBtnH * 0.5f, wg.Min.x + 3 + i * (kBtnW + 2) + kBtnW, cy + kBtnH * 0.5f);
      bool on = (i == 0) == (sl->warp == 0);
      Hit h = HitR(br);
      Box(br, on ? K(pal::coral, 0.2f) : 0, on ? K(pal::coral, 0.5f) : 0, 3);
      Icon(wi[i], ImVec2((br.Min.x + br.Max.x) * 0.5f, cy), kIco, K(on ? pal::coral : pal::t77));
      if (h.hover) CursorHand();
      if (h.click && !on) { A.pushHist(); sl->warp = i; }
    }
  }
  // right side: zoom group, history/reset group, tools. Large icons; the zoom group is just − % + and focus mode.
  float xr = tb.Max.x - 6;
  {
    char zl[16]; snprintf(zl, sizeof zl, "%d%%", (int)std::round(A.mapZ * 100));
    const float pctW = 46.f;
    float gw = 3 + kBtnW + pctW + kBtnW + 5 + kBtnW + 3;
    ImRect zg(xr - gw, cy - kGrpH, xr, cy + kGrpH);
    Box(zg, K(pal::g1c), K(pal::g22), 3);
    float zx = zg.Min.x + 3;
    auto iconBtn = [&](float x, const char* ic, bool on) {
      ImRect br(x, cy - kBtnH * 0.5f, x + kBtnW, cy + kBtnH * 0.5f);
      Hit h = HitR(br);
      if (h.hover) Fill(br, K(pal::g22), 3);
      Icon(ic, ImVec2((br.Min.x + br.Max.x) * 0.5f, cy), kIco, K(on ? pal::coral : h.hover ? pal::white : pal::tcc));
      if (h.hover) CursorHand();
      return h.click;
    };
    if (iconBtn(zx, "zoom-out", false)) A.setZoom(A.mapZ / 1.4f);
    zx += kBtnW;
    {   // the readout is also the "fit everything" button
      ImRect pr(zx, cy - kBtnH * 0.5f, zx + pctW, cy + kBtnH * 0.5f);
      Hit h = HitR(pr);
      if (h.hover) { Fill(pr, K(pal::g22), 3); CursorHand(); }
      TextC((pr.Min.x + pr.Max.x) * 0.5f, cy, MONO_B, 11, K(h.hover ? pal::white : pal::tcc), zl);
      if (h.click && sc) FitAll(StageArea(r), *sc);   // frames points dragged outside the output too
      zx += pctW;
    }
    if (iconBtn(zx, "zoom-in", false)) A.setZoom(A.mapZ * 1.4f);
    zx += kBtnW;
    VLine(zx + 2, cy - 8, cy + 8, K(pal::g2a)); zx += 5;
    if (iconBtn(zx, A.mapFocus ? "minimize-2" : "expand", A.mapFocus)) A.mapFocus = !A.mapFocus;
    xr = zg.Min.x - 4;
    float hw = 3 + kBtnW * 2 + 2 + 5 + kBtnW + 3;
    ImRect hg(xr - hw, cy - kGrpH, xr, cy + kGrpH);
    Box(hg, K(pal::g1c), K(pal::g22), 3);
    float hx = hg.Min.x + 3;
    for (int i = 0; i < 2; ++i) {
      ImRect br(hx, cy - kBtnH * 0.5f, hx + kBtnW, cy + kBtnH * 0.5f);
      bool can = i == 0 ? CanUndo() : CanRedo();
      Hit h = HitR(br);
      if (h.hover && can) Fill(br, K(pal::g22), 3);
      float prev = g.alpha; if (!can) g.alpha *= 0.4f;
      Icon(i == 0 ? "undo-2" : "redo-2", ImVec2((br.Min.x + br.Max.x) * 0.5f, cy), kIco, K(can ? (h.hover ? pal::white : pal::tcc) : pal::t66));
      g.alpha = prev;
      if (h.hover && can) CursorHand();
      if (h.click) { if (i == 0) A.undoMap(); else A.redoMap(); }
      hx += kBtnW + 2;
    }
    VLine(hx + 1, cy - 8, cy + 8, K(pal::g2a)); hx += 5;
    ImRect rb(hx, cy - kBtnH * 0.5f, hx + kBtnW, cy + kBtnH * 0.5f);
    Hit rh = HitR(rb);
    if (rh.hover) Fill(rb, K(pal::g22), 3);
    Icon("rotate-ccw", ImVec2((rb.Min.x + rb.Max.x) * 0.5f, cy), kIco, K(rh.hover ? pal::white : pal::t88));
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
    {   // stage tools: hand (left-drag pans) and magnet (drags snap to points and edges; hold Alt to bypass)
      ImRect tg(xr - (3 + 2 * (kBtnW + 2) + 1), cy - kGrpH, xr, cy + kGrpH);
      Box(tg, K(pal::g1c), K(pal::g22), 3);
      const char* ti[2] = {"hand", "magnet"}; bool* tv[2] = {&A.mapHand, &A.mapSnap};
      for (int i = 0; i < 2; ++i) {
        ImRect br(tg.Min.x + 3 + i * (kBtnW + 2), cy - kBtnH * 0.5f, tg.Min.x + 3 + i * (kBtnW + 2) + kBtnW, cy + kBtnH * 0.5f);
        Hit h = HitR(br); bool on = *tv[i];
        Box(br, on ? K(pal::coral, 0.2f) : 0, on ? K(pal::coral, 0.5f) : 0, 3);
        Icon(ti[i], ImVec2((br.Min.x + br.Max.x) * 0.5f, cy), kIco, K(on ? pal::coral : h.hover ? pal::white : pal::tcc));
        if (h.hover) CursorHand();
        if (h.click) *tv[i] = !*tv[i];
      }
      xr = tg.Min.x - 4;
    }
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
  // Mouse wheel = zoom about the cursor (the point under it stays put); Shift+wheel scrolls vertically; a sideways wheel / trackpad pans
  // horizontally. Panning otherwise: the hand tool, or right-drag.
  if (inArea && io.MouseWheelH != 0.f) A.mapScrollX -= io.MouseWheelH * 48.f;
  if (inArea && io.MouseWheel != 0.f && io.KeyShift) A.mapScrollY -= io.MouseWheel * 48.f;
  if (inArea && io.MouseWheel != 0.f && !io.KeyShift) {
    ImVec2 p((m.x - ac.x + A.mapScrollX) / s + SW * 0.5f, (m.y - ac.y + A.mapScrollY) / s + SH * 0.5f);
    A.mapZ = std::clamp(A.mapZ * std::pow(1.15f, std::clamp(io.MouseWheel, -3.f, 3.f)), kMinZoom, kMaxZoom);   // a wheel notch = one 15% step; trackpad fractions are smooth
    s = baseW * A.mapZ / SW;
    A.mapScrollX = (p.x - SW * 0.5f) * s + (ac.x - m.x); A.mapScrollY = (p.y - SH * 0.5f) * s + (ac.y - m.y);
  }
  static int panBtn = -1; static ImVec2 panM; static float panX, panY;   // right-drag pan, or left-drag while the hand tool is on
  bool rClick = false;   // right button released without dragging = context menu, not a pan
  if (panBtn < 0 && inArea) {
    if (ImGui::IsMouseClicked(1)) panBtn = 1; else if (A.mapHand && ImGui::IsMouseClicked(0)) panBtn = 0;
    if (panBtn >= 0) { panM = m; panX = A.mapScrollX; panY = A.mapScrollY; }
  }
  if (panBtn >= 0) {
    if (ImGui::IsMouseDown(panBtn)) { A.mapScrollX = panX - (m.x - panM.x); A.mapScrollY = panY - (m.y - panM.y); ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll); }
    else { rClick = panBtn == 1 && std::hypot(m.x - panM.x, m.y - panM.y) < 4.f; panBtn = -1; }
  } else if (A.mapHand && inArea) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
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
  gGuideLines.clear(); gGuideDots.clear();
  if (A.mpage != 1) A.mapPts.clear();   // picked points belong to the Output page
  const bool snapOn = A.mapSnap && !io.KeyAlt;   // magnet on; hold Alt to place freely
  const float snapThr = 8.f / std::max(0.05f, s);   // 8 screen px, in stage px
  if (dragKind && sl) {
    ImVec2 to = Vadd(mo, dragOff);
    if (dragKind == 4) {   // the mesh lives in keystone space: store where the cursor lands in it
      int rr = dragIdx / 100, cc = dragIdx % 100;
      ImVec2 loc, tp = inOutput(to);
      if (snapOn) tp = inOutput(SnapPoint(tp, BuildSnap(sc, sl, nullptr, false, -1), snapThr));
      if (Keystone(sl->q).Inv(tp, loc)) {
        auto lg = LocalGrid(*sl);
        if (rr < (int)lg.size() && cc < (int)lg[rr].size()) { lg[rr][cc] = loc; sl->meshLocal = lg; }
      }
    }
    else if (dragKind == 7) {   // several points at once: the grabbed one follows the pointer (and the magnet), the rest keep their offsets
      ImVec2 tp = inOutput(to);
      if (snapOn) {
        SnapSet S = BuildSnap(sc, sl, nullptr, false, -1);
        auto moving = [&](ImVec2 q) { for (auto& p : gGroup) if (std::fabs(p.out.x - q.x) < 0.01f && std::fabs(p.out.y - q.y) < 0.01f) return true; return false; };
        S.pts.erase(std::remove_if(S.pts.begin(), S.pts.end(), moving), S.pts.end());
        S.segs.erase(std::remove_if(S.segs.begin(), S.segs.end(), [&](const std::pair<ImVec2, ImVec2>& g2) { return moving(g2.first) || moving(g2.second); }), S.segs.end());
        tp = inOutput(SnapPoint(tp, S, snapThr));
      }
      MovePointsTo(*sc, gGroup, ImVec2(tp.x - gGrabStart.x, tp.y - gGrabStart.y));
    }
    else if (dragKind == 1) {   // the mesh follows on its own — it is keystone-relative
      ImVec2 tp = inOutput(to);
      if (snapOn) tp = inOutput(SnapPoint(tp, BuildSnap(sc, sl, nullptr, false, dragIdx), snapThr));
      sl->q[dragIdx] = tp;
    }
    else if (dragKind == 2 || dragKind == 5 || dragKind == 6) {
      // one editor for both the slice's input rect and a mask: read the rect, edit it in its own rotated frame, write it back
      const bool onMask = dragOnMask && mk, onQuad = dragOnQuad && !onMask;
      RectXf R = onQuad ? RectOfQuad(sl->q) : onMask ? RectOfMask(*mk) : RectOfSlice(*sl);
      const float minSz = onMask || onQuad ? 8.f : 20.f;
      const int snapSkip = onQuad ? -2 : -1;
      if (dragKind == 2) {   // resize: the opposite corner / edge middle stays put
        float co = std::cos(R.rot * kDegToRad), si = std::sin(R.rot * kDegToRad);
        // the slice's input rect may reach past the canvas (the part outside is simply empty); a mask has nothing to cut out there
        ImVec2 P = onMask && R.rot == 0.f ? mu : inOutput(mo);
        if (snapOn) { P = SnapPoint(P, BuildSnap(sc, sl, mk, onMask, snapSkip), snapThr); P = onMask && R.rot == 0.f ? inCanvas(P) : inOutput(P); }
        float dx = P.x - dragAnchor.x, dy = P.y - dragAnchor.y;
        float lx = dx * co + dy * si, ly = -dx * si + dy * co;   // pointer relative to the anchor, in rect-local axes
        float w = R.w, h = R.h, hx = 0, hy = 0;                  // hx/hy: new centre offset from the anchor, local axes
        int k = dragIdx % 10;
        if (dragIdx >= 10) {
          if (k == 0) { h = std::max(minSz, -ly); hy = -h * 0.5f; }
          else if (k == 1) { w = std::max(minSz, lx); hx = w * 0.5f; }
          else if (k == 2) { h = std::max(minSz, ly); hy = h * 0.5f; }
          else { w = std::max(minSz, -lx); hx = -w * 0.5f; }
        } else {
          float sx = (k == 1 || k == 2) ? 1.f : -1.f, sy = (k == 2 || k == 3) ? 1.f : -1.f;
          w = std::max(minSz, sx * lx); h = std::max(minSz, sy * ly); hx = sx * w * 0.5f; hy = sy * h * 0.5f;
        }
        float cx = dragAnchor.x + hx * co - hy * si, cy = dragAnchor.y + hx * si + hy * co;
        R.w = w; R.h = h; R.x = cx - w * 0.5f; R.y = cy - h * 0.5f;
      } else if (dragKind == 5) {   // move: an upright rect stays inside the canvas, a rotated one just keeps its centre on it
        float cx = to.x, cy = to.y;
        if (onQuad) { R.x = cx - R.w * 0.5f; R.y = cy - R.h * 0.5f; }   // the output space has no border to keep inside
        else if (onMask && R.rot == 0.f && R.w <= A.canvasW && R.h <= A.canvasH) {
          R.x = std::clamp(cx - R.w * 0.5f, 0.f, A.canvasW - R.w); R.y = std::clamp(cy - R.h * 0.5f, 0.f, A.canvasH - R.h);
        } else {
          R.x = std::clamp(cx, 0.f, (float)A.canvasW) - R.w * 0.5f; R.y = std::clamp(cy, 0.f, (float)A.canvasH) - R.h * 0.5f;
        }
      } else {   // rotate about the centre; Shift snaps to 15 degrees
        float ccx = R.x + R.w * 0.5f, ccy = R.y + R.h * 0.5f;
        float r = dragRot0 + (std::atan2(mo.y - ccy, mo.x - ccx) - dragAng0) / kDegToRad;
        if (ImGui::GetIO().KeyShift) r = std::round(r / 15.f) * 15.f;
        r -= 360.f * std::floor((r + 180.f) / 360.f);   // keep within -180..180
        R.rot = std::fabs(r) < 0.05f ? 0.f : r;
      }
      if (snapOn && dragKind == 5) SnapRectMove(R, BuildSnap(sc, sl, mk, onMask, snapSkip), snapThr);
      if (onQuad) { ImVec2 qc[4]; RectCorners(R, qc); for (int k = 0; k < 4; ++k) sl->q[k] = inOutput(qc[k]); }
      else if (onMask) { mk->x = R.x; mk->y = R.y; mk->w = R.w; mk->h = R.h; mk->rot = R.rot; MaskRebuild(*mk); }
      else {   // the slice's rect is whole pixels: round the size first, then keep the centre
        float cx = R.x + R.w * 0.5f, cy = R.y + R.h * 0.5f;
        sl->iw = (int)std::lround(R.w); sl->ih = (int)std::lround(R.h);
        sl->ix = (int)std::lround(cx - sl->iw * 0.5f); sl->iy = (int)std::lround(cy - sl->ih * 0.5f); sl->irot = R.rot;
      }
    }
  }

  bool scVis = sc->visible;
  // anywhere on the stage, not just inside the output box — a handle dragged outside must be grabbable again
  bool overStage = inArea && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  bool clickPending = ImGui::IsMouseClicked(0) && overStage && !dragKind && !A.mapHand;   // the hand tool only pans

  bool frameTookClick = false;   // the selected slice's own frame (handles / rings / body) has first claim on a click
    auto frame = [&](const RectXf& R, uint32_t hex, int target, int phase) {   // target: 0 slice input rect, 1 mask, 2 output quad (4-key); phase: -1 both, 0 hit-test only, 1 draw only
      const bool onMask = target == 1, onQuad = target == 2;
      ImVec2 cp[4]; RectCorners(R, cp);
      ImVec2 cpx[4], mpx[4], mcv[4];
      for (int i = 0; i < 4; ++i) { cpx[i] = toPx(cp[i]); mcv[i] = ImVec2((cp[i].x + cp[(i + 1) % 4].x) * 0.5f, (cp[i].y + cp[(i + 1) % 4].y) * 0.5f); mpx[i] = toPx(mcv[i]); }
      ImVec2 ctrCv(R.x + R.w * 0.5f, R.y + R.h * 0.5f), ctrPx = toPx(ctrCv);
      const float kSqR = 8.f, kRingR = 16.f;
      auto dist = [&](ImVec2 a2, ImVec2 b2) { return std::hypot(a2.x - b2.x, a2.y - b2.y); };
      const bool live = !A.maskPen && !A.mapHand;
      int hot = 0, hotIdx = 0;   // what the pointer is over: 2 resize, 6 rotate, 5 move
      if (inArea && dragKind == 0 && live && !frameTookClick) {
        for (int i = 0; i < 4 && !hot; ++i) if (dist(m, cpx[i]) <= kSqR) { hot = 2; hotIdx = i; }
        for (int i = 0; i < 4 && !hot; ++i) if (dist(m, mpx[i]) <= kSqR) { hot = 2; hotIdx = 10 + i; }
        for (int i = 0; i < 4 && !hot; ++i) if (dist(m, cpx[i]) <= kRingR) { hot = 6; hotIdx = i; }
        // the whole side is a handle too, not just its middle square: grab anywhere along an edge to resize that side
        for (int i = 0; i < 4 && !hot; ++i) {
          ImVec2 a = cpx[i], b = cpx[(i + 1) % 4], ab(b.x - a.x, b.y - a.y);
          float l2 = ab.x * ab.x + ab.y * ab.y; if (l2 < 1.f) continue;
          float t = std::clamp(((m.x - a.x) * ab.x + (m.y - a.y) * ab.y) / l2, 0.f, 1.f);
          if (std::hypot(a.x + ab.x * t - m.x, a.y + ab.y * t - m.y) <= 6.f) { hot = 2; hotIdx = 10 + i; }
        }
        if (!hot && PointInPoly(m, cpx, 4)) hot = 5;
      }
      if (hot) {
        bool edge = hot == 2 && hotIdx >= 10 && R.rot == 0.f;   // an upright rect's sides only go one way
        ImGui::SetMouseCursor(hot == 5 ? ImGuiMouseCursor_Hand : edge ? ((hotIdx % 10) % 2 == 0 ? ImGuiMouseCursor_ResizeNS : ImGuiMouseCursor_ResizeEW) : ImGuiMouseCursor_ResizeAll);
      }
      if (hot && clickPending && phase != 1) {
        frameTookClick = true;
        A.pushHist(); dragKind = hot; dragIdx = hotIdx; dragOnMask = onMask; dragOnQuad = onQuad;
        if (hot == 2) dragAnchor = hotIdx >= 10 ? mcv[(hotIdx % 10 + 2) % 4] : cp[(hotIdx + 2) % 4];   // opposite corner / edge middle
        else if (hot == 5) dragOff = Vsub(ctrCv, mo);
        else { dragAng0 = std::atan2(mo.y - ctrCv.y, mo.x - ctrCv.x); dragRot0 = R.rot; }
      }
      if (phase == 0) return;
      if (!onMask && !onQuad) {
        if (rClick && live && PointInPoly(m, cpx, 4)) InputRectMenu(m);
        g.dl->AddConvexPolyFilled(cpx, 4, Ca(K(hex, 0.05f)));   // faint on purpose: the source thumbnail underneath has to stay readable
        char dm[64]; int len = snprintf(dm, sizeof dm, "%d \xC3\x97 %d", sl->iw, sl->ih);
        if (sl->irot != 0.f) len += snprintf(dm + len, sizeof dm - len, " \xC2\xB7 %.0f\xC2\xB0", sl->irot);
        if (sl->iflipX || sl->iflipY) snprintf(dm + len, sizeof dm - len, " \xC2\xB7 flip %s%s", sl->iflipX ? "X" : "", sl->iflipY ? "Y" : "");
        TextC(ctrPx.x, ctrPx.y - 7, MONO_B, 11, K(hex), sl->name.c_str());
        TextC(ctrPx.x, ctrPx.y + 7, MONO_R, 9, K(pal::tcc), dm);
      }
      if (!onQuad) g.dl->AddPolyline(cpx, 4, Ca(K(hex, onMask ? 0.8f : 1.f)), ImDrawFlags_Closed, onMask ? 1.5f : 2.f);
      const bool rotating = dragKind == 6 || (dragKind == 0 && hot == 6);
      if (live) for (int i = 0; i < 4; ++i) {
        bool ringHot = rotating && (dragKind == 6 || hotIdx == i);
        g.dl->AddCircle(cpx[i], 11.f, Ca(K(ringHot ? pal::white : hex)), 24, 1.5f);
      }
      if (live) for (const ImVec2* set : {cpx, mpx}) for (int i = 0; i < 4; ++i) {
        ImRect hr(set[i].x - 5, set[i].y - 5, set[i].x + 5, set[i].y + 5);
        Box(hr, K(pal::white), K(hex), 2);
        Border(hr, K(hex), 2, 2);
      }
    };
  if (A.mpage == 0) {
    if (!sl) A.cancelMaskPen();
    // The other slices of this screen show only their input outline (no handles), so it is clear which parts of the source
    // are already taken while another slice is being edited. Drawn first so the selected slice's frame stays on top.
    if (scVis) for (auto& o : sc->slices) {
      if (!o.visible || (sl && o.id == sl->id)) continue;
      ImVec2 oc[4], opx[4]; InputCorners(o, oc);
      for (int i = 0; i < 4; ++i) opx[i] = toPx(oc[i]);
      if (A.MapKind() == 1 && A.mapSelCount() > 1 && A.mapIsSel(1, sc->id, o.id, "")) g.dl->AddPolyline(opx, 4, Ca(K(pal::cyan)), ImDrawFlags_Closed, 2.f);   // selected together with the primary: solid
      else DashedPoly(opx, 4, K(pal::cyan, 0.7f), 1.5f, 8, 5);   // dashed and unnamed: only the selected slice carries its name
    }
    // ---- transform frames: the slice's input rect, or — when a mask is selected — that mask ----
    // Both are edited the same way (like the Preview Cue frame): drag inside to move, the small squares (corners and edge middles)
    // to resize, the rings around the corners to rotate. Only the SELECTED mask is shown; the others stay hidden until picked in the tree.
    const bool maskMode = mk != nullptr && A.selKind == 2;
    if (scVis && sl && sl->visible && A.maskPen) {   // pen: click points, click the first point / Enter / double-click closes
      frameTookClick = true;
      if (clickPending) {
        bool closes = A.penPts.size() >= 3 && std::hypot(m.x - toPx(A.penPts[0]).x, m.y - toPx(A.penPts[0]).y) <= 10.f;
        if (closes || ImGui::IsMouseDoubleClicked(0)) A.finishMaskPen();
        else A.penPts.push_back(inCanvas(mo));
      }
      if (A.maskPen && ImGui::IsKeyPressed(ImGuiKey_Enter, false)) A.finishMaskPen();
      if (inArea) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (sl && sl->visible && scVis) {
      if (maskMode) {
        // the slice is just an outline while its mask is edited; clicking it (outside the mask's frame) goes back to the slice
        ImVec2 sc4[4], sp4[4]; InputCorners(*sl, sc4); for (int i = 0; i < 4; ++i) sp4[i] = toPx(sc4[i]);
        g.dl->AddPolyline(sp4, 4, Ca(K(pal::cyan, 0.8f)), ImDrawFlags_Closed, 1.5f);
        for (auto& M : sl->masks) if (M.id != mk->id && A.mapSelCount() > 1 && A.mapIsSel(2, sc->id, sl->id, M.id)) {   // masks selected together: shown, edited via the primary
          std::vector<ImVec2> op(M.pts.size()); for (size_t i = 0; i < op.size(); ++i) op[i] = toPx(M.pts[i]);
          g.dl->AddConcavePolyFilled(op.data(), (int)op.size(), Ca(K(pal::yellow, 0.18f)));
          DashedPoly(op.data(), (int)op.size(), K(pal::yellow, 0.8f), 1.5f, 9, 7);
        }
        std::vector<ImVec2> pp(mk->pts.size()); for (size_t i = 0; i < pp.size(); ++i) pp[i] = toPx(mk->pts[i]);
        g.dl->AddConcavePolyFilled(pp.data(), (int)pp.size(), Ca(K(pal::yellow, 0.30f)));   // the mask tone of the design: yellow
        DashedPoly(pp.data(), (int)pp.size(), K(pal::yellow), 2.f, 9, 7);
        frame(RectOfMask(*mk), pal::yellow, 1, -1);
        if (clickPending && !frameTookClick && !A.maskPen && PointInPoly(m, sp4, 4)) { A.selKind = 1; A.selMk.clear(); frameTookClick = true; }
      } else frame(RectOfSlice(*sl), pal::cyan, 0, -1);
    }
    if (A.maskPen && scVis) {   // the shape being drawn, plus a rubber-band segment to the cursor
      std::vector<ImVec2> pp(A.penPts.size()); for (size_t i = 0; i < pp.size(); ++i) pp[i] = toPx(A.penPts[i]);
      for (size_t i = 0; i + 1 < pp.size(); ++i) g.dl->AddLine(pp[i], pp[i + 1], Ca(K(pal::yellow)), 2.f);
      if (!pp.empty()) g.dl->AddLine(pp.back(), m, Ca(K(pal::yellow, 0.6f)), 1.5f);
      for (size_t i = 0; i < pp.size(); ++i) {
        bool first = i == 0 && pp.size() >= 3 && std::hypot(m.x - pp[0].x, m.y - pp[0].y) <= 10.f;
        g.dl->AddCircleFilled(pp[i], first ? 8.f : 5.f, Ca(K(first ? pal::white : pal::yellow)), 20);
      }
      TextC((area.Min.x + area.Max.x) * 0.5f, area.Min.y + 16, MONO_R, 10, K(pal::yellow), "PEN \xC2\xB7 click points \xC2\xB7 click the first point / Enter / double-click to close \xC2\xB7 Esc cancels");
    }
    // Clicking inside another slice's dashed outline selects it (the topmost one when they overlap).
    if (scVis && inArea && dragKind == 0 && !frameTookClick && !A.mapHand) {
      for (auto it = sc->slices.rbegin(); it != sc->slices.rend(); ++it) {
        if (!it->visible || (sl && it->id == sl->id)) continue;
        ImVec2 oc[4], opx[4]; InputCorners(*it, oc);
        for (int i = 0; i < 4; ++i) opx[i] = toPx(oc[i]);
        if (!PointInPoly(m, opx, 4)) continue;
        CursorHand();
        if (clickPending) {
          if (MultiMod()) A.mapToggle(1, {sc->id, it->id, ""});
          else { A.mapMulti.clear(); A.selSc = sc->id; A.selSl = it->id; A.selMk.clear(); A.selKind = 1; }
        }
        break;
      }
    }
  } else if (scVis) {
    bool consumed = false;
    auto polyPx = [&](const ImVec2* q, int n, ImVec2* out) { for (int i = 0; i < n; ++i) out[i] = toPx(q[i]); };
    auto outlinePx = [&](const Slice& S) { std::vector<ImVec2> o = SliceOutline(S); for (auto& p : o) p = toPx(p); return o; };
    if (rClick && inArea && !A.mapHand) {   // right-click a slice: select it and offer the same list as the Input frame's menu
      for (auto it = sc->slices.rbegin(); it != sc->slices.rend(); ++it) if (it->visible) {
        auto ol = outlinePx(*it);
        if (!PointInPoly(m, ol.data(), (int)ol.size())) continue;
        if (!(sl && it->id == sl->id)) { A.mapMulti.clear(); A.selSc = sc->id; A.selSl = it->id; A.selMk.clear(); A.selKind = 1; sl = A.curSlice(); }
        InputRectMenu(m, true);
        break;
      }
    }
    if (sl && sl->visible && sl->warp == 0 && !A.maskPen) { frame(RectOfQuad(sl->q), pal::coral, 2, 0); consumed = frameTookClick; }   // 4-key: hit-test first, drawn on top of the thumbnails below
    // handles keep a fixed on-screen size at any zoom (they used to scale with it and vanish when zoomed out)
    const float kCornerR = 8.f, kMeshR = 5.5f, kGrabPad = 4.f;
    auto nearPt = [&](ImVec2 outPt, float rad) { ImVec2 p = toPx(outPt); return std::hypot(m.x - p.x, m.y - p.y) <= rad; };
    // forget picked points that no longer exist (a slice or mesh point was deleted / re-meshed)
    A.mapPts.erase(std::remove_if(A.mapPts.begin(), A.mapPts.end(), [&](const App::PtRef& r) { Slice* ps = SliceById(*sc, r.sl); ImVec2 tmp; return !ps || !PointPos(*ps, r.idx, tmp); }), A.mapPts.end());
    auto grab = [&](int kind, int idx, ImVec2 at) { A.mapPts.clear(); A.pushHist(); dragKind = kind; dragIdx = idx; dragOff = Vsub(at, mo); consumed = true; };
    if (clickPending && !consumed && A.mapPts.size() > 1) {   // pressing one of several picked points drags the whole group
      for (auto& r : A.mapPts) if (Slice* ps = SliceById(*sc, r.sl)) {
        ImVec2 pos; if (!PointPos(*ps, r.idx, pos) || !nearPt(pos, kCornerR + kGrabPad)) continue;
        A.pushHist(); gGroup = CapturePoints(*sc, A.mapPts); gGrabStart = pos; dragKind = 7; dragIdx = 0; dragOff = Vsub(pos, mo); consumed = true; break;
      }
    }
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
    if (sl && sl->visible && sl->warp != 0 && clickPending && !consumed)   // free corners belong to Mesh mode; 4-key edits a rectangle
      for (int i = 0; i < 4 && !consumed; ++i) if (nearPt(sl->q[i], kCornerR + kGrabPad)) grab(1, i, sl->q[i]);
    if (clickPending && !consumed && sl && sl->visible && sl->warp != 0) {
      auto gr = MeshGrid(*sl);
      int R = (int)gr.size(), C = (int)gr[0].size();
      for (int rr = 0; rr < R && !consumed; ++rr) for (int cc = 0; cc < C && !consumed; ++cc) {
        bool corner = (rr == 0 || rr == R - 1) && (cc == 0 || cc == C - 1);   // the mesh's corners ARE the keystone corners
        if (!corner && nearPt(gr[rr][cc], kMeshR + kGrabPad)) grab(4, rr * 100 + cc, gr[rr][cc]);
      }
    }
    if (clickPending && !consumed) {
      for (auto it = sc->slices.rbegin(); it != sc->slices.rend() && !consumed; ++it) if (it->visible) {
        auto ol = outlinePx(*it);
        if (!PointInPoly(m, ol.data(), (int)ol.size())) continue;
        if (MultiMod()) { A.mapToggle(1, {sc->id, it->id, ""}); consumed = true; break; }   // Ctrl / Cmd / Shift: add or remove
        bool wasOn = sl && it->id == sl->id;
        A.mapMulti.clear();
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
    {   // what each slice actually sends out — after its input rect, warp, masks and the Screen's opacity / colour — drawn as its thumbnail
      bool anySolo = false;
      for (auto& S : sc->slices) if (S.solo && S.visible) anySolo = true;
      const float t = (float)g.time * 1.2f;   // same time base as the projector and the Live Output monitor
      g.dl->PushClipRect(cv.Min, cv.Max, true);   // the projector only shows the 1920x1080 box
      for (auto& S : sc->slices) if (S.visible && !(anySolo && !S.solo)) DrawSliceOutput(*sc, S, cv.Min.x, cv.Min.y, s, s, t);
      g.dl->PopClipRect();
    }
    for (auto& S : sc->slices) {
      if (!S.visible) continue;
      bool on = (sl && S.id == sl->id) || (A.MapKind() == 1 && A.mapSelCount() > 1 && A.mapIsSel(1, sc->id, S.id, ""));
      // no tinted fill on the Output page: a slice is only an outline, so the thumbnail underneath shows exactly what it sends out
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
    if (sl && sl->visible && sl->warp == 0) frame(RectOfQuad(sl->q), pal::coral, 2, 1);   // 4-key: the same frame as Input — squares, turning rings, drag inside to move
    else if (sl && sl->visible) for (int i = 0; i < 4; ++i) {
      ImVec2 p = toPx(sl->q[i]);
      bool hot = (dragKind == 1 && dragIdx == i) || (!dragKind && overStage && nearPt(sl->q[i], kCornerR + kGrabPad));
      g.dl->AddCircleFilled(p, kCornerR + (hot ? 2.f : 0.f), Ca(K(pal::coral)), 24); g.dl->AddCircle(p, kCornerR + (hot ? 2.f : 0.f), Ca(K(pal::white)), 24, 2.5f);
      if (hot) CursorHand();
    }
    for (auto& r : A.mapPts) if (Slice* ps = SliceById(*sc, r.sl)) {   // the picked points: white discs with a coral ring, on any slice
      ImVec2 pos; if (!PointPos(*ps, r.idx, pos)) continue;
      ImVec2 pp = toPx(pos); bool hotP = !dragKind && inArea && std::hypot(m.x - pp.x, m.y - pp.y) <= kCornerR + kGrabPad;
      g.dl->AddCircleFilled(pp, kCornerR + (hotP ? 2.f : 0.f), Ca(K(pal::white)), 24); g.dl->AddCircle(pp, kCornerR + (hotP ? 2.f : 0.f), Ca(K(pal::coral)), 24, 2.5f);
      if (hotP) CursorHand();
    }
    // live readout of the point being dragged — most useful exactly when it is outside the output box
    if (dragKind == 1 || dragKind == 4) {
      ImVec2 at = dragKind == 1 ? sl->q[dragIdx] : ImVec2(0, 0);
      if (dragKind == 4) { auto gr = MeshGrid(*sl); int rr = dragIdx / 100, cc = dragIdx % 100; if (rr < (int)gr.size() && cc < (int)gr[rr].size()) at = gr[rr][cc]; }
      char rd[40]; snprintf(rd, sizeof rd, "%d, %d", (int)std::round(at.x), (int)std::round(at.y));
      float tw = TextW(MONO_B, 10, rd);
      ImRect tb2(m.x + 14, m.y + 12, m.x + 14 + tw + 12, m.y + 12 + 18);
      Box(tb2, K(0x000000, 0.8f), K(pal::coral, 0.6f), 3);
      Text(tb2.Min.x + 6, (tb2.Min.y + tb2.Max.y) * 0.5f, MONO_B, 10, K(pal::white), rd);
    }
  }
  {   // marquee: drag a rectangle on the stage to pick the points inside (Output) or the slices it touches (Input); Ctrl/Cmd/Shift adds
    const bool armOk = overStage && !A.mapHand && !A.maskPen && scVis;
    if (gMarquee == 0 && ImGui::IsMouseClicked(0) && armOk && dragKind == 0) { gMarquee = 1; gMarqueeA = m; }
    if (gMarquee) {
      if (!ImGui::IsMouseDown(0)) {
        const bool add = MultiMod();
        if (gMarquee == 2) {
          ImRect mr(std::min(gMarqueeA.x, m.x), std::min(gMarqueeA.y, m.y), std::max(gMarqueeA.x, m.x), std::max(gMarqueeA.y, m.y));
          if (A.mpage == 1) {
            std::vector<App::PtRef> hit;
            for (auto& S : sc->slices) if (S.visible) {
              if (S.warp != 0) for (int i = 0; i < 4; ++i) if (mr.Contains(toPx(S.q[i]))) hit.push_back({S.id, i});
              if (sl && S.id == sl->id && S.warp != 0) {
                auto gr = MeshGrid(S); int R = (int)gr.size(), C = (int)gr[0].size();
                for (int rr = 0; rr < R; ++rr) for (int cc = 0; cc < C; ++cc)
                  if (!((rr == 0 || rr == R - 1) && (cc == 0 || cc == C - 1)) && mr.Contains(toPx(gr[rr][cc]))) hit.push_back({S.id, 1000 + rr * 100 + cc});
              }
            }
            if (add) { for (auto& h : hit) { bool have = false; for (auto& q : A.mapPts) if (q.sl == h.sl && q.idx == h.idx) have = true; if (!have) A.mapPts.push_back(h); } }
            else A.mapPts = hit;
          } else {
            std::vector<App::MapRef> refs;
            if (add && A.MapKind() == 1) refs = A.mapSelection();
            for (auto& S : sc->slices) if (S.visible) {
              ImVec2 rc[4]; InputCorners(S, rc); ImVec2 lo = toPx(rc[0]), hi = lo;
              for (int i = 1; i < 4; ++i) { ImVec2 q = toPx(rc[i]); lo = ImVec2(std::min(lo.x, q.x), std::min(lo.y, q.y)); hi = ImVec2(std::max(hi.x, q.x), std::max(hi.y, q.y)); }
              bool touches = lo.x <= mr.Max.x && hi.x >= mr.Min.x && lo.y <= mr.Max.y && hi.y >= mr.Min.y, have = false;
              for (auto& r : refs) if (r.sl == S.id) have = true;
              if (touches && !have) refs.push_back({sc->id, S.id, ""});
            }
            if (!refs.empty()) A.mapSelectRefs(1, refs);
          }
        } else if (!add && A.mpage == 1) A.mapPts.clear();   // a plain click on the stage lets go of the picked points
        gMarquee = 0;
      } else {
        if (gMarquee == 1 && std::hypot(m.x - gMarqueeA.x, m.y - gMarqueeA.y) > 4.f) gMarquee = 2;
        if (dragKind != 0) gMarquee = 0;
        if (gMarquee == 2) {
          ImRect mr(std::min(gMarqueeA.x, m.x), std::min(gMarqueeA.y, m.y), std::max(gMarqueeA.x, m.x), std::max(gMarqueeA.y, m.y));
          g.dl->AddRectFilled(mr.Min, mr.Max, Ca(K(pal::white, 0.06f)));
          ImVec2 mp[4] = {mr.Min, ImVec2(mr.Max.x, mr.Min.y), mr.Max, ImVec2(mr.Min.x, mr.Max.y)};
          DashedPoly(mp, 4, K(pal::white, 0.85f), 1.5f, 6, 4);
        }
      }
    }
  }
  for (auto& gl : gGuideLines) g.dl->AddLine(toPx(gl.first), toPx(gl.second), Ca(K(pal::white, 0.9f)), 2.f);   // what the magnet locked onto
  for (auto& gd : gGuideDots) g.dl->AddCircle(toPx(gd), 7.f, Ca(K(pal::white, 0.9f)), 16, 1.5f);
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

// A number cell in the app's own property style: small mono label above a field (the "Input rectangle" grid look).
static bool NumCell(float x, float y, float w, const char* label, const char* id, float& v, int decimals) {
  Text(x, y + 4.5f, MONO_R, 9, K(pal::t66), label);
  return FloatField(id, Rc(x, y + 11, w, 24), v, decimals);
}
// A slider row like the Clip transform panel: label left, toned mono value right, the slider underneath. Returns the height used.
static float SliderRow(float x, float y, float w, const char* label, const char* val, uint32_t id, float& v, float mn, float mx, uint32_t hex, bool* changed = nullptr) {
  Text(x, y + 5, UI_S, 10, K(pal::t88), label);
  TextR(x + w, y + 5, MONO_B, 10, K(hex), val);
  bool c = Slider(id, Rc(x, y + 10 + 6 + 4, w, 6), v, hex, mn, mx);
  if (changed) *changed = c;
  return 10 + 6 + 14 + 8;
}
// The INPUT MASK shape buttons (heart, square, circle, triangle, hexagon, pen). With a slice selected they ADD a mask; with a mask
// selected (forMask) they RE-SHAPE it (the pen redraws its outline). Returns the y below the buttons.
static float InputMaskBar(float ox, float oy, float W, float x, float w, float y, bool forMask) {
  HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 6;
  Label(x, oy + y, forMask ? "Mask shape" : "Input mask"); y += 9 + 4;
  const int shapes[5] = {App::MS_HEART, App::MS_SQUARE, App::MS_CIRCLE, App::MS_TRIANGLE, App::MS_HEXAGON};
  const Mask* cur = forMask ? A.curMask() : nullptr;
  const float bw = 34.f, gap = (w - 6 * bw) / 5.f;
  for (int i = 0; i < 6; ++i) {
    ImRect br(x + i * (bw + gap), oy + y, x + i * (bw + gap) + bw, oy + y + 30);
    Hit h = HitR(br);
    bool on = i == 5 ? (A.maskPen || (cur && cur->shape < 0)) : (cur && cur->shape == shapes[i]);
    Box(br, h.hover ? K(pal::ctrlHover) : K(pal::g1c), on ? K(pal::yellow) : h.hover ? K(pal::g33) : K(pal::g22), 3);
    ImVec2 c((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f);
    ImU32 col = Ca(K(on ? pal::yellow : h.hover ? pal::white : pal::tcc));
    if (i == 5) Icon("pencil", c, 15, col);
    else { auto gp = MaskShapePts(shapes[i], c, 7.f, 7.f); g.dl->AddPolyline(gp.data(), (int)gp.size(), col, ImDrawFlags_Closed, 1.6f); }
    if (h.hover) CursorHand();
    if (h.click) {
      if (i == 5) A.startMaskPen(forMask);
      else if (forMask) A.setMaskShape(shapes[i]);
      else A.addMask(shapes[i]);
    }
  }
  return y + 30 + 6;
}

static void PropsPanel(ImRect r) {
  Fill(r, K(pal::g12));
  VLine(r.Min.x, r.Min.y, r.Max.y, K(pal::g2a));
  Screen* sc = A.curScreen(); Slice* sl = A.curSlice(); Mask* mk = A.curMask();
  int kind = A.MapKind();
  if (kind == 2 && !mk) kind = 1;
  if (kind == 1 && !sl) kind = 0;
  PanelHeader(Rc(r.Min.x + 1, r.Min.y, r.GetWidth() - 1, 24), kind == 0 ? "Screen properties" : kind == 2 ? "Mask properties" : "Slice properties", pal::t88);
  {
    int nsel = A.MapKind() == kind ? A.mapSelCount() : 1;
    std::string bl = kind == 0 ? "Screen" : kind == 2 ? "Mask" : "Slice";
    if (nsel > 1) bl = std::to_string(nsel) + " " + bl + "s";   // several selected together: the panel edits the primary, copy / delete take all
    Badge(r.Max.x - 6, r.Min.y + 11.5f, bl.c_str(), kind == 0 ? T_AUDIO : kind == 2 ? T_STANDBY : T_LIVE, true);
  }
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
    if (output) {
      // ── Output slice properties (info as in Resolume's slice panel, MikMap's own widgets) ──
      auto section = [&](const char* title, const char* note = nullptr) {
        HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 6;
        Text(x, oy + y + 4.5f, MONO_R, 9, K(pal::t88), Upper(title).c_str(), 0.09f);
        if (note) TextR(x + w, oy + y + 4.5f, MONO_R, 8, K(pal::t66), note);
        y += 9 + 6;
      };
      auto checkRow = [&](const char* label, bool& v, uint32_t hex, bool dim = false) {
        ImRect ir(x, oy + y, x + w, oy + y + 24);
        Hit ih = HitR(ir);
        Text(x, ir.Min.y + 12, UI_S, 10, K(dim ? pal::t66 : pal::te0), label);
        ImRect cb(ir.Max.x - 14, ir.Min.y + 5, ir.Max.x, ir.Min.y + 19);
        Box(cb, v ? K(hex) : K(pal::g050), v ? K(hex) : K(pal::g22), 2);
        if (v) Check(ImVec2((cb.Min.x + cb.Max.x) * 0.5f, (cb.Min.y + cb.Max.y) * 0.5f), 12, K(0x0f0f0f));
        if (ih.hover) CursorHand();
        bool ch = ih.click; if (ch) v = !v;
        y += 24 + 4;
        return ch;
      };
      auto intRow = [&](const char* label, int& v, int lo, int hi, uint32_t id, uint32_t hex) {
        float fv = (float)v; char vb[24]; snprintf(vb, sizeof vb, "%d", v); bool ch = false;
        y += SliderRow(x, oy + y, w, label, vb, id, fv, (float)lo, (float)hi, hex, &ch);
        if (ch) v = (int)std::lround(std::clamp(fv, (float)lo, (float)hi));
      };
      auto fltRow = [&](const char* label, float& v, float lo, float hi, uint32_t id, uint32_t hex, bool dim) {
        char vb[24]; snprintf(vb, sizeof vb, "%.2f", v); bool ch = false; float prev = g.alpha; if (dim) g.alpha *= 0.5f;
        y += SliderRow(x, oy + y, w, label, vb, id, v, lo, hi, hex, &ch);
        g.alpha = prev;
      };
      HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 6;
      Label(x, oy + y, "Flip"); y += 9 + 4;
      {   // 0 none, 1 mirror X, 2 mirror Y, 3 both
        const char* fl4[4] = {"NONE", "X", "Y", "X+Y"}; float bw = (w - 3 * 4) / 4.f;
        for (int i = 0; i < 4; ++i) {
          ImRect br(x + i * (bw + 4), oy + y, x + i * (bw + 4) + bw, oy + y + 26);
          Hit h = HitR(br); bool on = sl->oflip == i;
          Box(br, on ? K(pal::coral, 0.2f) : h.hover ? K(pal::ctrlHover) : K(pal::g1c), on ? K(pal::coral) : K(pal::g22), 3);
          TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, MONO_B, 9, K(on ? pal::coral : pal::tcc), fl4[i], 0.06f);
          if (h.hover) CursorHand();
          if (h.click && !on) { A.pushHist(); sl->oflip = i; }
        }
        y += 26 + 8;
      }
      checkRow("Is key", sl->isKey, pal::coral);
      checkRow("Black BG", sl->blackBg, pal::coral);
      y += 2;
      intRow("Brightness", sl->brightness, -100, 100, 0x3201, pal::yellow);
      intRow("Contrast", sl->contrast, -100, 100, 0x3202, pal::yellow);
      intRow("Red", sl->red, -100, 100, 0x3203, pal::red);
      intRow("Green", sl->green, -100, 100, 0x3204, pal::mint);
      intRow("Blue", sl->blue, -100, 100, 0x3205, pal::cyan);
      section("Soft edge", "NOT RENDERED YET");
      checkRow("Enabled", sl->softEdge, pal::mint);
      fltRow("Gamma red", sl->seGammaR, 0.1f, 4.f, 0x3211, pal::red, !sl->softEdge);
      fltRow("Gamma green", sl->seGammaG, 0.1f, 4.f, 0x3212, pal::mint, !sl->softEdge);
      fltRow("Gamma blue", sl->seGammaB, 0.1f, 4.f, 0x3213, pal::cyan, !sl->softEdge);
      fltRow("Gamma", sl->seGamma, 0.1f, 4.f, 0x3214, pal::yellow, !sl->softEdge);
      fltRow("Luminance", sl->seLum, 0.f, 1.f, 0x3215, pal::yellow, !sl->softEdge);
      fltRow("Power", sl->sePower, 0.1f, 8.f, 0x3216, pal::yellow, !sl->softEdge);
      section("Black level compensation", "NOT RENDERED YET");
      intRow("Red", sl->blR, 0, 100, 0x3221, pal::red);
      intRow("Green", sl->blG, 0, 100, 0x3222, pal::mint);
      intRow("Blue", sl->blB, 0, 100, 0x3223, pal::cyan);
      section("Warping");
      Text(x, oy + y + 5, UI_S, 10, K(pal::t88), "Point mode");
      TextR(x + w, oy + y + 5, MONO_B, 10, K(pal::coral), "Linear");
      y += 10 + 8;
    }
    if (output && sl->warp != 0) {
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
    if (output && sl->warp == 0) {   // 4-key: the output is one rectangle, edited like the input rect
      HLine(ox + 8, ox + 8 + w, oy + y, K(pal::g2a)); y += 1 + 6;
      Label(x, oy + y, "Output rectangle (px)"); y += 9 + 4;
      RectXf R = RectOfQuad(sl->q);
      float vals[7] = {R.x + R.w * 0.5f, R.y + R.h * 0.5f, R.x, R.y, R.w, R.h, R.rot};
      const char* fl[7] = {"X", "Y", "Left", "Top", "Width", "Height", "Rotation"};
      float fw = (w - 6) / 2.f;
      for (int i = 0; i < 7; ++i) {
        float fx = x + (i % 2) * (fw + 6), fy = y + (i / 2) * (35 + 6);
        char id[24]; snprintf(id, sizeof id, "##op%d", i);
        if (!NumCell(fx, oy + fy, fw, fl[i], id, vals[i], i == 6 ? 1 : 0)) continue;
        float v = std::round(vals[i]);
        if (i == 0) R.x = v - R.w * 0.5f; else if (i == 1) R.y = v - R.h * 0.5f;
        else if (i == 2) R.x = v; else if (i == 3) R.y = v;
        else if (i == 4) { float cx = R.x + R.w * 0.5f; R.w = std::clamp(v, 8.f, 16384.f); R.x = cx - R.w * 0.5f; }
        else if (i == 5) { float cy = R.y + R.h * 0.5f; R.h = std::clamp(v, 8.f, 16384.f); R.y = cy - R.h * 0.5f; }
        else R.rot = std::fabs(vals[6]) < 0.05f ? 0.f : std::clamp(vals[6], -180.f, 180.f);
        ImVec2 qc[4]; RectCorners(R, qc); for (int k = 0; k < 4; ++k) sl->q[k] = ImVec2(std::clamp(qc[k].x, -4000.f, 8000.f), std::clamp(qc[k].y, -4000.f, 8000.f));
      }
      y += 41 * 4 + 2;
    } else if (output) {
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
      // X/Y are the rect's centre, Left/Top its unrotated top-left corner (what the stage frame edits)
      float vals[7] = {(float)(sl->ix + sl->iw / 2), (float)(sl->iy + sl->ih / 2), (float)sl->ix, (float)sl->iy, (float)sl->iw, (float)sl->ih, sl->irot};
      const char* fl[7] = {"X", "Y", "Left", "Top", "Width", "Height", "Rotation"};
      float fw = (w - 6) / 2.f;
      for (int i = 0; i < 7; ++i) {
        float fx = x + (i % 2) * (fw + 6), fy = y + (i / 2) * (35 + 6);
        char id[24]; snprintf(id, sizeof id, "##sp%d", i);
        if (!NumCell(fx, oy + fy, fw, fl[i], id, vals[i], i == 6 ? 1 : 0)) continue;
        int iv = (int)std::lround(vals[i]);
        if (i == 0) sl->ix = iv - sl->iw / 2; else if (i == 1) sl->iy = iv - sl->ih / 2;
        else if (i == 2) sl->ix = iv; else if (i == 3) sl->iy = iv;
        else if (i == 4) sl->iw = std::clamp(iv, 20, 16384); else if (i == 5) sl->ih = std::clamp(iv, 20, 16384);
        else sl->irot = std::fabs(vals[6]) < 0.05f ? 0.f : std::clamp(vals[6], -180.f, 180.f);
      }
      y += 41 * 4 + 2;
    }
    // Input Mask: pick a shape to add a mask to this slice (pen = draw your own outline). Masks are edited on this Input stage.
    y = InputMaskBar(ox, oy, W, x, w, y, false);
    Text(x, oy + y + 6, UI_S, 9, K(pal::t66), "Add a mask that cuts the picture.", 0.01f);
    y += 16 + 6;
  }
  if (kind == 2) {
    Label(x, oy + y, "Mask name"); y += 9 + 6;
    TextField("##maskname", Rc(x, oy + y, w, 28), mk->name);
    y += 28 + 6;
    HLine(x, x + w, oy + y, K(pal::g2a)); y += 1 + 4;
    {   // Invert: on = cut a hole (the default), off = keep only the inside
      ImRect ir(x, oy + y, x + w, oy + y + 24);
      Hit ih = HitR(ir);
      Text(x, ir.Min.y + 12, UI_S, 10, K(pal::te0), "Invert (cut hole)");
      ImRect cb(ir.Max.x - 14, ir.Min.y + 5, ir.Max.x, ir.Min.y + 19);
      Box(cb, mk->inverted ? K(pal::yellow) : K(pal::g050), mk->inverted ? K(pal::yellow) : K(pal::g22), 2);
      if (mk->inverted) Check(ImVec2((cb.Min.x + cb.Max.x) * 0.5f, (cb.Min.y + cb.Max.y) * 0.5f), 12, K(0x0f0f0f));
      if (ih.hover) CursorHand();
      if (ih.click) mk->inverted = !mk->inverted;
      y += 24 + 6;
    }
    Text(x, oy + y + 5, UI_S, 10, K(pal::t88), "Feather");
    char fb[16]; snprintf(fb, sizeof fb, "%dpx", mk->feather);
    TextR(x + w, oy + y + 5, MONO_B, 10, K(pal::yellow), fb);
    y += 10 + 6;
    float fv = mk->feather / 40.f * 100.f;
    if (Slider(0x3001, Rc(x, oy + y + 4, w, 6), fv, pal::yellow)) mk->feather = (int)std::round(fv / 100.f * 40.f);
    y += 14 + 6;
    HLine(x, x + w, oy + y, K(pal::g2a)); y += 1 + 6;
    Label(x, oy + y, "Mask rectangle (px)"); y += 9 + 4;
    {   // same fields as the slice's input rectangle: X/Y = centre, Left/Top = unrotated corner
      float vals[7] = {mk->x + mk->w * 0.5f, mk->y + mk->h * 0.5f, mk->x, mk->y, mk->w, mk->h, mk->rot};
      const char* fl[7] = {"X", "Y", "Left", "Top", "Width", "Height", "Rotation"};
      float fw = (w - 6) / 2.f; bool ch = false;
      for (int i = 0; i < 7; ++i) {
        float fx = x + (i % 2) * (fw + 6), fy = y + (i / 2) * (35 + 6);
        char id[24]; snprintf(id, sizeof id, "##mk%d", i);
        if (!NumCell(fx, oy + fy, fw, fl[i], id, vals[i], 1)) continue;
        ch = true;
        if (i == 0) mk->x = vals[0] - mk->w * 0.5f; else if (i == 1) mk->y = vals[1] - mk->h * 0.5f;
        else if (i == 2) mk->x = vals[2]; else if (i == 3) mk->y = vals[3];
        else if (i == 4) { float cx = mk->x + mk->w * 0.5f; mk->w = std::clamp(vals[4], 4.f, 16384.f); mk->x = cx - mk->w * 0.5f; }
        else if (i == 5) { float cy = mk->y + mk->h * 0.5f; mk->h = std::clamp(vals[5], 4.f, 16384.f); mk->y = cy - mk->h * 0.5f; }
        else mk->rot = std::fabs(vals[6]) < 0.005f ? 0.f : std::clamp(vals[6], -180.f, 180.f);
      }
      if (ch) MaskRebuild(*mk);
      y += 41 * 4 + 2;
    }
    y = InputMaskBar(ox, oy, W, x, w, y, true);
    y += 2;
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
    // Resolume's screen rows: device, size, and the colour block (opacity + brightness/contrast/RGB, each with a marker bar).
    // The colour values reach the projector window (output.cpp); width/height describe the display and the tree label.
    Label(x, oy + y, "Screen name"); y += 9 + 4;
    TextField("##scname", Rc(x, oy + y, w, 28), sc->name); y += 28 + 8;
    {   // F2/I1: the physical display this screen is sent to. This one dropdown IS the projector output choice.
      Label(x, oy + y, "Output device"); y += 9 + 4;
      ImRect dr2(x, oy + y, x + w, oy + y + 26);
      Hit dh2 = HitR(dr2);
      int devMon = DeviceMonitor(sc->outDev);
      std::string cur = devMon >= 0 ? MonitorName(devMon) : sc->outDev;   // a physical display shows its live name and resolution
      Box(dr2, dh2.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
      Icon("monitor", ImVec2(dr2.Min.x + 14, (dr2.Min.y + dr2.Max.y) * 0.5f), 11, K(pal::cyan));
      TextEll(dr2.Min.x + 28, (dr2.Min.y + dr2.Max.y) * 0.5f, dr2.GetWidth() - 50, UI_S, 10, K(pal::tf3), cur.c_str());
      Icon("chevron-down", ImVec2(dr2.Max.x - 12, (dr2.Min.y + dr2.Max.y) * 0.5f), 10, K(pal::t66));
      if (dh2.hover) CursorHand();
      if (dh2.click) {
        std::string scId = sc->id;
        std::vector<MenuItem> mi;
        for (int i = 0; i < MonitorCount(); ++i) {
          MenuItem it; it.label = MonitorName(i); it.icon = "monitor"; it.toneHex = (i == devMon) ? pal::cyan : 0;
          it.run = [i, scId] {
            A.pushHist();
            for (auto& S : A.screens) if (S.id == scId) { S.outDev = MonitorName(i); int rw, rh; if (DeviceResolution(S.outDev, rw, rh)) { S.w = rw; S.h = rh; } }
            A.outMonitor = i;
            if (OutputOpen()) OpenOutput(glfwWin(), A.outMonitor);
          };
          mi.push_back(it);
        }
        if (mi.empty()) { MenuItem it; it.label = "No display detected"; it.disabled = true; mi.push_back(it); }
        { MenuItem d; d.label = ""; d.disabled = true; d.divider = true; mi.push_back(d); }
        for (int k = 0; k < 3; ++k) {   // virtual outputs: no display window, and the only case where the resolution can be typed
          MenuItem it; it.label = kVirtualDevices[k]; it.icon = k == 0 ? "video" : k == 1 ? "layers" : "square"; it.divider = false;
          it.toneHex = sc->outDev == kVirtualDevices[k] ? pal::cyan : 0;
          it.run = [k, scId] { A.pushHist(); for (auto& S : A.screens) if (S.id == scId) S.outDev = kVirtualDevices[k]; if (OutputOpen() && A.curScreen() && A.curScreen()->id == scId) CloseOutput(); };
          mi.push_back(it);
        }
        A.openCtx(ImVec2(dr2.Min.x, dr2.Max.y + 4), mi);
      }
      y += 26 + 6;
    }
    {   // display size (the tree label) and the colour block: the same slider rows the Clip transform panel uses
      float vals[2] = {(float)sc->w, (float)sc->h}, fw = (w - 6) / 2.f;
      const char* fl[2] = {"Width", "Height"};
      int rw, rh; const bool locked = DeviceResolution(sc->outDev, rw, rh);   // a physical display: its real resolution, not editable
      for (int i = 0; i < 2; ++i) {
        char id[16]; snprintf(id, sizeof id, "##scwh%d", i);
        float fx = x + i * (fw + 6);
        if (locked) {
          Text(fx, oy + y + 4.5f, MONO_R, 9, K(pal::t66), fl[i]);
          ImRect fr(fx, oy + y + 11, fx + fw, oy + y + 35);
          Box(fr, K(pal::g12), K(pal::g22), 3);
          char vb[16]; snprintf(vb, sizeof vb, "%d", i ? sc->h : sc->w);
          Text(fr.Min.x + 8, (fr.Min.y + fr.Max.y) * 0.5f, MONO_R, 11, K(pal::t88), vb);
          TextR(fr.Max.x - 8, (fr.Min.y + fr.Max.y) * 0.5f, MONO_R, 8, K(pal::t66), "DISPLAY", 0.09f);
        } else if (NumCell(fx, oy + y, fw, fl[i], id, vals[i], 0)) (i ? sc->h : sc->w) = std::clamp((int)std::lround(vals[i]), 16, 16384);
      }
      y += 35 + 8;
    }
    auto colorRow = [&](const char* label, int& v, int lo, int hi, const char* unit, uint32_t id, uint32_t hex) {
      float fv = (float)v; char vb[24]; snprintf(vb, sizeof vb, "%d%s", v, unit);
      bool ch = false;
      y += SliderRow(x, oy + y, w, label, vb, id, fv, (float)lo, (float)hi, hex, &ch);
      if (ch) v = (int)std::lround(std::clamp(fv, (float)lo, (float)hi));
    };
    colorRow("Opacity", sc->opacity, 0, 100, "%", 0x3101, pal::coral);
    colorRow("Brightness", sc->brightness, -100, 100, "", 0x3102, pal::yellow);
    colorRow("Contrast", sc->contrast, -100, 100, "", 0x3103, pal::yellow);
    colorRow("Red", sc->red, -100, 100, "", 0x3104, pal::red);
    colorRow("Green", sc->green, -100, 100, "", 0x3105, pal::mint);
    colorRow("Blue", sc->blue, -100, 100, "", 0x3106, pal::cyan);
    HLine(x, x + w, oy + y, K(pal::g2a)); y += 1 + 4;
    {
      ImRect er(x, oy + y, x + w, oy + y + 22);
      Hit eh = HitR(er);
      Text(x, er.Min.y + 11, UI_S, 10, K(pal::t88), "Edge blending");
      const char* el = sc->edgeBlend ? "ENABLED" : "DISABLED";
      float bw2 = TextW(MONO_B, 9, el, 0.09f) + 12 + 2;
      ImRect bb(er.Max.x - bw2, er.Min.y + 2, er.Max.x, er.Min.y + 20);
      Box(bb, sc->edgeBlend ? K(pal::mint, 0.2f) : K(pal::g1c), sc->edgeBlend ? K(pal::mint, 0.4f) : K(pal::g22), 3);
      Text(bb.Min.x + 7, (bb.Min.y + bb.Max.y) * 0.5f, MONO_B, 9, K(sc->edgeBlend ? pal::mint : pal::t66), el, 0.09f);
      if (eh.hover) CursorHand();
      if (eh.click) sc->edgeBlend = !sc->edgeBlend;
      y += 22 + 8;
    }
    // ── F2/I1: open / close the output window on the display chosen in OUTPUT DEVICE above ──
    HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 8;
    {
      ImRect ob(x, oy + y, x + w, oy + y + 30);
      const bool virt = IsVirtualDevice(sc->outDev);   // NDI / Spout / Virtual have no display window (and no sender yet)
      Hit oh = HitR(ob);
      bool on = OutputOpen() && !virt;
      float prevA = g.alpha; if (virt) g.alpha *= 0.4f;
      if (on) Glow(ob, pal::coral, 0.35f, 12, 3);
      Box(ob, on ? K(pal::coral, 0.2f) : K(pal::g1c), on ? K(pal::coral) : K(pal::g22), 3);
      const char* lb = on ? "\xC4\x90\xC3\x93NG OUTPUT (F11)" : "M\xE1\xBB\x9E OUTPUT (F11)";
      TextC((ob.Min.x + ob.Max.x) * 0.5f, (ob.Min.y + ob.Max.y) * 0.5f, UI_B, 10, K(on ? pal::coral : pal::tcc), lb, 0.09f);
      g.alpha = prevA;
      if (oh.hover && !virt) CursorHand();
      if (oh.click && !virt) ToggleOutput(glfwWin(), A.outMonitor);
      y += 30 + 8;
      if (virt) { Text(x, oy + y + 2, UI_S, 9, K(pal::t66), "Virtual output: no display window. NDI / Spout sender is not built yet.", 0.01f); y += 16; }
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




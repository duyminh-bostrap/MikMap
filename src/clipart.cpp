// Procedural "video" samples: lightweight generative clips (ported from LiveCanvas.tsx drawClipContent, plus extras).
// Every clip is drawn live with ImDrawList primitives, so there are no media files to ship or decode.
// Clip transform (position / scale / rotation / flip) and opacity are applied here, matching LiveCanvas's ctx transform.
#include "app.h"
#include <cmath>
#include <cstdio>
#include <cstring>

// <windows.h> is only needed here because <GL/gl.h> requires it on Windows
// (APIENTRY/WINGDIAPI macros) — no actual Win32 API is called in this file.
#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <GL/gl.h>
#elif defined(__APPLE__)
#include <OpenGL/gl.h>
#else
#include <GL/gl.h>
#endif

#include "stb_image.h"   // declarations only; the implementation lives in main.cpp
#include "imgui_impl_opengl3.h"   // A4: ImGui_ImplOpenGL3_RenderDrawData, to render a thumbnail into an FBO
#include <map>
#include <string>

using namespace ui;

static const float TAU = 6.2831853f;

enum Style { S_PLASMA, S_HEX, S_VORTEX, S_AURORA, S_STARS, S_TUNNEL, S_RAIN, S_BARS, S_PULSE, S_COUNT };

int ClipStyleOf(const std::string& n) {
  struct M { const char* name; int st; };
  static const M table[] = {
      {"Plasma Waves 01", S_PLASMA}, {"Grid Warp", S_PLASMA},
      {"Cyber Hex Grid", S_HEX}, {"Geometric Wire", S_HEX},
      {"Particle Vortex", S_VORTEX}, {"Chrome Fold", S_VORTEX},
      {"Aurora Flow", S_AURORA}, {"Liquid Chrome", S_AURORA}, {"Deep Ambient", S_AURORA},
      {"Starfield Warp", S_STARS}, {"Slow Drift", S_STARS},
      {"Strobe Tunnel", S_TUNNEL}, {"Concentric Tunnel", S_TUNNEL},
      {"Neon Rain", S_RAIN},
      {"Spectrum Bars", S_BARS},
      {"Beat Pulse", S_PULSE}, {"Mesh Pulse", S_PULSE}, {"Kick Flash", S_PULSE}};
  for (auto& m : table) if (n == m.name) return m.st;
  unsigned h = 2166136261u;
  for (char c : n) h = (h ^ (unsigned char)c) * 16777619u;
  return (int)(h % S_COUNT);
}

static float Hash(int i, int k = 0) {
  unsigned x = (unsigned)(i * 374761393 + k * 668265263);
  x = (x ^ (x >> 13)) * 1274126177u;
  return ((x ^ (x >> 16)) & 0xffff) / 65535.f;
}
static float Fract(float v) { return v - std::floor(v); }

// ── GL blend modes (D4). glBlendEquation is GL 1.4, not in the Win32 gl.h 1.1 header, so it is loaded at runtime.
#ifndef APIENTRY  // only <windows.h> defines it; macOS/Linux <GL/gl.h> don't
#define APIENTRY
#endif
typedef void(APIENTRY* PFN_glBlendEquation)(GLenum);
static PFN_glBlendEquation p_glBlendEquation = nullptr;
#ifndef GL_FUNC_ADD
#define GL_FUNC_ADD 0x8006
#define GL_MIN 0x8007
#define GL_MAX 0x8008
#endif
#ifndef GL_FUNC_REVERSE_SUBTRACT
#define GL_FUNC_REVERSE_SUBTRACT 0x800B
#endif

// ── A4: FBO functions for cached thumbnails. Also GL 1.4+/framebuffer-object, not in the Win32 gl.h 1.1 header —
// loaded the same way and at the same call site as glBlendEquation above.
typedef void(APIENTRY* PFN_glGenFramebuffers)(GLsizei, GLuint*);
typedef void(APIENTRY* PFN_glBindFramebuffer)(GLenum, GLuint);
typedef void(APIENTRY* PFN_glFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef GLenum(APIENTRY* PFN_glCheckFramebufferStatus)(GLenum);
static PFN_glGenFramebuffers p_glGenFramebuffers = nullptr;
static PFN_glBindFramebuffer p_glBindFramebuffer = nullptr;
static PFN_glFramebufferTexture2D p_glFramebufferTexture2D = nullptr;
static PFN_glCheckFramebufferStatus p_glCheckFramebufferStatus = nullptr;
#ifndef GL_FRAMEBUFFER
#define GL_FRAMEBUFFER 0x8D40
#define GL_COLOR_ATTACHMENT0 0x8CE0
#define GL_FRAMEBUFFER_COMPLETE 0x8CD5
#define GL_FRAMEBUFFER_BINDING 0x8CA6
#endif

void InitBlendModes(void* (*getProc)(const char*)) {
  p_glBlendEquation = (PFN_glBlendEquation)getProc("glBlendEquation");
  p_glGenFramebuffers = (PFN_glGenFramebuffers)getProc("glGenFramebuffers");
  p_glBindFramebuffer = (PFN_glBindFramebuffer)getProc("glBindFramebuffer");
  p_glFramebufferTexture2D = (PFN_glFramebufferTexture2D)getProc("glFramebufferTexture2D");
  p_glCheckFramebufferStatus = (PFN_glCheckFramebufferStatus)getProc("glCheckFramebufferStatus");
}

int BlendIndex(const std::string& name) {
  static const char* N[BLEND_COUNT] = {"Normal", "Add", "Screen", "Multiply", "Overlay", "Difference", "Lighten", "Darken"};
  for (int i = 0; i < BLEND_COUNT; ++i) if (name == N[i]) return i;
  return 0;
}

// Fixed-function approximations; Overlay has no fixed-function equivalent and falls back to Screen.
static void ApplyBlendGL(int mode) {
  if (p_glBlendEquation) p_glBlendEquation(GL_FUNC_ADD);
  switch (mode) {
    case 1: glBlendFunc(GL_SRC_ALPHA, GL_ONE); break;                          // Add
    case 2: case 4: glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE); break;        // Screen / Overlay≈Screen
    case 3: glBlendFunc(GL_DST_COLOR, GL_ONE_MINUS_SRC_ALPHA); break;          // Multiply
    case 5: if (p_glBlendEquation) { p_glBlendEquation(GL_FUNC_REVERSE_SUBTRACT); glBlendFunc(GL_SRC_ALPHA, GL_ONE); }
            else glBlendFunc(GL_SRC_ALPHA, GL_ONE); break;                     // Difference≈subtract
    case 6: if (p_glBlendEquation) { p_glBlendEquation(GL_MAX); glBlendFunc(GL_ONE, GL_ONE); }
            else glBlendFunc(GL_SRC_ALPHA, GL_ONE); break;                     // Lighten
    case 7: if (p_glBlendEquation) { p_glBlendEquation(GL_MIN); glBlendFunc(GL_ONE, GL_ONE); }
            else glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;     // Darken
    // 8..11 are not layer blend modes (the UI lists BLEND_COUNT of them): colour-correction passes, RGB of the vertex colour only
    case 8: glBlendFunc(GL_DST_COLOR, GL_ZERO); break;                         // multiply by the colour
    case 9: glBlendFunc(GL_DST_COLOR, GL_ONE); break;                          // gain: dst * (1 + colour)
    case 10: glBlendFunc(GL_ONE, GL_ONE); break;                               // add the colour
    case 11: if (p_glBlendEquation) p_glBlendEquation(GL_FUNC_REVERSE_SUBTRACT); glBlendFunc(GL_ONE, GL_ONE); break;   // subtract it
    default: glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;         // Normal
  }
}

// ── slice masks via the stencil buffer ──
static bool gStencilOn = false;   // a mask is active in the draw list being built
static void CbStencilClear(const ImDrawList*, const ImDrawCmd*) {
  glDisable(GL_SCISSOR_TEST); glStencilMask(0xFF); glClearStencil(0); glClear(GL_STENCIL_BUFFER_BIT); glEnable(GL_SCISSOR_TEST);
}
static void CbStencilWrite(const ImDrawList*, const ImDrawCmd* cmd) {   // following fills only write the stencil: ref 1 = allowed, 0 = cut
  glEnable(GL_STENCIL_TEST); glStencilMask(0xFF); glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glStencilFunc(GL_ALWAYS, (GLint)(intptr_t)cmd->UserCallbackData, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
}
static void CbStencilUse(const ImDrawList*, const ImDrawCmd*) {   // draw only where the stencil is 1
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_EQUAL, 1, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); glStencilMask(0);
}
static void CbStencilOff(const ImDrawList*, const ImDrawCmd*) { glDisable(GL_STENCIL_TEST); glStencilMask(0xFF); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); }
void MaskBegin(const std::vector<std::vector<ImVec2>>& keep, const std::vector<std::vector<ImVec2>>& holes, ImRect bounds) {
  ImDrawList* dl = g.dl;
  dl->AddCallback(CbStencilClear, nullptr);
  dl->AddCallback(CbStencilWrite, (void*)(intptr_t)1);
  if (keep.empty()) dl->AddRectFilled(bounds.Min, bounds.Max, IM_COL32_WHITE);
  for (auto& p : keep) if (p.size() >= 3) dl->AddConcavePolyFilled(p.data(), (int)p.size(), IM_COL32_WHITE);
  dl->AddCallback(CbStencilWrite, (void*)(intptr_t)0);
  for (auto& p : holes) if (p.size() >= 3) dl->AddConcavePolyFilled(p.data(), (int)p.size(), IM_COL32_WHITE);
  dl->AddCallback(CbStencilUse, nullptr);
  gStencilOn = true;
}
void MaskEnd() { g.dl->AddCallback(CbStencilOff, nullptr); gStencilOn = false; }

void SetBlendMode(int mode) {
  if (mode <= 0) {
    g.dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    if (gStencilOn) g.dl->AddCallback(CbStencilUse, nullptr);   // the reset above switched the stencil test off
    return;
  }
  // ImDrawList callbacks take a void* payload; the mode rides in the pointer value.
  g.dl->AddCallback([](const ImDrawList*, const ImDrawCmd* cmd) { ApplyBlendGL((int)(intptr_t)cmd->UserCallbackData); },
                    (void*)(intptr_t)mode);
}
void SetAdditive(bool on) { SetBlendMode(on ? 1 : 0); }

void DrawColorAdjust(const ImVec2* poly, int n, float contrast, float brightness, float r, float gch, float b) {
  if (n < 3) return;
  const float ch[3] = {r, gch, b}, kc = 1.f + contrast;
  float K[3], B[3]; bool mul = false, gain = false, add = false, sub = false;
  for (int i = 0; i < 3; ++i) {   // x' = x*K + B per channel: contrast about mid-grey, then the channel scale, then brightness
    float gc = 1.f + ch[i];
    K[i] = std::clamp(kc * gc, 0.f, 2.f);
    B[i] = 0.5f * (1.f - kc) * gc + 0.5f * brightness;
    mul |= K[i] < 0.998f; gain |= K[i] > 1.002f; add |= B[i] > 0.002f; sub |= B[i] < -0.002f;
  }
  auto col = [](float a, float b2, float c2) {
    auto q = [](float v) { return (int)std::lround(std::clamp(v, 0.f, 1.f) * 255.f); };
    return IM_COL32(q(a), q(b2), q(c2), 255);
  };
  auto pass = [&](int mode, ImU32 c) { SetBlendMode(mode); g.dl->AddConcavePolyFilled(poly, n, c); };
  if (mul) pass(8, col(std::min(K[0], 1.f), std::min(K[1], 1.f), std::min(K[2], 1.f)));
  if (gain) pass(9, col(std::max(K[0] - 1.f, 0.f), std::max(K[1] - 1.f, 0.f), std::max(K[2] - 1.f, 0.f)));
  if (add) pass(10, col(std::max(B[0], 0.f), std::max(B[1], 0.f), std::max(B[2], 0.f)));
  if (sub) pass(11, col(std::max(-B[0], 0.f), std::max(-B[1], 0.f), std::max(-B[2], 0.f)));
  SetBlendMode(0);
}

// ── playback (C1/C2/C4/C5) ──
float ClipSeconds(const Clip& c) {
  float v = 0; int n = 0;
  if (std::sscanf(c.dur.c_str(), "%f%n", &v, &n) == 1 && n > 0 && v > 0.f) return v;   // "16s"; "∞" or empty do not parse
  return 10.f;
}

void AdvanceClip(Clip& c, float dt) {
  if (c.st == Clip::Empty || c.st == Clip::Armed || c.paused) return;
  // playback range [lo, hi] (the in/out markers, % of the clip); the default 0..100 is the whole clip
  float lo = std::clamp(c.inPt, 0.f, 99.f), hi = std::clamp(c.outPt, lo + 1.f, 100.f), span = hi - lo;
  float d = dt * (100.f / ClipSeconds(c)) * (c.speed / 100.f) * (c.dir < 0 ? -1.f : 1.f);
  float p = c.progress + d;
  switch (c.playMode) {
    case PM_BOUN:
      if (p > hi) { p = 2.f * hi - p; c.dir = -1; }
      else if (p < lo) { p = 2.f * lo - p; c.dir = 1; }
      p = std::clamp(p, lo, hi);
      break;
    case PM_HOLD: p = std::clamp(p, lo, hi); break;
    case PM_ONCE:
      if (p >= hi) { p = hi; if (c.st == Clip::Live) c.st = Clip::Loaded; else if (c.st == Clip::LiveSel) c.st = Clip::Selected; }
      p = std::clamp(p, lo, hi);
      break;
    default:   // PM_LOOP
      if (p > hi) p = lo + std::fmod(p - lo, span);
      else if (p < lo) p = hi - std::fmod(lo - p, span);
      break;
  }
  c.progress = p;
}

const char* PlayModeName(int m) {
  static const char* N[4] = {"LOOP", "BOUN", "HOLD", "ONCE"};
  return N[std::clamp(m, 0, 3)];
}

// ── composition (A1) ──
ImRect CanvasRect(ImRect fit) {
  float ar = (float)A.canvasW / std::max(1, A.canvasH);
  float w = fit.GetWidth(), h = fit.GetHeight();
  if (w / h > ar) w = h * ar; else h = w / ar;
  ImVec2 c((fit.Min.x + fit.Max.x) * 0.5f, (fit.Min.y + fit.Max.y) * 0.5f);
  return ImRect(c.x - w * 0.5f, c.y - h * 0.5f, c.x + w * 0.5f, c.y + h * 0.5f);
}

void DrawComposite(ImRect canvas, float t, float alpha) { Slice comp; DrawSliceSource(comp, canvas, t, alpha); }

static const Layer* FindLayer(const std::string& id) { for (auto& l : A.layers) if (!id.empty() && l.id == id) return &l; return nullptr; }
bool SliceSourceValid(const Slice& s) {
  if (s.srcKind == Slice::SrcLayer) return FindLayer(s.srcRef) != nullptr;
  if (s.srcKind == Slice::SrcGroup) return A.group(s.srcRef) != nullptr;
  return true;
}
std::string SliceSourceName(const Slice& s) {
  if (s.srcKind == Slice::SrcLayer) if (const Layer* l = FindLayer(s.srcRef)) return "Layer \xC2\xB7 " + l->name;
  if (s.srcKind == Slice::SrcGroup) if (const Group* g = A.group(s.srcRef)) return "Group \xC2\xB7 " + g->name;
  return "Composition";
}

// Properties > Comp > Transform, applied to the whole composite by folding it into each clip's own transform:
// composite = R(rot)·S(scale) about the anchor, then translated. Clips are centred art (rotate/scale about their own
// centre), so composing the two transforms gives exactly what transforming the finished picture would.
static bool XfActive(float px, float py, float scalePct, float rot, float ax, float ay) {
  return px != 0 || py != 0 || scalePct != 100 || rot != 0 || ax != 0 || ay != 0;
}
static void ApplyXf(Clip& c, float px, float py, float scalePct, float rotDeg, float axPx, float ayPx) {
  float sc = std::max(0.01f, scalePct / 100.f), rot = rotDeg * 3.14159265f / 180.f, cs = std::cos(rot), sn = std::sin(rot);
  float kx = 960.f / std::max(1, A.canvasW), ky = 540.f / std::max(1, A.canvasH);   // canvas px -> art units (DrawSliceSource draws at base 960)
  float ax = axPx * kx, ay = ayPx * ky;
  float dx = (c.posX - ax) * sc, dy = (c.posY - ay) * sc;
  c.posX = dx * cs - dy * sn + ax + px * kx; c.posY = dx * sn + dy * cs + ay + py * ky;
  c.scale *= sc; c.rotation += rotDeg;
}

// Show TestCard: a full-composition test pattern that TAKES OVER from the deck. It is a source like any other — drawn in
// canvas pixels — so the Live Output monitor, the Input selection stage and every slice's output (input rect -> keystone/mesh,
// masks, colour) all show it, each slice getting exactly the part of the card its input rect takes.
static void DrawTestCard(ImRect canvas, float alpha) {
  ImDrawList* dl = g.dl;
  const float W = (float)std::max(1, A.canvasW), H = (float)std::max(1, A.canvasH);
  const bool warped = g.warp != nullptr;
  const bool mesh = warped && g.warp->slice && g.warp->slice->warp != 0;
  auto P = [&](float x, float y) { return warped ? g.warp->Map(x, y) : ImVec2(canvas.Min.x + x / W * canvas.GetWidth(), canvas.Min.y + y / H * canvas.GetHeight()); };
  ImVec2 o0 = P(0, 0), o1 = P(W, 0);
  const float k = std::max(0.05f, std::hypot(o1.x - o0.x, o1.y - o0.y) / W);   // screen px per canvas px, for line weights
  auto C = [&](uint32_t hex, float al) { return Ca(K(hex, al * alpha)); };
  const ImDrawListFlags fl = dl->Flags;
  dl->Flags &= ~ImDrawListFlags_AntiAliasedFill;   // neighbouring fills must not show seams
  auto quad = [&](float x0, float y0, float x1, float y1, ImU32 c) {
    int seg = mesh ? 8 : 1;   // a keystone keeps straight edges straight; only a mesh bends them
    for (int iy = 0; iy < seg; ++iy) for (int ix = 0; ix < seg; ++ix) {
      float a0 = x0 + (x1 - x0) * ix / seg, a1 = x0 + (x1 - x0) * (ix + 1) / seg, b0 = y0 + (y1 - y0) * iy / seg, b1 = y0 + (y1 - y0) * (iy + 1) / seg;
      ImVec2 q[4] = {P(a0, b0), P(a1, b0), P(a1, b1), P(a0, b1)};
      dl->AddConvexPolyFilled(q, 4, c);
    }
  };
  static const uint32_t bars[7] = {0xc0c0c0, 0xc0c000, 0x00c0c0, 0x00c000, 0xc000c0, 0xc00000, 0x0000c0};
  for (int i = 0; i < 7; ++i) quad(W * i / 7.f, 0, W * (i + 1) / 7.f, H * 0.7f, C(bars[i], 1.f));
  quad(0, H * 0.7f, W, H, C(0x101010, 1.f));
  for (int i = 0; i < 8; ++i) {   // grey ramp along the bottom
    uint32_t v = (uint32_t)(i * 255 / 7); quad(W * (0.1f + 0.1f * i), H * 0.8f, W * (0.2f + 0.1f * i), H * 0.92f, C((v << 16) | (v << 8) | v, 1.f));
  }
  dl->Flags = fl;
  auto line = [&](float x0, float y0, float x1, float y1, ImU32 c, float w) { dl->AddLine(P(x0, y0), P(x1, y1), c, std::max(1.f, w * k)); };
  for (int c = 1; c < 16; ++c) line(W * c / 16.f, 0, W * c / 16.f, H, C(0xffffff, 0.35f), 2.f);   // 16 x 9 grid: straight lines show every warp error
  for (int r = 1; r < 9; ++r) line(0, H * r / 9.f, W, H * r / 9.f, C(0xffffff, 0.35f), 2.f);
  line(0, 0, W, H, C(pal::cyan, 0.5f), 3.f); line(W, 0, 0, H, C(pal::cyan, 0.5f), 3.f);
  std::vector<ImVec2> ring; const float rad = std::min(W, H) * 0.25f;
  for (int i = 0; i < 96; ++i) { float a = i * 6.2831853f / 96.f; ring.push_back(P(W * 0.5f + std::cos(a) * rad, H * 0.5f + std::sin(a) * rad)); }
  dl->AddPolyline(ring.data(), (int)ring.size(), C(pal::coral, 1.f), ImDrawFlags_Closed, std::max(1.5f, 5.f * k));
  line(W * 0.5f - 60, H * 0.5f, W * 0.5f + 60, H * 0.5f, C(pal::coral, 1.f), 5.f);
  line(W * 0.5f, H * 0.5f - 60, W * 0.5f, H * 0.5f + 60, C(pal::coral, 1.f), 5.f);
  if (unsigned lt = LogoTexture()) {   // the MikMap mark in the middle of the ring, bent by the warp like everything else
    const float hl = rad * 0.62f; const int seg = mesh ? 8 : 1;
    for (int iy = 0; iy < seg; ++iy) for (int ix = 0; ix < seg; ++ix) {
      float u0 = (float)ix / seg, u1 = (float)(ix + 1) / seg, v0 = (float)iy / seg, v1 = (float)(iy + 1) / seg;
      auto Q = [&](float u, float v) { return P(W * 0.5f - hl + 2 * hl * u, H * 0.5f - hl + 2 * hl * v); };
      dl->AddImageQuad((ImTextureID)(intptr_t)lt, Q(u0, v0), Q(u1, v0), Q(u1, v1), Q(u0, v1), ImVec2(u0, v0), ImVec2(u1, v0), ImVec2(u1, v1), ImVec2(u0, v1), C(0xffffff, 1.f));
    }
  }
  {   // one differently coloured disc per corner: tells at a glance if the picture is mirrored or turned
    const uint32_t cc[4] = {0xe63946, 0x2ecc71, 0x3b82f6, 0xf1c40f}; const float m = std::min(W, H) * 0.06f;
    const ImVec2 at[4] = {{m, m}, {W - m, m}, {W - m, H - m}, {m, H - m}};
    for (int i = 0; i < 4; ++i) {
      std::vector<ImVec2> d; for (int j = 0; j < 32; ++j) { float a = j * 6.2831853f / 32.f; d.push_back(P(at[i].x + std::cos(a) * m * 0.55f, at[i].y + std::sin(a) * m * 0.55f)); }
      dl->AddConvexPolyFilled(d.data(), (int)d.size(), C(cc[i], 1.f)); dl->AddPolyline(d.data(), (int)d.size(), C(0xffffff, 1.f), ImDrawFlags_Closed, std::max(1.5f, 3.f * k));
    }
  }
  ImVec2 fr[4] = {P(4, 4), P(W - 4, 4), P(W - 4, H - 4), P(4, H - 4)};   // border: shows where the canvas ends
  dl->AddPolyline(fr, 4, C(pal::yellow, 1.f), ImDrawFlags_Closed, std::max(1.5f, 6.f * k));
}

void DrawSliceSource(const Slice& s, ImRect canvas, float t, float alpha) {
  if (A.testCard) { DrawTestCard(canvas, alpha); return; }   // takes over from the deck everywhere the composition is shown
  int kind = SliceSourceValid(s) ? s.srcKind : (int)Slice::SrcComp;
  auto member = [&](const Layer& l) {
    return kind == Slice::SrcComp || (kind == Slice::SrcLayer && l.id == s.srcRef) || (kind == Slice::SrcGroup && l.group == s.srcRef);
  };
  // Solo is a composition-mix control: it only silences layers within the set this source draws, so routing one
  // layer to a slice keeps working while the operator solos something else on the main output.
  bool anySolo = false;
  for (auto& l : A.layers) if (l.solo && member(l)) anySolo = true;
  const float compA = std::clamp(A.comp.master / 100.f, 0.f, 1.f) * std::clamp(A.comp.opacity / 100.f, 0.f, 1.f);   // Comp > Master x Video opacity
  const CompProps& ck = A.comp;
  const bool xf = XfActive(ck.posX, ck.posY, ck.scale, ck.rotation, ck.anchorX, ck.anchorY);
  // a layer's own transform applies first, then the composition's on top
  auto draw = [&](const Layer& ly, const Clip& cl, float a2) {
    bool lx = XfActive(ly.posX, ly.posY, ly.scale, ly.rotation, ly.anchorX, ly.anchorY);
    if (!xf && !lx) { DrawClipContent(canvas, cl, t, 960.f, a2); return; }
    Clip tc = cl;
    if (lx) ApplyXf(tc, ly.posX, ly.posY, ly.scale, ly.rotation, ly.anchorX, ly.anchorY);
    if (xf) ApplyXf(tc, ck.posX, ck.posY, ck.scale, ck.rotation, ck.anchorX, ck.anchorY);
    DrawClipContent(canvas, tc, t, 960.f, a2);
  };
  for (int li = (int)A.layers.size() - 1; li >= 0; --li) {   // bottom layer first, top layer draws last
    const Layer& l = A.layers[li];
    if (!member(l) || l.bypassed || l.muted || (anySolo && !l.solo)) continue;
    const Clip* lc = nullptr;
    for (auto& c : l.clips) if (c.isLive()) { lc = &c; break; }
    if (!lc) continue;
    int bm = lc->blend > 0 ? lc->blend - 1 : BlendIndex(l.blend);   // the clip's own blend mode wins over its layer's
    if (bm) SetBlendMode(bm);
    float la = std::clamp(l.opacity / 100.f, 0.f, 1.f) * std::clamp(l.master / 100.f, 0.f, 1.f) * alpha * compA;
    if (!l.group.empty()) if (const Group* gp = A.group(l.group)) la *= std::clamp(gp->opacity / 100.f, 0.f, 1.f);
    if (l.fadeT < 1.f) {   // A10: cross-dissolve from the previous clip
      draw(l, l.fadeFrom, la * (1.f - l.fadeT));
      la *= l.fadeT;
    }
    draw(l, *lc, la);
    if (bm) SetBlendMode(0);
  }
}

// ── image sources (B2) ──
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE 0x812F
#endif
struct MediaTex { unsigned tex = 0; int w = 0, h = 0; bool failed = false; };
static std::map<std::string, MediaTex> gMedia;
static const MediaTex* GetMedia(const std::string& path) {
  auto it = gMedia.find(path);
  if (it != gMedia.end()) return it->second.failed ? nullptr : &it->second;
  MediaTex m; int n = 0;
  unsigned char* px = stbi_load(path.c_str(), &m.w, &m.h, &n, 4);
  if (!px || m.w <= 0 || m.h <= 0) { if (px) stbi_image_free(px); m.failed = true; gMedia[path] = m; return nullptr; }
  GLuint t = 0; glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, m.w, m.h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
  stbi_image_free(px);
  m.tex = t; gMedia[path] = m;
  return &gMedia[path];
}
void PreloadMedia(const std::string& path) { GetMedia(path); }
bool MediaImageSize(const std::string& path, int& w, int& h) { const MediaTex* m = GetMedia(path); if (!m) return false; w = m->w; h = m->h; return true; }   // do the disk read + upload at load time, not mid-frame

// ── A4: cached clip thumbnails ──
// Rendering the real generator art into every deck cell every frame cost ~16s/frame (S_STARS worst case, ~40
// cells) — so each Clip gets its own small texture, redrawn into an FBO at most a few times a second and budgeted
// across frames (ThumbBudget below, spent by the deck grid loop in deck.cpp), never all at once.
static const int kThumbW = 128, kThumbH = 72;
static int gThumbBudget = 0;
void ResetThumbBudget(int n) { gThumbBudget = n; }
bool ThumbBudgetLeft() { return gThumbBudget > 0; }

void RenderClipThumbnail(Clip& c) {
  if (!p_glGenFramebuffers || !p_glBindFramebuffer || !p_glFramebufferTexture2D) return;   // FBO funcs failed to load
  --gThumbBudget;
  if (c.thumbTex == 0) {
    glGenTextures(1, &c.thumbTex);
    glBindTexture(GL_TEXTURE_2D, c.thumbTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, kThumbW, kThumbH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
  }
  static GLuint sFbo = 0;
  if (sFbo == 0) p_glGenFramebuffers(1, &sFbo);   // one scratch FBO reused for every clip, never deleted (app lifetime)

  GLint prevFbo = 0, prevViewport[4];
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo);
  glGetIntegerv(GL_VIEWPORT, prevViewport);
  p_glBindFramebuffer(GL_FRAMEBUFFER, sFbo);
  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, c.thumbTex, 0);
  bool ok = !p_glCheckFramebufferStatus || p_glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
  if (ok) {
    glViewport(0, 0, kThumbW, kThumbH);
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);

    // Build a standalone ImDrawList (see ImGui::GetDrawListSharedData() docs) instead of the real window's g.dl,
    // so this offscreen pass cannot leak vertices into — or pick up clip rects/state from — the frame being built.
    ImDrawList dl(ImGui::GetDrawListSharedData());
    dl._ResetForNewFrame();
    dl.PushTexture(ImGui::GetIO().Fonts->TexRef);
    dl.PushClipRectFullScreen();
    ImDrawList* prevDl = g.dl; const WarpMap* prevWarp = g.warp; float prevAlpha = g.alpha;
    g.dl = &dl; g.warp = nullptr; g.alpha = 1.f;
    DrawClipContent(ImRect(0, 0, (float)kThumbW, (float)kThumbH), c, (float)g.time * 1.2f, 300.f, 1.f, 0.35f);
    g.dl = prevDl; g.warp = prevWarp; g.alpha = prevAlpha;
    dl.PopClipRect();
    dl.PopTexture();

    if (dl.VtxBuffer.Size > 0) {
      ImDrawData dd; dd.Clear();
      dd.DisplayPos = ImVec2(0, 0); dd.DisplaySize = ImVec2((float)kThumbW, (float)kThumbH); dd.FramebufferScale = ImVec2(1, 1);
      dd.AddDrawList(&dl);
      dd.Valid = true;
      // Newer Dear ImGui (texture refactor) only actually uploads the font atlas to the GPU when a
      // RenderDrawData call processes pending textures via draw_data->Textures — the main frame's own render
      // call does that, but only once per frame, at the END of the frame; this thumbnail pass runs mid-frame
      // (inside DrawDeck), before that has happened, so ImGui::GetPlatformIO().Textures is still the EMPTY
      // list UpdateTexturesEndFrame() resets it to — pointing Textures there is a no-op, not a fix. Building
      // our own one-entry list from io.Fonts->TexData (valid as soon as the atlas is built, which NewFrame()
      // already guarantees by this point) makes this call upload the atlas itself if nothing else has yet,
      // instead of asserting "ImDrawCmd is referring to ImTextureData that wasn't uploaded" (IM_ASSERT is a
      // no-op in Release, which is why --shot never caught this — only a real Debug run did).
      static ImVector<ImTextureData*> fontTexList;
      fontTexList.resize(0);
      fontTexList.push_back(ImGui::GetIO().Fonts->TexData);
      dd.Textures = &fontTexList;
      ImGui_ImplOpenGL3_RenderDrawData(&dd);
    }
  }
  p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);
  glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
  c.thumbAt = g.time;
}

// ── clip FX that can be done on vector art (E5 hue shift · E8 mirror · E10 strobe) ──
// Every place that shows a clip (Preview, Live Output, projector window) goes through DrawClipContent, so an effect
// applied here is seen everywhere. Blur / pixelate / trails / kaleidoscope need a framebuffer and are not implemented.
static uint32_t HueSat(uint32_t hex, float hueDeg, float satMul) {
  float r = ((hex >> 16) & 255) / 255.f, gr = ((hex >> 8) & 255) / 255.f, b = (hex & 255) / 255.f;
  float mx = std::max({r, gr, b}), mn = std::min({r, gr, b}), d = mx - mn, h = 0;
  if (d > 1e-6f) { if (mx == r) h = std::fmod((gr - b) / d, 6.f); else if (mx == gr) h = (b - r) / d + 2.f; else h = (r - gr) / d + 4.f; h *= 60.f; if (h < 0) h += 360.f; }
  float s = mx > 0 ? d / mx : 0, v = mx;
  h = std::fmod(h + hueDeg + 720.f, 360.f); s = std::clamp(s * satMul, 0.f, 1.f);
  float cc = v * s, x = cc * (1.f - std::fabs(std::fmod(h / 60.f, 2.f) - 1.f)), m = v - cc, R = 0, G = 0, B = 0;
  int sec = (int)(h / 60.f) % 6;
  switch (sec) { case 0: R = cc; G = x; break; case 1: R = x; G = cc; break; case 2: G = cc; B = x; break; case 3: G = x; B = cc; break; case 4: R = x; B = cc; break; default: R = cc; B = x; }
  auto q = [](float u) { return (uint32_t)std::clamp(u * 255.f + 0.5f, 0.f, 255.f); };
  return (q(R + m) << 16) | (q(G + m) << 8) | q(B + m);
}

static void DrawClipCore(ImRect a, const Clip& c, float t, float base, float alpha, float lod);

// T(v) = R·S·(v - A) + A + pos = R·S·v + (A - R·S·A + pos): the anchor is just an extra translation.
void ClipEffectivePos(const Clip& c, float base, float& px, float& py) {
  px = c.posX; py = c.posY;
  if (c.anchorX == 0.f && c.anchorY == 0.f) return;
  float kx = base / std::max(1, A.canvasW), ky = base * 9.f / 16.f / std::max(1, A.canvasH);   // canvas px -> art units
  float ax = c.anchorX * kx, ay = c.anchorY * ky, sc = std::max(0.01f, c.scale), rot = c.rotation * 3.14159265f / 180.f;
  float rx = (ax * sc) * std::cos(rot) - (ay * sc) * std::sin(rot), ry = (ax * sc) * std::sin(rot) + (ay * sc) * std::cos(rot);
  px += ax - rx; py += ay - ry;
}

void DrawClipContent(ImRect a, const Clip& c0, float t, float base, float alpha, float lod) {
  if (c0.st == Clip::Empty || c0.st == Clip::Armed) return;
  // Anchor: scale/rotation pivot, folded into the position (see ClipEffectivePos).
  Clip anchored;
  const Clip* pc = &c0;
  if (c0.anchorX != 0.f || c0.anchorY != 0.f) {
    anchored = c0;
    ClipEffectivePos(c0, base, anchored.posX, anchored.posY);
    pc = &anchored;
  }
  const Clip& c = *pc;
  const Fx* mirror = nullptr;
  for (const Fx& f : c.fx) {
    if (!f.on) continue;
    if (f.kind == 4) {   // strobe: dim to near-black during the "off" part of each cycle
      float rate = 0.5f + f.p[0] * 0.12f, duty = (10 + f.p[1] * 0.6f) / 100.f;
      if (std::fmod((float)g.time * rate, 1.f) > duty) alpha *= 1.f - 0.9f * std::clamp(f.mix / 100.f, 0.f, 1.f);
    } else if (f.kind == 3 && !mirror) mirror = &f;
  }
  if (!mirror) { DrawClipCore(a, c, t, base, alpha, lod); return; }
  // mirror: draw the clip in 2 or 4 clipped regions, flipping the copies (the inner call has no mirror, so no recursion)
  Clip cc = c;
  cc.fx.erase(std::remove_if(cc.fx.begin(), cc.fx.end(), [](const Fx& f) { return f.kind == 3; }), cc.fx.end());
  float mx = (a.Min.x + a.Max.x) * 0.5f, my = (a.Min.y + a.Max.y) * 0.5f;
  bool h = mirror->en == 0 || mirror->en == 2, v = mirror->en == 1 || mirror->en == 2;
  int nx = h ? 2 : 1, ny = v ? 2 : 1;
  for (int iy = 0; iy < ny; ++iy) for (int ix = 0; ix < nx; ++ix) {
    ImRect part(nx == 2 ? (ix ? mx : a.Min.x) : a.Min.x, ny == 2 ? (iy ? my : a.Min.y) : a.Min.y,
                nx == 2 ? (ix ? a.Max.x : mx) : a.Max.x, ny == 2 ? (iy ? a.Max.y : my) : a.Max.y);
    Clip part_c = cc;
    if (ix) part_c.flipH = !cc.flipH;
    if (iy) part_c.flipV = !cc.flipV;
    g.dl->PushClipRect(part.Min, part.Max, true);
    DrawClipCore(a, part_c, t, base, alpha, lod);
    g.dl->PopClipRect();
  }
}

static void DrawClipCore(ImRect a, const Clip& c, float t, float base, float alpha, float lod) {
  alpha *= std::clamp(c.opacity / 100.f, 0.f, 1.f);
  if (alpha <= 0.004f) return;
  ImDrawList* dl = g.dl;
  // The generator's picture is NOT tinted by Clip::color: that colour only marks the clip's cell in the deck. (Real tinting is
  // the Hue/Saturation effect below.) Fixed to the default palette entry so recolouring a cell never changes what is projected.
  uint32_t col = CLIP_COLORS[0];
  for (const Fx& f : c.fx) if (f.on && f.kind == 7)   // hue shift (p0 = hue, 0..100 -> 0..360 deg; p1 = saturation, 50 = unchanged)
    col = HueSat(col, f.p[0] * 3.6f * (f.mix / 100.f), 1.f + (f.p[1] - 50.f) / 50.f * (f.mix / 100.f));
  float W = a.GetWidth(), k = W / base;
  ImVec2 C((a.Min.x + a.Max.x) * 0.5f, (a.Min.y + a.Max.y) * 0.5f);
  float bw = base, bh = base * 9.f / 16.f;  // logical canvas units

  // clip transform (D1/D2/D5/D6): flip → scale → rotate → translate
  float sc = std::max(0.01f, c.scale), rot = c.rotation * 3.14159265f / 180.f;
  float cs = std::cos(rot), sn = std::sin(rot);
  float fx = c.flipH ? -1.f : 1.f, fy = c.flipV ? -1.f : 1.f;
  float cw2 = A.canvasW / base, ch2 = A.canvasH / (base * 9.f / 16.f);  // art units → canvas pixels
  auto P = [&](float x, float y) {
    float ux = x * fx * sc, uy = y * fy * sc;
    float ax = ux * cs - uy * sn + c.posX, ay = ux * sn + uy * cs + c.posY;
    if (g.warp) return g.warp->Map(A.canvasW * 0.5f + ax * cw2, A.canvasH * 0.5f + ay * ch2);
    return ImVec2(C.x + ax * k, C.y + ay * k);
  };
  auto Rk = [&](float r) {                                 // radius in screen px
    if (g.warp) { ImVec2 a0 = P(0, 0), a1 = P(r, 0); return std::sqrt((a1.x - a0.x) * (a1.x - a0.x) + (a1.y - a0.y) * (a1.y - a0.y)); }
    return r * sc * k;
  };
  auto Ctr = [&]() { return P(0, 0); };
  const uint32_t chanMask = (c.chan & 1 ? 0xFF0000u : 0u) | (c.chan & 2 ? 0x00FF00u : 0u) | (c.chan & 4 ? 0x0000FFu : 0u);   // Clip > R G B toggles
  auto Col = [&](uint32_t hex, float al) { return Ca(K(hex & chanMask, al * alpha)); };
  auto N = [&](int n) { return std::max(3, (int)(n * lod)); };
  dl->PushClipRect(a.Min, a.Max, true);

  if (!c.media.empty()) {
    // B2: aspect-fit the image inside the canvas (art units: canvas = bw x bh centred on the origin); it goes through the same
    // transform P() as generators, and is drawn as a grid of quads when warped so a mesh/keystone bends it correctly
    if (const MediaTex* mt = GetMedia(c.media)) {
      float s0 = std::min(bw / mt->w, bh / mt->h), hw = mt->w * s0 * 0.5f, hh = mt->h * s0 * 0.5f;
      ImTextureID tid = (ImTextureID)(intptr_t)mt->tex;
      ImU32 tint = Col(0xffffff, 1.f);
      int seg = g.warp ? 12 : 1;
      for (int iy = 0; iy < seg; ++iy) for (int ix = 0; ix < seg; ++ix) {
        float u0 = (float)ix / seg, u1 = (float)(ix + 1) / seg, v0 = (float)iy / seg, v1 = (float)(iy + 1) / seg;
        dl->AddImageQuad(tid, P(-hw + 2 * hw * u0, -hh + 2 * hh * v0), P(-hw + 2 * hw * u1, -hh + 2 * hh * v0),
                         P(-hw + 2 * hw * u1, -hh + 2 * hh * v1), P(-hw + 2 * hw * u0, -hh + 2 * hh * v1),
                         ImVec2(u0, v0), ImVec2(u1, v0), ImVec2(u1, v1), ImVec2(u0, v1), tint);
      }
    } else if (MediaKindOf(c.media) == MEDIA_VIDEO || MediaKindOf(c.media) == MEDIA_AUDIO) {
      // No decoder for these yet (B1): say so instead of the misleading "MISSING MEDIA" the file-not-found case shows.
      bool vid = MediaKindOf(c.media) == MEDIA_VIDEO;
      TextC(Ctr().x, Ctr().y - 7, MONO_B, 11, K(vid ? pal::cyan : pal::mint), vid ? "VIDEO" : "AUDIO");
      TextC(Ctr().x, Ctr().y + 7, MONO_R, 9, K(pal::t88), "playback not available yet");
    } else TextC(Ctr().x, Ctr().y, MONO_M, 11, K(pal::red), "MISSING MEDIA");
  } else switch (c.style >= 0 ? c.style : ClipStyleOf(c.name)) {
    case S_PLASMA: {
      RadialFan(Ctr(), Rk(bw * 0.7f), Col(col, 0.33f), Col(col, 0.f), 0, TAU, N(64));
      RadialFan(Ctr(), Rk(bw * 0.35f), Col(pal::cyan, 0.25f), Col(pal::cyan, 0.f), 0, TAU, N(48));
      float step = 36.f / std::max(0.35f, lod);
      for (float x = -bw / 2; x <= bw / 2; x += step) dl->AddLine(P(x, -bh / 2), P(x, bh / 2), Col(col, 0.13f));
      for (float y = -bh / 2; y <= bh / 2; y += step) dl->AddLine(P(-bw / 2, y), P(bw / 2, y), Col(col, 0.13f));
      float sy = std::sin(t * 1.5f) * bh / 2;
      dl->AddLine(P(-bw / 2, sy), P(bw / 2, sy), Col(col, 0.4f), 1.5f * k + 0.5f);
      break;
    }
    case S_HEX: {
      for (int i = 1; i <= 5; ++i) {
        float rad = 35 * i + std::sin(t * 1.5f + i) * 12;
        ImVec2 pts[6];
        for (int s = 0; s < 6; ++s) {
          float ang = s * TAU / 6 + t * (i % 2 == 0 ? 0.35f : -0.35f);
          pts[s] = P(std::cos(ang) * rad, std::sin(ang) * rad);
        }
        dl->AddPolyline(pts, 6, Col(col, 1.f), ImDrawFlags_Closed, 2.f * k + 0.5f);
      }
      dl->AddCircle(Ctr(), Rk((std::sin(t * 2) * 0.5f + 0.5f) * 80 + 20), Col(col, 0.53f), N(40), k + 0.5f);
      break;
    }
    case S_VORTEX: {
      const uint32_t wc[3] = {col, 0xffd166, 0x06d6a0};
      float step = 10.f / std::max(0.3f, lod);
      for (int w = 0; w < 3; ++w) {
        std::vector<ImVec2> pts;
        for (float x = -bw / 2; x <= bw / 2; x += step) {
          float amp = 35 + w * 14;
          pts.push_back(P(x, std::sin(x * 0.008f + t * 2 + w * 0.8f) * std::cos(x * 0.003f + t) * amp));
        }
        dl->AddPolyline(pts.data(), (int)pts.size(), Col(wc[w], 1.f), 0, 2.5f * k + 0.5f);
      }
      int np = N(24);
      for (int p = 0; p < np; ++p) {
        float ang = p * (TAU / np) + t * 0.6f, dist = 110 + std::sin(t * 2 + p) * 50;
        dl->AddCircleFilled(P(std::cos(ang) * dist, std::sin(ang) * dist), 2.5f * k + 0.5f, Col(0xffd166, 1.f), 10);
      }
      break;
    }
    case S_AURORA: {
      const uint32_t ac[4] = {0x06d6a0, 0x118ab2, col, 0xb388ff};
      float step = 12.f / std::max(0.3f, lod);
      for (int b = 0; b < 5; ++b) {
        std::vector<ImVec2> pts;
        for (float x = -bw / 2; x <= bw / 2; x += step) {
          float y = (b - 2) * 46 + std::sin(x * 0.011f + t * (0.6f + b * 0.15f) + b * 1.7f) * 34 + std::sin(x * 0.027f - t * 0.9f + b) * 12;
          pts.push_back(P(x, y));
        }
        uint32_t cc = ac[b % 4];
        dl->AddPolyline(pts.data(), (int)pts.size(), Col(cc, 0.10f), 0, 30.f * k * sc);
        dl->AddPolyline(pts.data(), (int)pts.size(), Col(cc, 0.22f), 0, 14.f * k * sc);
        dl->AddPolyline(pts.data(), (int)pts.size(), Col(cc, 0.85f), 0, 1.8f * k + 0.3f);
      }
      break;
    }
    case S_STARS: {
      int n = N(140);
      for (int i = 0; i < n; ++i) {
        float z = Fract(t * 0.22f + Hash(i, 1));
        float ang = Hash(i, 2) * TAU, r = z * z * bw * 0.75f;
        ImVec2 p0 = P(std::cos(ang) * r, std::sin(ang) * r), p1 = P(std::cos(ang) * r * 0.88f, std::sin(ang) * r * 0.88f);
        uint32_t cc = Hash(i, 3) > 0.7f ? col : 0xffffff;
        // Thickness must stay >= 1: ImGui's textured-AA line path indexes TexUvLines by (int)thickness
        // and degenerates badly for sub-pixel widths.
        dl->AddLine(p1, p0, Col(cc, z), std::max(1.f, (0.6f + z * 2.2f) * k + 0.3f));
      }
      break;
    }
    case S_TUNNEL: {
      int n = N(12);
      for (int i = 0; i < n; ++i) {
        float s = Fract(t * 0.3f + i / (float)n), half = 10 + s * s * bw * 0.8f, rt = t * 0.2f + i * 0.05f;
        ImVec2 pts[4];
        for (int q = 0; q < 4; ++q) {
          float ang = rt + q * TAU / 4 + TAU / 8;
          pts[q] = P(std::cos(ang) * half, std::sin(ang) * half);
        }
        dl->AddPolyline(pts, 4, Col(i % 3 == 0 ? pal::cyan : col, (1 - s) * 0.9f), ImDrawFlags_Closed, (1.f + s * 2.f) * k + 0.3f);
      }
      break;
    }
    case S_RAIN: {
      int n = N(46);
      for (int i = 0; i < n; ++i) {
        float x = (Hash(i, 4) - 0.5f) * bw, spd = 0.25f + Hash(i, 5) * 0.55f;
        float y = Fract(t * spd + Hash(i, 6)) * (bh + 90) - bh / 2 - 45;
        float len = 26 + Hash(i, 7) * 40;
        uint32_t cc = Hash(i, 8) > 0.5f ? 0x06d6a0 : 0x118ab2;
        for (int s = 0; s < 4; ++s) dl->AddLine(P(x, y - len * (s + 1) / 4), P(x, y - len * s / 4), Col(cc, 0.75f * (1 - s / 4.f)), 1.6f * k + 0.3f);
        dl->AddCircleFilled(P(x, y), 2.f * k + 0.3f, Col(0xffffff, 0.9f), 8);
      }
      break;
    }
    case S_BARS: {
      const int NB = std::max(8, (int)(32 * lod));
      float bwid = bw / (NB + 2), base0 = bh * 0.32f;
      for (int i = 0; i < NB; ++i) {
        float h = (0.15f + 0.85f * std::fabs(std::sin(t * 2 + i * 0.5f) * std::cos(t * 1.3f + i * 0.21f))) * bh * 0.6f;
        float x = (i - NB / 2.f + 0.5f) * (bwid * 1.04f);
        float u = h / (bh * 0.6f);
        uint32_t cc = u < 0.5f ? 0x06d6a0 : u < 0.8f ? 0xffd166 : 0xef4444;
        ImVec2 q[4] = {P(x - bwid * 0.4f, base0 - h), P(x + bwid * 0.4f, base0 - h), P(x + bwid * 0.4f, base0), P(x - bwid * 0.4f, base0)};
        dl->AddConvexPolyFilled(q, 4, Col(cc, 0.9f));
        ImVec2 r2[4] = {P(x - bwid * 0.4f, base0 + 4), P(x + bwid * 0.4f, base0 + 4), P(x + bwid * 0.4f, base0 + 4 + h * 0.25f), P(x - bwid * 0.4f, base0 + 4 + h * 0.25f)};
        dl->AddConvexPolyFilled(r2, 4, Col(cc, 0.18f));
      }
      break;
    }
    default: {  // S_PULSE: expanding rings on a 128 BPM beat
      float ph = t * 1.2f * (128.f / 60.f) * 0.5f;
      for (int i = 0; i < 4; ++i) {
        float f = Fract(ph + i / 4.f);
        dl->AddCircle(Ctr(), Rk(f * bw * 0.5f), Col(i % 2 ? pal::cyan : col, (1 - f) * 0.9f), N(48), (1.f + (1 - f) * 3.f) * k + 0.3f);
      }
      float pk = std::pow(1.f - Fract(ph), 3.f);
      RadialFan(Ctr(), Rk(30 + pk * 60), Col(col, 0.55f * pk + 0.1f), Col(col, 0.f), 0, TAU, N(40));
      break;
    }
  }
  dl->PopClipRect();
}

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
    case 12: glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA); break;                // premultiplied alpha: a slice source texture (see SliceSourceTexture)
    default: glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;         // Normal
  }
}

// ── slice outline + masks via the stencil buffer ──
// Stencil values: 1 = inside the slice's outline, 2 = also inside a "keep" mask; the picture is drawn where the stencil equals the
// reference the current mask needs (1 with no keep masks, 2 otherwise). Holes reset the value to 0.
static bool gStencilOn = false;   // a mask is active in the draw list being built
static int gStencilRef = 1;
static void CbStencilClear(const ImDrawList*, const ImDrawCmd*) {
  glDisable(GL_SCISSOR_TEST); glStencilMask(0xFF); glClearStencil(0); glClear(GL_STENCIL_BUFFER_BIT); glEnable(GL_SCISSOR_TEST);
}
static void CbStencilWrite(const ImDrawList*, const ImDrawCmd* cmd) {   // following fills only write the stencil: ALWAYS -> ref
  glEnable(GL_STENCIL_TEST); glStencilMask(0xFF); glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glStencilFunc(GL_ALWAYS, (GLint)(intptr_t)cmd->UserCallbackData, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
}
static void CbStencilRaise(const ImDrawList*, const ImDrawCmd*) {   // following fills turn 1 into 2 (only where the outline already is)
  glEnable(GL_STENCIL_TEST); glStencilMask(0xFF); glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glStencilFunc(GL_EQUAL, 1, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_INCR);
}
static void CbStencilClearWhere(const ImDrawList*, const ImDrawCmd* cmd) {   // following fills set the value back to 0 where it equals the reference
  glEnable(GL_STENCIL_TEST); glStencilMask(0xFF); glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
  glStencilFunc(GL_EQUAL, (GLint)(intptr_t)cmd->UserCallbackData, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_ZERO);
}
static void CbStencilUse(const ImDrawList*, const ImDrawCmd* cmd) {   // draw only where the stencil equals the reference
  glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); glEnable(GL_STENCIL_TEST);
  glStencilFunc(GL_EQUAL, (GLint)(intptr_t)cmd->UserCallbackData, 0xFF); glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP); glStencilMask(0);
}
static void CbStencilOff(const ImDrawList*, const ImDrawCmd*) { glDisable(GL_STENCIL_TEST); glStencilMask(0xFF); glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE); }
void MaskBegin(const std::vector<ImVec2>& outline, const std::vector<std::vector<ImVec2>>& keep, const std::vector<std::vector<ImVec2>>& holes) {
  ImDrawList* dl = g.dl;
  dl->AddCallback(CbStencilClear, nullptr);
  dl->AddCallback(CbStencilWrite, (void*)(intptr_t)1);
  if (outline.size() >= 3) dl->AddConcavePolyFilled(outline.data(), (int)outline.size(), IM_COL32_WHITE);
  int ref = 1;
  if (!keep.empty()) {
    dl->AddCallback(CbStencilRaise, nullptr);
    for (auto& p : keep) if (p.size() >= 3) dl->AddConcavePolyFilled(p.data(), (int)p.size(), IM_COL32_WHITE);
    ref = 2;
  }
  if (!holes.empty()) {
    dl->AddCallback(CbStencilClearWhere, (void*)(intptr_t)ref);
    for (auto& p : holes) if (p.size() >= 3) dl->AddConcavePolyFilled(p.data(), (int)p.size(), IM_COL32_WHITE);
  }
  dl->AddCallback(CbStencilUse, (void*)(intptr_t)ref);
  gStencilOn = true; gStencilRef = ref;
}
void MaskEnd() { g.dl->AddCallback(CbStencilOff, nullptr); gStencilOn = false; }

void SetBlendMode(int mode) {
  if (mode <= 0) {
    g.dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr);
    if (gStencilOn) g.dl->AddCallback(CbStencilUse, (void*)(intptr_t)gStencilRef);   // the reset above switched the stencil test off
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

// ── Show TestCard (design: "MikMap Test Pattern") ──
// A test pattern that TAKES OVER from the deck. It is drawn once per frame into a texture that is exactly the composition's
// resolution (so the pixel grid, the circles and the labels change with Comp > Resolution), and is then a source like any
// other: the Live Output monitor, the Input selection stage, every slice's output (input rect -> keystone/mesh, masks, colour)
// and the projector window all sample that texture. Text cannot be bent by a warp any other way.
static unsigned gTpTex = 0;
static int gTpW = 0, gTpH = 0;
static long gTpKey = -1;

static void PaintTestPattern(float W, float H) {
  ImDrawList* dl = g.dl;
  const float u = std::min(W / 1920.f, H / 1080.f);   // the design is 1920x1080: sizes scale with this, so the layout stays whole at any aspect
  const float cx = W * 0.5f, cy = H * 0.5f;
  auto C = [](uint32_t hex, float a = 1.f) { return K(hex, a); };
  const int gs = 60 * std::max(1, (int)std::lround(H / 1080.f));   // grid pitch, canvas px
  dl->AddRectFilled(ImVec2(0, 0), ImVec2(W, H), C(0x050505));
  // checkerboard in two tones only: MikMap orange and near-black grey
  const int cols = (int)std::ceil(W / gs);
  for (int r = 0; r * gs < H; ++r) for (int c = 0; c < cols; ++c) {
    ImU32 k = (c + r) % 2 == 0 ? C(0xFF7F50, .22f) : C(0x121212);
    dl->AddRectFilled(ImVec2((float)c * gs, (float)r * gs), ImVec2((float)(c + 1) * gs, (float)(r + 1) * gs), k);
  }
  for (int p = gs, i = 1; p < W; p += gs, ++i) dl->AddLine(ImVec2(p + .5f, 0), ImVec2(p + .5f, H), C(0xffffff, i % 4 == 0 ? .18f : .08f), 1.f);
  for (int p = gs, i = 1; p < H; p += gs, ++i) dl->AddLine(ImVec2(0, p + .5f), ImVec2(W, p + .5f), C(0xffffff, i % 4 == 0 ? .18f : .08f), 1.f);
  dl->AddLine(ImVec2(0, 0), ImVec2(W, H), C(0xCCCCCC), 1.5f * std::max(1.f, u));
  dl->AddLine(ImVec2(W, 0), ImVec2(0, H), C(0xCCCCCC), 1.5f * std::max(1.f, u));
  dl->AddLine(ImVec2(cx, 0), ImVec2(cx, H), C(0x888888), std::max(1.f, u));
  dl->AddLine(ImVec2(0, cy), ImVec2(W, cy), C(0x888888), std::max(1.f, u));
  const float lw = std::max(1.f, u);
  dl->AddCircle(ImVec2(cx, cy), 520 * u, C(0xF3F3F3), 128, 2 * lw);
  dl->AddCircle(ImVec2(cx, cy), 380 * u, C(0x666666), 128, lw);
  for (int i = 0; i < 4; ++i)   // one in each corner: proves the corners of the canvas reach the corners of the surface
    dl->AddCircle(ImVec2(i % 2 ? W - 180 * u : 180 * u, i / 2 ? H - 180 * u : 180 * u), 160 * u, C(0xF3F3F3), 64, 2 * lw);
  dl->AddCircleFilled(ImVec2(cx, cy), 200 * u, C(0x050505), 96);
  dl->AddCircle(ImVec2(cx, cy), 200 * u, C(0x2a2a2a), 96, lw);
  dl->AddRect(ImVec2(1, 1), ImVec2(W - 1, H - 1), C(0xFF7F50), 0.f, 0, 2 * lw);

  // 0..360 degree hue bar with its scale
  const float bx = cx - 480 * u, bw = 960 * u, hy = cy - 288 * u, hh = 64 * u;
  Text(bx, hy + 8 * u, MONO_B, 14 * u / kTextScale, C(0x888888), "RGB", 0.12f);
  const char* hl[4] = {"0\xC2\xB0", "120\xC2\xB0", "240\xC2\xB0", "360\xC2\xB0"};
  for (int i = 0; i < 4; ++i) {
    float fx = bx + bw * (1 + i) / 4.f;   // the design spreads the labels with space-between
    if (i == 3) TextR(bx + bw, hy + 8 * u, MONO_B, 14 * u / kTextScale, C(0x888888), hl[i], 0.12f);
    else TextC(fx, hy + 8 * u, MONO_B, 14 * u / kTextScale, C(0x888888), hl[i], 0.12f);
  }
  const float by = hy + 23 * u;
  static const uint32_t hue[7] = {0xff0000, 0xffff00, 0x00ff00, 0x00ffff, 0x0000ff, 0xff00ff, 0xff0000};
  for (int i = 0; i < 6; ++i)
    dl->AddRectFilledMultiColor(ImVec2(bx + bw * i / 6.f, by), ImVec2(bx + bw * (i + 1) / 6.f, by + hh), C(hue[i]), C(hue[i + 1]), C(hue[i + 1]), C(hue[i]));
  dl->AddRect(ImVec2(bx, by), ImVec2(bx + bw, by + hh), C(0x2a2a2a), 0.f, 0, lw);

  // 11-step grey ramp, 0..100 %
  const float gy = cy + 220 * u, cw = bw / 11.f;
  for (int i = 0; i < 11; ++i) {
    uint32_t v = (uint32_t)std::lround(i * 25.5); char lb[8]; snprintf(lb, sizeof lb, "%d%%", i * 10);
    dl->AddRectFilled(ImVec2(bx + cw * i, gy), ImVec2(bx + cw * (i + 1), gy + hh), C((v << 16) | (v << 8) | v));
    TextC(bx + cw * (i + 0.5f), gy + hh + 15 * u, MONO_B, 14 * u / kTextScale, C(0x888888), lb);
  }
  dl->AddRect(ImVec2(bx, gy), ImVec2(bx + bw, gy + hh), C(0x2a2a2a), 0.f, 0, lw);

  // centrepiece: logo, name, and the resolution this pattern was drawn at
  float ly = cy - 154 * u;
  if (unsigned lt = LogoTexture()) dl->AddImage((ImTextureID)(intptr_t)lt, ImVec2(cx - 95 * u, ly), ImVec2(cx + 95 * u, ly + 190 * u));
  TextC(cx, ly + 226 * u, UI_X, 52 * u / kTextScale, C(0xFFFFFF), "MikMap", -0.02f);
  TextC(cx, ly + 272 * u, MONO_B, 15 * u / kTextScale, C(0x888888), "TEST PATTERN", 0.14f);
  int iw = (int)W, ih = (int)H, gd = 1;
  for (int a2 = iw, b2 = ih; b2; ) { int t2 = a2 % b2; a2 = b2; b2 = t2; gd = a2; }
  char ar[24];
  if (iw / gd <= 64 && ih / gd <= 64) snprintf(ar, sizeof ar, "%d:%d", iw / gd, ih / gd); else snprintf(ar, sizeof ar, "%.2f:1", W / H);
  char res[32], tail[40];
  snprintf(res, sizeof res, "%d\xC3\x97%d", iw, ih); snprintf(tail, sizeof tail, " \xC2\xB7 %s", ar);
  float rw = TextW(MONO_B, 16 * u / kTextScale, res, 0.06f), tw = TextW(MONO_B, 16 * u / kTextScale, tail, 0.06f), rx = cx - (rw + tw) * 0.5f;
  Text(rx, ly + 302 * u, MONO_B, 16 * u / kTextScale, C(0xCCCCCC), res, 0.06f);
  Text(rx + rw, ly + 302 * u, MONO_B, 16 * u / kTextScale, C(0x666666), tail, 0.06f);

  // footer: grid pitch (left) and a running timecode (right)
  char gl[16], gv[24]; snprintf(gl, sizeof gl, "GRID"); snprintf(gv, sizeof gv, " %d px", gs);
  Text(24 * u, H - 28 * u, MONO_B, 16 * u / kTextScale, C(0xCCCCCC), gl, 0.06f);
  Text(24 * u + TextW(MONO_B, 16 * u / kTextScale, gl, 0.06f), H - 28 * u, MONO_B, 16 * u / kTextScale, C(0x666666), gv, 0.06f);
  double sec = g.time; char tc[32];
  snprintf(tc, sizeof tc, "%02d:%02d:%02d:%02d", (int)(sec / 3600), (int)std::fmod(sec / 60, 60), (int)std::fmod(sec, 60), (int)std::fmod(sec * 30, 30));
  TextR(W - 24 * u, H - 28 * u, MONO_B, 16 * u / kTextScale, C(0x06D6A0), tc, 0.06f);
}

// Once per frame (main context, before the UI is built) while Show TestCard is on: repaint the pattern texture at the comp's resolution.
// ── slice sources as textures (the warp samples them) ──
// Each source (the composition, one layer, one group) is drawn ONCE per frame into a texture of the composition's size, and every
// slice then samples it through a fine triangle mesh that follows the warp (DrawSliceTextured, mapping.cpp). Drawing the vector art
// straight through the warp only moved its vertices: a long line stayed straight on a curved mesh and a circle stayed round under
// perspective. The texture holds premultiplied colour (the ImGui backend blends alpha with ONE, ONE_MINUS_SRC_ALPHA), so it is drawn
// with blend mode 12. Framebuffers are per GL context: textures are only rendered in the main window's context — RenderOutput asks for
// them before it switches to the projector's context, and inside that context only cached ones are handed out.
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif
struct SrcTex { unsigned tex = 0; int w = 0, h = 0, frame = -1; };
static std::map<std::string, SrcTex> gSrcTex;
static bool gSrcRenderOk = true;
void SliceSourcesRenderable(bool ok) { gSrcRenderOk = ok; }
unsigned SliceSourceTexture(const Slice& s, float t) {
  if (!p_glGenFramebuffers || !p_glBindFramebuffer || !p_glFramebufferTexture2D) return 0;
  const int kind = SliceSourceValid(s) ? s.srcKind : (int)Slice::SrcComp;
  const std::string key = kind == Slice::SrcComp ? std::string("C") : (kind == Slice::SrcLayer ? "L:" : "G:") + s.srcRef;
  const int W = std::clamp(A.canvasW, 16, 8192), H = std::clamp(A.canvasH, 16, 8192), frame = ImGui::GetFrameCount();
  SrcTex& st = gSrcTex[key];
  if (st.tex && st.w == W && st.h == H && st.frame == frame) return st.tex;
  if (!gSrcRenderOk) return 0;   // not in this context: the caller falls back to drawing the source directly
  if (!st.tex || st.w != W || st.h != H) {
    if (!st.tex) glGenTextures(1, &st.tex);
    glBindTexture(GL_TEXTURE_2D, st.tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);   // outside the canvas = transparent
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    st.w = W; st.h = H;
  }
  st.frame = frame;
  static GLuint sFbo = 0;
  if (!sFbo) p_glGenFramebuffers(1, &sFbo);
  GLint prevFbo = 0, prevViewport[4];
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo); glGetIntegerv(GL_VIEWPORT, prevViewport);
  p_glBindFramebuffer(GL_FRAMEBUFFER, sFbo);
  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, st.tex, 0);
  if (!p_glCheckFramebufferStatus || p_glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
    glViewport(0, 0, W, H);
    glClearColor(0, 0, 0, 0); glClear(GL_COLOR_BUFFER_BIT);
    const bool stencilWas = gStencilOn; gStencilOn = false;   // a mask being built in the caller's list is not this list's business
    ImDrawList dl(ImGui::GetDrawListSharedData());
    dl._ResetForNewFrame();
    dl.PushTexture(ImGui::GetIO().Fonts->TexRef);
    dl.PushClipRect(ImVec2(0, 0), ImVec2((float)W, (float)H), false);
    ImDrawList* prevDl = g.dl; const WarpMap* prevWarp = g.warp; float prevAlpha = g.alpha;
    g.dl = &dl; g.warp = nullptr; g.alpha = 1.f;
    DrawSliceSource(s, ImRect(0, 0, (float)W, (float)H), t, 1.f);
    g.dl = prevDl; g.warp = prevWarp; g.alpha = prevAlpha;
    gStencilOn = stencilWas;
    dl.PopClipRect(); dl.PopTexture();
    if (dl.VtxBuffer.Size > 0) {
      ImDrawData dd; dd.Clear();
      dd.DisplayPos = ImVec2(0, 0); dd.DisplaySize = ImVec2((float)W, (float)H); dd.FramebufferScale = ImVec2(1, 1);
      dd.AddDrawList(&dl); dd.Valid = true;
      static ImVector<ImTextureData*> fontTexList;   // see RenderClipThumbnail
      fontTexList.resize(0); fontTexList.push_back(ImGui::GetIO().Fonts->TexData); dd.Textures = &fontTexList;
      ImGui_ImplOpenGL3_RenderDrawData(&dd);
    }
  }
  p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);
  glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
  glFlush();   // the projector window's context samples this texture too
  return st.tex;
}

void UpdateTestPattern() {
  if (!A.testCard || !p_glGenFramebuffers || !p_glBindFramebuffer || !p_glFramebufferTexture2D) return;
  const int W = std::clamp(A.canvasW, 16, 8192), H = std::clamp(A.canvasH, 16, 8192);
  const long key = (long)(g.time * 30.0);   // the timecode changes at 30 fps; more than one paint per tick would be wasted
  if (W == gTpW && H == gTpH && key == gTpKey && gTpTex) return;
  if (!gTpTex || W != gTpW || H != gTpH) {
    if (!gTpTex) glGenTextures(1, &gTpTex);
    glBindTexture(GL_TEXTURE_2D, gTpTex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, W, H, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    gTpW = W; gTpH = H;
  }
  gTpKey = key;
  static GLuint sFbo = 0;
  if (!sFbo) p_glGenFramebuffers(1, &sFbo);
  GLint prevFbo = 0, prevViewport[4];
  glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFbo); glGetIntegerv(GL_VIEWPORT, prevViewport);
  p_glBindFramebuffer(GL_FRAMEBUFFER, sFbo);
  p_glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gTpTex, 0);
  if (!p_glCheckFramebufferStatus || p_glCheckFramebufferStatus(GL_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE) {
    glViewport(0, 0, W, H);
    glClearColor(0.02f, 0.02f, 0.02f, 1.f); glClear(GL_COLOR_BUFFER_BIT);
    ImDrawList dl(ImGui::GetDrawListSharedData());   // standalone list, same as RenderClipThumbnail: cannot leak into the frame being built
    dl._ResetForNewFrame();
    dl.PushTexture(ImGui::GetIO().Fonts->TexRef);
    dl.PushClipRect(ImVec2(0, 0), ImVec2((float)W, (float)H), false);   // not FullScreen: that one is the WINDOW's size and would cut a larger comp off
    ImDrawList* prevDl = g.dl; const WarpMap* prevWarp = g.warp; float prevAlpha = g.alpha;
    g.dl = &dl; g.warp = nullptr; g.alpha = 1.f;
    PaintTestPattern((float)W, (float)H);
    g.dl = prevDl; g.warp = prevWarp; g.alpha = prevAlpha;
    dl.PopClipRect(); dl.PopTexture();
    if (dl.VtxBuffer.Size > 0) {
      ImDrawData dd; dd.Clear();
      dd.DisplayPos = ImVec2(0, 0); dd.DisplaySize = ImVec2((float)W, (float)H); dd.FramebufferScale = ImVec2(1, 1);
      dd.AddDrawList(&dl); dd.Valid = true;
      static ImVector<ImTextureData*> fontTexList;   // see RenderClipThumbnail: the atlas may not be uploaded yet this early in the frame
      fontTexList.resize(0); fontTexList.push_back(ImGui::GetIO().Fonts->TexData); dd.Textures = &fontTexList;
      ImGui_ImplOpenGL3_RenderDrawData(&dd);
    }
  }
  p_glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFbo);
  glViewport(prevViewport[0], prevViewport[1], prevViewport[2], prevViewport[3]);
}

static void DrawTestCard(ImRect canvas, float alpha) {
  if (!gTpTex) return;
  const float W = (float)std::max(1, A.canvasW), H = (float)std::max(1, A.canvasH);
  const bool warped = g.warp != nullptr;
  auto P = [&](float x, float y) { return warped ? g.warp->Map(x, y) : ImVec2(canvas.Min.x + x / W * canvas.GetWidth(), canvas.Min.y + y / H * canvas.GetHeight()); };
  const int seg = warped ? 12 : 1;   // a keystone bends the picture in perspective, so even it needs the grid of quads
  const ImU32 tint = Ca(K(0xffffff, alpha));
  for (int iy = 0; iy < seg; ++iy) for (int ix = 0; ix < seg; ++ix) {
    float u0 = (float)ix / seg, u1 = (float)(ix + 1) / seg, v0 = (float)iy / seg, v1 = (float)(iy + 1) / seg;
    g.dl->AddImageQuad((ImTextureID)(intptr_t)gTpTex, P(W * u0, H * v0), P(W * u1, H * v0), P(W * u1, H * v1), P(W * u0, H * v1),
                       ImVec2(u0, 1.f - v0), ImVec2(u1, 1.f - v0), ImVec2(u1, 1.f - v1), ImVec2(u0, 1.f - v1), tint);   // the FBO texture is stored bottom-up
  }
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

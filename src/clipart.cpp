// Procedural "video" samples: lightweight generative clips (ported from LiveCanvas.tsx drawClipContent, plus extras).
// Every clip is drawn live with ImDrawList primitives, so there are no media files to ship or decode.
// Clip transform (position / scale / rotation / flip) and opacity are applied here, matching LiveCanvas's ctx transform.
#include "app.h"
#include <cmath>
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

void InitBlendModes(void* (*getProc)(const char*)) {
  p_glBlendEquation = (PFN_glBlendEquation)getProc("glBlendEquation");
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
    default: glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); break;         // Normal
  }
}

void SetBlendMode(int mode) {
  if (mode <= 0) { g.dl->AddCallback(ImDrawCallback_ResetRenderState, nullptr); return; }
  // ImDrawList callbacks take a void* payload; the mode rides in the pointer value.
  g.dl->AddCallback([](const ImDrawList*, const ImDrawCmd* cmd) { ApplyBlendGL((int)(intptr_t)cmd->UserCallbackData); },
                    (void*)(intptr_t)mode);
}
void SetAdditive(bool on) { SetBlendMode(on ? 1 : 0); }

// ── playback (C1/C2/C4/C5) ──
void AdvanceClip(Clip& c, float dt) {
  if (c.st == Clip::Empty || c.st == Clip::Armed) return;
  float d = dt * 10.f * (c.speed / 100.f) * (c.dir < 0 ? -1.f : 1.f);
  float p = c.progress + d;
  switch (c.playMode) {
    case PM_BOUN:
      if (p > 100.f) { p = 200.f - p; c.dir = -1; }
      else if (p < 0.f) { p = -p; c.dir = 1; }
      break;
    case PM_HOLD: p = std::clamp(p, 0.f, 100.f); break;
    case PM_ONCE:
      if (p >= 100.f) { p = 100.f; if (c.st == Clip::Live) c.st = Clip::Loaded; else if (c.st == Clip::LiveSel) c.st = Clip::Selected; }
      p = std::clamp(p, 0.f, 100.f);
      break;
    default: p = p < 0 ? p + 100.f : std::fmod(p, 100.f); break;  // PM_LOOP
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

void DrawComposite(ImRect canvas, float t, float alpha) {
  bool anySolo = false;
  for (auto& l : A.layers) if (l.solo) anySolo = true;
  for (int li = (int)A.layers.size() - 1; li >= 0; --li) {   // bottom layer first, top layer draws last
    const Layer& l = A.layers[li];
    if (l.bypassed || l.muted || (anySolo && !l.solo)) continue;
    const Clip* lc = nullptr;
    for (auto& c : l.clips) if (c.isLive()) { lc = &c; break; }
    if (!lc) continue;
    int bm = BlendIndex(l.blend);
    if (bm) SetBlendMode(bm);
    DrawClipContent(canvas, *lc, t, 960.f, std::clamp(l.opacity / 100.f, 0.f, 1.f) * alpha);
    if (bm) SetBlendMode(0);
  }
}

// ── clip art ──
void DrawClipContent(ImRect a, const Clip& c, float t, float base, float alpha, float lod) {
  if (c.st == Clip::Empty || c.st == Clip::Armed) return;
  alpha *= std::clamp(c.opacity / 100.f, 0.f, 1.f);
  if (alpha <= 0.004f) return;
  ImDrawList* dl = g.dl;
  uint32_t col = CLIP_COLORS[std::clamp(c.color, 0, 5)];
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
  auto Col = [&](uint32_t hex, float al) { return Ca(K(hex, al * alpha)); };
  auto N = [&](int n) { return std::max(3, (int)(n * lod)); };
  dl->PushClipRect(a.Min, a.Max, true);

  switch (ClipStyleOf(c.name)) {
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

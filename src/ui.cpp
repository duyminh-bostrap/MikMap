#include "ui.h"
#include <cmath>
#include <cstring>
#include <cstdio>
#include <algorithm>

namespace ui {

Ctx g;

// ───────────────────────────── colour ─────────────────────────────
ImU32 Ca(ImU32 c) {
  if (g.alpha >= 0.999f) return c;
  int a = (int)(((c >> IM_COL32_A_SHIFT) & 255) * g.alpha);
  return (c & ~IM_COL32_A_MASK) | ((ImU32)a << IM_COL32_A_SHIFT);
}
ImU32 Lerp(ImU32 a, ImU32 b, float t) {
  ImVec4 x = ImGui::ColorConvertU32ToFloat4(a), y = ImGui::ColorConvertU32ToFloat4(b);
  return ImGui::ColorConvertFloat4ToU32(ImVec4(x.x + (y.x - x.x) * t, x.y + (y.y - x.y) * t, x.z + (y.z - x.z) * t, x.w + (y.w - x.w) * t));
}
ImU32 MixHex(uint32_t base, uint32_t tint, float t) { return Lerp(K(base), K(tint), t) | 0xFF000000u; }

// ───────────────────────────── text ─────────────────────────────
ImFont* F(FontId f) {
  ImFont* fn = g.fonts[f];
  if (fn && ImGui::GetCurrentContext()) {
    float d = ImGui::GetIO().DisplayFramebufferScale.x;
    if (d > 0.f) fn->CurrentRasterizerDensity = d;
  }
  return fn;
}

static float SnapPx(float v) {
  float s = ImGui::GetCurrentContext() ? ImGui::GetIO().DisplayFramebufferScale.x : 1.f;
  return s > 0.f ? std::floor(v * s + 0.5f) / s : std::floor(v + 0.5f);
}

static int Utf8(const char* s, unsigned int* out) {
  unsigned int c = (unsigned char)s[0];
  if (c < 0x80) { *out = c; return 1; }
  if ((c & 0xE0) == 0xC0) { *out = ((c & 0x1F) << 6) | (s[1] & 0x3F); return 2; }
  if ((c & 0xF0) == 0xE0) { *out = ((c & 0x0F) << 12) | ((s[1] & 0x3F) << 6) | (s[2] & 0x3F); return 3; }
  *out = ((c & 7) << 18) | ((s[1] & 0x3F) << 12) | ((s[2] & 0x3F) << 6) | (s[3] & 0x3F);
  return 4;
}

// TextW and Text are the only two entry points that apply kTextScale; TextR/TextC/TextEll go through them with the
// logical size, so alignment and ellipsis stay consistent with what is drawn.
float TextW(FontId f, float sz, const char* s, float ls) {
  if (!s || !*s) return 0;
  sz = TextPx(sz);
  if (std::abs(ls) < 0.02f) return F(f)->CalcTextSizeA(sz, FLT_MAX, 0.f, s).x;
  float w = 0;
  for (const char* p = s; *p;) {
    unsigned int cp; int n = Utf8(p, &cp);
    char buf[5] = {}; memcpy(buf, p, n);
    w += F(f)->CalcTextSizeA(sz, FLT_MAX, 0.f, buf).x + ls * sz;
    p += n;
  }
  return w;
}

static void DrawText(float x, float y, FontId f, float sz, ImU32 col, const char* s, float ls) {
  col = Ca(col);
  ImFont* fn = F(f);
  ImDrawListFlags prevFlags = g.dl->Flags;
  g.dl->Flags |= ImDrawListFlags_TextNoPixelSnap;
  if (std::abs(ls) < 0.02f) {
    g.dl->AddText(fn, sz, ImVec2(x, y), col, s);
    g.dl->Flags = prevFlags;
    return;
  }
  for (const char* p = s; *p;) {
    unsigned int cp; int n = Utf8(p, &cp);
    char buf[5] = {}; memcpy(buf, p, n);
    g.dl->AddText(fn, sz, ImVec2(x, y), col, buf);
    x += fn->CalcTextSizeA(sz, FLT_MAX, 0.f, buf).x + ls * sz;
    p += n;
  }
  g.dl->Flags = prevFlags;
}

void Text(float x, float cy, FontId f, float sz, ImU32 col, const char* s, float ls) {
  sz = TextPx(sz);
  DrawText(SnapPx(x), SnapPx(cy - sz * 0.5f), f, sz, col, s, ls);
}
void TextR(float xr, float cy, FontId f, float sz, ImU32 col, const char* s, float ls) {
  Text(xr - TextW(f, sz, s, ls), cy, f, sz, col, s, ls);
}
void TextC(float cx, float cy, FontId f, float sz, ImU32 col, const char* s, float ls) {
  Text(cx - TextW(f, sz, s, ls) * 0.5f, cy, f, sz, col, s, ls);
}
void TextEll(float x, float cy, float maxW, FontId f, float sz, ImU32 col, const char* s, float ls) {
  if (maxW <= 0) return;
  if (TextW(f, sz, s, ls) <= maxW) { Text(x, cy, f, sz, col, s, ls); return; }
  std::string t;
  float ew = TextW(f, sz, "\xE2\x80\xA6", ls);
  for (const char* p = s; *p;) {
    unsigned int cp; int n = Utf8(p, &cp);
    std::string nt = t + std::string(p, n);
    if (TextW(f, sz, nt.c_str(), ls) + ew > maxW) break;
    t = nt; p += n;
  }
  t += "\xE2\x80\xA6";
  Text(x, cy, f, sz, col, t.c_str(), ls);
}
static unsigned int UpCp(unsigned int c) {
  if (c < 128) return (c >= 'a' && c <= 'z') ? c - 32 : c;
  if ((c >= 0xE0 && c <= 0xFE && c != 0xF7)) return c - 32;
  if (c == 0x103 || c == 0x111 || c == 0x129 || c == 0x169 || c == 0x1A1 || c == 0x1B0) return c - 1;
  if (c >= 0x1EA0 && c <= 0x1EF9 && (c & 1)) return c - 1;
  return c;
}
static void PutUtf8(std::string& o, unsigned int c) {
  if (c < 0x80) o += (char)c;
  else if (c < 0x800) { o += (char)(0xC0 | (c >> 6)); o += (char)(0x80 | (c & 0x3F)); }
  else if (c < 0x10000) { o += (char)(0xE0 | (c >> 12)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
  else { o += (char)(0xF0 | (c >> 18)); o += (char)(0x80 | ((c >> 12) & 0x3F)); o += (char)(0x80 | ((c >> 6) & 0x3F)); o += (char)(0x80 | (c & 0x3F)); }
}
std::string Upper(const std::string& s) {
  std::string o;
  for (const char* p = s.c_str(); *p;) { unsigned int cp; int n = Utf8(p, &cp); PutUtf8(o, UpCp(cp)); p += n; }
  return o;
}
// ───────────────────────────── shapes ─────────────────────────────
void Fill(ImRect r, ImU32 c, float rd) { g.dl->AddRectFilled(r.Min, r.Max, Ca(c), rd); }
void Border(ImRect r, ImU32 c, float rd, float w) {
  float h = w * 0.5f;
  g.dl->AddRect(ImVec2(r.Min.x + h, r.Min.y + h), ImVec2(r.Max.x - h, r.Max.y - h), Ca(c), std::max(0.f, rd - h), 0, w);
}
void Box(ImRect r, ImU32 bg, ImU32 bd, float rd) {
  if ((bg >> IM_COL32_A_SHIFT) & 255) Fill(r, bg, rd);
  if ((bd >> IM_COL32_A_SHIFT) & 255) Border(r, bd, rd);
}
void Glow(ImRect r, uint32_t hex, float a, float blur, float rd) {
  if (!kGlowFx) return;   // see kGlowFx: halos are temporarily off everywhere except the logo
  const int N = 6;
  for (int k = N; k >= 1; --k) {
    float s = blur * k / N;
    float al = a * 0.16f * (1.f - (k - 1.f) / N * 0.55f);
    g.dl->AddRectFilled(ImVec2(r.Min.x - s, r.Min.y - s), ImVec2(r.Max.x + s, r.Max.y + s), Ca(K(hex, al)), rd + s);
  }
}
void HLine(float x0, float x1, float y, ImU32 c) { g.dl->AddRectFilled(ImVec2(x0, y), ImVec2(x1, y + 1), Ca(c)); }
void VLine(float x, float y0, float y1, ImU32 c) { g.dl->AddRectFilled(ImVec2(x, y0), ImVec2(x + 1, y1), Ca(c)); }

void GradDiag(ImRect r, uint32_t c1, uint32_t c2, float a) {
  ImU32 A = Ca(K(c1, a)), B = Ca(K(c2, a));
  ImU32 M = Lerp(A, B, 0.5f);
  // two triangles per half approximates a 135deg linear gradient
  g.dl->AddRectFilledMultiColor(r.Min, r.Max, A, M, B, M);
}

void RadialFan(ImVec2 c, float r, ImU32 inner, ImU32 outer, float a0, float a1, int seg) {
  ImDrawList* dl = g.dl;
  inner = Ca(inner); outer = Ca(outer);
  ImVec2 uv = ImGui::GetDrawListSharedData()->TexUvWhitePixel;
  dl->PrimReserve(seg * 3, seg + 2);
  ImDrawIdx base = (ImDrawIdx)dl->_VtxCurrentIdx;
  dl->PrimWriteVtx(c, uv, inner);
  for (int i = 0; i <= seg; ++i) {
    float a = a0 + (a1 - a0) * i / seg;
    dl->PrimWriteVtx(ImVec2(c.x + cosf(a) * r, c.y + sinf(a) * r), uv, outer);
  }
  for (int i = 0; i < seg; ++i) {
    dl->PrimWriteIdx(base); dl->PrimWriteIdx((ImDrawIdx)(base + 1 + i)); dl->PrimWriteIdx((ImDrawIdx)(base + 2 + i));
  }
}

void Dot(ImVec2 c, float d, uint32_t hex, bool glow, float alpha) {
  if (glow && kGlowFx) for (int k = 3; k >= 1; --k) g.dl->AddCircleFilled(c, d * 0.5f + k * 1.6f, Ca(K(hex, alpha * 0.10f)), 20);
  g.dl->AddCircleFilled(c, d * 0.5f, Ca(K(hex, alpha)), 20);
}

void DashedPoly(const ImVec2* p, int n, ImU32 c, float th, float dash, float gap) {
  c = Ca(c);
  float acc = 0; bool on = true;
  for (int i = 0; i < n; ++i) {
    ImVec2 a = p[i], b = p[(i + 1) % n];
    float len = std::sqrt((b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y));
    if (len < 0.001f) continue;
    ImVec2 d((b.x - a.x) / len, (b.y - a.y) / len);
    float pos = 0;
    while (pos < len) {
      float seg = (on ? dash : gap) - acc;
      float e = std::min(len, pos + seg);
      if (on) g.dl->AddLine(ImVec2(a.x + d.x * pos, a.y + d.y * pos), ImVec2(a.x + d.x * e, a.y + d.y * e), c, th);
      acc += e - pos; pos = e;
      if (acc >= (on ? dash : gap) - 0.0001f) { acc = 0; on = !on; }
    }
  }
}

void Check(ImVec2 c, float sz, ImU32 col) {
  float s = sz / 24.f;
  ImVec2 p[3] = {{c.x + (5 - 12) * s, c.y + (13 - 12) * s}, {c.x + (10 - 12) * s, c.y + (18 - 12) * s}, {c.x + (19 - 12) * s, c.y + (7 - 12) * s}};
  g.dl->AddPolyline(p, 3, Ca(col), 0, std::max(1.4f, 2.6f * s));
}

// Lucide-flavoured glyphs, drawn on a 24-unit grid with smooth curves and zero miter spikes.
void Icon(const char* n, ImVec2 c, float sz, ImU32 col) {
  ImDrawList* dl = g.dl;
  col = Ca(col);
  float s = sz / 24.f, th = std::max(1.15f, 1.85f * s);
  auto P = [&](float x, float y) { return ImVec2(c.x + (x - 12.f) * s, c.y + (y - 12.f) * s); };
  auto Poly = [&](std::initializer_list<ImVec2> pts, bool closed = false) {
    std::vector<ImVec2> v(pts);
    dl->AddPolyline(v.data(), (int)v.size(), col, closed ? ImDrawFlags_Closed : 0, th);
  };
  auto Line = [&](float a, float b, float x, float y) { dl->AddLine(P(a, b), P(x, y), col, th); };
  auto Rect = [&](float x0, float y0, float x1, float y1, float r) { dl->AddRect(P(x0, y0), P(x1, y1), col, r * s, 0, th); };
  auto Circ = [&](float x, float y, float r) { dl->AddCircle(P(x, y), r * s, col, 0, th); };
  auto DotF = [&](float x, float y, float r) { dl->AddCircleFilled(P(x, y), std::max(1.1f, r * s), col, 12); };
  auto is = [&](const char* k) { return strcmp(n, k) == 0; };

  if (is("chevron-down")) Poly({P(6, 9), P(12, 15), P(18, 9)});
  else if (is("chevron-up")) Poly({P(18, 15), P(12, 9), P(6, 15)});
  else if (is("chevron-right")) Poly({P(9, 6), P(15, 12), P(9, 18)});
  else if (is("chevron-left")) Poly({P(15, 18), P(9, 12), P(15, 6)});
  else if (is("play")) Poly({P(6.5f, 4), P(19.5f, 12), P(6.5f, 20)}, true);
  else if (is("square")) Rect(5.5f, 5.5f, 18.5f, 18.5f, 2.f);
  else if (is("minus")) Line(5, 12, 19, 12);
  else if (is("restore")) { Rect(8.5f, 8.5f, 20.5f, 20.5f, 2); Poly({P(16, 4.5f), P(4.5f, 4.5f), P(4.5f, 16)}); }
  else if (is("pause")) { Rect(6, 4.5f, 10, 19.5f, 1.2f); Rect(14, 4.5f, 18, 19.5f, 1.2f); }
  else if (is("skip-back")) { Poly({P(19, 19), P(9, 12), P(19, 5)}, true); Line(5, 18.5f, 5, 5.5f); }
  else if (is("skip-forward")) { Poly({P(5, 5), P(15, 12), P(5, 19)}, true); Line(19, 5.5f, 19, 18.5f); }
  else if (is("folder") || is("folder-plus")) {
    Poly({P(2.5f, 6), P(2.5f, 19), P(3.8f, 20), P(20.2f, 20), P(21.5f, 19), P(21.5f, 8.2f), P(20.2f, 7), P(11.8f, 7), P(9.6f, 4.5f), P(3.8f, 4.5f), P(2.5f, 5.5f)}, true);
    if (is("folder-plus")) { Line(12, 10.5f, 12, 16.5f); Line(9, 13.5f, 15, 13.5f); }
  }
  else if (is("folder-open")) {
    Poly({P(2.5f, 19), P(2.5f, 5.5f), P(3.8f, 4.5f), P(9.5f, 4.5f), P(11.5f, 7), P(19.5f, 7), P(19.5f, 10)});
    Poly({P(2.5f, 19), P(5.2f, 10.5f), P(22.f, 10.5f), P(19.5f, 19), P(2.5f, 19)}, true);
  }
  else if (is("file-plus")) {
    Poly({P(5.5f, 2.5f), P(14, 2.5f), P(19.5f, 8), P(19.5f, 21.5f), P(5.5f, 21.5f)}, true);
    Poly({P(14, 2.5f), P(14, 8), P(19.5f, 8)});
    Line(12.5f, 11.5f, 12.5f, 17.5f); Line(9.5f, 14.5f, 15.5f, 14.5f);
  }
  else if (is("clock")) { Circ(12, 12, 9.5f); Poly({P(12, 6.5f), P(12, 12), P(16, 14.2f)}); }
  else if (is("save")) {
    Poly({P(3.5f, 3.5f), P(16.5f, 3.5f), P(20.5f, 7.5f), P(20.5f, 20.5f), P(3.5f, 20.5f)}, true);
    Poly({P(7.5f, 3.5f), P(7.5f, 8.5f), P(15.5f, 8.5f), P(15.5f, 3.5f)});
    Poly({P(7, 20.5f), P(7, 13.5f), P(17, 13.5f), P(17, 20.5f)});
  }
  else if (is("download")) {
    Line(12, 3.5f, 12, 15);
    Poly({P(7.5f, 10.5f), P(12, 15), P(16.5f, 10.5f)});
    Poly({P(3.5f, 15.5f), P(3.5f, 20.5f), P(20.5f, 20.5f), P(20.5f, 15.5f)});
  }
  else if (is("eye") || is("eye-off")) {
    dl->PathLineTo(P(2, 12));
    dl->PathBezierCubicCurveTo(P(6, 5.2f), P(18, 5.2f), P(22, 12), 16);
    dl->PathBezierCubicCurveTo(P(18, 18.8f), P(6, 18.8f), P(2, 12), 16);
    dl->PathStroke(col, ImDrawFlags_Closed, th);
    Circ(12, 12, 3.1f);
    if (is("eye-off")) Line(3.5f, 3.5f, 20.5f, 20.5f);
  }
  else if (is("settings")) {
    Circ(12, 12, 3.2f);
    const int teeth = 8;
    const float rIn = 7.4f, rOut = 9.8f, step = 6.2831853f / teeth;
    for (int i = 0; i < teeth; ++i) {
      float a = i * step;
      dl->PathLineTo(P(12 + cosf(a - step * 0.28f) * rIn, 12 + sinf(a - step * 0.28f) * rIn));
      dl->PathLineTo(P(12 + cosf(a - step * 0.16f) * rOut, 12 + sinf(a - step * 0.16f) * rOut));
      dl->PathLineTo(P(12 + cosf(a + step * 0.16f) * rOut, 12 + sinf(a + step * 0.16f) * rOut));
      dl->PathLineTo(P(12 + cosf(a + step * 0.28f) * rIn, 12 + sinf(a + step * 0.28f) * rIn));
    }
    dl->PathStroke(col, ImDrawFlags_Closed, th);
  }
  else if (is("circle-help")) {
    Circ(12, 12, 9.5f);
    dl->PathLineTo(P(9.3f, 9.2f));
    dl->PathBezierCubicCurveTo(P(9.6f, 6.6f), P(14.6f, 6.6f), P(14.6f, 9.5f), 12);
    dl->PathBezierCubicCurveTo(P(14.6f, 11.2f), P(12, 11.6f), P(12, 13.6f), 12);
    dl->PathStroke(col, 0, th);
    DotF(12, 17, 1.2f);
  }
  else if (is("info")) { Circ(12, 12, 9.5f); Line(12, 16.5f, 12, 11.2f); DotF(12, 8, 1.2f); }
  else if (is("rotate-ccw")) {
    dl->PathArcTo(P(12, 12), 8.5f * s, -2.35f, 3.14f, 24);
    dl->PathStroke(col, 0, th);
    Poly({P(3.5f, 4), P(3.5f, 9), P(8.5f, 9)});
  }
  else if (is("rotate-cw")) {
    dl->PathArcTo(P(12, 12), 8.5f * s, 0.f, 5.49f, 24);
    dl->PathStroke(col, 0, th);
    Poly({P(20.5f, 4), P(20.5f, 9), P(15.5f, 9)});
  }
  else if (is("monitor")) { Rect(2.5f, 3.5f, 21.5f, 16.5f, 2); Line(8, 20.5f, 16, 20.5f); Line(12, 16.5f, 12, 20.5f); }
  else if (is("maximize") || is("maximize-2")) {
    Poly({P(8, 3.5f), P(3.5f, 3.5f), P(3.5f, 8)});
    Poly({P(20.5f, 8), P(20.5f, 3.5f), P(16, 3.5f)});
    Poly({P(3.5f, 16), P(3.5f, 20.5f), P(8, 20.5f)});
    Poly({P(16, 20.5f), P(20.5f, 20.5f), P(20.5f, 16)});
  }
  else if (is("pentagon")) Poly({P(12, 2.5f), P(21.5f, 9.4f), P(17.9f, 20.5f), P(6.1f, 20.5f), P(2.5f, 9.4f)}, true);
  else if (is("scissors")) { Circ(6, 6, 3); Circ(6, 18, 3); Line(20, 4, 8.1f, 15.9f); Line(14.5f, 14.5f, 20, 20); Line(8.1f, 8.1f, 12, 12); }
  else if (is("plus")) { Line(5, 12, 19, 12); Line(12, 5, 12, 19); }
  else if (is("x")) { Line(18, 6, 6, 18); Line(6, 6, 18, 18); }
  else if (is("trash-2")) {
    Line(3.5f, 6, 20.5f, 6);
    Poly({P(18.5f, 6), P(18.5f, 19.5f), P(17.2f, 20.8f), P(6.8f, 20.8f), P(5.5f, 19.5f), P(5.5f, 6)});
    Poly({P(8.5f, 6), P(8.5f, 3.8f), P(9.5f, 3), P(14.5f, 3), P(15.5f, 3.8f), P(15.5f, 6)});
    Line(10, 10.5f, 10, 16.8f); Line(14, 10.5f, 14, 16.8f);
  }
  else if (is("layers")) { Poly({P(12, 2.5f), P(2.5f, 7.2f), P(12, 12), P(21.5f, 7.2f)}, true); Poly({P(2.5f, 12.2f), P(12, 16.8f), P(21.5f, 12.2f)}); Poly({P(2.5f, 16.8f), P(12, 21.5f), P(21.5f, 16.8f)}); }
  else if (is("move-3d")) { Poly({P(5, 3.5f), P(5, 19), P(20.5f, 19)}); Line(5, 19, 16, 8); Poly({P(16, 12.5f), P(16, 8), P(11.5f, 8)}); }
  else if (is("activity")) Poly({P(22, 12), P(18, 12), P(15, 20.5f), P(9, 3.5f), P(6, 12), P(2, 12)});
  else if (is("video")) { Poly({P(16, 10.5f), P(21.5f, 7), P(21.5f, 17), P(16, 13.5f)}, true); Rect(2.5f, 6, 16, 18, 2); }
  else if (is("audio-waveform")) { Line(2.5f, 10, 2.5f, 14); Line(6.3f, 6, 6.3f, 18); Line(10.1f, 3, 10.1f, 21); Line(13.9f, 8, 13.9f, 16); Line(17.7f, 5, 17.7f, 19); Line(21.5f, 10, 21.5f, 14); }
  else if (is("wand-sparkles") || is("wand-2")) {
    Line(3.5f, 20.5f, 15.5f, 8.5f);
    Line(13.5f, 6.5f, 17.5f, 10.5f);
    Line(19, 2.5f, 19, 6.5f); Line(17, 4.5f, 21, 4.5f);
    Line(20, 14.5f, 20, 18.5f); Line(18, 16.5f, 22, 16.5f);
    Line(9, 2.5f, 9, 5.5f); Line(7.5f, 4, 10.5f, 4);
  }
  else if (is("sliders-horizontal")) {
    Line(21, 5, 14, 5); Line(10, 5, 3, 5); Circ(12, 5, 2.f);
    Line(21, 12, 10, 12); Line(6, 12, 3, 12); Circ(8, 12, 2.f);
    Line(21, 19, 18, 19); Line(14, 19, 3, 19); Circ(16, 19, 2.f);
  }
  else if (is("sliders-vertical")) {
    Line(5, 21, 5, 14); Line(5, 10, 5, 3); Circ(5, 12, 2.f);
    Line(12, 21, 12, 10); Line(12, 6, 12, 3); Circ(12, 8, 2.f);
    Line(19, 21, 19, 18); Line(19, 14, 19, 3); Circ(19, 16, 2.f);
  }
  else if (is("film")) {
    Rect(3, 3, 21, 21, 2.2f);
    Line(7.2f, 3, 7.2f, 21); Line(16.8f, 3, 16.8f, 21); Line(3, 12, 21, 12);
    Line(3, 7.5f, 7.2f, 7.5f); Line(3, 16.5f, 7.2f, 16.5f);
    Line(16.8f, 7.5f, 21, 7.5f); Line(16.8f, 16.5f, 21, 16.5f);
  }
  else if (is("panel-left-close") || is("panel-left-open")) {
    Rect(3, 3, 21, 21, 2); Line(9, 3, 9, 21);
    if (is("panel-left-close")) Poly({P(16, 15), P(13, 12), P(16, 9)}); else Poly({P(14, 9), P(17, 12), P(14, 15)});
  }
  else if (is("crosshair")) { Circ(12, 12, 9); Line(22, 12, 17.5f, 12); Line(6.5f, 12, 2, 12); Line(12, 6.5f, 12, 2); Line(12, 22, 12, 17.5f); }
  else if (is("target")) { Circ(12, 12, 9.5f); Circ(12, 12, 5.5f); DotF(12, 12, 1.8f); }
  else if (is("radar")) { Circ(12, 12, 9.5f); Circ(12, 12, 5.5f); DotF(12, 12, 1.8f); Line(12, 12, 18.7f, 5.3f); }
  else if (is("camera")) { Poly({P(2.5f, 7.5f), P(6.5f, 7.5f), P(8.5f, 4.5f), P(15.5f, 4.5f), P(17.5f, 7.5f), P(21.5f, 7.5f), P(21.5f, 19.5f), P(2.5f, 19.5f)}, true); Circ(12, 13.2f, 3.8f); }
  else if (is("zap")) Poly({P(13, 2.5f), P(3.5f, 13.8f), P(11.8f, 13.8f), P(11, 21.5f), P(20.5f, 10.2f), P(12.2f, 10.2f)}, true);
  else if (is("arrow-up")) { Line(12, 19, 12, 5); Poly({P(5.5f, 11.5f), P(12, 5), P(18.5f, 11.5f)}); }
  else if (is("arrow-down")) { Line(12, 5, 12, 19); Poly({P(18.5f, 12.5f), P(12, 19), P(5.5f, 12.5f)}); }
  else if (is("copy")) {
    Rect(8.5f, 8.5f, 21, 21, 2);
    Poly({P(5, 15.5f), P(3.8f, 15.5f), P(3, 14.5f), P(3, 4), P(3.8f, 3), P(14.5f, 3), P(15.5f, 4), P(15.5f, 5)});
  }
  else if (is("droplet")) {
    dl->PathLineTo(P(12, 2.8f));
    dl->PathBezierCubicCurveTo(P(16.8f, 8.5f), P(19, 11.8f), P(19, 14.8f), 14);
    dl->PathArcTo(P(12, 14.6f), 7.f * s, 0.05f, 3.09f, 16);
    dl->PathBezierCubicCurveTo(P(5, 11.8f), P(7.2f, 8.5f), P(12, 2.8f), 14);
    dl->PathStroke(col, ImDrawFlags_Closed, th);
  }
  else if (is("snowflake")) {
    Line(12, 2.5f, 12, 21.5f); Line(2.5f, 12, 21.5f, 12);
    Line(5.3f, 5.3f, 18.7f, 18.7f); Line(18.7f, 5.3f, 5.3f, 18.7f);
  }
  else if (is("flip-horizontal")) { Line(12, 3, 12, 21); Poly({P(8, 7.5f), P(3, 12), P(8, 16.5f)}, true); Poly({P(16, 7.5f), P(21, 12), P(16, 16.5f)}, true); }
  else if (is("grid-3x3")) { Rect(3, 3, 21, 21, 2); Line(3, 9, 21, 9); Line(3, 15, 21, 15); Line(9, 3, 9, 21); Line(15, 3, 15, 21); }
  else if (is("frame")) { Line(21.5f, 6.5f, 2.5f, 6.5f); Line(21.5f, 17.5f, 2.5f, 17.5f); Line(6.5f, 2.5f, 6.5f, 21.5f); Line(17.5f, 2.5f, 17.5f, 21.5f); }
  else if (is("undo-2")) {
    Poly({P(8.5f, 5), P(4, 9.2f), P(8.5f, 13.4f)});
    dl->PathLineTo(P(4.2f, 9.2f));
    dl->PathArcTo(P(14.5f, 14.4f), 5.2f * s, -1.5708f, 1.5708f, 16);
    dl->PathLineTo(P(9.5f, 19.6f));
    dl->PathStroke(col, 0, th);
  }
  else if (is("redo-2")) {
    Poly({P(15.5f, 5), P(20, 9.2f), P(15.5f, 13.4f)});
    dl->PathLineTo(P(19.8f, 9.2f));
    dl->PathArcTo(P(9.5f, 14.4f), 5.2f * s, -1.5708f, -4.7124f, 16);
    dl->PathLineTo(P(14.5f, 19.6f));
    dl->PathStroke(col, 0, th);
  }
  else if (is("arrow-left-to-line")) { Line(3.5f, 5, 3.5f, 19); Line(20.5f, 12, 7.5f, 12); Poly({P(11.5f, 16), P(7.5f, 12), P(11.5f, 8)}); }
  else if (is("arrow-right-to-line")) { Line(20.5f, 5, 20.5f, 19); Line(3.5f, 12, 16.5f, 12); Poly({P(12.5f, 8), P(16.5f, 12), P(12.5f, 16)}); }
  else if (is("pencil")) { Poly({P(16.8f, 3.2f), P(20.8f, 7.2f), P(8, 20), P(3, 21), P(4, 16)}, true); Line(13.8f, 6.2f, 17.8f, 10.2f); }
  else if (is("eraser")) { Poly({P(7, 20.5f), P(3, 16.5f), P(14, 4.5f), P(20, 10.5f), P(10.5f, 20.5f)}, true); Line(10.8f, 8.2f, 16.8f, 14.2f); Line(7, 20.5f, 21, 20.5f); }
  else if (is("languages")) {
    Line(2.5f, 6.2f, 11.2f, 6.2f);
    Line(6.8f, 3.2f, 6.8f, 6.2f);
    dl->PathLineTo(P(9.6f, 6.2f));
    dl->PathBezierCubicCurveTo(P(9.2f, 11.2f), P(6.5f, 15.f), P(2.5f, 17.5f), 12);
    dl->PathStroke(col, 0, th);
    dl->PathLineTo(P(4.2f, 8.8f));
    dl->PathBezierCubicCurveTo(P(6.f, 12.2f), P(8.5f, 15.f), P(11.2f, 17.f), 12);
    dl->PathStroke(col, 0, th);
    Poly({P(12.8f, 20.2f), P(17.2f, 8.8f), P(21.5f, 20.2f)});
    Line(14.3f, 16.5f, 20.f, 16.5f);
  }
  else if (is("type")) { Poly({P(4, 7), P(4, 4), P(20, 4), P(20, 7)}); Line(8.5f, 20, 15.5f, 20); Line(12, 4, 12, 20); }
  else if (is("palette")) {
    dl->PathArcTo(P(12, 12), 9.5f * s, 0.55f, 5.95f, 24);
    dl->PathBezierCubicCurveTo(P(21.f, 14.5f), P(18.5f, 16.2f), P(16.2f, 15.5f), 10);
    dl->PathBezierCubicCurveTo(P(14.2f, 14.8f), P(13.2f, 16.8f), P(14.2f, 18.8f), 10);
    dl->PathBezierCubicCurveTo(P(15.f, 20.2f), P(13.8f, 21.5f), P(12, 21.5f), 10);
    dl->PathStroke(col, ImDrawFlags_Closed, th);
    DotF(7.2f, 11.2f, 1.35f);
    DotF(9.8f, 7.2f, 1.35f);
    DotF(14.5f, 7.2f, 1.35f);
    DotF(16.8f, 11.f, 1.35f);
  }
  else if (is("a-large-small")) {
    Poly({P(2.5f, 18.5f), P(7.2f, 5.5f), P(12.f, 18.5f)});
    Line(4.1f, 14.2f, 10.4f, 14.2f);
    Poly({P(14.8f, 18.5f), P(18.2f, 9.8f), P(21.5f, 18.5f)});
    Line(15.9f, 15.8f, 20.4f, 15.8f);
  }
  else if (is("zoom-in") || is("zoom-out") || is("scan-search")) {
    if (is("scan-search")) {
      Poly({P(3, 7), P(3, 3), P(7, 3)}); Poly({P(17, 3), P(21, 3), P(21, 7)});
      Poly({P(21, 17), P(21, 21), P(17, 21)}); Poly({P(7, 21), P(3, 21), P(3, 17)});
      Circ(11.2f, 11.2f, 4.f); Line(14.2f, 14.2f, 18, 18);
    } else {
      Circ(10.8f, 10.8f, 7.5f); Line(20.5f, 20.5f, 16.2f, 16.2f);
      Line(7.8f, 10.8f, 13.8f, 10.8f);
      if (is("zoom-in")) Line(10.8f, 7.8f, 10.8f, 13.8f);
    }
  }
  else if (is("expand")) { Poly({P(15, 3.5f), P(20.5f, 3.5f), P(20.5f, 9)}); Poly({P(9, 20.5f), P(3.5f, 20.5f), P(3.5f, 15)}); Line(20.5f, 3.5f, 14, 10); Line(3.5f, 20.5f, 10, 14); }
  else if (is("minimize-2")) { Poly({P(4, 14), P(10, 14), P(10, 20)}); Poly({P(20, 10), P(14, 10), P(14, 4)}); Line(14, 10, 20.5f, 3.5f); Line(3.5f, 20.5f, 10, 14); }
  else if (is("hand")) {
    // Clean round-topped fingers and palm without self-reversing polyline spikes
    dl->PathLineTo(P(7.5f, 12.5f));
    dl->PathLineTo(P(7.5f, 5.8f));
    dl->PathArcTo(P(9.0f, 5.8f), 1.5f * s, 3.14159f, 6.28318f, 8);
    dl->PathLineTo(P(10.5f, 11.2f));
    dl->PathStroke(col, 0, th);

    dl->PathLineTo(P(10.5f, 11.2f));
    dl->PathLineTo(P(10.5f, 4.2f));
    dl->PathArcTo(P(12.0f, 4.2f), 1.5f * s, 3.14159f, 6.28318f, 8);
    dl->PathLineTo(P(13.5f, 11.2f));
    dl->PathStroke(col, 0, th);

    dl->PathLineTo(P(13.5f, 11.2f));
    dl->PathLineTo(P(13.5f, 5.5f));
    dl->PathArcTo(P(15.0f, 5.5f), 1.5f * s, 3.14159f, 6.28318f, 8);
    dl->PathLineTo(P(16.5f, 12.f));
    dl->PathStroke(col, 0, th);

    dl->PathLineTo(P(16.5f, 11.2f));
    dl->PathLineTo(P(16.5f, 7.5f));
    dl->PathArcTo(P(18.0f, 7.5f), 1.5f * s, 3.14159f, 6.28318f, 8);
    dl->PathLineTo(P(19.5f, 14.5f));
    dl->PathArcTo(P(13.2f, 14.8f), 6.3f * s, 0.f, 2.35f, 14);
    dl->PathLineTo(P(4.2f, 13.f));
    dl->PathArcTo(P(5.5f, 11.8f), 1.6f * s, 2.35f, 5.2f, 8);
    dl->PathLineTo(P(7.5f, 13.5f));
    dl->PathStroke(col, 0, th);
  }
  else if (is("magnet")) {
    dl->PathLineTo(P(4.5f, 5));
    dl->PathLineTo(P(4.5f, 12.5f));
    dl->PathArcTo(P(12, 12.5f), 7.5f * s, 3.14159f, 0.f, 16);
    dl->PathLineTo(P(19.5f, 5));
    dl->PathLineTo(P(15.5f, 5));
    dl->PathLineTo(P(15.5f, 12.5f));
    dl->PathArcTo(P(12, 12.5f), 3.5f * s, 0.f, 3.14159f, 12);
    dl->PathLineTo(P(8.5f, 5));
    dl->PathStroke(col, ImDrawFlags_Closed, th);
    Line(4.5f, 8.5f, 8.5f, 8.5f);
    Line(15.5f, 8.5f, 19.5f, 8.5f);
  }
  else if (is("repeat")) {
    Poly({P(17, 2.5f), P(20.5f, 6), P(17, 9.5f)});
    dl->PathLineTo(P(3.5f, 11));
    dl->PathArcTo(P(6.5f, 9), 3.f * s, 3.14159f, 4.71239f, 8);
    dl->PathLineTo(P(20.5f, 6));
    dl->PathStroke(col, 0, th);
    Poly({P(7, 21.5f), P(3.5f, 18), P(7, 14.5f)});
    dl->PathLineTo(P(20.5f, 13));
    dl->PathArcTo(P(17.5f, 15), 3.f * s, 0.f, 1.5708f, 8);
    dl->PathLineTo(P(3.5f, 18));
    dl->PathStroke(col, 0, th);
  }
  else {
    // Unknown name: draw a visible placeholder instead of nothing, and report it once in debug builds.
    Rect(5.f, 5.f, 19.f, 19.f, 2.5f);
    DotF(12.f, 12.f, 1.6f);
#ifndef NDEBUG
    static std::vector<std::string> reported;
    if (std::find(reported.begin(), reported.end(), n) == reported.end()) {
      reported.emplace_back(n);
      fprintf(stderr, "[ui::Icon] unknown icon name \"%s\"\n", n);
    }
#endif
  }
}

// ───────────────────────────── interaction ─────────────────────────────
bool Hover(ImRect r) {
  if (g.blocked) return false;
  if (!ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) return false;
  ImVec2 m = ImGui::GetIO().MousePos;
  ImRect clip(ImGui::GetCurrentWindow()->ClipRect);
  return r.Contains(m) && clip.Contains(m);
}
Hit HitR(ImRect r) {
  Hit h;
  h.hover = Hover(r);
  if (!h.hover) return h;
  h.click = ImGui::IsMouseClicked(0);
  h.dbl = ImGui::IsMouseDoubleClicked(0);
  h.rclick = ImGui::IsMouseClicked(1);
  h.down = ImGui::IsMouseDown(0);
  h.release = ImGui::IsMouseReleased(0);
  return h;
}
void CursorHand() { ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }

// ───────────────────────────── components ─────────────────────────────
uint32_t ToneHex(Tone t) {
  switch (t) {
    case T_LIVE: return pal::coral; case T_PREVIEW: return pal::cyan; case T_AUDIO: return pal::mint;
    case T_STANDBY: return pal::yellow; case T_ALERT: return pal::red; default: return pal::t88;
  }
}

struct BSize { float h, px, fs; };
static BSize Bs(int s) {
  static const BSize t[] = {{16, 4, 8}, {20, 6, 9}, {24, 8, 10}, {28, 10, 11}};
  return t[std::clamp(s, 0, 3)];
}
float ButtonW(const char* label, int size, bool hasIcon) {
  BSize b = Bs(size);
  std::string u = Upper(label);
  return TextW(UI_B, b.fs, u.c_str(), 0.09f) + b.px * 2 + 2 + (hasIcon ? b.fs + 4 : 0);
}

bool Button(ImRect r, const char* label, Tone tone, bool active, int size, bool uppercase, const char* icon) {
  BSize b = Bs(size);
  Hit h = HitR(r);
  uint32_t hex = ToneHex(tone);
  ImU32 bg = K(pal::g1c), bd, fg;
  if (active) {
    Glow(r, hex, 0.30f * 1.0f, 12, 3);
    bg = K(hex, 0.15f); bd = K(hex); fg = K(hex);
    Fill(r, K(pal::g1c), 3);
  } else {
    fg = h.hover ? K(pal::white) : K(pal::t77);
    bd = h.hover ? K(pal::g33) : K(pal::g22);
  }
  Box(r, bg, bd, 3);
  std::string t = uppercase ? Upper(label) : label;
  float ls = uppercase ? 0.09f : 0.01f, cy = (r.Min.y + r.Max.y) * 0.5f, cx = (r.Min.x + r.Max.x) * 0.5f;
  if (icon) {
    float tw = TextW(UI_B, b.fs, t.c_str(), ls), total = b.fs + 4 + tw, x0 = cx - total * 0.5f;
    Icon(icon, ImVec2(x0 + b.fs * 0.5f, cy), b.fs, fg);
    Text(x0 + b.fs + 4, cy, UI_B, b.fs, fg, t.c_str(), ls);
  } else TextC(cx, cy, UI_B, b.fs, fg, t.c_str(), ls);
  if (h.hover) CursorHand();
  return h.click;
}

bool ToggleBtn(ImRect r, const char* label, Tone tone, bool on) {
  Hit h = HitR(r);
  uint32_t hex = ToneHex(tone);
  ImU32 bg = K(pal::g1c), bd, fg;
  if (on) { Glow(r, hex, 0.30f, 10, 2); Fill(r, K(pal::g1c), 2); bg = K(hex, 0.15f); bd = K(hex); fg = K(hex); }
  else { fg = h.hover ? K(pal::white) : K(pal::t77); bd = h.hover ? K(pal::g33) : K(pal::g22); }
  Box(r, bg, bd, 2);
  TextC((r.Min.x + r.Max.x) * 0.5f, (r.Min.y + r.Max.y) * 0.5f, UI_B, 9, fg, label, 0.09f);
  if (h.hover) CursorHand();
  return h.click;
}

void PanelHeader(ImRect r, const char* title, uint32_t titleHex, ImU32 bg) {
  Fill(r, bg ? bg : K(pal::g18));
  HLine(r.Min.x, r.Max.x, r.Max.y - 1, K(pal::g2a));
  std::string u = Upper(title);
  Text(r.Min.x + 6, (r.Min.y + r.Max.y - 1) * 0.5f, UI_B, 9, K(titleHex), u.c_str(), 0.14f);
}

float BadgeW(const char* text, bool dot) {
  std::string u = Upper(text);
  return TextW(MONO_B, 10, u.c_str(), 0.09f) + 10 + 2 + (dot ? 10 : 0);
}
void Badge(float xr, float cy, const char* text, Tone tone, bool dot, float* outW) {
  uint32_t hex = ToneHex(tone);
  std::string u = Upper(text);
  float w = BadgeW(text, dot);
  ImRect r(xr - w, cy - 8, xr, cy + 8);
  uint32_t bdHex = hex;
  ImU32 bg = tone == T_NEUTRAL ? 0 : K(hex, 0.15f);
  Box(r, bg, tone == T_NEUTRAL ? K(pal::g22) : K(bdHex), 2);
  float x = r.Min.x + 6;
  if (dot) {
    float pulse = 0.7f + 0.3f * cosf((float)g.time * 2.f * 3.14159f / 1.4f);
    Dot(ImVec2(x + 3, cy), 6, hex, true, pulse);
    x += 10;
  }
  Text(x, cy, MONO_B, 10, K(hex), u.c_str(), 0.09f);
  if (outW) *outW = w;
}

bool Slider(uint32_t id, ImRect tr, float& v, uint32_t hex, float mn, float mx, bool neutralIdle) {
  ImRect hit(tr.Min.x - 5, tr.Min.y - 5, tr.Max.x + 5, tr.Max.y + 5);
  Hit h = HitR(hit);
  bool changed = false;
  if (h.click) g.active = id;
  if (g.active == id) {
    if (ImGui::IsMouseDown(0)) {
      float t = std::clamp((ImGui::GetIO().MousePos.x - tr.Min.x) / std::max(1.f, tr.GetWidth()), 0.f, 1.f);
      float nv = std::round(mn + t * (mx - mn));
      if (nv != v) { v = nv; changed = true; }
    } else g.active = 0;
  }
  float pct = std::clamp((v - mn) / (mx - mn), 0.f, 1.f);
  Box(tr, K(pal::meterTrack), K(pal::g22), 999);
  float fw = (tr.GetWidth() - 2) * pct;
  if (fw > 0.5f) {
    ImRect fr(tr.Min.x + 1, tr.Min.y + 1, tr.Min.x + 1 + std::max(fw, 4.f), tr.Max.y - 1);
    // neutralIdle bars stay grey (no halo) and only take the caller's accent while the user is dragging them,
    // so a panel full of sliders reads as one calm control instead of a rainbow.
    if (neutralIdle && g.active != id) Fill(fr, K(pal::meterFill), 999);
    else {
      if (kGlowFx) for (int k = 3; k >= 1; --k) g.dl->AddRectFilled(ImVec2(fr.Min.x - k, fr.Min.y - k), ImVec2(fr.Max.x + k, fr.Max.y + k), Ca(K(hex, 0.07f)), 999);
      Fill(fr, K(hex), 999);
    }
  }
  if (h.hover) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  return changed;
}

void PropertyRow(ImRect r, const char* label, const char* value, const char* unit, uint32_t hex) {
  HLine(r.Min.x, r.Max.x, r.Max.y - 1, K(pal::g2a));
  float cy = (r.Min.y + r.Max.y - 1) * 0.5f;
  std::string u = Upper(label);
  TextEll(r.Min.x + 6, cy, 80, UI_S, 10, K(pal::t77), u.c_str(), 0.09f);
  float xr = r.Max.x - 6;
  if (unit && *unit) { TextR(xr, cy, MONO_M, 10, K(pal::t66), unit); xr -= TextW(MONO_M, 10, unit) + 1; }
  float maxW = xr - (r.Min.x + 6 + 80 + 6);
  float w = TextW(MONO_M, 10, value);
  if (w > maxW) TextEll(xr - maxW, cy, maxW, MONO_M, 10, K(hex), value);
  else TextR(xr, cy, MONO_M, 10, K(hex), value);
}

bool TextField(const char* id, ImRect r, std::string& v, FontId f, float sz, ImU32 textCol) {
  ImGui::SetCursorScreenPos(r.Min);
  sz = TextPx(sz);
  ImGui::PushFont(F(f), sz);
  ImGui::PushStyleColor(ImGuiCol_FrameBg, K(pal::g050));
  ImGui::PushStyleColor(ImGuiCol_Border, K(pal::g22));
  ImGui::PushStyleColor(ImGuiCol_Text, textCol ? textCol : K(pal::tf3));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, std::max(0.f, (r.GetHeight() - sz) * 0.5f)));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
  ImGui::SetNextItemWidth(r.GetWidth());
  char buf[256]; snprintf(buf, sizeof buf, "%s", v.c_str());
  bool ch = ImGui::InputText(id, buf, sizeof buf);
  ImGui::PopStyleVar(3); ImGui::PopStyleColor(3); ImGui::PopFont();
  if (ch) v = buf;
  return ch;
}

bool IntField(const char* id, ImRect r, int& v) {
  static std::string edit;
  ImGuiID gid = ImGui::GetID(id);
  if (ImGui::GetActiveID() != gid) edit = std::to_string(v);
  ImGui::SetCursorScreenPos(r.Min);
  ImGui::PushFont(F(MONO_R), TextPx(11));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, K(pal::g050));
  ImGui::PushStyleColor(ImGuiCol_Border, K(pal::g22));
  ImGui::PushStyleColor(ImGuiCol_Text, K(pal::tf3));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, std::max(0.f, (r.GetHeight() - TextPx(11)) * 0.5f)));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
  ImGui::SetNextItemWidth(r.GetWidth());
  char buf[32]; snprintf(buf, sizeof buf, "%s", edit.c_str());
  bool ch = ImGui::InputText(id, buf, sizeof buf, ImGuiInputTextFlags_CharsDecimal);
  ImGui::PopStyleVar(3); ImGui::PopStyleColor(3); ImGui::PopFont();
  if (ch) { edit = buf; v = atoi(buf); }
  return ch;
}

bool FloatField(const char* id, ImRect r, float& v, int decimals) {
  static std::string edit;
  ImGuiID gid = ImGui::GetID(id);
  if (ImGui::GetActiveID() != gid) {
    char b[32]; snprintf(b, sizeof b, "%.*f", std::max(0, decimals), v);
    std::string t = b;
    if (decimals > 0 && t.find('.') != std::string::npos) { while (!t.empty() && t.back() == '0') t.pop_back(); if (!t.empty() && t.back() == '.') t.pop_back(); }
    if (t == "-0") t = "0";
    edit = t;
  }
  ImGui::SetCursorScreenPos(r.Min);
  ImGui::PushFont(F(MONO_R), TextPx(11));
  ImGui::PushStyleColor(ImGuiCol_FrameBg, K(pal::g050));
  ImGui::PushStyleColor(ImGuiCol_Border, K(pal::g22));
  ImGui::PushStyleColor(ImGuiCol_Text, K(pal::tf3));
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, std::max(0.f, (r.GetHeight() - TextPx(11)) * 0.5f)));
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.f);
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.f);
  ImGui::SetNextItemWidth(r.GetWidth());
  char buf[32]; snprintf(buf, sizeof buf, "%s", edit.c_str());
  bool ch = ImGui::InputText(id, buf, sizeof buf, ImGuiInputTextFlags_CharsDecimal);
  ImGui::PopStyleVar(3); ImGui::PopStyleColor(3); ImGui::PopFont();
  if (ch) { edit = buf; v = (float)atof(buf); }
  return ch;
}

}  // namespace ui





// MikMap UI toolkit: design-system tokens + immediate-mode drawing primitives on top of Dear ImGui.
#pragma once
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

namespace pal {
// Surfaces (theme-dependent: filled from the selected "Workspace base" ramp)
inline uint32_t g050 = 0x050505, g0f = 0x0f0f0f, g10 = 0x101010, g12 = 0x121212, g14 = 0x141414,
                g16 = 0x161616, g18 = 0x181818, g1c = 0x1c1c1c;
// Borders / fixed greys
constexpr uint32_t g22 = 0x222222, g2a = 0x2a2a2a, g33 = 0x333333, g3a = 0x3a3a3a, ctrlHover = 0x232323, layerHover = 0x151515;
// Text ramp
constexpr uint32_t white = 0xffffff, tf3 = 0xf3f3f3, te0 = 0xe0e0e0, tcc = 0xcccccc, t88 = 0x888888,
                   t77 = 0x777777, t66 = 0x666666;
// Accents (coral/cyan/mint follow the selected "Signal colors")
inline uint32_t coral = 0xff7f50, cyan = 0x118ab2, mint = 0x06d6a0;
constexpr uint32_t yellow = 0xffd166, red = 0xef4444, violet = 0xb388ff;
// Clip cell bands (design tokens §3.2) — three states, not just "loaded vs coral": loaded (idle, warm),
// preview (cued, cool cyan-tinted — distinct from live so you can tell "about to show" from "showing"),
// live (hot burnt-orange, NOT the flat accent coral — a darker, richer tone that survives sitting lit for hours).
constexpr uint32_t clipLoadedBg = 0x1a0e07, clipLoadedBorder = 0x4a2411, clipLoadedText = 0xe8c4a2;
constexpr uint32_t clipBarLoaded = 0x2e1a0e, clipBodyLoaded = 0x150b05, clipFootLoaded = 0x8a6244;
constexpr uint32_t clipBarPreview = 0x0e2430, clipBodyPreview = 0x0b141b, clipPreviewText = 0xcfe9f5, clipFootPreview = 0x5d8ba3;
constexpr uint32_t clipBarLive = 0x8a3c14, clipBodyLive = 0x2a1408, clipLiveText = 0xffcbaa, clipFootLive = 0xffcbaa;
constexpr uint32_t meterTrack = 0x161616;
}  // namespace pal

struct WarpMap;  // defined in app.h — kept at global scope so ui:: does not declare a second one

namespace ui {

enum FontId { UI_R, UI_S, UI_B, UI_X, MONO_R, MONO_M, MONO_B, FONT_COUNT };

struct Ctx {
  ImDrawList* dl = nullptr;
  ImFont* fonts[FONT_COUNT] = {};
  float alpha = 1.f;         // global multiplier (bypassed layers dim to .45)
  bool blocked = false;      // an overlay is capturing the pointer
  uint32_t active = 0;       // id of the widget being dragged
  double time = 0;
  ImVec2 mouse{};
  const WarpMap* warp = nullptr;  // non-null while drawing into the warped projector output
};
extern Ctx g;

// ---- colour helpers ----
inline ImU32 K(uint32_t hex, float a = 1.f) {
  return IM_COL32((hex >> 16) & 255, (hex >> 8) & 255, hex & 255, (int)(a * 255.f + 0.5f));
}
ImU32 Ca(ImU32 c);                                  // applies g.alpha
ImU32 MixHex(uint32_t base, uint32_t tint, float t);  // color-mix(in srgb, tint t%, base)
ImU32 Lerp(ImU32 a, ImU32 b, float t);

// ---- geometry ----
inline ImRect Rc(float x, float y, float w, float h) { return ImRect(x, y, x + w, y + h); }
inline ImRect Inset(ImRect r, float d) { return ImRect(r.Min.x + d, r.Min.y + d, r.Max.x - d, r.Max.y - d); }
inline ImRect Inset(ImRect r, float dx, float dy) { return ImRect(r.Min.x + dx, r.Min.y + dy, r.Max.x - dx, r.Max.y - dy); }

// ---- text ----
ImFont* F(FontId f);
float TextW(FontId f, float sz, const char* s, float ls = 0.f);
void Text(float x, float cy, FontId f, float sz, ImU32 col, const char* s, float ls = 0.f);      // left, vertically centred on cy
void TextR(float xr, float cy, FontId f, float sz, ImU32 col, const char* s, float ls = 0.f);     // right aligned
void TextC(float cx, float cy, FontId f, float sz, ImU32 col, const char* s, float ls = 0.f);     // centred
void TextEll(float x, float cy, float maxW, FontId f, float sz, ImU32 col, const char* s, float ls = 0.f);
std::string Upper(const std::string& s);

// ---- shapes ----
void Fill(ImRect r, ImU32 c, float rd = 0);
void Border(ImRect r, ImU32 c, float rd = 0, float w = 1.f);       // inside stroke, CSS-style
void Box(ImRect r, ImU32 bg, ImU32 bd, float rd = 0);
void Glow(ImRect r, uint32_t hex, float a, float blur = 12.f, float rd = 3.f);
void HLine(float x0, float x1, float y, ImU32 c);
void VLine(float x, float y0, float y1, ImU32 c);
void GradDiag(ImRect r, uint32_t c1, uint32_t c2, float a);          // "to bottom right"
void RadialFan(ImVec2 c, float r, ImU32 inner, ImU32 outer, float a0 = 0.f, float a1 = 6.2831853f, int seg = 64);
void Dot(ImVec2 c, float d, uint32_t hex, bool glow = true, float alpha = 1.f);
void DashedPoly(const ImVec2* p, int n, ImU32 c, float th, float dash, float gap);
void Icon(const char* name, ImVec2 c, float sz, ImU32 col);
void Check(ImVec2 c, float sz, ImU32 col);

// ---- interaction ----
struct Hit { bool hover = false, click = false, dbl = false, rclick = false, down = false, release = false; };
Hit HitR(ImRect r);
bool Hover(ImRect r);
void CursorHand();

// ---- composite widgets (design-system components) ----
enum Tone { T_LIVE, T_PREVIEW, T_AUDIO, T_STANDBY, T_ALERT, T_NEUTRAL };
uint32_t ToneHex(Tone t);
float ButtonW(const char* label, int size, bool hasIcon = false);
bool Button(ImRect r, const char* label, Tone tone, bool active, int size = 1, bool uppercase = true, const char* icon = nullptr);  // sm = 1
bool ToggleBtn(ImRect r, const char* label, Tone tone, bool on);
void PanelHeader(ImRect r, const char* title, uint32_t titleHex, ImU32 bg = 0);
void Badge(float xr, float cy, const char* text, Tone tone, bool dot, float* outW = nullptr);   // right-aligned at xr
float BadgeW(const char* text, bool dot);
bool Slider(uint32_t id, ImRect track, float& v, uint32_t hex, float mn = 0, float mx = 100);
void PropertyRow(ImRect r, const char* label, const char* value, const char* unit = nullptr, uint32_t hex = pal::tcc);
bool TextField(const char* id, ImRect r, std::string& v, FontId f = UI_S, float sz = 10.f, ImU32 textCol = 0);
bool IntField(const char* id, ImRect r, int& v);

// ---- overlay menus ----
struct MenuItem {
  std::string label, icon, shortcut;
  bool disabled = false, danger = false, divider = false;
  uint32_t toneHex = 0;
  std::function<void()> run;
};

}  // namespace ui




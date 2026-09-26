// MikMap Pro — VJ / projection-mapping workspace (C++ / Dear ImGui / OpenGL).
#include "app.h"

#include <GLFW/glfw3.h>
#ifdef _WIN32
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#include <dwmapi.h>
#endif

#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <chrono>
#include <ctime>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>

using namespace ui;
namespace fs = std::filesystem;

App A;
extern const char* BLEND_NAMES[8];

static GLFWwindow* gWin = nullptr;
GLFWwindow* glfwWin() { return gWin; }
static GLuint gLogoTex = 0;
unsigned LogoTexture() { return gLogoTex; }   // the MikMap mark, also the centrepiece of the test card (clipart.cpp)
static std::string gAssets;

static std::string FindAssets(const char* argv0) {
  std::vector<fs::path> cands = {fs::path(argv0).parent_path() / "assets", fs::current_path() / "assets", fs::path(argv0).parent_path() / ".." / "assets"};
  for (auto& c : cands) if (fs::exists(c / "fonts")) return c.string();
  return "assets";
}

static GLuint LoadTexture(const std::string& path) {
  int w, h, n;
  unsigned char* px = stbi_load(path.c_str(), &w, &h, &n, 4);
  if (!px) return 0;
  GLuint t; glGenTextures(1, &t);
  glBindTexture(GL_TEXTURE_2D, t);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
  glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, px);
  stbi_image_free(px);
  return t;
}

// Shorten a UTF-8 string to at most n code points, adding an ellipsis; never cuts inside a multi-byte character.
static std::string Trunc(const std::string& s, size_t n) {
  size_t cp = 0, i = 0;
  while (i < s.size()) { if ((s[i] & 0xC0) != 0x80) { if (cp == n) break; ++cp; } ++i; }
  return i >= s.size() ? s : s.substr(0, i) + "\xE2\x80\xA6";
}

// ───────────────────────── chrome ─────────────────────────
static void TitleBar(ImRect r) {
  Fill(r, K(pal::g1c));
  HLine(r.Min.x, r.Max.x, r.Max.y - 1, K(pal::g2a));
  float cy = (r.Min.y + r.Max.y - 1) * 0.5f;
  float x = 8;

  // logo / project menu trigger
  std::string subS = Trunc(A.projectName, 16); const char* sub = subS.c_str();
  float titleW = TextW(UI_X, 12, "MIKMAP", 0.09f) + 4 + 9, subW = TextW(MONO_M, 9, sub);
  float colW = std::max(titleW, subW);
  bool hot = A.logoHover || A.projectMenu;
  float gw = 6 + 26 + 6 + colW + 6 + (A.logoHover ? 6 + 8 : 0);
  ImRect lg(x, r.Min.y, x + gw, r.Max.y - 1);
  Hit lh = HitR(lg);
  A.logoHover = lh.hover;
  if (hot) { Box(lg, K(pal::g18), K(pal::coral), 4); for (int k = 4; k >= 1; --k) g.dl->AddRectFilled(ImVec2(lg.Min.x - k * 2, lg.Min.y - k), ImVec2(lg.Max.x + k * 2, lg.Max.y + k), Ca(K(pal::coral, 0.025f)), 4 + k * 2); Box(lg, K(pal::g18), K(pal::coral), 4); }
  if (gLogoTex) {
    ImVec2 c(lg.Min.x + 6 + 13, cy);
    if (hot) for (int k = 3; k >= 1; --k) g.dl->AddCircleFilled(c, 13 + k * 2, Ca(K(pal::coral, 0.06f)), 24);
    g.dl->AddImage((ImTextureID)(intptr_t)gLogoTex, ImVec2(c.x - 13, c.y - 13), ImVec2(c.x + 13, c.y + 13), ImVec2(0, 0), ImVec2(1, 1), Ca(IM_COL32_WHITE));
  }
  float tx = lg.Min.x + 6 + 26 + 6;
  ImU32 fg = K(hot ? pal::coral : pal::tf3);
  Text(tx, cy - 6.5f, UI_X, 12, fg, "MIKMAP", 0.09f);
  Icon("chevron-down", ImVec2(tx + TextW(UI_X, 12, "MIKMAP", 0.09f) + 4 + 4.5f, cy - 6.5f), 9, fg);
  Text(tx, cy + 7, MONO_M, 9, K(pal::t66), sub);
  if (A.logoHover) Dot(ImVec2(lg.Max.x - 6 - 4, cy), 8, pal::mint);
  if (lh.hover) CursorHand();
  if (lh.click) A.projectMenu = !A.projectMenu;
  x = lg.Max.x + 6;
  VLine(x, cy - 7, cy + 7, K(pal::g2a));
  x += 1 + 6;

  // screen tabs
  struct T { const char* label; const char* icon; } tabs[3] = {{"Composition", "layers"}, {"Advanced Mapping", "move-3d"}, {"Sensor I/O", "activity"}};
  float tw[3], total = 10 + 8;
  for (int i = 0; i < 3; ++i) { tw[i] = 10 + 11 + 4 + TextW(UI_B, 12, Upper(tabs[i].label).c_str()) + 10 + 2; total += tw[i] + (i ? 4 : 0); }
  ImRect grp(x, cy - 12, x + total, cy + 12);
  Box(grp, K(pal::g12), K(pal::g2a), 4);
  float tx0 = grp.Min.x + 5 + 1;
  for (int i = 0; i < 3; ++i) {
    ImRect tr(tx0, cy - 10, tx0 + tw[i], cy + 10);
    bool on = A.screen == i;
    Hit h = HitR(tr);
    if (on) { Glow(tr, pal::coral, 0.30f, 12, 3); Fill(tr, K(pal::g12), 3); Box(tr, K(pal::coral, 0.12f), K(pal::coral), 3); }
    ImU32 c = K(on ? pal::coral : pal::t77);
    Icon(tabs[i].icon, ImVec2(tr.Min.x + 10 + 5.5f, cy), 11, c);
    Text(tr.Min.x + 10 + 11 + 4, cy, UI_B, 12, c, Upper(tabs[i].label).c_str());
    if (h.hover) CursorHand();
    if (h.click) A.screen = i;
    tx0 += tw[i] + 4;
  }
  // right cluster
  float xr = r.Max.x - 8;
  {
    ImRect gb(xr - 20, cy - 10, xr, cy + 10);
    Hit gh = HitR(gb);
    Box(gb, K(pal::g1c), gh.hover ? K(pal::g3a) : K(pal::g22), 3);
    Icon("settings", ImVec2((gb.Min.x + gb.Max.x) * 0.5f, cy), 11, K(gh.hover ? pal::tf3 : pal::t88));
    if (gh.hover) CursorHand();
    if (gh.click) { A.settingsOpen = true; A.projectMenu = false; }
    xr -= 20 + 6;
  }
  std::string fileS = A.projectName + ".mikmap" + (A.projectDirty ? " *" : "") + (A.projectPath.empty() ? " (unsaved)" : ""); const char* file = fileS.c_str();
  float fw = TextW(MONO_M, 10, file);
  TextR(xr, cy, MONO_M, 10, K(pal::t66), file);
  xr -= fw + 6;
  const char* ol = A.blackout ? "Blackout" : "Live";
  float bw = BadgeW(ol, true);
  Badge(xr, cy, ol, A.blackout ? T_ALERT : T_LIVE, true);
  xr -= bw + 6;
  float w2 = ButtonW("Blackout", 1);
  if (Button(ImRect(xr - w2, cy - 10, xr, cy + 10), "Blackout", T_ALERT, A.blackout, 1)) A.blackout = !A.blackout;
  xr -= w2 + 6;
  float w1 = ButtonW("Show TestCard", 1);
  if (Button(ImRect(xr - w1, cy - 10, xr, cy + 10), "Show TestCard", T_AUDIO, A.testCard, 1)) A.testCard = !A.testCard;
}

static void StatusBar(ImRect r) {
  Fill(r, K(pal::g1c));
  HLine(r.Min.x, r.Max.x, r.Min.y, K(pal::g2a));
  float cy = r.Min.y + 11.5f;
  float x = 8;
  Dot(ImVec2(x + 3, cy), 6, pal::mint, true, A.beat ? 1.f : 0.6f);
  x += 6 + 8;
  {
    int on = 0; for (auto& d : A.devices) if (d.connected) ++on;
    char dv[64]; snprintf(dv, sizeof dv, "SENSORS %d/%d", on, (int)A.devices.size());
    Text(x, cy, MONO_M, 10, K(pal::t66), dv); x += TextW(MONO_M, 10, dv) + 8;
    {   // tempo: click = tap tempo (average of the last taps), wheel = fine adjust, right-click = reset to 128
      char bp[24]; snprintf(bp, sizeof bp, "%.1f BPM", A.bpm);
      float bw = TextW(MONO_M, 10, bp) + 8;
      ImRect br(x - 2, r.Min.y + 2, x + bw, r.Max.y - 2);
      Hit bh = HitR(br);
      if (bh.hover) { CursorHand(); Fill(br, K(pal::g18), 2); }
      static double taps[6]; static int nt = 0;
      if (bh.click) {
        double now = g.time;
        if (nt > 0 && now - taps[nt - 1] > 2.0) nt = 0;                    // a pause starts a new tap sequence
        if (nt == 6) { for (int i = 1; i < 6; ++i) taps[i - 1] = taps[i]; nt = 5; }
        taps[nt++] = now;
        if (nt >= 2) A.bpm = std::clamp((float)(60.0 * (nt - 1) / (taps[nt - 1] - taps[0])), 40.f, 240.f);
      }
      if (bh.rclick) A.bpm = 128.f;
      if (bh.hover && ImGui::GetIO().MouseWheel != 0.f) A.bpm = std::clamp(std::round((A.bpm + ImGui::GetIO().MouseWheel) * 10.f) / 10.f, 40.f, 240.f);
      Text(x, cy, MONO_M, 10, K(bh.hover ? pal::coral : pal::t66), bp); x += bw + 4;
    }
    char out[64]; snprintf(out, sizeof out, "OUTPUT %s", OutputOpen() ? "OPEN" : "CLOSED");
    Text(x, cy, MONO_M, 10, K(OutputOpen() ? pal::mint : pal::t66), out); x += TextW(MONO_M, 10, out) + 8;
  }
  // G9: real frame statistics instead of the mock timecode
  char perf[64];
  snprintf(perf, sizeof perf, "%.0f FPS \xC2\xB7 P99 %.1f MS \xC2\xB7 DROP %d", PerfFps(), PerfP99(), PerfDrops(20.f));
  const char* tc = perf;
  TextR(r.Max.x - 8, cy, MONO_M, 10, K(PerfP99() > 20.f ? pal::yellow : pal::t66), tc);
  const char* hint = (!A.toast.empty() && g.time < A.toastUntil) ? A.toast.c_str() : A.blackout ? "OUTPUT MUTED \xE2\x80\x94 PRESS BLACKOUT TO RESUME" : "CLICK NAME TO CUE \xC2\xB7 CLICK ART TO PLAY \xC2\xB7 RIGHT-CLICK NAME FOR ACTIONS";
  TextR(r.Max.x - 8 - TextW(MONO_M, 10, tc) - 8, cy, MONO_M, 10, K(pal::t88), hint);
}

// ───────────────────────── overlays ─────────────────────────
static bool Raw(ImRect r) { return r.Contains(ImGui::GetIO().MousePos); }
static void Shadow(ImRect r, float rd, float spread, float a) {
  for (int k = 6; k >= 1; --k) g.dl->AddRectFilled(ImVec2(r.Min.x - spread * k / 6, r.Min.y - spread * k / 6 + 4), ImVec2(r.Max.x + spread * k / 6, r.Max.y + spread * k / 6 + 4), Ca(K(0x000000, a / 6)), rd + k);
}

struct PMItem { const char* icon; uint32_t tone; const char* label; const char* sub; const char* sc; bool chev, sep; };
static const PMItem PM[] = {
    {"file-plus", 1, "D\xE1\xBB\xB1 \xC3\xA1n m\xE1\xBB\x9Bi", "B\xE1\xBA\xAFt \xC4\x91\xE1\xBA\xA7u v\xE1\xBB\x9Bi deck v\xC3\xA0 mapping tr\xE1\xBB\x91ng", "Ctrl+N", false, true},
    {"folder-open", 2, "M\xE1\xBB\x9F d\xE1\xBB\xB1 \xC3\xA1n...", "Ch\xE1\xBB\x8Dn t\xE1\xBB\x87p .mikmap trong Documents/MikMap", "Ctrl+O", false, false},
    {"clock", 3, "M\xE1\xBB\x9F g\xE1\xBA\xA7n \xC4\x91\xC3\xA2y", "D\xE1\xBB\xB1 \xC3\xA1n l\xC6\xB0u g\xE1\xBA\xA7n nh\xE1\xBA\xA5t l\xC3\xAAn \xC4\x91\xE1\xBA\xA7u", "", true, false},
    {"save", 4, "L\xC6\xB0u d\xE1\xBB\xB1 \xC3\xA1n", "Ghi v\xC3\xA0o Documents/MikMap", "Ctrl+S", false, false},
    {"download", 3, "L\xC6\xB0u b\xE1\xBA\xA3n sao", "Th\xC3\xAAm m\xE1\xBB\x99t b\xE1\xBA\xA3n .mikmap c\xC3\xB3 ng\xC3\xA0y gi\xE1\xBB\x9D", "Ctrl+Shift+S", false, false},
    {"eye", 0, "Ch\xE1\xBA\xBF \xC4\x91\xE1\xBB\x99 Show", "Ch\xE1\xBB\x89 hi\xE1\xBB\x87n h\xC3\xACnh ra, \xE1\xBA\xA9n giao di\xE1\xBB\x87n ch\xE1\xBB\x89nh s\xE1\xBB\xAD" "a", "Tab", false, true},
    {"settings", 1, "C\xC3\xA0i \xC4\x91\xE1\xBA\xB7t h\xE1\xBB\x87 th\xE1\xBB\x91ng", "Ng\xC3\xB4n ng\xE1\xBB\xAF, font, m\xC3\xA0u, c\xE1\xBB\xA1 ch\xE1\xBB\xAF", "", true, true},
    {"circle-help", 3, "Tr\xE1\xBB\xA3 gi\xC3\xBAp & Ph\xC3\xADm t\xE1\xBA\xAFt", "B\xE1\xBA\xA3ng ph\xC3\xADm t\xE1\xBA\xAFt", "", true, false},
    {"info", 2, "Gi\xE1\xBB\x9Bi thi\xE1\xBB\x87u MikMap", "v1.0.0 Enterprise Engine", "", true, false},
    {"rotate-ccw", 4, "N\xE1\xBA\xA1p l\xE1\xBA\xA1i m\xE1\xBA\xABu Demo", "Thay d\xE1\xBB\xB1 \xC3\xA1n hi\xE1\xBB\x87n t\xE1\xBA\xA1i b\xE1\xBA\xB1ng b\xE1\xBA\xA3n m\xE1\xBA\xABu", "", false, true},
};

static uint32_t PmHex(uint32_t c) { switch (c) { case 1: return pal::red; case 2: return pal::coral; case 3: return pal::cyan; case 4: return pal::mint; default: return pal::t88; } }
static ImRect ProjectMenuRect() { return Rc(8, 39, 322, 24 + 8 + 28 + 6 + 10 + 8 + 1 + 4 + 10 * 36 + 4 + 28 + 1); }

static void GuardedDiscard(const char* what, const std::function<void()>& go) {
  if (A.projectDirty && g.time >= A.discardUntil) { A.discardUntil = g.time + 4; A.notify(std::string("Unsaved changes \xE2\x80\x94 repeat to discard and ") + what, 4); return; }
  go();
}
static void OpenDialogShow() { A.openList = ListProjects(); A.openDialog = true; A.helpOpen = false; }
static void RunProjectItem(int i) {
  switch (i) {
    case 0: GuardedDiscard("start a new project", [] { NewBlankProject(); A.notify("New project"); }); break;
    case 1: case 2: OpenDialogShow(); break;
    case 3: A.notify(DoSave(false)); A.projectDirty = ProjectDirty(); break;
    case 4: A.notify(DoSave(true)); break;
    case 5: A.showMode = true; A.notify("Show Mode \xE2\x80\x94 press Esc or Tab to leave", 3); break;
    case 6: A.settingsOpen = true; break;
    case 7: A.helpOpen = true; A.openDialog = false; break;
    case 8: A.notify("MikMap v1.0.0 \xE2\x80\x94 projection mapping engine"); break;
    case 9: GuardedDiscard("reload the demo", [] { NewProject(); A.notify("Demo project reloaded"); }); break;
    default: A.notify("Not available yet"); break;
  }
}

static void ProjectMenu() {
  if (!A.projectMenu) return;
  ImRect r = ProjectMenuRect();
  Shadow(r, 4, 40, 0.7f);
  Box(r, K(pal::g12), K(pal::g2a), 4);
  g.dl->PushClipRect(r.Min, r.Max, true);
  ImRect hd(r.Min.x + 1, r.Min.y + 1, r.Max.x - 1, r.Min.y + 25);
  Fill(hd, K(pal::g18)); HLine(hd.Min.x, hd.Max.x, hd.Max.y - 1, K(pal::g2a));
  float cy = (hd.Min.y + hd.Max.y - 1) * 0.5f;
  Icon("folder", ImVec2(hd.Min.x + 8 + 5, cy), 10, K(pal::coral));
  Text(hd.Min.x + 8 + 10 + 6, cy, UI_B, 9, K(pal::tcc), "D\xE1\xBB\xB0 \xC3\x81N HI\xE1\xBB\x86N T\xE1\xBA\xA0I", 0.14f);
  Badge(hd.Max.x - 8, cy, A.projectDirty ? "Ch\xC6\xB0" "a l\xC6\xB0u" : "\xC4\x90\xC3\xA3 l\xC6\xB0u", A.projectDirty ? T_STANDBY : T_AUDIO, true);
  float y = hd.Max.y + 8;
  ImRect nm(r.Min.x + 9, y, r.Max.x - 9, y + 28);
  Box(nm, K(pal::g050), K(pal::g22), 3);
  Text(nm.Min.x + 6, (nm.Min.y + nm.Max.y) * 0.5f, UI_B, 13, K(pal::tf3), Trunc(A.projectName, 28).c_str());
  y += 28 + 6;
  { char eb[48]; snprintf(eb, sizeof eb, "Engine v1.0 \xC2\xB7 %d\xC3\x97%d", A.canvasW, A.canvasH); Text(r.Min.x + 9, y + 5, MONO_M, 10, K(pal::t66), eb); }
  { char hm[8]; std::time_t tt = std::time(nullptr); std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &tt);
#else
    localtime_r(&tt, &tmv);
#endif
    std::strftime(hm, sizeof hm, "%H:%M", &tmv); TextR(r.Max.x - 9, y + 5, MONO_M, 10, K(pal::t66), hm); }
  y += 10 + 8;
  HLine(r.Min.x + 1, r.Max.x - 1, y, K(pal::g2a)); y += 1 + 4;
  for (int i = 0; i < 10; ++i) {
    const PMItem& it = PM[i];
    ImRect ir(r.Min.x + 1, y, r.Max.x - 1, y + 36);
    Hit h; h.hover = Raw(ir); h.click = h.hover && ImGui::IsMouseClicked(0);
    if (h.hover) Fill(ir, K(pal::layerHover));
    if (it.sep && i > 0) HLine(ir.Min.x, ir.Max.x, ir.Min.y, K(pal::g2a));
    float icy = (ir.Min.y + ir.Max.y) * 0.5f;
    Icon(it.icon, ImVec2(ir.Min.x + 8 + 7.5f, icy), 15, K(PmHex(it.tone)));
    Text(ir.Min.x + 8 + 15 + 8, icy - 6, UI_B, 12, K(pal::te0), it.label);
    Text(ir.Min.x + 8 + 15 + 8, icy + 7, MONO_M, 9, K(pal::t66), it.sub);
    float xr = ir.Max.x - 8;
    if (it.chev) { Icon("chevron-right", ImVec2(xr - 5.5f, icy), 11, K(pal::t66)); xr -= 11 + 8; }
    if (*it.sc) TextR(xr, icy, MONO_M, 9, K(pal::t66), it.sc);
    if (h.hover) CursorHand();
    if (h.click) { A.projectMenu = false; RunProjectItem(i); }
    y += 36;
  }
  y += 4;
  ImRect ft(r.Min.x + 1, y, r.Max.x - 1, r.Max.y - 1);
  Fill(ft, K(pal::g18)); HLine(ft.Min.x, ft.Max.x, ft.Min.y, K(pal::g2a));
  Text(ft.Min.x + 8, (ft.Min.y + ft.Max.y) * 0.5f, MONO_M, 9, K(pal::t66), "MIKMAP PROJECTION ENGINE", 0.09f);
  TextR(ft.Max.x - 8, (ft.Min.y + ft.Max.y) * 0.5f, MONO_M, 9, K(pal::t66), "v1.0.0");
  g.dl->PopClipRect();
}

struct PItem { const char* label; const char* icon; const char* sc; int tone; bool disabled, divider; };

// Popover component: title + rows. Returns the clicked row index (or -1); rect receives the popover bounds.
// extraH reserves room at the bottom of the SAME box for content the caller draws itself (the clip popover's colour swatches).
static int PopoverList(ImVec2 pos, ImVec2 disp, const char* title, const std::vector<PItem>& items, bool fresh, ImRect& out, float extraH = 0.f) {
  float w = 168, h = 2 + 3 + 9 + 4 + 2 + extraH;
  for (auto& it : items) h += it.divider ? 7 : 22;
  ImVec2 p(std::min(pos.x, disp.x - w - 4), std::min(pos.y, disp.y - h - 4 - 50));
  ImRect r(p.x, p.y, p.x + w, p.y + h);
  out = r;
  Shadow(r, 4, 24, 0.7f);
  Box(r, K(pal::g14), K(pal::g2a), 4);
  Text(r.Min.x + 2 + 6, r.Min.y + 2 + 3 + 5, UI_B, 9, K(pal::t66), Upper(title).c_str(), 0.14f);
  float y = r.Min.y + 2 + 3 + 9 + 4;
  int clicked = -1;
  ImGuiIO& io = ImGui::GetIO();
  for (int i = 0; i < (int)items.size(); ++i) {
    const PItem& it = items[i];
    if (it.divider) { HLine(r.Min.x + 2, r.Max.x - 2, y + 3, K(pal::g2a)); y += 7; continue; }
    ImRect ir(r.Min.x + 2, y, r.Max.x - 2, y + 22);
    bool hv = Raw(ir) && !it.disabled;
    if (hv) Fill(ir, K(pal::g1c), 2);
    uint32_t col = it.disabled ? pal::t66 : it.tone == 1 ? pal::coral : it.tone == 2 ? pal::red : (hv ? pal::white : pal::tcc);
    float cy = (ir.Min.y + ir.Max.y) * 0.5f;
    Icon(it.icon, ImVec2(ir.Min.x + 6 + 5.5f, cy), 11, K(col));
    Text(ir.Min.x + 6 + 11 + 6, cy, UI_S, 10, K(col), it.label);
    if (*it.sc) TextR(ir.Max.x - 6, cy, MONO_M, 10, K(pal::t66), it.sc);
    if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (hv && io.MouseClicked[0] && !fresh) clicked = i;
    y += 22;
  }
  return clicked;
}

// Modal list of projects found in Documents/MikMap, newest first. Click a row to open it; Esc or a click outside closes.
static void DrawOpenDialog(ImVec2 disp, bool fresh) {
  if (!A.openDialog) return;
  ImGuiIO& io = ImGui::GetIO();
  int n = (int)A.openList.size(), rows = std::max(1, std::min(n, 10));
  float w = 520, h = 30 + rows * 36 + 30;
  ImRect r((disp.x - w) * 0.5f, std::max(48.f, (disp.y - h) * 0.4f), (disp.x + w) * 0.5f, std::max(48.f, (disp.y - h) * 0.4f) + h);
  Fill(ImRect(0, 0, disp.x, disp.y), K(0x000000, 0.55f));
  Shadow(r, 4, 40, 0.7f);
  Box(r, K(pal::g12), K(pal::g2a), 4);
  ImRect hd(r.Min.x + 1, r.Min.y + 1, r.Max.x - 1, r.Min.y + 30);
  Fill(hd, K(pal::g18)); HLine(hd.Min.x, hd.Max.x, hd.Max.y - 1, K(pal::g2a));
  Icon("folder-open", ImVec2(hd.Min.x + 14, (hd.Min.y + hd.Max.y) * 0.5f), 11, K(pal::coral));
  Text(hd.Min.x + 26, (hd.Min.y + hd.Max.y) * 0.5f, UI_B, 10, K(pal::tcc), "OPEN PROJECT", 0.14f);
  TextEll(hd.Min.x + 150, (hd.Min.y + hd.Max.y) * 0.5f, hd.Max.x - hd.Min.x - 160, MONO_M, 9, K(pal::t66), ProjectsDir().c_str());
  float y = hd.Max.y;
  if (n == 0) TextC((r.Min.x + r.Max.x) * 0.5f, y + 18, UI_S, 11, K(pal::t88), "No .mikmap projects yet \xE2\x80\x94 use Save project (Ctrl+S)");
  for (int i = 0; i < rows && i < n; ++i) {
    const ProjectFile& pf = A.openList[i];
    ImRect ir(r.Min.x + 1, y + i * 36, r.Max.x - 1, y + (i + 1) * 36);
    bool hv = Raw(ir);
    if (hv) Fill(ir, K(pal::layerHover));
    if (i) HLine(ir.Min.x, ir.Max.x, ir.Min.y, K(pal::g2a));
    float cy2 = (ir.Min.y + ir.Max.y) * 0.5f;
    bool cur = pf.path == A.projectPath;
    Icon("layers", ImVec2(ir.Min.x + 16, cy2), 13, K(cur ? pal::coral : pal::t88));
    TextEll(ir.Min.x + 32, cy2 - 6, 330, UI_B, 12, K(cur ? pal::coral : pal::te0), pf.name.c_str());
    char when[32] = ""; std::time_t tt = (std::time_t)pf.mtime; std::tm tmv{};
#ifdef _WIN32
    localtime_s(&tmv, &tt);
#else
    localtime_r(&tt, &tmv);
#endif
    std::strftime(when, sizeof when, "%Y-%m-%d %H:%M", &tmv);
    Text(ir.Min.x + 32, cy2 + 8, MONO_M, 9, K(pal::t66), when);
    if (cur) TextR(ir.Max.x - 12, cy2, MONO_B, 9, K(pal::coral), "OPEN");
    if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (hv && io.MouseClicked[0] && !fresh) {
      std::string path = pf.path, nm = pf.name;
      GuardedDiscard("open another project", [path, nm] {
        std::string err;
        if (LoadProject(path, err)) { A.openDialog = false; A.notify("Opened: " + nm); }
        else A.notify("Cannot open: " + err, 6);
      });
    }
  }
  TextR(r.Max.x - 10, r.Max.y - 15, MONO_M, 9, K(pal::t66), "ESC TO CLOSE");
  if (!fresh && (io.MouseClicked[0] || io.MouseClicked[1]) && !Raw(r)) A.openDialog = false;
  g.blocked = true;
}

static void DrawHelpDialog(ImVec2 disp, bool fresh) {
  if (!A.helpOpen) return;
  ImGuiIO& io = ImGui::GetIO();
  static const char* rows[][2] = {{"Ctrl/Cmd + N", "New blank project"}, {"Ctrl/Cmd + O", "Open project"}, {"Ctrl/Cmd + S", "Save project"},
                                  {"Ctrl/Cmd + Shift + S", "Save a timestamped copy"}, {"Ctrl/Cmd + Z", "Undo"},
                                  {"Ctrl/Cmd + Shift + Z / Y", "Redo"}, {"F11", "Open / close projector output"}, {"Esc / F11 / Ctrl+W", "Close output (while the output window has focus)"}, {"Tab", "Show Mode (hide all editing UI)"},
                                  {"Ctrl/Cmd + C / X / V / D  (Mapping)", "Copy / cut / paste / duplicate the selected screens, slices or masks"}, {"Delete  (Mapping)", "Delete the selected screens, slices or masks"}, {"Arrows / Shift+Arrows  (Mapping)", "Move the selection 1 / 10 px"}, {"Ctrl/Cmd/Shift + click  (Mapping)", "Add to / remove from the selection"}, {"Space", "Play / pause"}, {"Enter", "Trigger selected clip"}, {"Left / Right", "Previous / next column"}, {"L", "Selected clip: loop mode"}, {"Delete", "Clear selected clip"}, {"Esc", "Close menu, dialog or popover"}, {"Double-click layer", "Rename layer"}, {"Alt + wheel (Mapping)", "Zoom at cursor"}};
  int n = (int)(sizeof rows / sizeof rows[0]);
  float w = 460, h = 30 + n * 26 + 30;
  ImRect r((disp.x - w) * 0.5f, std::max(48.f, (disp.y - h) * 0.4f), (disp.x + w) * 0.5f, std::max(48.f, (disp.y - h) * 0.4f) + h);
  Fill(ImRect(0, 0, disp.x, disp.y), K(0x000000, 0.55f));
  Shadow(r, 4, 40, 0.7f);
  Box(r, K(pal::g12), K(pal::g2a), 4);
  ImRect hd(r.Min.x + 1, r.Min.y + 1, r.Max.x - 1, r.Min.y + 30);
  Fill(hd, K(pal::g18)); HLine(hd.Min.x, hd.Max.x, hd.Max.y - 1, K(pal::g2a));
  Icon("circle-help", ImVec2(hd.Min.x + 14, (hd.Min.y + hd.Max.y) * 0.5f), 11, K(pal::cyan));
  Text(hd.Min.x + 26, (hd.Min.y + hd.Max.y) * 0.5f, UI_B, 10, K(pal::tcc), "KEYBOARD SHORTCUTS", 0.14f);
  for (int i = 0; i < n; ++i) {
    float cy2 = hd.Max.y + 13 + i * 26;
    Text(r.Min.x + 16, cy2, MONO_B, 10, K(pal::coral), rows[i][0]);
    Text(r.Min.x + 210, cy2, UI_S, 11, K(pal::te0), rows[i][1]);
  }
  TextR(r.Max.x - 10, r.Max.y - 15, MONO_M, 9, K(pal::t66), "ESC TO CLOSE");
  if (!fresh && (io.MouseClicked[0] || io.MouseClicked[1]) && !Raw(r)) A.helpOpen = false;
  g.blocked = true;
}

void DrawOverlays(ImVec2 disp) {
  static bool prevBlend = false, prevPop = false, prevCtx = false, prevLayer = false;
  bool freshBlend = A.blendDD.open && !prevBlend, freshPop = A.pop.open && !prevPop, freshCtx = A.ctx.open && !prevCtx, freshLayer = A.layerMenu.open && !prevLayer;
  static bool prevCol = false; bool freshCol = A.colMenu.open && !prevCol; prevCol = A.colMenu.open;
  static bool prevDeckMenu = false; bool freshDeckMenu = A.deckMenu.open && !prevDeckMenu; prevDeckMenu = A.deckMenu.open;
  prevBlend = A.blendDD.open; prevPop = A.pop.open; prevCtx = A.ctx.open; prevLayer = A.layerMenu.open;
  g.blocked = false;
  ImGuiIO& io = ImGui::GetIO();
  ImDrawList* rootDl = g.dl;
  g.dl = ImGui::GetForegroundDrawList();  // scroll children render after the root window; overlays must sit above them

  ProjectMenu();
  static bool prevOpen = false, prevHelp = false; bool freshOpen = A.openDialog && !prevOpen, freshHelp = A.helpOpen && !prevHelp; prevOpen = A.openDialog; prevHelp = A.helpOpen;
  DrawOpenDialog(disp, freshOpen);
  DrawHelpDialog(disp, freshHelp);

  // blend dropdown
  if (A.blendDD.open) {
    ImRect an = A.blendDD.anchor;
    ImRect r(an.Min.x, an.Max.y + 2, an.Min.x + std::max(an.GetWidth(), 96.f), an.Max.y + 2 + 8 * 20 + 4);
    Shadow(r, 3, 16, 0.6f);
    Box(r, K(pal::g14), K(pal::g3a), 3);
    for (int i = 0; i < 8; ++i) {
      ImRect ir(r.Min.x + 2, r.Min.y + 2 + i * 20, r.Max.x - 2, r.Min.y + 2 + (i + 1) * 20);
      bool hv = Raw(ir);
      bool cur = A.blendDD.layer >= 0 && A.layers[A.blendDD.layer].blend == BLEND_NAMES[i];
      if (hv) Fill(ir, K(pal::g1c), 2);
      Text(ir.Min.x + 6, (ir.Min.y + ir.Max.y) * 0.5f, UI_S, 10, K(cur ? pal::coral : hv ? pal::white : pal::tcc), Upper(BLEND_NAMES[i]).c_str(), 0.09f);
      if (hv && io.MouseClicked[0] && !freshBlend) { A.layers[A.blendDD.layer].blend = BLEND_NAMES[i]; A.blendDD.open = false; }
      if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    if (!freshBlend && (io.MouseClicked[0] || io.MouseClicked[1]) && !Raw(r)) A.blendDD.open = false;
    g.blocked = true;
  }

  // clip popover (+ clip colour swatches)
  if (A.pop.open) {
    std::vector<PItem> items = {{"Trigger", "play", "\xE2\x86\xB5", 1, false, false}, {"Cue to Preview", "eye", "C", 0, false, false},
                                {"Loop", "repeat", "L", 0, false, false}, {"Rename", "pencil", "", 0, A.layers[A.pop.li].clips[A.pop.ci].st == Clip::Empty, false},
                                {"", "", "", 0, false, true}, {"Clear Slot", "trash-2", "", 2, false, false}};
    // the colour swatches live INSIDE the popover box (one popover, not a second box hanging below it)
    const float colH = 1 + 6 + 9 + 4 + 18 + 6;
    ImRect pr; int hit = PopoverList(A.pop.pos, disp, "Clip", items, freshPop, pr, colH);
    ImRect cb(pr.Min.x + 1, pr.Max.y - colH - 1, pr.Max.x - 1, pr.Max.y - 1);
    HLine(cb.Min.x + 1, cb.Max.x - 1, cb.Min.y, K(pal::g2a));
    Text(cb.Min.x + 8, cb.Min.y + 1 + 6 + 4.5f, MONO_R, 9, K(pal::t66), "CLIP COLOR", 0.09f);
    Clip& cell = A.layers[A.pop.li].clips[A.pop.ci];
    float sw = (cb.GetWidth() - 16 - 5 * 4) / 6.f;
    for (int k = 0; k < 6; ++k) {
      ImRect sr(cb.Min.x + 8 + k * (sw + 4), cb.Min.y + 1 + 6 + 9 + 4, cb.Min.x + 8 + k * (sw + 4) + sw, cb.Min.y + 1 + 6 + 9 + 4 + 18);
      bool on = cell.color == k;
      if (on) Glow(sr, CLIP_COLORS[k], 0.45f, 10, 2);
      Box(sr, MixHex(pal::g16, CLIP_COLORS[k], 0.28f), on ? K(CLIP_COLORS[k]) : K(pal::g22), 2);
      if (Raw(sr)) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      if (Raw(sr) && io.MouseClicked[0] && !freshPop) { if (cell.st != Clip::Empty) cell.color = k; A.pop.open = false; }
    }
    if (hit >= 0) {
      int li = A.pop.li, ci = A.pop.ci;
      if (hit == 0) A.trigger(li, ci); else if (hit == 1) A.cue(li, ci); else if (hit == 2) A.layers[li].clips[ci].playMode = PM_LOOP; else if (hit == 3) A.beginRename(2, li * 1000 + ci, A.pop.pos, A.layers[li].clips[ci].name); else if (hit == 5) A.layers[li].clips[ci] = Clip();
      A.pop.open = false;
    } else if (!freshPop && (io.MouseClicked[0] || io.MouseClicked[1]) && !Raw(pr)) A.pop.open = false;
    g.blocked = true;
  }

  // layer options menu
  if (A.layerMenu.open) {
    int li = A.layerMenu.li, n = (int)A.layers.size();
    std::vector<PItem> items = {{"Move up", "chevron-up", "", 0, li == 0, false}, {"Move down", "chevron-down", "", 0, li >= n - 1, false},
                                {"Duplicate", "copy", "", 0, false, false}, {"Rename", "pencil", "", 0, false, false},
                                {"Clear clips", "eraser", "", 0, false, false}, {"Delete layer", "trash-2", "", 2, false, false}};
    ImRect pr; int hit = PopoverList(A.layerMenu.pos, disp, "Layer", items, freshLayer, pr);
    if (hit >= 0) {
      if (hit == 0 || hit == 1) {
        int to = li + (hit == 0 ? -1 : 1);
        if (to >= 0 && to < n) { std::swap(A.layers[li], A.layers[to]); A.selLayer = to; }
      } else if (hit == 2) {
        Layer c = A.layers[li]; c.name += " copy"; c.live = false; c.id.clear();   // a copy is a new routing target
        for (auto& k : c.clips) if (k.isLive()) k.st = Clip::Loaded;
        A.layers.insert(A.layers.begin() + li + 1, c); A.selLayer = li + 1;
        EnsureLayerIds(A.layers);
      } else if (hit == 3) { A.beginRename(0, li, A.layerMenu.pos, A.layers[li].name); }
      else if (hit == 4) { for (auto& k : A.layers[li].clips) k = Clip(); A.layers[li].live = false; }
      else if (hit == 5 && n > 1) { A.layers.erase(A.layers.begin() + li); A.selLayer = std::clamp(A.selLayer, 0, (int)A.layers.size() - 1); A.selLi = std::clamp(A.selLi, 0, (int)A.layers.size() - 1); }
      A.layerMenu.open = false;
    } else if (!freshLayer && (io.MouseClicked[0] || io.MouseClicked[1]) && !Raw(pr)) A.layerMenu.open = false;
    g.blocked = true;
  }

  // column menu
  if (A.colMenu.open) {
    int ci = A.colMenu.ci, n = A.colCount();
    bool isAutoStart = A.autoStartCol == ci;
    std::vector<PItem> items = {{"Insert column before", "arrow-left-to-line", "", 0, false, false}, {"Insert column after", "arrow-right-to-line", "", 0, false, false},
                                {"Move left", "chevron-left", "", 0, ci == 0, false}, {"Move right", "chevron-right", "", 0, ci >= n - 1, false},
                                {"Rename column", "pencil", "", 0, false, false}, {"Clear column", "eraser", "", 0, false, false},
                                {isAutoStart ? "Clear auto-start on open" : "Set as auto-start on open", "zap", "", isAutoStart ? 1 : 0, false, false},
                                {"Delete column", "trash-2", "", 2, n < 2, false}};
    ImRect pr; int hit = PopoverList(A.colMenu.pos, disp, "Column", items, freshCol, pr);
    if (hit >= 0) {
      if (hit == 0) A.insertCol(ci); else if (hit == 1) A.insertCol(ci + 1);
      else if (hit == 2) A.moveColTo(ci, ci - 1); else if (hit == 3) A.moveColTo(ci, ci + 1);
      else if (hit == 4) { A.beginRename(1, ci, A.colMenu.pos, A.colName(ci)); }
      else if (hit == 5) { for (auto& l : A.layers) { l.clips[ci] = Clip(); l.live = false; for (auto& k : l.clips) if (k.isLive()) l.live = true; } }
      else if (hit == 6) { A.autoStartCol = isAutoStart ? -1 : ci; A.notify(isAutoStart ? "Auto-start cleared" : ("Auto-start: column " + std::to_string(ci + 1)), 2.0); }
      else if (hit == 7) A.deleteCol(ci);
      A.colMenu.open = false;
    } else if (!freshCol && (io.MouseClicked[0] || io.MouseClicked[1]) && !Raw(pr)) A.colMenu.open = false;
    g.blocked = true;
  }

  // deck tab menu
  if (A.deckMenu.open) {
    int di = A.deckMenu.idx, n = (int)A.decks.size();
    std::vector<PItem> items = {{"Add deck", "plus", "", 0, false, false}, {"Rename deck", "pencil", "", 0, false, false}, {"Duplicate deck", "copy", "", 0, false, false},
                                {"Move left", "chevron-left", "", 0, di == 0, false}, {"Move right", "chevron-right", "", 0, di >= n - 1, false},
                                {"Delete deck", "trash-2", "", 2, n < 2, false},
                                {"", "", "", 0, false, true},
                                {"Add layer", "plus", "", 0, false, false}, {"New group from selected layer", "folder-plus", "", 0, A.layers.empty() && di == A.curDeckIdx, false},
                                {"Add column", "plus", "", 0, false, false}, {A.quantize ? "Sync to beat: ON" : "Sync to beat: OFF", "clock", "", A.quantize ? 1 : 0, false, false}};
    ImRect pr; int hit = PopoverList(A.deckMenu.pos, disp, "Deck", items, freshDeckMenu, pr);
    if (hit >= 0) {
      if (hit == 0) A.addDeck();
      else if (hit == 1) A.beginRename(4, di, A.deckMenu.pos, A.decks[di].name);
      else if (hit == 2) A.duplicateDeck(di);
      else if (hit == 3) A.moveDeckTo(di, di - 1);
      else if (hit == 4) A.moveDeckTo(di, di + 1);
      else if (hit == 5) A.deleteDeck(di);
      else if (hit >= 7) {   // structure actions apply to the deck being edited: make the clicked tab current first
        if (di != A.curDeckIdx) A.switchDeck(di);
        if (hit == 7) A.addLayer(); else if (hit == 8) A.groupSelectedLayer(); else if (hit == 9) A.insertCol(A.colCount()); else if (hit == 10) A.toggleSync();
      }
      A.deckMenu.open = false;
    } else if (!freshDeckMenu && (io.MouseClicked[0] || io.MouseClicked[1]) && !Raw(pr)) A.deckMenu.open = false;
    g.blocked = true;
  }

  // drag ghost (browser source)
  if (A.dragSrc.active) {
    ImVec2 m = io.MousePos;
    ImRect gr(m.x + 10, m.y + 8, m.x + 10 + 118, m.y + 8 + 22);
    Fill(gr, K(A.dragSrc.fxKind >= 0 ? pal::cyan : pal::coral, 0.85f), 3);
    TextEll(gr.Min.x + 8, (gr.Min.y + gr.Max.y) * 0.5f, 102, UI_B, 10, K(0x000000), A.dragSrc.name.c_str());
  }
  // drag ghost
  if (A.dragging && A.dragLi >= 0 && A.dragLi < (int)A.layers.size()) {
    const Clip& c = A.layers[A.dragLi].clips[A.dragCi];
    ImVec2 m = io.MousePos;
    ImRect gr(m.x + 10, m.y + 8, m.x + 10 + 110, m.y + 8 + 22);
    Fill(gr, K(pal::coral, 0.85f), 3);
    TextEll(gr.Min.x + 8, (gr.Min.y + gr.Max.y) * 0.5f, 94, UI_B, 10, K(0x000000), c.name.c_str());
  }

  DrawSettings(disp);
  // context menu
  if (A.ctx.open) {
    // rows are 24px, a divider row 8px (a line); an item without an icon starts its text at the left edge
    float rowsH = 0; for (auto& it : A.ctx.items) rowsH += it.divider ? 8.f : 24.f;
    float w = 182, h = 8 + rowsH;
    ImVec2 p(std::min(A.ctx.pos.x, disp.x - 190), std::min(A.ctx.pos.y, disp.y - (rowsH + 16)));
    ImRect r(p.x, p.y, p.x + w, p.y + h);
    Shadow(r, 4, 24, 0.7f);
    Box(r, K(pal::g16), K(pal::g3a), 4);
    bool acted = false;
    float ry = r.Min.y + 4;
    for (size_t i = 0; i < A.ctx.items.size(); ++i) {
      auto& it = A.ctx.items[i];
      if (it.divider) { HLine(r.Min.x + 6, r.Max.x - 6, ry + 4, K(pal::g2a)); ry += 8; continue; }
      ImRect ir(r.Min.x + 4, ry, r.Max.x - 4, ry + 24);
      ry += 24;
      bool hv = Raw(ir);
      float prev = g.alpha; if (it.disabled) g.alpha = 0.45f;
      if (hv && !it.disabled) Fill(ir, K(pal::g18), 3);
      uint32_t col = it.disabled ? pal::t66 : it.danger ? pal::red : it.toneHex ? it.toneHex : pal::te0;
      float cy = (ir.Min.y + ir.Max.y) * 0.5f;
      if (!it.icon.empty()) { Icon(it.icon.c_str(), ImVec2(ir.Min.x + 8 + 5.5f, cy), 11, K(col)); Text(ir.Min.x + 8 + 11 + 8, cy, UI_S, 10, K(col), it.label.c_str()); }
      else Text(ir.Min.x + 10, cy, UI_S, 10, K(col), it.label.c_str());
      g.alpha = prev;
      if (hv && !it.disabled) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      if (hv && io.MouseClicked[0] && !freshCtx) { acted = true; if (!it.disabled && it.run) { auto f = it.run; A.ctx.open = false; f(); } }
    }
    if (!freshCtx && (io.MouseClicked[0] || io.MouseClicked[1])) A.ctx.open = false;
    (void)acted;
    g.blocked = true;
  }
  if (A.rename.open) {
    ImGui::SetNextWindowPos(A.rename.pos);
    ImGui::SetNextWindowSize(ImVec2(220, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 8));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 1.f);
    ImGui::PushStyleColor(ImGuiCol_WindowBg, K(pal::g16));
    ImGui::PushStyleColor(ImGuiCol_Border, K(pal::coral));
    ImGui::PushStyleColor(ImGuiCol_FrameBg, K(pal::g050));
    ImGui::PushStyleColor(ImGuiCol_Text, K(pal::tf3));
    bool submit = false;
    ImGui::Begin("##rename", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_AlwaysAutoResize);
    ImGui::PushFont(F(UI_B), TextPx(12));
    ImGui::TextUnformatted(A.rename.kind == 0 ? "RENAME LAYER" : A.rename.kind == 1 ? "RENAME COLUMN" : A.rename.kind == 3 ? "RENAME GROUP" : A.rename.kind == 4 ? "RENAME DECK" : "RENAME CLIP");
    ImGui::SetNextItemWidth(-1);
    if (A.rename.fresh) ImGui::SetKeyboardFocusHere();
    submit = ImGui::InputText("##rn", A.rename.buf, sizeof A.rename.buf, ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll);
    ImGui::PopFont();
    bool inside = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
    ImGui::End();
    ImGui::PopStyleColor(4); ImGui::PopStyleVar(3);
    if (submit) { A.commitRename(A.rename.buf); A.rename.open = false; }
    else if (!A.rename.fresh && !inside && (io.MouseClicked[0] || io.MouseClicked[1])) A.rename.open = false;
    A.rename.fresh = false;
    g.blocked = true;
  }
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { A.cancelMaskPen(); A.pop.open = A.ctx.open = A.blendDD.open = A.projectMenu = A.rename.open = A.openDialog = A.helpOpen = A.deckMenu.open = false; }
  g.dl = rootDl;
}

// ───────────────────────── main ─────────────────────────
static void SetupStyle() {
  ImGuiStyle& s = ImGui::GetStyle();
  s.ScrollbarSize = 8; s.ScrollbarRounding = 999; s.WindowBorderSize = 0; s.ChildBorderSize = 0; s.FrameBorderSize = 0;
  ImVec4* c = s.Colors;
  c[ImGuiCol_ScrollbarBg] = ImGui::ColorConvertU32ToFloat4(K(pal::g0f));
  c[ImGuiCol_ScrollbarGrab] = ImGui::ColorConvertU32ToFloat4(K(pal::g33));
  c[ImGuiCol_ScrollbarGrabHovered] = ImGui::ColorConvertU32ToFloat4(K(pal::g3a));
  c[ImGuiCol_ScrollbarGrabActive] = ImGui::ColorConvertU32ToFloat4(K(pal::g3a));
  c[ImGuiCol_TextSelectedBg] = ImGui::ColorConvertU32ToFloat4(K(pal::coral, 0.3f));
  c[ImGuiCol_WindowBg] = ImGui::ColorConvertU32ToFloat4(K(pal::g0f));
  c[ImGuiCol_NavHighlight] = ImVec4(0, 0, 0, 0);
}

struct Script { int kind; float x0, y0, x1, y1; int key = 0; bool ctrl = false, shift = false; };   // kind 3 = a key chord (--press)

int main(int argc, char** argv) {
  std::vector<Script> script;
  bool hoverTest = false; float hoverX = 0, hoverY = 0;
  std::string compTest, clipTest, pvTest; std::vector<std::string> layerTest, clipColors;
  OsDrop dropTest;   // --drop: injected at frame 8 of a --shot run, standing in for a real Explorer drag
  bool openOut = false; std::string outShot;
  bool outKeyTest = false, scriptCtrl = false, startSnap = false, startHand = false, startCard = false; int startTool = -1, startInTool = -1;
  std::string roundtrip; std::vector<int> fxTest;
  std::string shot; int startScreen = 0, frames = 12, W = 1440, H = 900, tab = -1, page = -1;
  bool sel = false, scaleGiven = false; int selLi = 0, selCi = 0, ctxTest = 0, cliScale = 100;
  for (int i = 1; i < argc; ++i) {
    std::string a = argv[i];
    if (a == "--shot" && i + 1 < argc) shot = argv[++i];
    else if (a == "--screen" && i + 1 < argc) startScreen = atoi(argv[++i]);
    else if (a == "--frames" && i + 1 < argc) frames = atoi(argv[++i]);
    else if (a == "--size" && i + 2 < argc) { W = atoi(argv[++i]); H = atoi(argv[++i]); }
    else if (a == "--tab" && i + 1 < argc) tab = atoi(argv[++i]);
    else if (a == "--page" && i + 1 < argc) page = atoi(argv[++i]);
    else if (a == "--roundtrip" && i + 1 < argc) roundtrip = argv[++i];
    else if (a == "--menu") A.projectMenu = true;
    else if (a == "--out") openOut = true;
    else if (a == "--outkeytest") outKeyTest = true;
    else if (a == "--snap") startSnap = true;    // test aid: start with the Mapping magnet on
    else if (a == "--hand") startHand = true;    // test aid: start with the Mapping hand tool on
    else if (a == "--wheel" && i + 1 < argc) {   // test aid: mouse wheel notch(es) at x,y,amount
      Script sc{}; sc.kind = 4; sscanf(argv[++i], "%f,%f,%f", &sc.x0, &sc.y0, &sc.x1); script.push_back(sc);
    }
    else if (a == "--shift") scriptCtrl = true;   // test aid: hold Shift through the whole scripted run (multi-selection screenshots; Ctrl+click is a right click on macOS)
    else if (a == "--outshot" && i + 1 < argc) { openOut = true; outShot = argv[++i]; }
    else if (a == "--scale" && i + 1 < argc) { A.prefs.scale = atoi(argv[++i]); cliScale = A.prefs.scale; scaleGiven = true; }
    else if (a == "--press" && i + 1 < argc) {   // test aid: a key chord, e.g. ctrl+c, delete, shift+left (Ctrl here is io.KeyCtrl = Cmd on macOS)
      std::string c = argv[++i]; Script sc{}; sc.kind = 3;
      for (size_t at; (at = c.find('+')) != std::string::npos; c.erase(0, at + 1)) { std::string m = c.substr(0, at); if (m == "ctrl") sc.ctrl = true; if (m == "shift") sc.shift = true; }
      static const struct { const char* n; ImGuiKey k; } keys[] = {{"c", ImGuiKey_C}, {"v", ImGuiKey_V}, {"x", ImGuiKey_X}, {"d", ImGuiKey_D}, {"delete", ImGuiKey_Delete}, {"backspace", ImGuiKey_Backspace},
        {"left", ImGuiKey_LeftArrow}, {"right", ImGuiKey_RightArrow}, {"up", ImGuiKey_UpArrow}, {"down", ImGuiKey_DownArrow}};
      for (auto& k : keys) if (c == k.n) sc.key = (int)k.k;
      script.push_back(sc);
    }
    else if (a == "--ctx") ctxTest = 1;
    else if (a == "--testcard") startCard = true;
    else if (a == "--tool" && i + 1 < argc) startTool = atoi(argv[++i]);     // test aid: Output tool, 0 Edit Points / 1 Transform
    else if (a == "--intool" && i + 1 < argc) startInTool = atoi(argv[++i]); // test aid: Input (mask) tool   // test aid: Show TestCard on from the first frame
    else if ((a == "--click" || a == "--rclick" || a == "--drag") && i + 1 < argc) {
      // scripted input for headless checks: x,y  (drag: x0,y0,x1,y1)
      Script s; s.kind = a == "--click" ? 0 : a == "--rclick" ? 1 : 2;
      sscanf(argv[++i], "%f,%f,%f,%f", &s.x0, &s.y0, &s.x1, &s.y1);
      script.push_back(s);
    }
    else if (a == "--inspscroll" && i + 1 < argc) gTestInspScroll = (float)atof(argv[++i]);   // test aid, see gTestInspScroll
    else if (a == "--clip" && i + 1 < argc) clipTest = argv[++i];   // test aid: --clip chan=1,blend=2,ax=300,ay=0,rot=30,scale=0.6 (applies to layer 0, column 2 = the demo's live clip)
    else if (a == "--clipcolor" && i + 1 < argc) clipColors.push_back(argv[++i]);   // test aid: --clipcolor layer,column,colorIndex
    else if (a == "--pv" && i + 1 < argc) pvTest = argv[++i];   // test aid: --pv zoom,panX,panY,hand (Preview Cue view)
    else if (a == "--hover" && i + 1 < argc) { hoverTest = true; std::sscanf(argv[++i], "%f,%f", &hoverX, &hoverY); }   // test aid: hold the pointer here (headless runs have no real pointer)
    else if (a == "--band" && i + 1 < argc) A.prefs.bandPct = std::clamp(atoi(argv[++i]), 25, 70);   // test aid: taller top band, so the whole Properties list fits in a screenshot
    else if (a == "--layer" && i + 1 < argc) layerTest.push_back(argv[++i]);   // test aid: --layer N,master=50,scale=60,rot=20,px=100,py=0,op=80,vol=50
    else if (a == "--comp" && i + 1 < argc) compTest = argv[++i];   // test aid: --comp scale=60,rot=20,master=50,px=100,py=-40,ax=0,ay=0,speed=200,op=80,w=3840,h=2160
    else if (a == "--fx" && i + 1 < argc) fxTest.push_back(atoi(argv[++i]));   // test aid: add FX kind N to the selected clip
    else if (a == "--cell" && i + 2 < argc) { sel = true; selLi = atoi(argv[++i]); selCi = atoi(argv[++i]); }
    else if (a == "--drop" && i + 2 < argc) {   // headless check of OS file drop: --drop x,y <path> (repeatable paths accumulate)
      float dx = 0, dy = 0; sscanf(argv[++i], "%f,%f", &dx, &dy);
      dropTest.pos = ImVec2(dx, dy); dropTest.paths.push_back(argv[++i]); dropTest.pending = true;
    }
  }
  if (!shot.empty() || !roundtrip.empty()) SetSettingsPersistence(false);
  if (!roundtrip.empty()) {
    // Headless self-check of project persistence: save -> edit -> load must restore the saved state exactly.
    auto fail = [](const char* w) { std::fprintf(stderr, "roundtrip FAILED: %s\n", w); return 1; };
    std::string err;
    NewProject();
    if (ProjectDirty()) return fail("fresh project reports dirty");
    A.layers[0].name = "Edited"; A.layers[0].clips[1].fx.push_back(Fx()); A.colNames.resize(A.colCount()); A.colNames[1] = "Renamed";
    A.screens[0].slices[0].q[2] = ImVec2(1500, 900); A.calib[0].mx = 0.123f; A.bpm = 97.5f; A.screens[0].slices[1].solo = true; A.layers[0].clips[1].media = "/no/such/file.png";
    if (!ProjectDirty()) return fail("edit not detected as dirty");
    if (!SaveProject(roundtrip, err)) return fail(err.c_str());
    if (ProjectDirty()) return fail("dirty right after save");
    A.layers[0].name = "Scribble"; A.screens[0].slices[0].q[2] = ImVec2(1, 1); A.colNames[1] = "x";
    if (!LoadProject(roundtrip, err)) return fail(err.c_str());
    if (A.layers[0].name != "Edited" || A.colName(1) != "Renamed" || A.screens[0].slices[0].q[2].x != 1500.f || std::fabs(A.calib[0].mx - 0.123f) > 1e-6f) return fail("state not restored");
    if (A.layers[0].clips[1].fx.empty()) return fail("fx chain lost");
    if (A.bpm != 97.5f) return fail("bpm not restored");
    if (A.layers[0].clips[1].media != "/no/such/file.png") return fail("image clip path not restored");
    if (!A.screens[0].slices[1].solo || A.screens[0].slices[0].solo) return fail("slice solo not restored");
    if (ProjectDirty()) return fail("dirty right after load");
    NewBlankProject();
    if (A.layers.size() != 4 || A.screens.size() != 1) return fail("blank project shape");
    if (!SaveProject(roundtrip + ".blank", err) || !LoadProject(roundtrip + ".blank", err)) return fail(err.c_str());
    { std::FILE* f = std::fopen(roundtrip.c_str(), "wb"); if (f) { std::fputs("{ this is not json", f); std::fclose(f); } }
    if (LoadProject(roundtrip, err)) return fail("corrupt file was accepted");
    if (A.layers.size() != 4) return fail("corrupt load damaged the live state");
    { std::FILE* f = std::fopen(roundtrip.c_str(), "wb"); if (f) { std::fputs("{\"format\":1,\"composition\":{\"layers\":[]}}", f); std::fclose(f); } }
    if (LoadProject(roundtrip, err)) return fail("empty project was accepted");
    // undo/redo: each committed edit is one step, redo re-applies, and a live clip keeps playing through an undo
    NewProject();
    std::string first = A.layers[0].name;
    A.trigger(0, 2); if (!A.layers[0].clips[2].isLive()) return fail("trigger setup");
    A.layers[0].name = "A1"; UndoCommit(); A.layers[0].name = "A2"; A.screens[0].slices[0].q[0] = ImVec2(9, 9); UndoCommit();
    if (!CanUndo() || CanRedo()) return fail("undo flags after edits");
    UndoStep(false); if (A.layers[0].name != "A1" || A.screens[0].slices[0].q[0].x == 9.f) return fail("undo 1");
    if (!A.layers[0].clips[2].isLive()) return fail("undo cut a playing clip");
    UndoStep(false); if (A.layers[0].name != first) return fail("undo 2");
    if (CanUndo()) return fail("history should be empty");
    UndoStep(true); if (A.layers[0].name != "A1") return fail("redo");
    A.layers[0].name = "A3"; UndoCommit(); if (CanRedo()) return fail("new edit should clear redo");
    // A10: triggering another clip on a layer with blend time > 0 starts a dissolve from the previous live clip
    NewProject(); A.layers[0].blendTime = 2.f;
    std::string prevName; for (auto& c : A.layers[0].clips) if (c.isLive()) prevName = c.name;
    A.trigger(0, 0);
    if (prevName.empty() || A.layers[0].fadeT != 0.f || A.layers[0].fadeFrom.name != prevName) return fail("dissolve not started");
    A.layers[0].blendTime = 0.f; A.layers[0].fadeT = 1.f; A.trigger(0, 1);
    if (A.layers[0].fadeT != 1.f) return fail("dissolve started with blend time 0");
    // columns: insert/delete/move stay consistent across layers and can be undone
    NewProject(); int cols0 = A.colCount();
    A.insertCol(2); UndoCommit(); A.deleteCol(0); UndoCommit();
    if (A.colCount() != cols0) return fail("col count after insert+delete");
    for (auto& l : A.layers) if ((int)l.clips.size() != A.colCount()) return fail("layers have different column counts");
    UndoStep(false); if (A.colCount() != cols0 + 1) return fail("undo delete column");
    UndoStep(false); if (A.colCount() != cols0) return fail("undo insert column");
    // groups: opacity persists, a dangling group id is dropped on load instead of crashing the deck
    NewProject(); Group ng; ng.id = "gx"; ng.name = "Extra"; ng.opacity = 40.f; A.groups.push_back(ng); A.layers[0].group = "gx";
    if (!SaveProject(roundtrip, err) || !LoadProject(roundtrip, err)) return fail(err.c_str());
    if (!A.group("gx") || A.group("gx")->opacity != 40.f || A.layers[0].group != "gx") return fail("group not restored");
    A.groups.erase(std::remove_if(A.groups.begin(), A.groups.end(), [](const Group& g) { return g.id == "gx"; }), A.groups.end());
    if (!SaveProject(roundtrip, err) || !LoadProject(roundtrip, err)) return fail(err.c_str());
    if (!A.layers[0].group.empty()) return fail("dangling group reference survived load");
    // group Cue N: selects (Selected/LiveSel, not just selectedCells bookkeeping) that column's clip on EVERY
    // layer in the group, leaves other layers alone, and switches Properties to the Clip tab
    NewProject(); { Group ng; ng.id = "gy"; ng.name = "GY"; A.groups.push_back(ng); A.layers[0].group = "gy"; A.layers[1].group = "gy";
      A.layers[0].clips[0].st = Clip::Live; A.layers[2].clips[0].st = Clip::Selected;   // layer 2 (outside the group) must not be touched
      A.tab = 0; A.selectGroupCue("gy", 0);
      if (A.layers[0].clips[0].st != Clip::LiveSel) return fail("group cue did not select the live clip on a group member");
      if (A.layers[1].clips[0].st != Clip::Selected) return fail("group cue did not select the loaded clip on a group member");
      if (A.layers[2].clips[0].st != Clip::Selected) return fail("group cue touched a layer outside the group");
      if (A.tab != 2) return fail("group cue did not switch Properties to the Clip tab");
      bool sawL0 = false, sawL1 = false; for (auto& c : A.selectedCells) { if (c.first == 0 && c.second == 0) sawL0 = true; if (c.first == 1 && c.second == 0) sawL1 = true; }
      if (!sawL0 || !sawL1) return fail("group cue did not select both group members' cells"); }
    // rename: commit trims, ignores empty text, and pins the clip look so the picture does not change
    NewProject(); { int st0 = ClipStyleOf(A.layers[0].clips[0].name); A.beginRename(2, 0, ImVec2(0, 0), ""); A.commitRename("  My clip  ");
      if (A.layers[0].clips[0].name != "My clip" || A.layers[0].clips[0].style != st0) return fail("clip rename / style pin"); }
    A.beginRename(0, 0, ImVec2(0, 0), ""); A.commitRename("   "); if (A.layers[0].name.empty() || A.layers[0].name == "   ") return fail("empty rename accepted");
    // Sync (quantize): a trigger waits for the next beat; the latest request per layer wins; off = immediate
    NewProject(); A.quantize = true; A.playing = true;
    A.trigger(0, 0); A.trigger(0, 1);
    if (A.layers[0].clips[0].isLive() || A.layers[0].clips[1].isLive() || A.pending.size() != 1) return fail("sync should queue exactly one trigger");
    A.flushPending();
    if (!A.layers[0].clips[1].isLive() || A.layers[0].clips[0].isLive() || !A.pending.empty()) return fail("sync flush");
    A.quantize = false; A.trigger(0, 0); if (!A.layers[0].clips[0].isLive()) return fail("trigger with sync off must be immediate");
    if (std::getenv("MIKMAP_BENCH")) {   // cost of one dirty/undo snapshot: it runs ~5x per second while the UI is idle
      auto t0 = std::chrono::steady_clock::now();
      for (int i = 0; i < 200; ++i) (void)ProjectDirty();
      double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count() / 200.0;
      std::printf("snapshot: %.3f ms each\n", ms);
    }
    // Empty column/cell = blackout: firing an empty column must STOP whatever was live elsewhere, not leave it running.
    NewProject(); A.quantize = false;
    if (!A.layers[0].clips[2].isLive()) return fail("fixture: expected a live clip at col2 before the test");
    A.fireColumn(A.colCount() - 1);   // the demo's trailing columns are empty in every layer
    for (auto& l : A.layers) if (l.live) return fail("firing an all-empty column must stop every layer");
    NewProject();
    int liveLi = -1; for (int li = 0; li < (int)A.layers.size(); ++li) if (A.layers[li].live) { liveLi = li; break; }
    if (liveLi < 0) return fail("fixture: expected some layer live");
    A.trigger(liveLi, A.colCount() - 1);   // click an empty cell's body on that same layer
    if (A.layers[liveLi].live) return fail("triggering an empty cell must stop that layer");
    // Armed cells (Ar(), the demo's "looks empty but isn't Clip::Empty" flavor) must be treated as empty everywhere:
    // cue() must not select them into Live/Selected, and firing one must not fabricate a nameless playing clip.
    NewProject();
    { bool foundArmed = false;
      for (auto& l : A.layers) for (auto& c : l.clips) if (c.st == Clip::Armed) foundArmed = true;
      if (!foundArmed) return fail("fixture: expected an Armed cell in the demo project"); }
    for (int li = 0; li < (int)A.layers.size() && true; ++li)
      for (int ci = 0; ci < (int)A.layers[li].clips.size(); ++ci)
        if (A.layers[li].clips[ci].st == Clip::Armed) {
          A.cue(li, ci); A.trigger(li, ci);
          if (A.layers[li].clips[ci].st != Clip::Armed) return fail("cue+trigger must not change an Armed cell's state");
          if (A.layers[li].live) return fail("triggering an Armed cell must not mark the layer live");
        }
    // Firing a column shows the topmost layer with real content there in Preview Cue (selLi/selCi).
    NewProject();
    A.fireColumn(0);
    if (A.selLi != 0 || A.layers[0].clips[0].name.empty()) return fail("fireColumn must preview the topmost layer with content");
    // Stopping via an empty cell must work for EVERY layer on its own (not just the first one) -- each layer is
    // independent, so clicking layer X's empty cell must never leave layer X still live, regardless of layer Y.
    NewProject();
    int stoppedCount = 0;
    for (int li = 0; li < (int)A.layers.size(); ++li) {
      Layer& l = A.layers[li];
      int emptyCi = -1;
      for (int ci = 0; ci < (int)l.clips.size(); ++ci) if (l.clips[ci].st == Clip::Empty || l.clips[ci].st == Clip::Armed) { emptyCi = ci; break; }
      if (emptyCi < 0) continue;
      bool wasLive = l.live;
      A.trigger(li, emptyCi);
      if (A.layers[li].live) { std::fprintf(stderr, "layer %d (%s) still live after clicking its own empty cell\n", li, l.name.c_str()); return 1; }
      if (wasLive) ++stoppedCount;
    }
    if (stoppedCount == 0) return fail("fixture: expected at least one layer to start live so the stop could be observed");
    // Auto-start column: off by default, only fires (and only on LOAD, not on plain NewProject) when set and saved.
    NewProject();
    if (A.autoStartCol != -1) return fail("autoStartCol must default to off");
    for (auto& l : A.layers) for (auto& c : l.clips) if (c.isLive()) c.st = Clip::Loaded;   // clean baseline: stop the demo's own default-live clips
    for (auto& l : A.layers) l.live = false;
    A.autoStartCol = 2;   // Column 3, non-empty in every layer of the demo
    if (!SaveProject(roundtrip, err)) return fail(err.c_str());
    NewProject();   // must NOT inherit the setting from the in-memory state -- only a real load applies it
    if (A.autoStartCol != -1) return fail("NewProject must not carry over autoStartCol");
    if (!LoadProject(roundtrip, err)) return fail(err.c_str());
    if (A.autoStartCol != 2) return fail("autoStartCol not restored");
    int nLive = 0; for (auto& l : A.layers) if (l.live) ++nLive;
    if (nLive == 0) return fail("opening a project with autoStartCol set must fire that column");
    if (A.activeCol != 2) return fail("opening must select the auto-start column");
    // A stale autoStartCol pointing past the real column count (e.g. saved before a column got deleted) must fall
    // back to off on load, not read out of bounds.
    { std::FILE* f = std::fopen(roundtrip.c_str(), "wb");
      if (f) { std::fputs("{\"format\":1,\"composition\":{\"autoStartCol\":999,\"layers\":[{\"name\":\"L\",\"clips\":[{\"name\":\"c\",\"st\":1}]}]},\"screens\":[{\"id\":\"s\",\"slices\":[{\"id\":\"sl\"}]}]}", f); std::fclose(f); } }
    if (!LoadProject(roundtrip, err)) return fail(err.c_str());
    if (A.autoStartCol != -1) return fail("out-of-range autoStartCol must clamp to off, not read out of bounds");
    // Timeline transport next/prev: unlike stepSel() (pure navigation), the button actions actually play the column.
    NewProject(); A.activeCol = 0;
    A.stepFireColumn(1);
    if (A.activeCol != 1) return fail("stepFireColumn(1) must move to column 1");
    { bool any = false; for (auto& l : A.layers) if (l.live) any = true;
      if (!any && !A.layers[0].clips[1].name.empty()) return fail("stepFireColumn must actually fire the column, not just select it"); }
    A.stepFireColumn(-100);   // clamps, does not go negative/out of range
    if (A.activeCol != 0) return fail("stepFireColumn must clamp to the first column");
    // Multi-deck: switching mirrors content in/out, addDeck starts blank+independent, delete keeps >=1.
    NewProject();
    if (A.decks.size() != 1 || A.decks[0].name != "Deck A") return fail("fixture: expected exactly Deck A");
    std::string deckAClip0 = A.layers[0].clips[0].name;
    A.addDeck();
    if (A.decks.size() != 2 || A.curDeckIdx != 1) return fail("addDeck must create and switch to a new deck");
    if (A.layers.empty() || !A.layers[0].clips[0].name.empty()) return fail("a new deck must start with empty clips, not a copy of Deck A");
    A.layers[0].name = "Solo"; A.trigger(0, 0);   // trigger on an empty cell just stops (already covered elsewhere); rename is the real check
    A.switchDeck(0);
    if (A.curDeckIdx != 0 || A.layers[0].clips[0].name != deckAClip0) return fail("switching back to Deck A must restore its own content");
    A.switchDeck(1);
    if (A.layers[0].name != "Solo") return fail("switching to Deck B must restore the edit made there");
    if (!SaveProject(roundtrip, err)) return fail(err.c_str());
    NewProject();
    if (!LoadProject(roundtrip, err)) return fail(err.c_str());
    if (A.decks.size() != 2 || A.curDeckIdx != 1 || A.layers[0].name != "Solo") return fail("multi-deck save/load did not round-trip");
    A.switchDeck(0);
    if (A.layers[0].clips[0].name != deckAClip0) return fail("Deck A content lost across save/load");
    A.deleteDeck(0);
    if (A.decks.size() != 1 || A.layers[0].name != "Solo") return fail("deleting the non-active deck must keep the active one showing");
    NewProject();
    if (A.decks.size() != 1) return fail("fixture: expected exactly 1 deck before the guard check");
    A.deleteDeck(0);
    if (A.decks.size() != 1) return fail("deleteDeck must refuse to remove the last remaining deck");
    // moveDeckTo (right-click "Move left/right", replacing drag): reorders decks[], remaps curDeckIdx to follow
    // whichever deck is actually current, and keeps that deck's own live content in groups/layers/colNames.
    NewProject(); A.addDeck(); A.addDeck();   // Deck A, Deck B, Deck C (curDeckIdx == 2, "Deck C")
    if (A.decks.size() != 3 || A.decks[2].name != "Deck C" || A.curDeckIdx != 2) return fail("fixture: expected 3 decks, current = Deck C");
    A.layers[0].name = "OnDeckC";
    A.moveDeckTo(2, 0);   // Deck C moves to the front: order becomes C, A, B — curDeckIdx must follow it to 0
    if (A.decks[0].name != "Deck C" || A.decks[1].name != "Deck A" || A.decks[2].name != "Deck B") return fail("moveDeckTo did not reorder decks[]");
    if (A.curDeckIdx != 0) return fail("moveDeckTo must keep curDeckIdx pointed at the deck that moved");
    if (A.layers[0].name != "OnDeckC") return fail("moveDeckTo must keep the current deck's own live content, not swap in another deck's");
    A.moveDeckTo(1, 2);   // Deck A (not current) moves past Deck B: curDeckIdx (still Deck C, now at slot 0) must NOT move
    if (A.curDeckIdx != 0 || A.decks[A.curDeckIdx].name != "Deck C") return fail("moveDeckTo must not disturb curDeckIdx when a different deck is reordered");
    if (A.decks[1].name != "Deck B" || A.decks[2].name != "Deck A") return fail("moveDeckTo(1,2) did not swap the two trailing decks");
    A.moveDeckTo(0, 0);   // no-op: same index
    if (A.decks[0].name != "Deck C" || A.curDeckIdx != 0) return fail("moveDeckTo(i,i) must be a no-op");
    // Input rect rotation/mirror: persisted, honoured by the warp map, exchanged with the output quad; slice clipboard + z-order.
    NewProject(); {
      Screen& sc0 = A.screens[0]; A.selSc = sc0.id; A.selSl = sc0.slices[0].id;
      Slice& s0 = sc0.slices[0];
      s0.warp = 0; s0.ix = 0; s0.iy = 0; s0.iw = A.canvasW; s0.ih = A.canvasH;
      s0.q[0] = {0, 0}; s0.q[1] = {1920, 0}; s0.q[2] = {1920, 1080}; s0.q[3] = {0, 1080};
      WarpMap wm; wm.slice = &s0;
      auto near2 = [](ImVec2 a, float x, float y) { return std::fabs(a.x - x) < 0.5f && std::fabs(a.y - y) < 0.5f; };
      if (!near2(wm.Map(0, 0), 0, 0) || !near2(wm.Map((float)A.canvasW, (float)A.canvasH), 1920, 1080)) return fail("unrotated input rect must map the canvas straight through");
      s0.iflipX = true; if (!near2(wm.Map(0, 0), 1920, 0)) return fail("Mirror X must flip the input horizontally"); s0.iflipX = false;
      s0.iflipY = true; if (!near2(wm.Map(0, 0), 0, 1080)) return fail("Mirror Y must flip the input vertically"); s0.iflipY = false;
      s0.irot = 180.f; if (!near2(wm.Map(0, 0), 1920, 1080)) return fail("a 180 degree input rotation must send the canvas origin to the opposite corner");
      // persistence
      s0.irot = 33.f; s0.iflipX = true; s0.iflipY = false;
      if (!SaveProject(roundtrip, err) || !LoadProject(roundtrip, err)) return fail(err.c_str());
      Slice& l0 = A.screens[0].slices[0];
      if (std::fabs(l0.irot - 33.f) > 1e-3f || !l0.iflipX || l0.iflipY) return fail("input rotation / mirror did not round-trip");
      // Match output to input keeps the rotation: the quad's top edge must point along the rect's rotated x axis
      A.selSc = A.screens[0].id; A.selSl = l0.id; A.matchOutputToInput();
      Slice& m0 = A.screens[0].slices[0];
      float ang = std::atan2(m0.q[1].y - m0.q[0].y, m0.q[1].x - m0.q[0].x) * 180.f / 3.14159265f;
      if (std::fabs(ang - 33.f) > 0.1f) return fail("matchOutputToInput must carry the input rotation onto the output quad");
    }
    NewProject(); {   // slice stacking + clipboard
      Screen& sc0 = A.screens[0]; A.selSc = sc0.id;
      if (sc0.slices.size() < 2) return fail("fixture: expected >= 2 slices on the first screen");
      std::string a0 = sc0.slices[0].id, a1 = sc0.slices[1].id; size_t n0 = sc0.slices.size();
      A.selSl = a0; A.moveSliceZ(-1);
      if (A.screens[0].slices[0].id != a0) return fail("Send Backwards on the bottom slice must be a no-op");
      A.moveSliceZ(1);
      if (A.screens[0].slices[0].id != a1 || A.screens[0].slices[1].id != a0 || A.selSl != a0) return fail("Bring Forward must swap with the slice above and keep the selection");
      A.duplicateSlice();
      if (A.screens[0].slices.size() != n0 + 1 || A.selSl == a0) return fail("Duplicate must add a slice and select the copy");
      bool dupIds = false; for (auto& x : A.screens[0].slices) for (auto& y : A.screens[0].slices) if (&x != &y && x.id == y.id) dupIds = true;
      if (dupIds) return fail("Duplicate produced a repeated slice id");
      if (A.curSlice() == nullptr || A.curSlice()->name.find(" copy") == std::string::npos) return fail("Duplicate should name the copy");
      A.copySlice(); if (!A.hasClip()) return fail("Copy must fill the slice clipboard");
      size_t n1 = A.screens[0].slices.size(); A.cutSlice();
      if (A.screens[0].slices.size() != n1 - 1) return fail("Cut must remove the slice");
      A.pasteSlice();
      if (A.screens[0].slices.size() != n1 || A.curSlice() == nullptr) return fail("Paste must bring the cut slice back and select it");
      // the clipboard survives being pasted twice, each time with fresh ids
      A.pasteSlice(); bool dup2 = false; for (auto& x : A.screens[0].slices) for (auto& y : A.screens[0].slices) if (&x != &y && x.id == y.id) dup2 = true;
      if (dup2) return fail("pasting twice produced a repeated slice id");
    }
    NewProject(); {   // Input Mask shapes are N-point polygons; the pen builds one point by point; soft edge is saved per slice
      Screen& sc0 = A.screens[0]; A.selSc = sc0.id; A.selSl = sc0.slices[0].id;
      size_t m0 = A.curSlice()->masks.size();
      const int want[5][2] = {{App::MS_HEART, 36}, {App::MS_SQUARE, 4}, {App::MS_CIRCLE, 32}, {App::MS_TRIANGLE, 3}, {App::MS_HEXAGON, 6}};
      for (auto& w : want) {
        A.addMask(w[0]);
        Mask* mk = A.curMask();
        if (!mk || (int)mk->pts.size() != w[1]) return fail("mask shape has the wrong point count");
        for (auto& p : mk->pts) if (p.x < 0 || p.x > 1920 || p.y < 0 || p.y > 1080) return fail("mask shape must stay inside the output box");
      }
      if (A.curSlice()->masks.size() != m0 + 5) return fail("each shape button must add exactly one mask");
      A.startMaskPen(); A.penPts = {{100, 100}, {300, 120}}; A.finishMaskPen();
      if (A.maskPen || A.curSlice()->masks.size() != m0 + 5) return fail("a pen shape with < 3 points must be dropped, not saved");
      A.startMaskPen(); A.penPts = {{100, 100}, {300, 120}, {200, 300}, {90, 250}, {60, 180}}; A.finishMaskPen();
      Mask* pm = A.curMask(); if (!pm || pm->pts.size() != 5 || A.maskPen) return fail("pen must create a mask with exactly the clicked points");
      A.curSlice()->softEdge = true;
      { Slice& os = *A.curSlice(); os.oflip = 3; os.isKey = true; os.blackBg = true; os.brightness = -20; os.contrast = 15; os.red = 5; os.green = -6; os.blue = 7;
        os.seGammaR = 1.5f; os.seGammaG = 2.5f; os.seGammaB = 3.f; os.seGamma = 1.25f; os.seLum = 0.75f; os.sePower = 3.5f; os.blR = 10; os.blG = 20; os.blB = 30; }
      pm->visible = false;   // the tree's eye: a hidden mask must stay hidden after save / load
      { Slice& bs = *A.curSlice(); bs.pointMode = 1; bs.meshCols = 3; bs.meshRows = 2; MeshHandle h; h.r = 1; h.c = 2; h.tu = ImVec2(0.5f, 0.25f); h.tv = ImVec2(-0.125f, 1.5f); bs.meshHandles = {h}; }
      std::string penId = pm->id; size_t nm = A.curSlice()->masks.size();
      if (!SaveProject(roundtrip, err) || !LoadProject(roundtrip, err)) return fail(err.c_str());
      Slice& ls = A.screens[0].slices[0];
      if (ls.masks.size() != nm || !ls.softEdge) return fail("masks / soft edge did not round-trip");
      if (ls.pointMode != 1 || ls.meshHandles.size() != 1 || ls.meshHandles[0].r != 1 || ls.meshHandles[0].c != 2 || ls.meshHandles[0].tu.x != 0.5f || ls.meshHandles[0].tv.y != 1.5f) return fail("Bezier point mode / handles did not round-trip");
      if (ls.oflip != 3 || !ls.isKey || !ls.blackBg || ls.brightness != -20 || ls.contrast != 15 || ls.red != 5 || ls.green != -6 || ls.blue != 7 ||
          ls.seGammaR != 1.5f || ls.seGammaG != 2.5f || ls.seGammaB != 3.f || ls.seGamma != 1.25f || ls.seLum != 0.75f || ls.sePower != 3.5f ||
          ls.blR != 10 || ls.blG != 20 || ls.blB != 30) return fail("output slice properties (flip / key / colour / soft edge / black level) did not round-trip");
      bool found = false; for (auto& m : ls.masks) if (m.id == penId) { found = m.pts.size() == 5 && m.pts[3].x == 90.f; if (m.visible) return fail("a hidden mask came back visible"); }
      if (!found) return fail("a 5-point mask did not round-trip its points");
      // every preset outline must touch all four sides of its rect (the hexagon used to stop short of top and bottom)
      for (auto& m : A.curSlice()->masks) if (m.shape >= 0 && m.rot == 0.f) {
        float lo = m.pts[0].y, hi = lo, l2 = m.pts[0].x, h2 = l2;
        for (auto& p : m.pts) { lo = std::min(lo, p.y); hi = std::max(hi, p.y); l2 = std::min(l2, p.x); h2 = std::max(h2, p.x); }
        if (std::fabs(lo - m.y) > 0.5f || std::fabs(hi - (m.y + m.h)) > 0.5f || std::fabs(l2 - m.x) > 0.5f || std::fabs(h2 - (m.x + m.w)) > 0.5f) return fail("a preset mask shape must fill its whole rect");
      }
      // preset shapes persist as shape + rect and are rebuilt from them; the pen mask stays a free outline
      int nShape = 0; for (auto& m : ls.masks) if (m.shape >= 0) { ++nShape; std::vector<ImVec2> before = m.pts; MaskRebuild(m); if (before.size() != m.pts.size() || before[0].x != m.pts[0].x) return fail("a shape mask's pts must equal its rect rebuilt"); }
      if (nShape != 5) return fail("the five preset shapes must keep their shape id across save/load");
      auto bboxW = [](const Mask& m) { float lo = m.pts[0].x, hi = lo; for (auto& p : m.pts) { lo = std::min(lo, p.x); hi = std::max(hi, p.x); } return hi - lo; };
      auto bboxH = [](const Mask& m) { float lo = m.pts[0].y, hi = lo; for (auto& p : m.pts) { lo = std::min(lo, p.y); hi = std::max(hi, p.y); } return hi - lo; };
      Mask* hm = nullptr; for (auto& m : ls.masks) if (m.shape == App::MS_SQUARE) hm = &m;
      if (!hm) return fail("fixture: expected the square mask");
      float w0 = bboxW(*hm), h0 = bboxH(*hm);
      hm->w *= 2.f; MaskRebuild(*hm);
      if (std::fabs(bboxW(*hm) - 2.f * w0) > 0.5f || std::fabs(bboxH(*hm) - h0) > 0.5f) return fail("doubling a mask's width must double only its width");
      hm->w /= 2.f; hm->rot = 90.f; MaskRebuild(*hm);
      if (std::fabs(bboxW(*hm) - h0) > 0.5f || std::fabs(bboxH(*hm) - w0) > 0.5f) return fail("a 90 degree rotation must swap the mask's bounding width and height");
      // Mask properties: the shape buttons re-shape the SELECTED mask and keep its rect
      A.selSl = ls.id; A.selMk = hm->id; A.selKind = 2;
      float mx0 = hm->x, mw0 = hm->w; A.setMaskShape(App::MS_TRIANGLE);
      if (hm->shape != App::MS_TRIANGLE || hm->pts.size() != 3 || hm->x != mx0 || hm->w != mw0) return fail("setMaskShape must swap the outline but keep the rect");
      // the pen can redraw the selected mask (replace) instead of adding one
      size_t nmask = ls.masks.size();
      A.startMaskPen(true); A.penPts = {{100, 100}, {400, 100}, {250, 300}}; A.finishMaskPen();
      if (ls.masks.size() != nmask || hm->shape != -1 || hm->pts.size() != 3 || std::fabs(hm->w - 300.f) > 0.01f) return fail("pen replace must redraw the selected mask in place, fitting its rect to the drawing");
    }
    NewProject(); {   // files from before masks lived in canvas px stored output px: converted on load by the canvas/1920x1080 ratio
      Slice& s0 = A.screens[0].slices[0];
      if (s0.masks.empty()) return fail("fixture: expected a mask on the first slice");
      ImVec2 p0 = s0.masks[0].pts[0];
      if (!SaveProject(roundtrip, err)) return fail(err.c_str());
      std::string txt; { std::FILE* f = std::fopen(roundtrip.c_str(), "rb"); if (!f) return fail("cannot reopen the saved project"); char buf[4096]; size_t n; while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) txt.append(buf, n); std::fclose(f); }
      auto rep = [&](const std::string& from, const std::string& to) { for (size_t at = txt.find(from); at != std::string::npos; at = txt.find(from, at + to.size())) txt.replace(at, from.size(), to); };
      rep("\"maskSpace\"", "\"maskSpaceX\"");      // pretend this file predates the field
      rep("\"canvasW\": 1920", "\"canvasW\": 3840");
      { std::FILE* f = std::fopen(roundtrip.c_str(), "wb"); if (!f) return fail("cannot rewrite the project"); std::fwrite(txt.data(), 1, txt.size(), f); std::fclose(f); }
      if (!LoadProject(roundtrip, err)) return fail(err.c_str());
      ImVec2 p1 = A.screens[0].slices[0].masks[0].pts[0];
      if (A.canvasW != 3840 || std::fabs(p1.x - p0.x * 2.f) > 0.5f || std::fabs(p1.y - p0.y) > 0.5f) return fail("legacy output-px masks must be scaled into canvas px on load");
    }
    NewProject(); {   // Screen properties: opacity / brightness / contrast / RGB persist, default is "untouched"
      Screen& sc0 = A.screens[0];
      if (sc0.colorActive()) return fail("a fresh screen must have no colour adjustment");
      sc0.opacity = 40; sc0.brightness = -25; sc0.contrast = 60; sc0.red = 100; sc0.green = -100; sc0.blue = 7; sc0.w = 800; sc0.h = 600;
      if (!sc0.colorActive()) return fail("colorActive must notice an adjustment");
      if (!SaveProject(roundtrip, err) || !LoadProject(roundtrip, err)) return fail(err.c_str());
      Screen& ls = A.screens[0];
      if (ls.opacity != 40 || ls.brightness != -25 || ls.contrast != 60 || ls.red != 100 || ls.green != -100 || ls.blue != 7 || ls.w != 800 || ls.h != 600)
        return fail("screen colour / size did not round-trip");
    }
    // Projector output window: Esc / F11 / Ctrl(Cmd)+W close it on key press only.
    if (!OutputKeyCloses(GLFW_KEY_ESCAPE, GLFW_PRESS, 0) || !OutputKeyCloses(GLFW_KEY_F11, GLFW_PRESS, 0) ||
        !OutputKeyCloses(GLFW_KEY_W, GLFW_PRESS, GLFW_MOD_CONTROL) || !OutputKeyCloses(GLFW_KEY_W, GLFW_PRESS, GLFW_MOD_SUPER))
      return fail("Esc / F11 / Ctrl+W / Cmd+W must close the output window");
    if (OutputKeyCloses(GLFW_KEY_ESCAPE, GLFW_RELEASE, 0) || OutputKeyCloses(GLFW_KEY_W, GLFW_PRESS, 0) || OutputKeyCloses(GLFW_KEY_A, GLFW_PRESS, GLFW_MOD_CONTROL))
      return fail("only the listed keys, on press, may close the output window");
    NewProject(); {   // Advanced Mapping: multi-selection (one kind at a time), copy/paste/delete with nested content, nudge
      auto allIdsUnique = []() {
        std::vector<std::string> ids;
        for (auto& sc : A.screens) { ids.push_back(sc.id); for (auto& sl : sc.slices) { ids.push_back(sl.id); for (auto& m : sl.masks) ids.push_back(m.id); } }
        std::sort(ids.begin(), ids.end()); return std::adjacent_find(ids.begin(), ids.end()) == ids.end();
      };
      Screen& sc0 = A.screens[0]; std::string s0 = sc0.id, sA = sc0.slices[0].id, sB = sc0.slices[1].id;
      A.selSc = s0; A.selSl = sA; A.selMk.clear(); A.selKind = 1;
      A.mapToggle(1, {s0, sB, ""});
      if (A.mapSelCount() != 2 || !A.mapIsSel(1, s0, sA, "") || !A.mapIsSel(1, s0, sB, "")) return fail("Ctrl-click must add a second slice to the selection");
      A.mapToggle(0, {A.screens[1].id, "", ""});   // a screen while slices are selected: ignored, the kinds never mix
      A.mapToggle(2, {s0, sA, A.screens[0].slices[0].masks[0].id});
      if (A.mapSelCount() != 2 || A.MapKind() != 1) return fail("selecting a different kind together with slices must be ignored");
      A.mapToggle(1, {s0, sB, ""});
      if (A.mapSelCount() != 1 || A.mapIsSel(1, s0, sB, "")) return fail("Ctrl-click on a selected slice must remove it");
      A.mapToggle(1, {s0, sB, ""}); A.selSl = sB; A.selSc = s0;   // the primary moving elsewhere by a plain click drops the multi-selection
      A.selSl = A.screens[1].slices[0].id; A.selSc = A.screens[1].id;
      if (A.mapSelCount() != 1) return fail("a plain selection elsewhere must end the multi-selection");
      // copy 2 slices (one carries a mask) and paste: fresh ids, masks come along
      A.selSc = s0; A.selSl = sA; A.selKind = 1; A.mapMulti.clear(); A.mapToggle(1, {s0, sB, ""});
      size_t nSl = A.screens[0].slices.size(), nMk = 0; for (auto& sl : A.screens[0].slices) nMk += sl.masks.size();
      A.copySelection(); A.pasteSelection();
      size_t nMk2 = 0; for (auto& sl : A.screens[0].slices) nMk2 += sl.masks.size();
      if (A.screens[0].slices.size() != nSl + 2 || nMk2 <= nMk || A.mapSelCount() != 2 || !allIdsUnique()) return fail("pasting two slices must add both (with their masks), select them, and keep every id unique");
      // delete the pasted pair, then a screen keeps its last slice
      A.deleteSelection();
      if (A.screens[0].slices.size() != nSl) return fail("Delete must remove every selected slice");
      A.selSc = A.screens[2].id; A.selSl = A.screens[2].slices[0].id; A.selMk.clear(); A.selKind = 1; A.mapMulti.clear();
      A.deleteSelection();
      if (A.screens[2].slices.size() != 1) return fail("the last slice of a screen must survive Delete");
      // screen copy carries its slices and their masks
      A.selSc = A.screens[1].id; A.selKind = 0; A.mapMulti.clear();
      size_t nScr = A.screens.size(), slIn = A.screens[1].slices.size(), mkIn = 0; for (auto& sl : A.screens[1].slices) mkIn += sl.masks.size();
      A.copySelection(); A.pasteSelection();
      if (A.screens.size() != nScr + 1 || !allIdsUnique()) return fail("pasting a screen must add one screen with unique ids");
      Screen* pc = nullptr; for (auto& sc : A.screens) if (sc.id == A.selSc) pc = &sc;
      size_t mkOut = 0; if (pc) for (auto& sl : pc->slices) mkOut += sl.masks.size();
      if (!pc || pc->slices.size() != slIn || mkOut != mkIn || pc->id == A.screens[1].id) return fail("a pasted screen must carry all its slices and masks");
      // multi-screen delete leaves at least one screen
      A.mapToggle(0, {A.screens[0].id, "", ""}); A.mapToggle(0, {A.screens[1].id, "", ""}); A.mapToggle(0, {A.screens[2].id, "", ""});
      while (A.screens.size() > 1 && A.mapSelCount() < (int)A.screens.size()) { A.mapToggle(0, {A.screens[A.mapSelCount() % A.screens.size()].id, "", ""}); break; }
      A.deleteSelection();
      if (A.screens.empty()) return fail("Delete must never remove the last screen");
    }
    NewProject(); {   // masks: copy into another slice, delete, nudge on both pages
      Screen& sc0 = A.screens[0]; std::string s0 = sc0.id, sA = sc0.slices[0].id, sB = sc0.slices[1].id;
      std::string mid = sc0.slices[0].masks[0].id;
      A.selSc = s0; A.selSl = sA; A.selMk = mid; A.selKind = 2; A.mapMulti.clear();
      A.copySelection();
      A.selSl = sB; A.selMk.clear(); A.selKind = 1;
      size_t before = A.screens[0].slices[1].masks.size(); A.pasteSelection();
      if (A.screens[0].slices[1].masks.size() != before + 1 || A.MapKind() != 2 || !A.curMask() || A.curMask()->id == mid) return fail("a copied mask must paste into the current slice as a new, selected mask");
      Mask* pm = A.curMask(); float mx0 = pm->x, my0 = pm->y;
      A.mpage = 0; A.nudgeSelection(3.f, -2.f);
      if (std::fabs(pm->x - mx0 - 3.f) > 1e-3f || std::fabs(pm->y - my0 + 2.f) > 1e-3f) return fail("arrow keys must move the selected mask on the Input page");
      A.deleteSelection();
      if (A.screens[0].slices[1].masks.size() != before) return fail("Delete must remove the selected mask");
      Slice& sb = A.screens[0].slices[1]; int ix0 = sb.ix; ImVec2 q0 = sb.q[0];
      A.selSl = sB; A.selMk.clear(); A.selKind = 1; A.mapMulti.clear();
      A.mpage = 0; A.nudgeSelection(10.f, 0.f);
      if (sb.ix != ix0 + 10) return fail("arrow keys must move the selected slice's input rect on the Input page");
      A.mpage = 1; A.nudgeSelection(0.f, 5.f);
      if (std::fabs(sb.q[0].y - q0.y - 5.f) > 1e-3f || std::fabs(sb.q[0].x - q0.x) > 1e-3f) return fail("arrow keys must move the selected slice's output quad on the Output page");
      A.selSc = s0; A.selKind = 0; A.mapMulti.clear(); ImVec2 qa = A.screens[0].slices[0].q[2];
      A.nudgeSelection(4.f, 4.f);
      if (std::fabs(A.screens[0].slices[0].q[2].x - qa.x - 4.f) > 1e-3f) return fail("arrow keys on a selected screen must move all its slices on the Output page");
    }
    NewProject(); {   // picked output points move together (arrow keys use the same path as the group drag)
      Screen& sc0 = A.screens[0]; A.selSc = sc0.id; A.selSl = sc0.slices[0].id; A.selKind = 1; A.mapMulti.clear();
      ImVec2 a0 = sc0.slices[0].q[0], b0 = sc0.slices[1].q[2], c0 = sc0.slices[0].q[1];
      A.mpage = 1; A.mapPts = {{sc0.slices[0].id, 0}, {sc0.slices[1].id, 2}};
      A.nudgeSelection(5.f, -3.f);
      if (std::fabs(sc0.slices[0].q[0].x - a0.x - 5.f) > 1e-3f || std::fabs(sc0.slices[0].q[0].y - a0.y + 3.f) > 1e-3f ||
          std::fabs(sc0.slices[1].q[2].x - b0.x - 5.f) > 1e-3f || std::fabs(sc0.slices[1].q[2].y - b0.y + 3.f) > 1e-3f) return fail("arrow keys must move every picked point, on every slice, by the same amount");
      if (sc0.slices[0].q[1].x != c0.x || sc0.slices[0].q[1].y != c0.y) return fail("points that were not picked must stay put");
      A.mapPts.clear(); A.mpage = 0;
    }
    // Timeline: tlLayout lays clips back-to-back by real duration; tlSync flips exactly the clip under the playhead live.
    NewProject();
    auto lay = A.tlLayout();
    if (lay.empty() || lay[0].empty()) return fail("fixture: expected timeline blocks on layer 0");
    if (lay[0][0].start != 0.f) return fail("first timeline block must start at 0%");
    for (size_t i = 1; i < lay[0].size(); ++i) if (std::fabs(lay[0][i].start - lay[0][i - 1].end) > 0.01f) return fail("timeline blocks must be back-to-back with no gap");
    for (auto& l : A.layers) for (auto& c : l.clips) if (c.isLive()) c.st = Clip::Loaded;   // clean baseline
    for (auto& l : A.layers) l.live = false;
    A.tlSync(0.f);
    if (!A.layers[0].clips[lay[0][0].ci].isLive()) return fail("tlSync(0) must make the first block live");
    float midPct = (lay[0].back().start + lay[0].back().end) * 0.5f;
    A.tlSync(midPct);
    if (!A.layers[0].clips[lay[0].back().ci].isLive()) return fail("tlSync at the last block's midpoint must make it live");
    if (A.layers[0].clips[lay[0][0].ci].isLive()) return fail("tlSync must turn off the block that is no longer under the playhead");
    // F8/G8: output preset / calibration profile are their own files (explicit path here, not PresetsDir()/
    // CalibDir(), so the self-test never touches the real ~/Documents/MikMap folders) — save/load must round-trip
    // every field and reject a file that isn't actually one of these.
    {
      NewProject();
      Screen& s0 = A.screens[0];
      s0.name = "TestScreen"; s0.w = 1280; s0.h = 720;
      std::string perr, presetPath = roundtrip + ".preset";
      if (!SaveOutputPreset(presetPath, s0, perr)) return fail(perr.c_str());
      Screen loaded;
      if (!LoadOutputPreset(presetPath, loaded, perr)) return fail(perr.c_str());
      if (loaded.name != "TestScreen" || loaded.w != 1280 || loaded.h != 720) return fail("output preset did not round-trip Screen fields");
      if (loaded.slices.size() != s0.slices.size()) return fail("output preset did not round-trip slices");
      { std::FILE* f = std::fopen(presetPath.c_str(), "wb"); if (f) { std::fputs("{\"format\":1,\"app\":\"MikMap\"}", f); std::fclose(f); } }
      if (LoadOutputPreset(presetPath, loaded, perr)) return fail("a file with no \"screen\" object was accepted as an output preset");

      A.calib = {{0, 0, 10, 10}, {100, 0, 110, 10}, {100, 100, 110, 110}, {0, 100, 10, 110}};
      for (int i = 0; i < 4; ++i) A.roi[i] = ImVec2(1.f * i, 2.f * i);
      A.noise = 3.5f; A.blobSize = 7.25f;
      std::string calibPath = roundtrip + ".calib";
      if (!SaveCalibProfile(calibPath, A.calib, A.roi, A.noise, A.blobSize, perr)) return fail(perr.c_str());
      std::vector<Calib> loadedCalib; ImVec2 loadedRoi[4]; float loadedNoise = 0, loadedBlob = 0;
      if (!LoadCalibProfile(calibPath, loadedCalib, loadedRoi, loadedNoise, loadedBlob, perr)) return fail(perr.c_str());
      if (loadedCalib.size() != 4 || loadedCalib[2].mx != 110) return fail("calibration profile did not round-trip points");
      if (loadedRoi[3].x != 3.f || loadedRoi[3].y != 6.f) return fail("calibration profile did not round-trip ROI");
      if (loadedNoise != 3.5f || loadedBlob != 7.25f) return fail("calibration profile did not round-trip noise/blobSize");
      { std::FILE* f = std::fopen(calibPath.c_str(), "wb"); if (f) { std::fputs("{\"format\":1,\"calib\":[1,2,3]}", f); std::fclose(f); } }
      if (LoadCalibProfile(calibPath, loadedCalib, loadedRoi, loadedNoise, loadedBlob, perr)) return fail("a calibration file without exactly 4 points was accepted");
    }
    // F5/F9: keystone is a true perspective map, and the mesh warp lives in keystone space so it follows the corners.
    {
      auto closeTo = [](ImVec2 a, ImVec2 b, float tol) { return std::fabs(a.x - b.x) <= tol && std::fabs(a.y - b.y) <= tol; };
      Slice k; k.q[0] = ImVec2(100, 100); k.q[1] = ImVec2(900, 200); k.q[2] = ImVec2(850, 700); k.q[3] = ImVec2(150, 900);
      for (int i = 0; i < 4; ++i) if (!closeTo(SliceMapUV(k, i == 1 || i == 2 ? 1.f : 0.f, i >= 2 ? 1.f : 0.f), k.q[i], 0.01f)) return fail("keystone does not hit its own corners");
      // a homography sends the square's centre to where the quad's diagonals cross (bilinear would not)
      ImVec2 p0 = k.q[0], p2 = k.q[2], p1 = k.q[1], p3 = k.q[3];
      float d1x = p2.x - p0.x, d1y = p2.y - p0.y, d2x = p3.x - p1.x, d2y = p3.y - p1.y;
      float t = ((p1.x - p0.x) * d2y - (p1.y - p0.y) * d2x) / (d1x * d2y - d1y * d2x);
      if (!closeTo(SliceMapUV(k, 0.5f, 0.5f), ImVec2(p0.x + d1x * t, p0.y + d1y * t), 0.05f)) return fail("keystone is not a perspective (homography) map");
      // mesh mode with an undeformed grid must look exactly like plain corner pin
      Slice w = k; w.warp = 1; w.meshCols = 4; w.meshRows = 3;
      for (float u : {0.1f, 0.5f, 0.83f}) for (float v : {0.2f, 0.6f}) if (!closeTo(SliceMapUV(w, u, v), SliceMapUV(k, u, v), 0.05f)) return fail("undeformed mesh differs from corner pin");
      // deform one interior vertex, then move a keystone corner: the vertex must move with it
      std::vector<float> us = {0, 0.25f, 0.5f, 0.75f, 1}, vs = {0, 1 / 3.f, 2 / 3.f, 1};
      for (float v : vs) { std::vector<ImVec2> row; for (float u : us) row.push_back(ImVec2(u, v)); w.meshLocal.push_back(row); }
      w.meshLocal[1][2] = ImVec2(0.5f, 0.2f);
      auto cornerPinAt = [&](const Slice& s, ImVec2 l) { Slice c = s; c.warp = 0; return SliceMapUV(c, l.x, l.y); };
      if (!closeTo(SliceMapUV(w, 0.5f, 1 / 3.f), cornerPinAt(w, ImVec2(0.5f, 0.2f)), 0.05f)) return fail("mesh vertex not mapped through the keystone");
      ImVec2 before = SliceMapUV(w, 0.5f, 1 / 3.f);
      w.q[1] = ImVec2(1300, -150);   // drag TR far outside the output box
      ImVec2 after = SliceMapUV(w, 0.5f, 1 / 3.f);
      if (closeTo(before, after, 1.f)) return fail("mesh did not follow the keystone corner");
      if (!closeTo(after, cornerPinAt(w, ImVec2(0.5f, 0.2f)), 0.05f)) return fail("mesh vertex left its place in keystone space");
      // bounds cover points outside both the quad and the output box
      ImVec2 bmn, bmx; SliceOutputBounds(w, bmn, bmx);
      if (bmx.x < 1300 || bmn.y > -150) return fail("slice bounds miss an outside corner");
      // meshLocal survives save/load
      NewProject();
      A.screens[0].slices[0] = w; A.screens[0].slices[0].id = "kw";
      if (!SaveProject(roundtrip, err)) return fail(err.c_str());
      NewProject();
      if (!LoadProject(roundtrip, err)) return fail(err.c_str());
      const Slice& lw = A.screens[0].slices[0];
      if (lw.meshLocal.size() != 4 || !closeTo(lw.meshLocal[1][2], ImVec2(0.5f, 0.2f), 1e-4f) || !closeTo(lw.q[1], ImVec2(1300, -150), 1e-3f)) return fail("meshLocal/keystone not saved");
      // older files stored the mesh in absolute output pixels and, in mesh mode, ignored q: loading must keep the look
      std::string oldPath = roundtrip + ".oldmesh";
      { std::FILE* f = std::fopen(oldPath.c_str(), "wb"); if (f) { std::fputs(
          "{\"format\":1,\"screen\":{\"id\":\"s\",\"name\":\"S\",\"slices\":[{\"id\":\"a\",\"name\":\"A\",\"warp\":1,\"meshCols\":2,\"meshRows\":2,"
          "\"q\":[[100,100],[900,100],[900,700],[100,700]],"
          "\"meshPts\":[[[0,0],[500,0],[1000,0]],[[0,500],[520,480],[1000,500]],[[0,1000],[500,1000],[1000,1000]]]}]}}", f); std::fclose(f); } }
      Screen old;
      if (!LoadOutputPreset(oldPath, old, err) || old.slices.size() != 1) return fail("old-format preset did not load");
      const Slice& os = old.slices[0];
      if (!closeTo(os.q[1], ImVec2(1000, 0), 1e-3f)) return fail("old mesh corners were not adopted as the keystone");
      if (!closeTo(SliceMapUV(os, 0.5f, 0.5f), ImVec2(520, 480), 0.05f) || !closeTo(SliceMapUV(os, 0.5f, 0.f), ImVec2(500, 0), 0.05f)) return fail("old absolute mesh changed shape on load");
    }
    { std::string why; if (!MappingSelfTest(why)) return fail(why.c_str()); }
    {   // an old project with a corner-pin-only slice (warp 0, the old 4 x 3 default) loads as a plain 1 x 1 grid and looks the same
      std::string oldPath = roundtrip + ".oldpin";
      { std::FILE* f = std::fopen(oldPath.c_str(), "wb"); if (f) { std::fputs(
          "{\"format\":1,\"screen\":{\"id\":\"s\",\"name\":\"S\",\"slices\":[{\"id\":\"a\",\"name\":\"A\",\"warp\":0,\"meshCols\":4,\"meshRows\":3,"
          "\"q\":[[100,100],[900,160],[880,700],[120,650]]}]}}", f); std::fclose(f); } }
      Screen old; std::string e2;
      if (!LoadOutputPreset(oldPath, old, e2) || old.slices.size() != 1) return fail("old corner-pin preset did not load");
      const Slice& os = old.slices[0];
      if (os.warp != 1 || os.meshCols != 1 || os.meshRows != 1 || !os.meshLocal.empty()) return fail("old corner-pin slice was not turned into a 1 x 1 grid");
    }
    // F22: slice input source = composition / one layer / one group, routed by stable ids
    {
      NewProject();
      for (size_t i = 0; i < A.layers.size(); ++i) for (size_t j = i + 1; j < A.layers.size(); ++j)
        if (A.layers[i].id.empty() || A.layers[i].id == A.layers[j].id) return fail("layers need unique non-empty ids");
      Slice& s = A.screens[0].slices[0];
      if (SliceSourceName(s) != "Composition") return fail("a new slice must default to the composition");
      std::string lid = A.layers[1].id;
      s.srcKind = Slice::SrcLayer; s.srcRef = lid;
      A.layers[1].name = "Renamed Layer";
      if (!SliceSourceValid(s) || SliceSourceName(s) != "Layer \xC2\xB7 Renamed Layer") return fail("layer routing must survive a rename");
      A.screens[0].slices[1].srcKind = Slice::SrcGroup; A.screens[0].slices[1].srcRef = A.groups[0].id;
      if (!SaveProject(roundtrip, err)) return fail(err.c_str());
      NewProject();
      if (!LoadProject(roundtrip, err)) return fail(err.c_str());
      const Slice& ls = A.screens[0].slices[0];
      if (ls.srcKind != Slice::SrcLayer || ls.srcRef != lid || !SliceSourceValid(ls)) return fail("layer source not saved/loaded");
      if (A.screens[0].slices[1].srcKind != Slice::SrcGroup || !SliceSourceValid(A.screens[0].slices[1])) return fail("group source not saved/loaded");
      if (A.layers[1].id != lid) return fail("layer id changed across save/load");
      A.layers.erase(A.layers.begin() + 1);
      if (SliceSourceValid(ls) || SliceSourceName(ls) != "Composition") return fail("a deleted layer must fall back to the composition");
      std::vector<Layer> dup(3); dup[0].id = "layer-1"; dup[1].id = "layer-1";
      EnsureLayerIds(dup);
      if (dup[0].id != "layer-1" || dup[1].id.empty() || dup[1].id == dup[0].id || dup[2].id.empty() || dup[2].id == dup[1].id) return fail("EnsureLayerIds must keep good ids and fix blank/duplicate ones");
    }
    // Deck tools menu actions: add layer (as wide as the deck), group the selected layer, Sync toggle
    {
      NewProject();
      size_t nl0 = A.layers.size(), ng0 = A.groups.size(); int nc0 = A.colCount(); bool q0 = A.quantize;
      A.addLayer();
      if (A.layers.size() != nl0 + 1 || (int)A.layers.back().clips.size() != nc0 || A.layers.back().id.empty()) return fail("addLayer must append a layer as wide as the deck, with an id");
      A.selLayer = (int)A.layers.size() - 1; A.groupSelectedLayer();
      if (A.groups.size() != ng0 + 1 || A.layers.back().group != A.groups.back().id) return fail("groupSelectedLayer must put the selected layer in a new group");
      A.toggleSync();
      if (A.quantize == q0) return fail("toggleSync must flip Sync");
      A.toggleSync();
    }
    // Preview Cue right-click presets (Center/Mirror/Halves/Reset) act on the previewed clip's transform
    {
      NewProject();
      Clip& tc = A.layers[0].clips[2];
      auto runItem = [&](const char* label) {
        PreviewTransformMenu(0, 2, ImVec2(0, 0));
        for (auto& it : A.ctx.items) if (it.label == label && it.run) { it.run(); break; }
        A.ctx.open = false;
      };
      tc.posX = 100; tc.posY = 50; tc.rotation = 30; tc.scale = 2; tc.flipH = false;
      runItem("Center X");  if (tc.posX != 0 || tc.posY != 50) return fail("Center X must zero only the horizontal position");
      runItem("Center Y");  if (tc.posY != 0) return fail("Center Y must zero the vertical position");
      runItem("Mirror X");  if (!tc.flipH) return fail("Mirror X must flip horizontally");
      runItem("Mirror Y");  if (!tc.flipV) return fail("Mirror Y must flip vertically");
      runItem("Left Half"); if (std::fabs(tc.scale - 0.5f) > 1e-4f || tc.posX != -240 || tc.posY != 0 || tc.rotation != 0) return fail("Left Half must fit the clip into the left half of the canvas");
      runItem("Top Half");  if (std::fabs(tc.scale - 0.5f) > 1e-4f || tc.posX != 0 || tc.posY != -135) return fail("Top Half must fit the clip into the top half");
      runItem("Right Half"); if (tc.posX != 240) return fail("Right Half");
      runItem("Bottom Half"); if (tc.posY != 135) return fail("Bottom Half");
      tc.anchorX = 50; tc.rotation = 20;
      runItem("Reset");     if (tc.posX != 0 || tc.posY != 0 || tc.rotation != 0 || tc.scale != 1 || tc.flipH || tc.flipV || tc.anchorX != 0) return fail("Reset must restore the identity transform");
      float px, py; tc.anchorX = 100; tc.scale = 2; tc.rotation = 0; tc.posX = 0;
      ClipEffectivePos(tc, 960.f, px, py);
      if (std::fabs(px - (100.f * 960.f / A.canvasW) * (1.f - 2.f)) > 1e-3f) return fail("anchor must fold into the effective position");
    }
    // Comp properties: a canvas resolution change rescales each slice's input rect per axis, and everything saves/loads
    {
      NewProject();
      Slice& s0 = A.screens[0].slices[0];
      s0.ix = 100; s0.iy = 50; s0.iw = 1720; s0.ih = 980;
      A.setCanvasSize(3840, 1080);
      if (A.canvasW != 3840 || A.canvasH != 1080 || s0.ix != 200 || s0.iw != 3440 || s0.iy != 50 || s0.ih != 980) return fail("resolution change must rescale the slice input rect per axis");
      A.setCanvasSize(3840, 2160);
      if (s0.iy != 100 || s0.ih != 1960 || s0.ix != 200 || s0.iw != 3440) return fail("vertical-only resize must leave x/width alone");
      A.setCanvasSize(3840, 2160);   // same size: no-op, no drift
      if (s0.iy != 100 || s0.ih != 1960) return fail("resizing to the same size must not change anything");
      A.setCanvasSize(1, 999999);
      if (A.canvasW != 64 || A.canvasH != 16384) return fail("canvas size must clamp to 64..16384");
      A.setCanvasSize(3840, 2160);
      A.comp.master = 42; A.comp.speed = 150; A.comp.volume = -6; A.comp.pan = 25; A.comp.opacity = 80; A.comp.xfBlend = 2; A.comp.xfBehaviour = 1; A.comp.xfCurve = 2;
      A.comp.posX = -123; A.comp.posY = 45; A.comp.scale = 75; A.comp.rotation = 30; A.comp.anchorX = 10; A.comp.anchorY = -20;
      if (!SaveProject(roundtrip, err)) return fail(err.c_str());
      A.comp = CompProps(); A.setCanvasSize(1280, 720);
      if (!LoadProject(roundtrip, err)) return fail(err.c_str());
      const CompProps& k = A.comp;
      if (A.canvasW != 3840 || A.canvasH != 2160 || k.master != 42 || k.speed != 150 || k.volume != -6 || k.pan != 25 || k.opacity != 80 ||
          k.xfBlend != 2 || k.xfBehaviour != 1 || k.xfCurve != 2 || k.posX != -123 || k.posY != 45 || k.scale != 75 || k.rotation != 30 ||
          k.anchorX != 10 || k.anchorY != -20) return fail("comp properties + resolution must survive save/load");
      if (ProjectDirty()) return fail("comp properties must not leave the project dirty right after load");
      A.comp.master = 10;
      if (!ProjectDirty()) return fail("changing a comp property must mark the project dirty");
    }
    // Clip transport: the in/out range bounds playback, pause holds still, the default range plays the whole clip as before
    {
      Clip c; c.st = Clip::Live; c.dur = "10s"; c.playMode = PM_LOOP; c.progress = 50; c.inPt = 20; c.outPt = 60;
      AdvanceClip(c, 2.f);   // +20% -> 70, past the out point: wraps to in + (70-20) mod 40 = 30
      if (std::fabs(c.progress - 30.f) > 0.01f) return fail("loop must wrap inside the in/out range");
      c.paused = true; float held = c.progress; AdvanceClip(c, 5.f);
      if (c.progress != held) return fail("a paused clip must not advance");
      Clip o; o.st = Clip::Live; o.dur = "10s"; o.playMode = PM_ONCE; o.progress = 55; o.outPt = 60;
      AdvanceClip(o, 2.f);
      if (std::fabs(o.progress - 60.f) > 0.01f || o.st != Clip::Loaded) return fail("ONCE must stop at the out point");
      Clip b; b.st = Clip::Live; b.dur = "10s"; b.playMode = PM_BOUN; b.progress = 55; b.inPt = 10; b.outPt = 60;
      AdvanceClip(b, 1.f);   // +10 -> 65: bounces off 60 -> 55, now running backwards
      if (std::fabs(b.progress - 55.f) > 0.01f || b.dir != -1) return fail("bounce must reflect at the out point");
      Clip d; d.st = Clip::Live; d.dur = "10s"; d.playMode = PM_LOOP; d.progress = 95;
      AdvanceClip(d, 1.f);
      if (std::fabs(d.progress - 5.f) > 0.01f) return fail("default range must still wrap 0..100");
      NewProject();
      Clip& s0 = A.layers[0].clips[1];
      s0.inPt = 15; s0.outPt = 85; s0.blend = 3; s0.chan = 5; s0.anchorX = 12; s0.anchorY = -8; s0.tMode = 1; s0.autoAction = 2; s0.autoLoops = 4;
      s0.width = 640; s0.height = 360; s0.volume = -12; s0.pan = 30; s0.paused = true;
      if (!SaveProject(roundtrip, err)) return fail(err.c_str());
      A.layers[0].clips[1].inPt = 0; A.layers[0].clips[1].blend = 0; A.layers[0].clips[1].chan = 7;
      if (!LoadProject(roundtrip, err)) return fail(err.c_str());
      const Clip& r = A.layers[0].clips[1];
      if (r.inPt != 15 || r.outPt != 85 || r.blend != 3 || r.chan != 5 || r.anchorX != 12 || r.anchorY != -8 || r.tMode != 1 || r.autoAction != 2 || r.autoLoops != 4 ||
          r.width != 640 || r.height != 360 || r.volume != -12 || r.pan != 30) return fail("clip properties must survive save/load");
      if (r.paused) return fail("pause is runtime state and must not be saved");
      if (ProjectDirty()) return fail("clip properties must not leave the project dirty right after load");
    }
    // Layer colour: changing it recolours the clips that had the layer's old colour, and only those
    {
      NewProject();
      Layer& L = A.layers[1];
      for (auto& c : L.clips) c.color = 0;
      L.color = 0; L.clips[0].color = 3;   // one clip the user coloured differently
      A.setLayerColor(1, 2);
      int same = 0, other = 0; for (auto& c : A.layers[1].clips) { if (c.color == 2) ++same; if (c.color == 3) ++other; }
      if (A.layers[1].color != 2 || other != 1 || same != (int)A.layers[1].clips.size() - 1) return fail("layer colour must recolour only the clips that shared the layer's old colour");
      A.setLayerColor(1, 2);   // same colour again: nothing changes
      if (A.layers[1].clips[0].color != 3) return fail("re-selecting the same layer colour must not touch clips");
      A.setLayerColor(9, 4); A.setLayerColor(-1, 4);   // out-of-range layer: ignored
      if (A.layers[0].color != 0) return fail("setLayerColor must ignore an invalid layer index");
    }
    // Layer properties (Properties > Layer) survive save/load; a project without them gets neutral defaults
    {
      NewProject();
      Layer& L = A.layers[1];
      if (L.master != 100 || L.scale != 100 || L.pan != 0 || L.width != 0) return fail("layer property defaults");
      L.master = 55; L.pan = -30; L.width = 1280; L.height = 720; L.autoSize = 2; L.transBlend = 3;
      L.color = 4; L.posX = 40; L.posY = -60; L.scale = 130; L.rotation = -15; L.anchorX = 5; L.anchorY = 6; L.audio = 50; L.blendTime = 1.5f;
      if (!SaveProject(roundtrip, err)) return fail(err.c_str());
      A.layers[1].master = 100; A.layers[1].scale = 100; A.layers[1].pan = 0; A.layers[1].width = 0;
      if (!LoadProject(roundtrip, err)) return fail(err.c_str());
      const Layer& R = A.layers[1];
      if (R.master != 55 || R.pan != -30 || R.width != 1280 || R.height != 720 || R.autoSize != 2 || R.transBlend != 3 || R.posX != 40 || R.posY != -60 ||
          R.scale != 130 || R.rotation != -15 || R.anchorX != 5 || R.anchorY != 6 || R.color != 4 || R.audio != 50 || R.blendTime != 1.5f) return fail("layer properties must survive save/load");
      if (ProjectDirty()) return fail("layer properties must not leave the project dirty right after load");
    }
    // OS file drop (media kinds by extension; a multi-file drop on a cell fills the following EMPTY cells of that layer)
    {
      if (MediaKindOf("a.PNG") != MEDIA_IMAGE || MediaKindOf("b.mov") != MEDIA_VIDEO || MediaKindOf("c.WAV") != MEDIA_AUDIO || MediaKindOf("d.txt") != MEDIA_NONE) return fail("media kind by extension");
      std::string f1 = roundtrip + ".drop1.mov", f2 = roundtrip + ".drop2.wav", f3 = roundtrip + ".drop3.txt";
      for (auto* f : {&f1, &f2, &f3}) { std::FILE* fp = std::fopen(f->c_str(), "wb"); if (fp) { std::fputs("x", fp); std::fclose(fp); } }
      NewBlankProject(); A.mediaExtra.clear();
      A.dropFilesOnCell(0, 1, {f1, f2, f3});
      if (A.layers[0].clips[1].media != f1 || A.layers[0].clips[2].media != f2) return fail("dropped files must land in the target cell and the next empty one");
      if (A.layers[0].clips[3].st != Clip::Empty) return fail("an unsupported file (.txt) must not create a clip");
      if (A.mediaExtra.size() != 2) return fail("dropped media must be remembered in the Browser list (and .txt ignored)");
      A.dropFilesOnCell(0, 1, {f1, f2});   // cell 1 replaced, cell 2 occupied -> the second file skips to the next empty cell (3)
      if (A.layers[0].clips[1].media != f1 || A.layers[0].clips[2].media != f2 || A.layers[0].clips[3].media != f2) return fail("a further file must skip occupied cells");
      if (ImportMedia({f1, f2}) != 0 || A.mediaExtra.size() != 2) return fail("re-importing the same files must not duplicate them");
      NewProject();
      if (A.mediaExtra.size() != 2) return fail("dropped media is machine-level: New project must keep it");
      A.mediaExtra.clear(); A.mediaStale = true;
      for (auto* f : {&f1, &f2, &f3}) std::remove(f->c_str());
    }
    std::printf("roundtrip OK\n"); return 0;
  }
  gAssets = FindAssets(argv[0]);

  if (!glfwInit()) return 1;
#ifdef __APPLE__
  // macOS only hands out GL 2.1 (legacy) or 3.2+ core; a plain "3.0" request fails to create the window.
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
#else
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
  glfwWindowHint(GLFW_SAMPLES, 4);
  if (!shot.empty()) glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
  GLFWwindow* win = glfwCreateWindow(W, H, "MikMap Pro \xE2\x80\x94 show_alpha_v3.mikmap", nullptr, nullptr);
  if (!win) return 2;
  gWin = win;
  glfwMakeContextCurrent(win);
  glfwSwapInterval(1);
  glfwSetWindowSizeLimits(win, 1100, 640, GLFW_DONT_CARE, GLFW_DONT_CARE);
#ifdef _WIN32
  {
    // Dark titlebar to match the app's own dark theme — Windows-only DWM
    // attribute, no equivalent needed on Linux/macOS (window manager already
    // follows the OS-level dark mode there).
    HWND hwnd = glfwGetWin32Window(win);
    BOOL dark = TRUE; DwmSetWindowAttribute(hwnd, 20, &dark, sizeof dark);
    COLORREF cap = RGB(0x1c, 0x1c, 0x1c), txt = RGB(0xf3, 0xf3, 0xf3), bd = RGB(0x2a, 0x2a, 0x2a);
    DwmSetWindowAttribute(hwnd, 35, &cap, sizeof cap); DwmSetWindowAttribute(hwnd, 36, &txt, sizeof txt); DwmSetWindowAttribute(hwnd, 34, &bd, sizeof bd);
  }
#endif

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGuiIO& io = ImGui::GetIO();
  io.IniFilename = nullptr;
  io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
  SetupStyle();
  ImGui_ImplGlfw_InitForOpenGL(win, true);
  // Files dragged in from Explorer/Finder. GLFW moves its cursor position to the drop point before this fires
  // (Win32 WM_DROPFILES, X11 XdndPosition), so glfwGetCursorPos tells the UI which panel/cell was hit.
  glfwSetDropCallback(win, [](GLFWwindow* w, int n, const char** paths) {
    double cx = 0, cy = 0; glfwGetCursorPos(w, &cx, &cy);
    const float zs = A.prefs.scale / 100.f;
    A.osDrop = OsDrop(); A.osDrop.pending = true; A.osDrop.pos = ImVec2((float)cx / zs, (float)cy / zs);
    for (int i = 0; i < n; ++i) A.osDrop.paths.push_back(paths[i]);
  });
  #ifdef __APPLE__
  ImGui_ImplOpenGL3_Init("#version 150");
#else
  ImGui_ImplOpenGL3_Init("#version 130");
#endif
  InitBlendModes([](const char* n) { return (void*)glfwGetProcAddress(n); });
  if (shot.empty()) LoadSettings();          // scripted screenshot runs must not depend on (or touch) the user's saved settings
  if (scaleGiven) A.prefs.scale = cliScale;
  LoadAllFonts(io, gAssets);
  ApplyPrefs();
  gLogoTex = LoadTexture(gAssets + "/mikmap-mark.png");

  NewProject();
  A.screen = startScreen;
  A.mapSnap = startSnap; A.mapHand = startHand; A.testCard = startCard;
  if (startTool >= 0) A.outTool = startTool; if (startInTool >= 0) A.inTool = startInTool;   // (NewProject just reset the app state)
  if (tab >= 0) A.tab = tab;
  if (page >= 0) A.mpage = page;
  if (outKeyTest) {   // headless check that the output window's own key callback is really installed and closes it
    OpenOutput(win, A.outMonitor);
    bool ok = OutputKeyWiringOk();
    std::printf("output key wiring: %s\n", ok ? "OK" : "FAILED");
    CloseOutput();
    return ok ? 0 : 1;
  }
  if (openOut) OpenOutput(win, A.outMonitor);
  if (ctxTest) A.openCtx(ImVec2(500, 300), A.sliceMenu("screen1", "slice1"));
  if (sel) A.cue(selLi, selCi);
  for (int k : fxTest) if (k >= 0 && k < FX_COUNT) A.addFx(k);

  for (auto& cc : clipColors) { int li = 0, ci = 0, k = 0; std::sscanf(cc.c_str(), "%d,%d,%d", &li, &ci, &k);
    if (li >= 0 && li < (int)A.layers.size() && ci >= 0 && ci < A.colCount()) A.layers[li].clips[ci].color = std::clamp(k, 0, 5); }
  if (!pvTest.empty()) { float z = 1, px = 0, py = 0; int hnd = 0; std::sscanf(pvTest.c_str(), "%f,%f,%f,%d", &z, &px, &py, &hnd); A.pvZoom = z; A.pvPanX = px; A.pvPanY = py; A.pvHand = hnd != 0; }
  if (!clipTest.empty()) {
    Clip& C0 = A.layers[0].clips[2]; size_t pos = 0;
    while (pos < clipTest.size()) {
      size_t e = clipTest.find(',', pos); if (e == std::string::npos) e = clipTest.size();
      std::string kv = clipTest.substr(pos, e - pos); pos = e + 1;
      size_t q = kv.find('='); if (q == std::string::npos) continue;
      std::string k = kv.substr(0, q); float v = (float)atof(kv.c_str() + q + 1);
      if (k == "chan") C0.chan = (int)v; else if (k == "blend") C0.blend = (int)v; else if (k == "ax") C0.anchorX = v; else if (k == "ay") C0.anchorY = v;
      else if (k == "rot") C0.rotation = v; else if (k == "scale") C0.scale = v; else if (k == "px") C0.posX = v;
    }
  }
  for (auto& lt : layerTest) {
    size_t pos = 0; int li = 0; bool first = true;
    while (pos < lt.size()) {
      size_t e = lt.find(',', pos); if (e == std::string::npos) e = lt.size();
      std::string kv = lt.substr(pos, e - pos); pos = e + 1;
      if (first) { li = std::clamp(atoi(kv.c_str()), 0, (int)A.layers.size() - 1); first = false; continue; }
      size_t q = kv.find('='); if (q == std::string::npos) continue;
      std::string k = kv.substr(0, q); float v = (float)atof(kv.c_str() + q + 1); Layer& L = A.layers[li];
      if (k == "master") L.master = v; else if (k == "op") L.opacity = v; else if (k == "scale") L.scale = v; else if (k == "rot") L.rotation = v;
      else if (k == "px") L.posX = v; else if (k == "py") L.posY = v; else if (k == "vol") L.audio = v; else if (k == "color") L.color = (int)v; else if (k == "dur") L.blendTime = v;
    }
  }
  if (!compTest.empty()) {
    std::string t = compTest; size_t pos = 0;
    while (pos < t.size()) {
      size_t e = t.find(',', pos); if (e == std::string::npos) e = t.size();
      std::string kv = t.substr(pos, e - pos); pos = e + 1;
      size_t q = kv.find('='); if (q == std::string::npos) continue;
      std::string k = kv.substr(0, q); float v = (float)atof(kv.c_str() + q + 1); CompProps& c = A.comp;
      if (k == "scale") c.scale = v; else if (k == "rot") c.rotation = v; else if (k == "master") c.master = v; else if (k == "px") c.posX = v;
      else if (k == "py") c.posY = v; else if (k == "ax") c.anchorX = v; else if (k == "ay") c.anchorY = v; else if (k == "speed") c.speed = v;
      else if (k == "op") c.opacity = v; else if (k == "w") A.setCanvasSize((int)v, A.canvasH); else if (k == "h") A.setCanvasSize(A.canvasW, (int)v);
    }
  }
  double last = glfwGetTime(); double progAcc = 0; int frame = 0;
  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();
    if (!shot.empty() && dropTest.pending && frame == 8) { A.osDrop = dropTest; dropTest.pending = false; }
    if (glfwWindowShouldClose(win) && shot.empty() && A.projectDirty && glfwGetTime() >= A.discardUntil) {
      glfwSetWindowShouldClose(win, GLFW_FALSE);    // unsaved edits: first close request only warns
      A.discardUntil = glfwGetTime() + 4; g.time = glfwGetTime(); A.notify("Unsaved changes \xE2\x80\x94 close again to quit and discard", 4);
    }
    if (glfwGetWindowAttrib(win, GLFW_ICONIFIED)) { glfwWaitEventsTimeout(0.1); continue; }
    double now = glfwGetTime(), dt = std::min(0.1, now - last); last = now;
    g.time = now;
    const double beatMs = 60000.0 / std::clamp((double)A.bpm, 40.0, 240.0);
    bool beatNow = std::fmod(now * 1000.0, beatMs) < beatMs * 0.35;
    if (beatNow && !A.beat) A.flushPending();   // rising edge = a new beat: release queued triggers (Sync)
    A.beat = beatNow;
    // C1/C2/C4/C5: the selected clips run their own transport (loop / bounce / hold / once, speed, direction)
    if (A.playing)
      for (auto& sc : A.selectedCells)
        if (sc.first < (int)A.layers.size() && sc.second < (int)A.layers[sc.first].clips.size())
          AdvanceClip(A.layers[sc.first].clips[sc.second], (float)dt * A.comp.speed / 100.f);   // Comp > Speed scales every clip's playback rate
    (void)progAcc;
    if (A.deckMode == 1) {   // Timeline run mode: advance the shared playhead, then re-derive which clip is live per layer
      if (A.playing) {
        A.tlProgress += 10.f * (float)dt * A.comp.speed / 100.f;   // 100% every 10s, matching the reference prototype's 1.5%/150ms rate
        if (A.tlLoopOn) { if (A.tlProgress >= A.tlOut || A.tlProgress < A.tlIn) A.tlProgress = A.tlIn; }
        else if (A.tlProgress >= 100.f) A.tlProgress = 0.f;
      }
      A.tlSync(A.tlProgress);   // also re-applies after a manual scrub even while paused
    }
    for (auto& l : A.layers) if (l.fadeT < 1.f) l.fadeT = std::min(1.f, l.fadeT + (float)dt / std::max(0.05f, l.blendTime));
    A.sweep = std::fmod(A.sweep + (float)dt * 2.4f, 6.2831853f);
    PerfPush((float)dt * 1000.f);   // G9

    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    // UI scale (Settings > Text size): render the whole workspace in a smaller logical space and stretch it
    const float Zs = A.prefs.scale / 100.f;
    {
      int ww, wh, fw, fh; glfwGetWindowSize(win, &ww, &wh); glfwGetFramebufferSize(win, &fw, &fh);
      io.DisplaySize = ImVec2(ww / Zs, wh / Zs);
      io.DisplayFramebufferScale = ImVec2(fw / io.DisplaySize.x, fh / io.DisplaySize.y);
      if (Zs != 1.f && shot.empty() && glfwGetWindowAttrib(win, GLFW_HOVERED)) {
        double mx, my; glfwGetCursorPos(win, &mx, &my);
        io.AddMousePosEvent((float)mx / Zs, (float)my / Zs);
      }
    }
    if (!shot.empty() && hoverTest && frame >= 3 + 6 * (int)script.size()) io.AddMousePosEvent(hoverX / Zs, hoverY / Zs);
    if (!shot.empty()) {
      if (scriptCtrl) io.AddKeyEvent(ImGuiMod_Shift, true);
      // each scripted action occupies 6 frames starting at frame 3
      int rel = frame - 3;
      if (rel >= 0 && rel / 6 < (int)script.size()) {
        const Script& s = script[rel / 6]; int st = rel % 6;
        if (s.kind == 4) {   // wheel at a position: move there on frame 0, scroll on frame 1
          io.AddMousePosEvent(s.x0 / Zs, s.y0 / Zs);
          if (st == 1) io.AddMouseWheelEvent(0.f, s.x1);
        } else
        if (s.kind == 3) {   // key chord: modifiers + key down on frame 1, up on frame 3
          const ImGuiKey cmdMod = io.ConfigMacOSXBehaviors ? ImGuiMod_Super : ImGuiMod_Ctrl;   // ImGui swaps Ctrl/Super on macOS: the Cmd key is what becomes io.KeyCtrl
          if (st == 1) { if (s.ctrl) io.AddKeyEvent(cmdMod, true); if (s.shift) io.AddKeyEvent(ImGuiMod_Shift, true); io.AddKeyEvent((ImGuiKey)s.key, true); }
          if (st == 3) { io.AddKeyEvent((ImGuiKey)s.key, false); if (s.ctrl) io.AddKeyEvent(cmdMod, false); if (s.shift) io.AddKeyEvent(ImGuiMod_Shift, false); }
        } else {
        io.AddMousePosEvent((st < 3 ? s.x0 : s.x1) / Zs, (st < 3 ? s.y0 : s.y1) / Zs);
        int btn = s.kind == 1 ? 1 : 0;
        if (st == 1) io.AddMouseButtonEvent(btn, true);
        if (s.kind == 2 && st == 3) io.AddMousePosEvent(s.x1 / Zs, s.y1 / Zs);
        if (st == 4 || (s.kind != 2 && st == 2)) io.AddMouseButtonEvent(btn, false);
        }
      }
    }
    ImGui::NewFrame();
    SyncScreenResolutions();
    UpdateTestPattern();   // Show TestCard: paint its texture before anything samples it this frame
    {
      static Prefs lastPrefs = A.prefs; static int lastMon = A.outMonitor; static std::string lastTitle;
      if (io.MouseDown[0] || io.MouseDown[1] || io.MouseDown[2] || io.MouseWheel != 0.f || io.MouseWheelH != 0.f || io.InputQueueCharacters.Size > 0 ||
          ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false) || ImGui::IsKeyPressed(ImGuiKey_L, false) ||
          (A.screen == 1 && (ImGui::IsKeyPressed(ImGuiKey_LeftArrow) || ImGui::IsKeyPressed(ImGuiKey_RightArrow) || ImGui::IsKeyPressed(ImGuiKey_UpArrow) ||
                             ImGui::IsKeyPressed(ImGuiKey_DownArrow) || ImGui::IsKeyPressed(ImGuiKey_V, false) || ImGui::IsKeyPressed(ImGuiKey_X, false) || ImGui::IsKeyPressed(ImGuiKey_D, false)))) UndoNote();
      UndoTick(ImGui::IsMouseDown(0) || ImGui::IsMouseDown(1) || io.WantTextInput || A.rename.open, glfwGetTime());   // also keeps A.projectDirty current
      if (shot.empty() && (std::memcmp(&lastPrefs, &A.prefs, sizeof(Prefs)) != 0 || lastMon != A.outMonitor)) { lastPrefs = A.prefs; lastMon = A.outMonitor; SaveSettings(); }
      std::string title = "MikMap Pro \xE2\x80\x94 " + A.projectName + ".mikmap" + (A.projectDirty ? " *" : "");
      if (title != lastTitle) { lastTitle = title; glfwSetWindowTitle(win, title.c_str()); }
      bool free = !io.WantTextInput && !A.settingsOpen && !A.openDialog && !A.helpOpen && !A.rename.open && !A.projectMenu &&
                  !A.pop.open && !A.ctx.open && !A.layerMenu.open && !A.colMenu.open && !A.blendDD.open;
      if ((free || A.showMode) && ImGui::IsKeyPressed(ImGuiKey_Tab, false)) { A.showMode = !A.showMode; if (A.showMode) A.notify("Show Mode \xE2\x80\x94 press Esc or Tab to leave", 3); }
      if (A.showMode && ImGui::IsKeyPressed(ImGuiKey_Escape, false)) A.showMode = false;
      if (!A.showMode && free && !io.KeyCtrl && !io.KeyAlt && A.screen == 0 && !A.layers.empty()) {
        int li = std::clamp(A.selLi, 0, (int)A.layers.size() - 1), ci = std::clamp(A.selCi, 0, std::max(0, A.colCount() - 1));
        if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) A.playing = !A.playing;
        else if (ImGui::IsKeyPressed(ImGuiKey_Enter, false) || ImGui::IsKeyPressed(ImGuiKey_KeypadEnter, false)) A.trigger(li, ci);
        else if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) A.stepSel(-1);
        else if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) A.stepSel(1);
        else if (ImGui::IsKeyPressed(ImGuiKey_L, false)) A.layers[li].clips[ci].playMode = PM_LOOP;
        else if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) {
          A.layers[li].clips[ci] = Clip();
          A.layers[li].live = false; for (auto& k : A.layers[li].clips) if (k.isLive()) A.layers[li].live = true;
        }
      }
      if (!A.showMode && free && A.screen == 1 && !io.KeyAlt) {   // Advanced Mapping: copy / cut / paste / duplicate / delete / nudge what is selected
        if (io.KeyCtrl) {   // Ctrl (Cmd on macOS)
          if (ImGui::IsKeyPressed(ImGuiKey_C, false)) { A.copySelection(); if (A.hasClip()) A.notify(A.mapClip.kind == 0 ? "Copied screen(s) with their slices and masks" : A.mapClip.kind == 1 ? "Copied slice(s) with their masks" : "Copied mask(s)", 1.5); }
          else if (ImGui::IsKeyPressed(ImGuiKey_X, false)) A.cutSelection();
          else if (ImGui::IsKeyPressed(ImGuiKey_V, false)) A.pasteSelection();
          else if (ImGui::IsKeyPressed(ImGuiKey_D, false)) A.duplicateSelection();
        } else {
          if (ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false)) A.deleteSelection();
          float st = io.KeyShift ? 10.f : 1.f, dx = 0, dy = 0;   // arrows: 1 px, Shift = 10 px (the pixel space of the page being shown)
          bool first = false;
          if (ImGui::IsKeyPressed(ImGuiKey_LeftArrow)) { dx = -st; first |= ImGui::IsKeyPressed(ImGuiKey_LeftArrow, false); }
          if (ImGui::IsKeyPressed(ImGuiKey_RightArrow)) { dx = st; first |= ImGui::IsKeyPressed(ImGuiKey_RightArrow, false); }
          if (ImGui::IsKeyPressed(ImGuiKey_UpArrow)) { dy = -st; first |= ImGui::IsKeyPressed(ImGuiKey_UpArrow, false); }
          if (ImGui::IsKeyPressed(ImGuiKey_DownArrow)) { dy = st; first |= ImGui::IsKeyPressed(ImGuiKey_DownArrow, false); }
          if (dx != 0 || dy != 0) { if (first) A.pushHist(); A.nudgeSelection(dx, dy); }   // one history step per press, not per key repeat
        }
      }
      if (!io.WantTextInput && io.KeyCtrl) {
        if (ImGui::IsKeyPressed(ImGuiKey_S, false)) { A.notify(DoSave(io.KeyShift)); A.projectDirty = ProjectDirty(); }
        else if (ImGui::IsKeyPressed(ImGuiKey_O, false)) OpenDialogShow();
        else if (ImGui::IsKeyPressed(ImGuiKey_N, false)) RunProjectItem(0);
        else if (ImGui::IsKeyPressed(ImGuiKey_Z, false)) { if (io.KeyShift) A.redoMap(); else A.undoMap(); }
        else if (ImGui::IsKeyPressed(ImGuiKey_Y, false)) A.redoMap();
      }
    }
    ImVec2 disp = io.DisplaySize;
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(disp);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::Begin("root", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoSavedSettings |
                                      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollWithMouse |
                                      ImGuiWindowFlags_NoNavInputs);
    ImGui::PopStyleVar();
    g.dl = ImGui::GetWindowDrawList();
    g.alpha = 1.f;
    // input blocking from overlays that were open at frame start
    g.blocked = A.pop.open || A.blendDD.open || A.ctx.open || A.layerMenu.open || A.colMenu.open || A.deckMenu.open || A.settingsOpen || A.rename.open || A.openDialog || A.helpOpen ||
                (A.projectMenu && Raw(ProjectMenuRect()));
    Fill(ImRect(0, 0, disp.x, disp.y), K(pal::g0f));
    ImRect body(0, 40, disp.x, disp.y - 22);
    if (A.showMode) {
      // Show Mode: nothing to click, nothing to mis-drag — just the composite the audience sees, letterboxed to the canvas.
      Fill(ImRect(0, 0, disp.x, disp.y), K(0x000000));
      ImRect cv = CanvasRect(ImRect(0, 0, disp.x, disp.y));
      if (!A.blackout) { g.dl->PushClipRect(cv.Min, cv.Max, true); DrawComposite(cv, (float)g.time, 1.f); g.dl->PopClipRect(); }
      if (g.time < A.toastUntil && !A.toast.empty()) TextC(disp.x * 0.5f, disp.y - 18, MONO_M, 10, K(pal::t66), A.toast.c_str());
    } else {
      switch (A.screen) {
        case 0: DrawDeck(body); break;
        case 1: DrawMapping(body); break;
        default: DrawSensor(body); break;
      }
      TitleBar(ImRect(0, 0, disp.x, 40));
      StatusBar(ImRect(0, disp.y - 22, disp.x, disp.y));
      DrawOverlays(disp);
    }
    if (ImGui::IsKeyPressed(ImGuiKey_F11, false)) {
      if (A.curScreen() && IsVirtualDevice(A.curScreen()->outDev)) A.notify("Virtual output has no display window");
      else ToggleOutput(win, A.outMonitor);
    }
    ImGui::End();

    if (A.osDrop.pending) {   // every drop target has had its chance this frame
      if (!A.osDrop.handled) A.notify("Drop files onto the Browser or onto a clip cell (Composition page)");
      A.osDrop = OsDrop();
    }
    ImGui::Render();
    int dw, dh; glfwGetFramebufferSize(win, &dw, &dh);
    glViewport(0, 0, dw, dh);
    glClearColor(0.059f, 0.059f, 0.059f, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

    if (!shot.empty() && ++frame >= std::max(frames, 3 + 6 * (int)script.size() + 4)) {
      std::vector<unsigned char> px((size_t)dw * dh * 4), flipped((size_t)dw * dh * 4);
      glReadPixels(0, 0, dw, dh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
      for (int y = 0; y < dh; ++y) memcpy(&flipped[(size_t)y * dw * 4], &px[(size_t)(dh - 1 - y) * dw * 4], (size_t)dw * 4);
      for (size_t i = 3; i < flipped.size(); i += 4) flipped[i] = 255;
      stbi_write_png(shot.c_str(), dw, dh, 4, flipped.data(), dw * 4);
      break;
    }
    glfwSwapBuffers(win);
    if (!outShot.empty() && frame >= std::max(6, 3 + 6 * (int)script.size() + 2)) { SetOutputCapture(outShot.c_str()); outShot.clear(); }
    RenderOutput();   // F2: draw the warped slices into the projector window, if it is open
  }
  CloseOutput();
  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImGui::DestroyContext();
  glfwDestroyWindow(win);
  glfwTerminate();
  return 0;
}












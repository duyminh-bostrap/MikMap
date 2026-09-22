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
static int PopoverList(ImVec2 pos, ImVec2 disp, const char* title, const std::vector<PItem>& items, bool fresh, ImRect& out) {
  float w = 168, h = 2 + 3 + 9 + 4 + 2;
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
                                  {"Ctrl/Cmd + Shift + Z / Y", "Redo"}, {"F11", "Open / close projector output"}, {"Tab", "Show Mode (hide all editing UI)"},
                                  {"Space", "Play / pause"}, {"Enter", "Trigger selected clip"}, {"Left / Right", "Previous / next column"}, {"L", "Selected clip: loop mode"}, {"Delete", "Clear selected clip"}, {"Esc", "Close menu, dialog or popover"}, {"Double-click layer", "Rename layer"}, {"Alt + wheel (Mapping)", "Zoom at cursor"}};
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
    ImRect pr; int hit = PopoverList(A.pop.pos, disp, "Clip", items, freshPop, pr);
    ImRect cb(pr.Min.x, pr.Max.y + 4, pr.Min.x + 180, pr.Max.y + 4 + 6 + 9 + 4 + 18 + 6);
    Shadow(cb, 4, 24, 0.7f);
    Box(cb, K(pal::g16), K(pal::g3a), 4);
    Text(cb.Min.x + 6, cb.Min.y + 6 + 4.5f, MONO_R, 9, K(pal::t66), "CLIP COLOR", 0.09f);
    Clip& cell = A.layers[A.pop.li].clips[A.pop.ci];
    float sw = (cb.GetWidth() - 12 - 5 * 4) / 6.f;
    bool inBox = Raw(cb);
    for (int k = 0; k < 6; ++k) {
      ImRect sr(cb.Min.x + 6 + k * (sw + 4), cb.Min.y + 6 + 9 + 4, cb.Min.x + 6 + k * (sw + 4) + sw, cb.Min.y + 6 + 9 + 4 + 18);
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
    } else if (!freshPop && !inBox && (io.MouseClicked[0] || io.MouseClicked[1]) && !Raw(pr)) A.pop.open = false;
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
        Layer c = A.layers[li]; c.name += " copy"; c.live = false;
        for (auto& k : c.clips) if (k.isLive()) k.st = Clip::Loaded;
        A.layers.insert(A.layers.begin() + li + 1, c); A.selLayer = li + 1;
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
    std::vector<PItem> items = {{"Insert column before", "arrow-left-to-line", "", 0, false, false}, {"Insert column after", "arrow-right-to-line", "", 0, false, false},
                                {"Move left", "chevron-left", "", 0, ci == 0, false}, {"Move right", "chevron-right", "", 0, ci >= n - 1, false},
                                {"Rename column", "pencil", "", 0, false, false}, {"Clear column", "eraser", "", 0, false, false},
                                {"Delete column", "trash-2", "", 2, n < 2, false}};
    ImRect pr; int hit = PopoverList(A.colMenu.pos, disp, "Column", items, freshCol, pr);
    if (hit >= 0) {
      if (hit == 0) A.insertCol(ci); else if (hit == 1) A.insertCol(ci + 1);
      else if (hit == 2) A.moveColTo(ci, ci - 1); else if (hit == 3) A.moveColTo(ci, ci + 1);
      else if (hit == 4) { A.beginRename(1, ci, A.colMenu.pos, A.colName(ci)); }
      else if (hit == 5) { for (auto& l : A.layers) { l.clips[ci] = Clip(); l.live = false; for (auto& k : l.clips) if (k.isLive()) l.live = true; } }
      else if (hit == 6) A.deleteCol(ci);
      A.colMenu.open = false;
    } else if (!freshCol && (io.MouseClicked[0] || io.MouseClicked[1]) && !Raw(pr)) A.colMenu.open = false;
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
    float w = 182, h = 8 + 24.f * A.ctx.items.size();
    ImVec2 p(std::min(A.ctx.pos.x, disp.x - 190), std::min(A.ctx.pos.y, disp.y - (A.ctx.items.size() * 24 + 16)));
    ImRect r(p.x, p.y, p.x + w, p.y + h);
    Shadow(r, 4, 24, 0.7f);
    Box(r, K(pal::g16), K(pal::g3a), 4);
    bool acted = false;
    for (size_t i = 0; i < A.ctx.items.size(); ++i) {
      auto& it = A.ctx.items[i];
      ImRect ir(r.Min.x + 4, r.Min.y + 4 + i * 24, r.Max.x - 4, r.Min.y + 4 + (i + 1) * 24);
      bool hv = Raw(ir);
      float prev = g.alpha; if (it.disabled) g.alpha = 0.45f;
      if (hv && !it.disabled) Fill(ir, K(pal::g18), 3);
      uint32_t col = it.disabled ? pal::t66 : it.danger ? pal::red : pal::te0;
      float cy = (ir.Min.y + ir.Max.y) * 0.5f;
      Icon(it.icon.c_str(), ImVec2(ir.Min.x + 8 + 5.5f, cy), 11, K(col));
      Text(ir.Min.x + 8 + 11 + 8, cy, UI_S, 10, K(col), it.label.c_str());
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
    ImGui::PushFont(F(UI_B), 12);
    ImGui::TextUnformatted(A.rename.kind == 0 ? "RENAME LAYER" : A.rename.kind == 1 ? "RENAME COLUMN" : A.rename.kind == 3 ? "RENAME GROUP" : "RENAME CLIP");
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
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) { A.pop.open = A.ctx.open = A.blendDD.open = A.projectMenu = A.rename.open = A.openDialog = A.helpOpen = false; }
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

struct Script { int kind; float x0, y0, x1, y1; };

int main(int argc, char** argv) {
  std::vector<Script> script;
  bool openOut = false; std::string outShot;
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
    else if (a == "--outshot" && i + 1 < argc) { openOut = true; outShot = argv[++i]; }
    else if (a == "--scale" && i + 1 < argc) { A.prefs.scale = atoi(argv[++i]); cliScale = A.prefs.scale; scaleGiven = true; }
    else if (a == "--ctx") ctxTest = 1;
    else if ((a == "--click" || a == "--rclick" || a == "--drag") && i + 1 < argc) {
      // scripted input for headless checks: x,y  (drag: x0,y0,x1,y1)
      Script s; s.kind = a == "--click" ? 0 : a == "--rclick" ? 1 : 2;
      sscanf(argv[++i], "%f,%f,%f,%f", &s.x0, &s.y0, &s.x1, &s.y1);
      script.push_back(s);
    }
    else if (a == "--fx" && i + 1 < argc) fxTest.push_back(atoi(argv[++i]));   // test aid: add FX kind N to the selected clip
    else if (a == "--cell" && i + 2 < argc) { sel = true; selLi = atoi(argv[++i]); selCi = atoi(argv[++i]); }
  }
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
  if (tab >= 0) A.tab = tab;
  if (page >= 0) A.mpage = page;
  if (openOut) OpenOutput(win, A.outMonitor);
  if (ctxTest) A.openCtx(ImVec2(500, 300), A.sliceMenu("screen1", "slice1"));
  if (sel) A.cue(selLi, selCi);
  for (int k : fxTest) if (k >= 0 && k < FX_COUNT) A.addFx(k);

  double last = glfwGetTime(); double progAcc = 0; int frame = 0;
  while (!glfwWindowShouldClose(win)) {
    glfwPollEvents();
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
          AdvanceClip(A.layers[sc.first].clips[sc.second], (float)dt);
    (void)progAcc;
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
    if (!shot.empty()) {
      // each scripted action occupies 6 frames starting at frame 3
      int rel = frame - 3;
      if (rel >= 0 && rel / 6 < (int)script.size()) {
        const Script& s = script[rel / 6]; int st = rel % 6;
        io.AddMousePosEvent((st < 3 ? s.x0 : s.x1) / Zs, (st < 3 ? s.y0 : s.y1) / Zs);
        int btn = s.kind == 1 ? 1 : 0;
        if (st == 1) io.AddMouseButtonEvent(btn, true);
        if (s.kind == 2 && st == 3) io.AddMousePosEvent(s.x1 / Zs, s.y1 / Zs);
        if (st == 4 || (s.kind != 2 && st == 2)) io.AddMouseButtonEvent(btn, false);
      }
    }
    ImGui::NewFrame();
    {
      static Prefs lastPrefs = A.prefs; static int lastMon = A.outMonitor; static std::string lastTitle;
      if (io.MouseDown[0] || io.MouseDown[1] || io.MouseDown[2] || io.MouseWheel != 0.f || io.MouseWheelH != 0.f || io.InputQueueCharacters.Size > 0 ||
          ImGui::IsKeyPressed(ImGuiKey_Delete, false) || ImGui::IsKeyPressed(ImGuiKey_Backspace, false) || ImGui::IsKeyPressed(ImGuiKey_L, false)) UndoNote();
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
    g.blocked = A.pop.open || A.blendDD.open || A.ctx.open || A.layerMenu.open || A.colMenu.open || A.settingsOpen || A.rename.open || A.openDialog || A.helpOpen ||
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
    if (ImGui::IsKeyPressed(ImGuiKey_F11, false)) ToggleOutput(win, A.outMonitor);
    ImGui::End();

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












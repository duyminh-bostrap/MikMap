// F2 / I1: the projector output — a separate borderless window that shows only the warped slices,
// with no editing overlay. The control window keeps the whole UI; this one is what the audience sees.
#include <cctype>
#include "app.h"

#include <GLFW/glfw3.h>
#include "imgui_impl_opengl3.h"

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

#include <cmath>

using namespace ui;

#include "stb_image_write.h"

// F17: one projector window per screen that is routed to a physical display. Outputs are switched on and off together (F11 / the
// Screen panel's button); while on, the windows follow the screens every frame — a screen moved to another display moves its window,
// a screen set to a virtual device loses it. One display shows one screen: the first screen in the list that asks for it.
struct OutWin { std::string sc; int mon = -1; GLFWwindow* w = nullptr; };
static std::vector<OutWin> gOuts;
static bool gOutputsOn = false;
static GLFWwindow* gMain = nullptr;
static std::string gCapture;   // one-shot screenshot of the (first) projector window (headless checks)

void SetOutputCapture(const char* path) { gCapture = path ? path : ""; }
bool OutputOpen() { return gOutputsOn; }
int OutputWindowCount() { return (int)gOuts.size(); }
int OutputMonitorOf(const std::string& screenId) { for (auto& o : gOuts) if (o.sc == screenId) return o.mon; return -1; }

bool OutputKeyCloses(int key, int action, int mods) {
  if (action != GLFW_PRESS) return false;
  if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_F11) return true;
  return key == GLFW_KEY_W && (mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER)) != 0;
}
// An output window is a bare GLFW window (no ImGui input), so it gets its own key callback: closing is just flagging it,
// RenderOutput() then turns the outputs off on the next frame like the window's own close button.
static void OutputKeyCb(GLFWwindow* w, int key, int, int action, int mods) {
  if (OutputKeyCloses(key, action, mods)) glfwSetWindowShouldClose(w, GLFW_TRUE);
}

bool OutputKeyWiringOk() {
  if (gOuts.empty() || !gOuts[0].w) return false;
  GLFWwindow* w = gOuts[0].w;
  GLFWkeyfun cb = glfwSetKeyCallback(w, OutputKeyCb);   // returns the callback that was installed when the window was made
  if (!cb) return false;
  cb(w, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  bool flagged = glfwWindowShouldClose(w) != 0;
  glfwSetWindowShouldClose(w, GLFW_FALSE);
  return flagged;
}

static GLFWwindow* MakeOutWindow(int monitorIdx, bool first) {   // borderless, covering that display
  int count = 0;
  GLFWmonitor** mons = glfwGetMonitors(&count);
  if (monitorIdx < 0 || monitorIdx >= count) return nullptr;
  GLFWmonitor* mon = mons[monitorIdx];
  const GLFWvidmode* vm = glfwGetVideoMode(mon);
  int mx = 0, my = 0;
  glfwGetMonitorPos(mon, &mx, &my);
  glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
  glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
  GLFWwindow* w = glfwCreateWindow(vm->width, vm->height, "MikMap Output", nullptr, gMain);
  glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
  glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
  if (!w) return nullptr;
  glfwSetWindowPos(w, mx, my);
  glfwSetKeyCallback(w, OutputKeyCb);
  glfwMakeContextCurrent(w);
  glfwSwapInterval(first ? 1 : 0);   // only one window waits for vsync, or every extra display would halve the frame rate
  if (gMain) glfwMakeContextCurrent(gMain);
  return w;
}
static void SyncOutputWindows() {
  std::vector<std::pair<std::string, int>> want;   // screen -> display, each display once
  if (gOutputsOn) for (auto& sc : A.screens) {
    int m = DeviceMonitor(sc.outDev);
    bool taken = false; for (auto& w : want) if (w.second == m) taken = true;
    if (m >= 0 && !taken) want.push_back({sc.id, m});
  }
  for (size_t i = 0; i < gOuts.size();) {   // windows no longer wanted, or wanted on another display
    bool keep = false; for (auto& w : want) if (w.first == gOuts[i].sc && w.second == gOuts[i].mon) keep = true;
    if (keep) { ++i; continue; }
    glfwDestroyWindow(gOuts[i].w); gOuts.erase(gOuts.begin() + i);
  }
  for (auto& w : want) {
    bool have = false; for (auto& o : gOuts) if (o.sc == w.first) have = true;
    if (have) continue;
    if (GLFWwindow* win = MakeOutWindow(w.second, gOuts.empty())) gOuts.push_back({w.first, w.second, win});
  }
  if (gMain) glfwMakeContextCurrent(gMain);
}

void CloseOutput() {
  gOutputsOn = false;
  for (auto& o : gOuts) glfwDestroyWindow(o.w);
  gOuts.clear();
  if (gMain) glfwMakeContextCurrent(gMain);
}

// Switches the outputs on: every screen routed to a physical display gets its window (monitorIdx is kept for old callers: unused).
void OpenOutput(GLFWwindow* share, int monitorIdx) {
  (void)monitorIdx;
  gMain = share;
  gOutputsOn = true;
  SyncOutputWindows();
}

void ToggleOutput(GLFWwindow* share, int monitorIdx) {
  if (gOutputsOn) CloseOutput(); else OpenOutput(share, monitorIdx);
}

int MonitorCount() { int c = 0; glfwGetMonitors(&c); return c; }
// A screen's Output device is either one of the physical displays (its stored name is MonitorName(i), or the older "Display N ..."
// form) or a virtual output. A physical display fixes the screen's resolution; only a virtual output lets it be typed in.
const char* const kVirtualDevices[3] = {"NDI Output", "Spout Output", "Virtual Output"};
bool IsVirtualDevice(const std::string& dev) { return dev.rfind("NDI", 0) == 0 || dev.rfind("Spout", 0) == 0 || dev.rfind("Virtual", 0) == 0; }
int DeviceMonitor(const std::string& dev) {
  if (dev.empty() || IsVirtualDevice(dev)) return -1;
  int idx = -1, n = 0;
  size_t at = dev.find("Display ");
  if (at != std::string::npos && std::sscanf(dev.c_str() + at + 8, "%d", &n) == 1) idx = n - 1;   // "Display 2 (HDMI ...)", "Projector / Display 2"
  else if (std::isdigit((unsigned char)dev[0]) && std::sscanf(dev.c_str(), "%d:", &n) == 1) idx = n - 1;   // "2: LG (1920x1080@60)"
  return idx >= 0 && idx < MonitorCount() ? idx : -1;
}
bool DeviceResolution(const std::string& dev, int& w, int& h) {
  int i = DeviceMonitor(dev); if (i < 0) return false;
  int c = 0; GLFWmonitor** m = glfwGetMonitors(&c); if (i >= c) return false;
  const GLFWvidmode* vm = glfwGetVideoMode(m[i]); if (!vm) return false;
  w = vm->width; h = vm->height; return true;
}
void SyncScreenResolutions() { for (auto& sc : A.screens) { int w, h; if (DeviceResolution(sc.outDev, w, h)) { sc.w = w; sc.h = h; } } }
std::string MonitorName(int i) {
  int c = 0;
  GLFWmonitor** m = glfwGetMonitors(&c);
  if (i < 0 || i >= c) return "—";
  const GLFWvidmode* vm = glfwGetVideoMode(m[i]);
  const char* n = glfwGetMonitorName(m[i]);
  char b[128];
  snprintf(b, sizeof b, "%d: %s (%dx%d@%d)", i + 1, n ? n : "Display", vm->width, vm->height, vm->refreshRate);
  return b;
}

// One slice as the projector shows it: input rect -> keystone/mesh, cut by its input masks, Screen opacity + colour correction.
// (ox, oy, sx, sy) map the screen's 1920x1080 output space to window pixels — the projector window and the Output stage share this.
void DrawSliceOutput(const Screen& sc, const Slice& sl, float ox, float oy, float sx, float sy, float t) {
  WarpMap wm;
  wm.slice = &sl;
  wm.ox = ox; wm.oy = oy; wm.sx = sx; wm.sy = sy;
  // clip to the warped slice's bounding box (ImGui clipping is rectangular) — the whole mesh, not just the 4 corners,
  // or a mesh point bulging past the keystone quad gets cut off on the projector
  ImVec2 omn, omx; SliceOutputBounds(sl, omn, omx);
  ImRect bb(ox + omn.x * sx, oy + omn.y * sy, ox + omx.x * sx, oy + omx.y * sy);
  ImDrawList* dl = g.dl;
  const WarpMap* prevWarp = g.warp;
  const unsigned srcTex = SliceSourceTexture(sl, t);   // before any mask is armed: the source is drawn into its own texture
  dl->PushClipRect(bb.Min, bb.Max, true);
  g.warp = &wm;
  {
    // Input masks (canvas-space polygons) cut the picture BEFORE it is warped: map each point through the same input-rect ->
    // keystone/mesh map as the content, then let the stencil limit everything this slice draws.
    std::vector<std::vector<ImVec2>> keep, holes;
    // Each mask outline (straight or Bezier) is cut into short pieces in canvas px before mapping, so its edges bend with the mesh.
    std::vector<std::vector<ImVec2>> canvasOl;   // per visible mask, the dense canvas outline (the feather band uses it too)
    std::vector<const Mask*> olMask;
    for (auto& mk : sl.masks) {
      if (mk.pts.size() < 3 || !mk.visible) continue;   // a hidden mask (eye off in the tree) does not cut
      std::vector<ImVec2> ol = MaskOutline(mk), dense;
      for (size_t i = 0; i < ol.size(); ++i) {
        ImVec2 a = ol[i], b = ol[(i + 1) % ol.size()];
        int n = std::clamp((int)(std::hypot(b.x - a.x, b.y - a.y) / 24.f), 1, 64);
        for (int j = 0; j < n; ++j) dense.push_back(ImVec2(a.x + (b.x - a.x) * j / n, a.y + (b.y - a.y) * j / n));
      }
      std::vector<ImVec2> pp; pp.reserve(dense.size());
      for (auto& p : dense) pp.push_back(wm.Map(p.x, p.y));
      (mk.inverted ? holes : keep).push_back(std::move(pp));
      canvasOl.push_back(std::move(dense)); olMask.push_back(&mk);
    }
    // The picture is limited to the slice's own outline (quad or mesh border) — the input rect is a crop, so nothing may spill
    // outside the warped shape — and then to the masks.
    std::vector<ImVec2> outline = SliceOutline(sl);
    for (auto& p : outline) p = ImVec2(ox + p.x * sx, oy + p.y * sy);
    MaskBegin(outline, keep, holes);
    if (sl.blackBg) { std::vector<ImVec2> o = SliceOutline(sl); for (auto& p : o) p = ImVec2(ox + p.x * sx, oy + p.y * sy); g.dl->AddConcavePolyFilled(o.data(), (int)o.size(), IM_COL32(0, 0, 0, 255)); }   // Black BG: opaque black behind the picture
    const float op = std::clamp(sc.opacity / 100.f, 0.f, 1.f);   // Screen > Opacity scales the slice
    if (srcTex) DrawSliceTextured(sl, srcTex, ox, oy, sx, sy, op);   // F22: composition, or the layer / group routed here, sampled through the warp
    else { g.warp = &wm; DrawSliceSource(sl, bb, t, op); }          // no texture (no FBO support): draw the vector art through the warp map
    // Screen > Brightness / Contrast / Red / Green / Blue over this slice's outline (inside the mask, so cut-out areas stay dark)
    if (sl.colorActive()) {   // the slice's own colour correction, then the Screen's on top
      std::vector<ImVec2> o = SliceOutline(sl);
      for (auto& p : o) p = ImVec2(ox + p.x * sx, oy + p.y * sy);
      DrawColorAdjust(o.data(), (int)o.size(), sl.contrast / 100.f, sl.brightness / 100.f, sl.red / 100.f, sl.green / 100.f, sl.blue / 100.f);
    }
    if (sc.brightness || sc.contrast || sc.red || sc.green || sc.blue) {
      std::vector<ImVec2> o = SliceOutline(sl);
      for (auto& p : o) p = ImVec2(ox + p.x * sx, oy + p.y * sy);
      DrawColorAdjust(o.data(), (int)o.size(), sc.contrast / 100.f, sc.brightness / 100.f, sc.red / 100.f, sc.green / 100.f, sc.blue / 100.f);
    }
    // Feather: a band along each mask edge on the visible side, black at the edge fading to clear over `feather` canvas px — the picture
    // fades into the cut instead of stopping hard. Drawn in canvas px and mapped through the warp like everything else.
    for (size_t mi = 0; mi < canvasOl.size(); ++mi) {
      const Mask& mk = *olMask[mi]; const auto& ol = canvasOl[mi];
      const int n = (int)ol.size();
      if (mk.feather <= 0 || n < 3) continue;
      double area2 = 0; for (int i = 0; i < n; ++i) area2 += (double)ol[i].x * ol[(i + 1) % n].y - (double)ol[(i + 1) % n].x * ol[i].y;
      // inward normal of an edge d is sign(area) * (-d.y, d.x); a keep-mask fades inside its edge, a hole fades outside it
      const float side = (area2 > 0 ? 1.f : -1.f) * (mk.inverted ? -1.f : 1.f), f = (float)mk.feather;
      std::vector<ImVec2> inner(n);
      for (int i = 0; i < n; ++i) {
        ImVec2 a = ol[(i + n - 1) % n], p = ol[i], b = ol[(i + 1) % n];
        ImVec2 d0(p.x - a.x, p.y - a.y), d1(b.x - p.x, b.y - p.y);
        float l0 = std::max(1e-4f, std::hypot(d0.x, d0.y)), l1 = std::max(1e-4f, std::hypot(d1.x, d1.y));
        ImVec2 n0(-d0.y / l0, d0.x / l0), n1(-d1.y / l1, d1.x / l1), nm(n0.x + n1.x, n0.y + n1.y);
        float lm = std::hypot(nm.x, nm.y); if (lm < 1e-4f) nm = n1, lm = 1.f;
        nm = ImVec2(nm.x / lm, nm.y / lm);
        float c = std::max(0.35f, nm.x * n1.x + nm.y * n1.y);   // miter, capped at sharp corners
        inner[i] = ImVec2(p.x + nm.x * side * f / c, p.y + nm.y * side * f / c);
      }
      const ImU32 edge = IM_COL32(0, 0, 0, 255), clear = IM_COL32(0, 0, 0, 0);
      const ImVec2 uv = ImGui::GetDrawListSharedData()->TexUvWhitePixel;
      for (int i = 0; i < n; ++i) {
        int j = (i + 1) % n;
        ImVec2 q[4] = {wm.Map(ol[i].x, ol[i].y), wm.Map(ol[j].x, ol[j].y), wm.Map(inner[j].x, inner[j].y), wm.Map(inner[i].x, inner[i].y)};
        ImU32 cl[4] = {edge, edge, clear, clear};
        dl->PrimReserve(6, 4);
        ImDrawIdx base = (ImDrawIdx)dl->_VtxCurrentIdx;
        for (int k = 0; k < 4; ++k) dl->PrimWriteVtx(q[k], uv, cl[k]);
        dl->PrimWriteIdx(base); dl->PrimWriteIdx((ImDrawIdx)(base + 1)); dl->PrimWriteIdx((ImDrawIdx)(base + 2));
        dl->PrimWriteIdx(base); dl->PrimWriteIdx((ImDrawIdx)(base + 2)); dl->PrimWriteIdx((ImDrawIdx)(base + 3));
      }
    }
    MaskEnd();
  }
  g.warp = prevWarp;
  dl->PopClipRect();
}

// Renders every open output window with its own screen. Call once per frame, after the UI frame.
static void RenderOneOutput(OutWin& ow, bool capture);
void RenderOutput() {
  if (!gOutputsOn) return;
  for (auto& o : gOuts) if (glfwWindowShouldClose(o.w)) { CloseOutput(); return; }   // Esc / F11 / Ctrl+W on any output: all off
  SyncOutputWindows();
  if (gOuts.empty()) return;
  // the slice sources are drawn into textures in the MAIN context (framebuffers are per context); the projectors then only sample them
  const float t = (float)g.time * 1.2f;
  for (auto& o : gOuts) for (auto& ps : A.screens) if (ps.id == o.sc) for (auto& sl : ps.slices) if (sl.visible) SliceSourceTexture(sl, t);
  SliceSourcesRenderable(false);
  for (size_t i = 0; i < gOuts.size(); ++i) RenderOneOutput(gOuts[i], i == 0);
  if (gMain) glfwMakeContextCurrent(gMain);
  SliceSourcesRenderable(true);
}

static void RenderOneOutput(OutWin& ow, bool capture) {
  GLFWwindow* gOut = ow.w;
  int fw = 0, fh = 0;
  glfwGetFramebufferSize(gOut, &fw, &fh);
  if (fw <= 0 || fh <= 0) return;
  glfwMakeContextCurrent(gOut);
  glViewport(0, 0, fw, fh);
  glClearColor(0, 0, 0, 1);
  glClear(GL_COLOR_BUFFER_BIT);

  ImDrawList dl(ImGui::GetDrawListSharedData());
  dl._ResetForNewFrame();
  dl.PushTexture(ImGui::GetIO().Fonts->TexRef);
  dl.PushClipRect(ImVec2(0, 0), ImVec2((float)fw, (float)fh), false);

  ImDrawList* prevDl = g.dl;
  float prevAlpha = g.alpha;
  g.dl = &dl;
  g.alpha = 1.f;

  Screen* sc = nullptr; for (auto& ps : A.screens) if (ps.id == ow.sc) sc = &ps;
  if (sc && sc->visible && !A.blackout) {
    // the screen's 1920x1080 output space stretches to fill the projector
    float sx = fw / 1920.f, sy = fh / 1080.f;
    float t = (float)g.time * 1.2f;
    bool anySolo = false;
    for (auto& sl : sc->slices) if (sl.solo && sl.visible) anySolo = true;
    for (auto& sl : sc->slices) {
      if (!sl.visible || (anySolo && !sl.solo)) continue;
      DrawSliceOutput(*sc, sl, 0.f, 0.f, sx, sy, t);
    }
  }

  // G13: sensor touches through H_s, drawn straight in output space (only the first screen: that is what was calibrated)
  if (A.sensorOverlay && sc && !A.screens.empty() && sc == &A.screens[0] && !A.touchPts.empty()) {
    float H[9], rms; if (!FitHomography(A.calib, H, &rms)) FitAffine(H, &rms);
    float sx = fw / 1920.f, sy = fh / 1080.f, ph = std::fmod((float)g.time * 1.4f, 1.f);
    for (auto& t : A.touchPts) {
      float ox, oy; ApplyH(H, t.x, t.y, ox, oy);
      ImVec2 p(ox * sx, oy * sy);
      dl.AddCircle(p, (12 + 44 * ph) * sx, IM_COL32(60, 255, 190, (int)(180 * (1.f - ph))), 32, 2.f);
      dl.AddCircleFilled(p, 8 * sx, IM_COL32(60, 255, 190, 255), 20);
    }
  }

  g.dl = prevDl;
  g.alpha = prevAlpha;
  dl.PopClipRect();
  dl.PopTexture();

  ImDrawData dd;
  dd.Valid = true;
  dd.CmdListsCount = 0;
  dd.TotalVtxCount = dd.TotalIdxCount = 0;
  dd.DisplayPos = ImVec2(0, 0);
  dd.DisplaySize = ImVec2((float)fw, (float)fh);
  dd.FramebufferScale = ImVec2(1, 1);
  dd.OwnerViewport = nullptr;
  dd.Textures = nullptr;   // the main window already uploads the atlas this frame
  dd.AddDrawList(&dl);
  ImGui_ImplOpenGL3_RenderDrawData(&dd);

  if (capture && !gCapture.empty()) {
    std::vector<unsigned char> px((size_t)fw * fh * 4), flip((size_t)fw * fh * 4);
    glReadPixels(0, 0, fw, fh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < fh; ++y) memcpy(&flip[(size_t)y * fw * 4], &px[(size_t)(fh - 1 - y) * fw * 4], (size_t)fw * 4);
    for (size_t i = 3; i < flip.size(); i += 4) flip[i] = 255;
    stbi_write_png(gCapture.c_str(), fw, fh, 4, flip.data(), fw * 4);
    gCapture.clear();
  }
  glfwSwapBuffers(gOut);
}

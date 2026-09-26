// F2 / I1: the projector output — a separate borderless window that shows only the warped slices,
// with no editing overlay. The control window keeps the whole UI; this one is what the audience sees.
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

static GLFWwindow* gOut = nullptr;
static GLFWwindow* gMain = nullptr;
static std::string gCapture;   // one-shot screenshot of the projector window (headless checks)

void SetOutputCapture(const char* path) { gCapture = path ? path : ""; }
bool OutputOpen() { return gOut != nullptr; }

bool OutputKeyCloses(int key, int action, int mods) {
  if (action != GLFW_PRESS) return false;
  if (key == GLFW_KEY_ESCAPE || key == GLFW_KEY_F11) return true;
  return key == GLFW_KEY_W && (mods & (GLFW_MOD_CONTROL | GLFW_MOD_SUPER)) != 0;
}
// The output window is a bare GLFW window (no ImGui input), so it gets its own key callback: closing is just flagging it,
// RenderOutput() then tears it down on the next frame like the window's own close button.
static void OutputKeyCb(GLFWwindow* w, int key, int, int action, int mods) {
  if (OutputKeyCloses(key, action, mods)) glfwSetWindowShouldClose(w, GLFW_TRUE);
}

bool OutputKeyWiringOk() {
  if (!gOut) return false;
  GLFWkeyfun cb = glfwSetKeyCallback(gOut, OutputKeyCb);   // returns the callback that was installed by OpenOutput
  if (!cb) return false;
  cb(gOut, GLFW_KEY_ESCAPE, 0, GLFW_PRESS, 0);
  bool flagged = glfwWindowShouldClose(gOut) != 0;
  glfwSetWindowShouldClose(gOut, GLFW_FALSE);
  return flagged;
}

void CloseOutput() {
  if (!gOut) return;
  GLFWwindow* w = gOut;
  gOut = nullptr;
  glfwDestroyWindow(w);
  if (gMain) glfwMakeContextCurrent(gMain);
}

// Opens borderless on the given monitor (index into glfwGetMonitors). Falls back to a window on the primary.
void OpenOutput(GLFWwindow* share, int monitorIdx) {
  gMain = share;
  CloseOutput();
  int count = 0;
  GLFWmonitor** mons = glfwGetMonitors(&count);
  if (count <= 0) return;
  GLFWmonitor* mon = mons[std::clamp(monitorIdx, 0, count - 1)];
  const GLFWvidmode* vm = glfwGetVideoMode(mon);
  int mx = 0, my = 0;
  glfwGetMonitorPos(mon, &mx, &my);
  glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
  glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_FALSE);
  glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
  gOut = glfwCreateWindow(vm->width, vm->height, "MikMap Output", nullptr, share);
  glfwWindowHint(GLFW_DECORATED, GLFW_TRUE);
  glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);
  if (!gOut) return;
  glfwSetWindowPos(gOut, mx, my);
  glfwSetKeyCallback(gOut, OutputKeyCb);
  glfwMakeContextCurrent(gOut);
  glfwSwapInterval(1);
  if (gMain) glfwMakeContextCurrent(gMain);
}

void ToggleOutput(GLFWwindow* share, int monitorIdx) {
  if (gOut) CloseOutput(); else OpenOutput(share, monitorIdx);
}

int MonitorCount() { int c = 0; glfwGetMonitors(&c); return c; }
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
  dl->PushClipRect(bb.Min, bb.Max, true);
  g.warp = &wm;
  {
    // Input masks (canvas-space polygons) cut the picture BEFORE it is warped: map each point through the same input-rect ->
    // keystone/mesh map as the content, then let the stencil limit everything this slice draws.
    std::vector<std::vector<ImVec2>> keep, holes;
    for (auto& mk : sl.masks) {
      if (mk.pts.size() < 3) continue;
      std::vector<ImVec2> pp; pp.reserve(mk.pts.size());
      for (auto& p : mk.pts) pp.push_back(wm.Map(p.x, p.y));
      (mk.inverted ? holes : keep).push_back(std::move(pp));
    }
    const bool masked = !keep.empty() || !holes.empty();
    if (masked) MaskBegin(keep, holes, bb);
    DrawSliceSource(sl, bb, t, std::clamp(sc.opacity / 100.f, 0.f, 1.f));   // F22: composition, or just the layer/group this slice is routed to; Screen > Opacity scales it
    // Screen > Brightness / Contrast / Red / Green / Blue over this slice's outline (inside the mask, so cut-out areas stay dark)
    if (sc.brightness || sc.contrast || sc.red || sc.green || sc.blue) {
      std::vector<ImVec2> o = SliceOutline(sl);
      for (auto& p : o) p = ImVec2(ox + p.x * sx, oy + p.y * sy);
      DrawColorAdjust(o.data(), (int)o.size(), sc.contrast / 100.f, sc.brightness / 100.f, sc.red / 100.f, sc.green / 100.f, sc.blue / 100.f);
    }
    if (masked) MaskEnd();
  }
  g.warp = prevWarp;
  dl->PopClipRect();
}

// Renders the current screen's slices into the output window. Call once per frame, after the UI frame.
void RenderOutput() {
  if (!gOut) return;
  if (glfwWindowShouldClose(gOut)) { CloseOutput(); return; }

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

  Screen* sc = A.curScreen();
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

  if (!gCapture.empty()) {
    std::vector<unsigned char> px((size_t)fw * fh * 4), flip((size_t)fw * fh * 4);
    glReadPixels(0, 0, fw, fh, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
    for (int y = 0; y < fh; ++y) memcpy(&flip[(size_t)y * fw * 4], &px[(size_t)(fh - 1 - y) * fw * 4], (size_t)fw * 4);
    for (size_t i = 3; i < flip.size(); i += 4) flip[i] = 255;
    stbi_write_png(gCapture.c_str(), fw, fh, 4, flip.data(), fw * 4);
    gCapture.clear();
  }
  glfwSwapBuffers(gOut);
  if (gMain) glfwMakeContextCurrent(gMain);
}

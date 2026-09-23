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

// ── test card drawn straight onto the output (F14) ──
static void OutputTestCard(ImRect r) {
  ImDrawList* dl = g.dl;
  float W = r.GetWidth(), H = r.GetHeight();
  for (int c = 0; c <= 16; ++c) { float x = r.Min.x + W * c / 16.f; dl->AddLine(ImVec2(x, r.Min.y), ImVec2(x, r.Max.y), Ca(K(0xffffff, 0.35f))); }
  for (int y0 = 0; y0 <= 9; ++y0) { float y = r.Min.y + H * y0 / 9.f; dl->AddLine(ImVec2(r.Min.x, y), ImVec2(r.Max.x, y), Ca(K(0xffffff, 0.35f))); }
  ImVec2 c((r.Min.x + r.Max.x) * 0.5f, (r.Min.y + r.Max.y) * 0.5f);
  dl->AddCircle(c, std::min(W, H) * 0.25f, Ca(K(pal::coral)), 64, 2.f);
  dl->AddLine(ImVec2(c.x - 30, c.y), ImVec2(c.x + 30, c.y), Ca(K(pal::coral)), 2.f);
  dl->AddLine(ImVec2(c.x, c.y - 30), ImVec2(c.x, c.y + 30), Ca(K(pal::coral)), 2.f);
  dl->AddLine(r.Min, r.Max, Ca(K(pal::cyan, 0.4f)), 1.5f);
  dl->AddLine(ImVec2(r.Max.x, r.Min.y), ImVec2(r.Min.x, r.Max.y), Ca(K(pal::cyan, 0.4f)), 1.5f);
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
      WarpMap wm;
      wm.slice = &sl;
      wm.ox = 0; wm.oy = 0; wm.sx = sx; wm.sy = sy;
      // clip to the warped slice's bounding box (ImGui clipping is rectangular) — the whole mesh, not just the 4 corners,
      // or a mesh point bulging past the keystone quad gets cut off on the projector
      ImVec2 omn, omx; SliceOutputBounds(sl, omn, omx);
      ImRect bb(omn.x * sx, omn.y * sy, omx.x * sx, omx.y * sy);
      dl.PushClipRect(bb.Min, bb.Max, true);
      g.warp = &wm;
      if (A.testCard) OutputTestCard(bb);
      else DrawSliceSource(sl, bb, t, 1.f);   // F22: composition, or just the layer/group this slice is routed to
      g.warp = nullptr;
      dl.PopClipRect();
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

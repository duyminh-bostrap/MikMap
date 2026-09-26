// Composition screen: browser · monitors · properties · layer/clip deck.
#include "app.h"
#include <numeric>
#include <array>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <ctime>
#include <filesystem>

using namespace ui;


float gTestInspScroll = 0;
// ───────────────────────── scroll helper ─────────────────────────
bool ScrollArea::Begin(const char* id, ImRect r, bool horizontal) {
  prev = g.dl;
  ImGui::SetCursorScreenPos(r.Min);
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
  ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.f);
  ImGui::PushStyleColor(ImGuiCol_ChildBg, 0);
  bool vis = ImGui::BeginChild(id, r.GetSize(), ImGuiChildFlags_None,
                               ImGuiWindowFlags_NoBackground | (horizontal ? ImGuiWindowFlags_HorizontalScrollbar : 0));
  ImGui::PopStyleColor();
  ImGui::PopStyleVar(2);
  g.dl = ImGui::GetWindowDrawList();
  origin = ImGui::GetCursorScreenPos();
  return vis;
}
void ScrollArea::End(float cw, float ch) {
  ImGui::SetCursorScreenPos(origin);
  ImGui::Dummy(ImVec2(cw, ch));
  ImGui::EndChild();
  g.dl = prev;
}

// ───────────────────────── model helpers ─────────────────────────
static Clip C(const char* n, const char* d) { Clip c; c.st = Clip::Loaded; c.name = n; c.dur = d; return c; }
static Clip E() { return Clip(); }
static Clip S(Clip::St st, const char* n, const char* d, const char* m = "") {
  Clip c; c.st = st; c.name = n; c.dur = d;
  for (int i = 0; i < 4; ++i) if (m && strcmp(m, PlayModeName(i)) == 0) c.playMode = i;
  return c;
}
static Clip Ar() { Clip c; c.st = Clip::Armed; return c; }

static Layer L(const char* name, const char* group, const char* blend, float bt, float op, float au, bool live, bool byp, std::vector<Clip> clips) {
  Layer l; l.name = name; l.group = group; l.blend = blend; l.blendTime = bt; l.opacity = op; l.audio = au; l.live = live; l.bypassed = byp;
  l.clips = std::move(clips);
  return l;
}

void EnsureLayerIds(std::vector<Layer>& layers) {
  // first holder of an id keeps it (so slice routing survives); later duplicates and blanks get a fresh one
  auto takenBefore = [&](const std::string& id, size_t i) { for (size_t k = 0; k < i; ++k) if (layers[k].id == id) return true; return false; };
  auto takenAnywhere = [&](const std::string& id) { for (auto& l : layers) if (l.id == id) return true; return false; };
  int n = 1;
  for (size_t i = 0; i < layers.size(); ++i) {
    if (!layers[i].id.empty() && !takenBefore(layers[i].id, i)) continue;
    std::string id;
    do id = "layer-" + std::to_string(n++); while (takenAnywhere(id));
    layers[i].id = id;
  }
}

static Fx MkFx(int kind);
void App::init() {
  groups = {{"g1", "Stage Left", 2, true, 2, 0}, {"g2", "Audio React", 1, true, 1, 0}};
  layers = {
      L("Layer 3", "", "Add", 0.5f, 82, 64, true, false,
        {C("Cyber Hex Grid", "8s"), S(Clip::Selected, "Plasma Waves 01", "12s"), S(Clip::Live, "Particle Vortex", "10s"),
         S(Clip::Loaded, "Strobe Tunnel", "6s", "BOUN"), C("Neon Rain", "18s"), Ar(), E(), E()}),
      L("Mask A", "g1", "Screen", 1.2f, 100, 0, false, false,
        {C("Geometric Wire", "16s"), C("Liquid Chrome", "14s"), Ar(), E(), C("Chrome Fold", "9s"), E(), E(), E()}),
      L("Wire Overlay", "g1", "Add", 0.8f, 72, 12, false, false,
        {C("Grid Warp", "20s"), Ar(), E(), E(), C("Mesh Pulse", "11s"), E(), E(), E()}),
      L("Spectrum", "g2", "Add", 0.f, 88, 96, true, false,
        {S(Clip::Live, "Spectrum Bars", "\xE2\x88\x9E", "HOLD"), E(), S(Clip::Loaded, "Beat Pulse", "\xE2\x88\x9E", "HOLD"), E(),
         S(Clip::Loaded, "Kick Flash", "\xE2\x88\x9E", "HOLD"), E(), E(), E()}),
      L("Backdrop", "", "Normal", 2.f, 100, 0, false, true,
        {C("Deep Ambient", "24s"), E(), E(), E(), C("Slow Drift", "40s"), E(), E(), E()}),
  };
  EnsureLayerIds(layers);
  layers[0].clips[2].fx = {MkFx(1), MkFx(2)};
  layers[3].clips[0].fx = {MkFx(4), MkFx(5)};
  // mapping
  auto quad = [](Slice& s, float x, float y, float w, float h) {
    s.q[0] = {x, y}; s.q[1] = {x + w, y}; s.q[2] = {x + w, y + h}; s.q[3] = {x, y + h};
  };
  auto mask = [](const char* id, const char* n, bool inv, int f, ImVec2 a, ImVec2 b, ImVec2 c, ImVec2 d) {
    Mask m; m.id = id; m.name = n; m.inverted = inv; m.feather = f; MaskFromPolygon(m, {a, b, c, d}); return m;
  };
  auto slice = [&](const char* id, const char* n, int warp, int ix, int iy, int iw, int ih) {
    Slice s; s.id = id; s.name = n; s.ix = ix; s.iy = iy; s.iw = iw; s.ih = ih; quad(s, (float)ix, (float)iy, (float)iw, (float)ih);
    if (warp) { s.meshCols = 4; s.meshRows = 3; }   // the demo's one subdivided slice (3 x 2 subdivisions)
    return s;
  };
  Screen s1; s1.id = "screen1"; s1.name = "Main Projector 1"; s1.outDev = "Display 1 (HDMI 1920x1080@60Hz)"; s1.w = 1920; s1.h = 1080; s1.role = 0;
  Slice a = slice("slice1", "Center Stage Wall", 0, 100, 50, 1720, 980);
  a.masks.push_back(mask("mask1", "DJ Console Cutout", true, 8, {720, 660}, {1210, 660}, {1140, 950}, {790, 950}));
  Slice b = slice("slice2", "Left Pillar Accent", 0, 60, 120, 420, 820);
  b.q[0] = {80, 140}; b.q[1] = {470, 110}; b.q[2] = {500, 960}; b.q[3] = {60, 930};
  s1.slices = {a, b};
  Screen s2; s2.id = "screen2"; s2.name = "LED Wall Upstage"; s2.outDev = "Art-Net \xC2\xB7 Universe 2"; s2.w = 3840; s2.h = 256; s2.edgeBlend = true; s2.role = 1;
  Slice c = slice("slice3", "Panel Row A", 1, 0, 0, 1920, 240); quad(c, 120, 180, 1680, 260);
  Slice d = slice("slice4", "Panel Row B", 0, 0, 260, 1920, 240); quad(d, 120, 520, 1680, 260);
  d.masks.push_back(mask("mask2", "Truss Shadow", false, 14, {700, 560}, {1000, 560}, {1000, 760}, {700, 760}));
  s2.slices = {c, d};
  Screen s3; s3.id = "screen3"; s3.name = "NDI Broadcast"; s3.outDev = "NDI Output 1"; s3.w = 1280; s3.h = 720; s3.role = 2;
  Slice e = slice("slice5", "Program Feed", 0, 0, 0, 1920, 1080); quad(e, 80, 80, 1760, 920);
  s3.slices = {e};
  screens = {s1, s2, s3};

  devices = {{"dev1", "LiDAR \xC2\xB7 Stage Front", "udp://192.168.1.40:2368", true, 40, 12, "184.2k"},
             {"dev2", "Depth Cam \xC2\xB7 Kinect v2", "usb://kinect-v2/0", true, 30, 22, "96.1k"},
             {"dev3", "OSC Bridge", "osc://0.0.0.0:9000/mikmap", false, 0, 0, "0"}};
  routes = {{"r1", "touch.down", "Layer 3 \xC2\xB7 Particle Vortex", true},
            {"r2", "touch.x (normalised)", "FX \xC2\xB7 Kaleido angle", true},
            {"r3", "blob.count", "Master \xC2\xB7 Opacity", false},
            {"r4", "touch.velocity", "Audio React \xC2\xB7 Gain", true}};
  calib = {{100, 100, -0.65f, -0.55f}, {1820, 100, 0.65f, -0.55f}, {1820, 980, 0.75f, 0.65f}, {100, 980, -0.75f, 0.65f}};
  roi[0] = {-0.7f, -0.6f}; roi[1] = {0.7f, -0.6f}; roi[2] = {0.85f, 0.7f}; roi[3] = {-0.85f, 0.7f};
  decks = {{"Deck A", groups, layers, colNames, activeCol}};
  curDeckIdx = 0;
}

void App::setTopProgress(float pct) {
  pct = std::clamp(pct, 0.f, 100.f);
  int best = -1, bc = 0;
  for (auto& c : selectedCells) if (c.first >= 0 && c.first < (int)layers.size() && c.second >= 0 && c.second < (int)layers[c.first].clips.size()
      && layers[c.first].clips[c.second].st != Clip::Empty && (best < 0 || c.first < best)) { best = c.first; bc = c.second; }
  if (best < 0 && selLi < (int)layers.size() && selCi < (int)layers[selLi].clips.size()) { best = selLi; bc = selCi; }
  if (best >= 0) layers[best].clips[bc].progress = pct;
}
void App::cue(int li, int ci) {
  for (auto& l : layers) for (auto& c : l.clips) {
    if (c.st == Clip::Selected) c.st = Clip::Loaded;
    else if (c.st == Clip::LiveSel) c.st = Clip::Live;
  }
  Clip& cell = layers[li].clips[ci];
  selLi = li; selCi = ci; selLayer = li; selectedCells = {{li, ci}};
  // Armed is a decorative "looks empty" state (Ar() in the demo fixture) — ClipCell/trigger()/fireColumn() all
  // treat it as empty, so cue() must too. Missing this let a body-click cue an Armed cell into Selected, and the
  // immediately-following trigger() then saw a non-empty/non-Armed state and played the (nameless, content-less)
  // clip for real — "clicking an empty-looking cell creates a new clip".
  if (cell.st == Clip::Empty || cell.st == Clip::Armed) return;
  cell.st = cell.st == Clip::Live ? Clip::LiveSel : Clip::Selected;
}
static void StartDissolve(Layer& l, int toCi) {
  if (l.blendTime <= 0.f) return;
  for (int k = 0; k < (int)l.clips.size(); ++k)
    if (k != toCi && l.clips[k].isLive()) { l.fadeFrom = l.clips[k]; l.fadeT = 0.f; return; }
}
void App::flushPending() {
  if (pending.empty()) return;
  std::vector<PendingTrig> p; p.swap(pending);
  flushing = true;
  for (auto& t : p) {
    if (t.column) { if (t.ci >= 0 && t.ci < colCount()) fireColumn(t.ci); }
    else if (t.li >= 0 && t.li < (int)layers.size() && t.ci >= 0 && t.ci < (int)layers[t.li].clips.size()) trigger(t.li, t.ci);
  }
  flushing = false;
}
void App::trigger(int li, int ci) {
  Layer& l = layers[li];
  if (ci < 0 || ci >= (int)l.clips.size()) return;
  if (quantize && playing && !flushing) {   // Sync: queue it (play or stop); the main loop fires it on the next beat (a repeat just replaces the earlier one)
    for (auto& q : pending) if (!q.column && q.li == li) { q.ci = ci; return; }
    pending.push_back({li, ci, false});
    return;
  }
  // An empty (or armed) slot means "nothing here": stop whatever this layer was playing instead of silently leaving
  // it running. Clicking an empty cell is how a layer gets blacked out, same as a lighting console cue with a dark
  // channel. (No cross-dissolve here — DrawComposite skips a layer with nothing live, so a "fade to nothing" would
  // need its own code path; this is a hard cut for now.)
  if (l.clips[ci].st == Clip::Empty || l.clips[ci].st == Clip::Armed) {
    for (auto& c : l.clips) if (c.isLive()) c.st = Clip::Loaded;
    l.live = false;
    activeCol = ci;
    return;
  }
  StartDissolve(l, ci);
  for (auto& c : l.clips) if (c.isLive()) c.st = Clip::Loaded;
  l.clips[ci].st = Clip::Live;
  l.clips[ci].paused = false;   // triggering a clip resumes it
  l.live = true;
  selLi = li; selCi = ci; selLayer = li; activeCol = ci; selectedCells = {{li, ci}};
}
// A6: fire column ci — plays every non-empty clip there and, symmetrically, STOPS any layer whose slot in this
// column is empty/armed (a column recall reflects exactly what is in it, not a mix of new content and old leftovers).
void App::fireColumn(int ci) {
  if (quantize && playing && !flushing) {
    for (auto& q : pending) if (q.column) { q.ci = ci; return; }
    pending.push_back({0, ci, true});
    return;
  }
  for (int li = 0; li < (int)layers.size(); ++li) {
    Layer& l = layers[li];
    if (ci >= (int)l.clips.size() || l.clips[ci].st == Clip::Empty || l.clips[ci].st == Clip::Armed) {
      for (auto& c : l.clips) if (c.isLive()) c.st = Clip::Loaded;
      l.live = false;
      continue;
    }
    StartDissolve(l, ci);
    for (auto& c : l.clips) if (c.isLive()) c.st = Clip::Loaded;
    l.clips[ci].st = Clip::Live;
    l.clips[ci].paused = false;
    l.live = true;
  }
  selMode = 2;
  activeCol = ci;
  for (auto& g : groups) g.activeCol = ci;   // column select also selects every group's cue, so the Cue N highlight follows the column
  selectedCells.clear();
  for (int li = 0; li < (int)layers.size(); ++li) selectedCells.push_back({li, ci});
  // Preview Cue (Monitor reads selLi/selCi directly) follows the topmost layer that actually has a clip here,
  // same "layer 0 draws on top" convention as DrawComposite. selCi always moves to the fired column — even an
  // all-empty one — so DrawClipContent's own empty check (it draws nothing for Clip::Empty/Armed) makes Preview
  // Cue go blank; leaving selCi unchanged here used to keep the PREVIOUS column's clip animating in Preview
  // after firing an empty column, which read as "the preview is still running" even though nothing was cued.
  selCi = ci;
  for (int li = 0; li < (int)layers.size(); ++li)
    if (ci < (int)layers[li].clips.size() && layers[li].clips[ci].st != Clip::Empty && layers[li].clips[ci].st != Clip::Armed) {
      selLi = li; selLayer = li;
      break;
    }
}

void App::selectColumn(int ci) {
  selMode = 2;
  for (auto& l : layers) for (auto& c : l.clips) {
    if (c.st == Clip::Selected) c.st = Clip::Loaded; else if (c.st == Clip::LiveSel) c.st = Clip::Live;
  }
  activeCol = ci;
  selectedCells.clear();
  for (int li = 0; li < (int)layers.size(); ++li) selectedCells.push_back({li, ci});
}
void App::stepSel(int dir) {
  if (selMode == 0) { selLayer = std::clamp(selLayer + dir, 0, (int)layers.size() - 1); tab = 1; return; }
  int cols = layers.empty() ? 1 : (int)layers[0].clips.size();
  activeCol = std::clamp(activeCol + dir, 0, cols - 1);
  selectedCells.clear();
  for (int li = 0; li < (int)layers.size(); ++li) selectedCells.push_back({li, activeCol});
}
// Timeline transport ⏮/⏭: unlike stepSel() (pure navigation, used by the ←/→ shortcuts to browse without
// interrupting playback), these buttons are media-transport controls — moving to a column also plays it,
// same as clicking that column's header (respects Sync/quantize via fireColumn, stops layers empty there).
void App::stepFireColumn(int dir) {
  int cols = colCount();
  fireColumn(std::clamp(activeCol + dir, 0, cols - 1));
}
void App::moveClip(int fl, int fc, int tl, int tc) {
  if (fl < 0 || (fl == tl && fc == tc)) return;
  Clip src = layers[fl].clips[fc], dst = layers[tl].clips[tc];
  if (src.st == Clip::Empty) return;
  layers[tl].clips[tc] = src;
  layers[fl].clips[fc] = dst.st != Clip::Empty ? dst : Clip();
  for (auto& l : layers) { l.live = false; for (auto& c : l.clips) if (c.isLive()) l.live = true; }
  selLi = tl; selCi = tc; selectedCells = {{tl, tc}}; selMode = 1;
}
void App::selectGroupCue(const std::string& gid, int ci) {
  // Mirrors cue()'s per-clip state flip (Loaded<->Selected, Live<->LiveSel), just applied to every layer in the
  // group at once — a plain cue(li,ci) call per layer would work too, but it also resets selectedCells on each
  // call, so the last layer would be the only one left selected once the loop finished.
  if (Group* g = group(gid)) g->activeCol = ci;
  selectedCells.clear();
  int firstLi = -1;
  for (int li = 0; li < (int)layers.size(); ++li) {
    if (layers[li].group != gid) continue;
    Layer& l = layers[li];
    for (auto& c : l.clips) {
      if (c.st == Clip::Selected) c.st = Clip::Loaded;
      else if (c.st == Clip::LiveSel) c.st = Clip::Live;
    }
    if (ci >= 0 && ci < (int)l.clips.size()) {
      Clip& cell = l.clips[ci];
      if (cell.st != Clip::Empty && cell.st != Clip::Armed) cell.st = cell.st == Clip::Live ? Clip::LiveSel : Clip::Selected;
    }
    selectedCells.push_back({li, ci});
    if (firstLi < 0) firstLi = li;
  }
  if (firstLi >= 0) { selLi = firstLi; selCi = ci; selLayer = firstLi; tab = 2; }   // Properties shows the group's own clips, same as cueing a single one
}

// ───────────────────────── FX library / column / browser helpers ─────────────────────────
// tone: 0 live, 1 audio, 2 preview, 3 standby
const FxDef FX_LIB[8] = {
    {"Blur", "droplet", 2, 1, {"Radius", nullptr}, {38, 0}, nullptr, {nullptr, nullptr, nullptr}},
    {"RGB Shift", "sliders-horizontal", 0, 2, {"Offset", "Angle"}, {26, 20}, nullptr, {nullptr, nullptr, nullptr}},
    {"Kaleido", "snowflake", 2, 2, {"Segments", "Rotate"}, {44, 30}, nullptr, {nullptr, nullptr, nullptr}},
    {"Mirror", "flip-horizontal", 3, 0, {nullptr, nullptr}, {0, 0}, "Axis", {"H", "V", "QUAD"}},
    {"Strobe", "zap", 3, 2, {"Rate", "Duty"}, {40, 50}, nullptr, {nullptr, nullptr, nullptr}},
    {"Trails", "repeat", 0, 2, {"Decay", "Feedback zoom"}, {60, 28}, nullptr, {nullptr, nullptr, nullptr}},
    {"Pixelate", "grid-3x3", 2, 1, {"Cell size", nullptr}, {34, 0}, nullptr, {nullptr, nullptr, nullptr}},
    {"Hue Shift", "palette", 1, 2, {"Hue", "Saturation"}, {20, 50}, nullptr, {nullptr, nullptr, nullptr}}};

static uint32_t FxTone(int t) { return t == 0 ? pal::coral : t == 1 ? pal::mint : t == 2 ? pal::cyan : pal::yellow; }
static std::string FxFmt(int kind, int pi, float v) {
  char b[32];
  switch (kind) {
    case 0: snprintf(b, sizeof b, "%.1f px", v * 0.24f); break;
    case 1: if (pi == 0) snprintf(b, sizeof b, "%.1f px", v * 0.25f); else snprintf(b, sizeof b, "%d\xC2\xB0", (int)std::round(v * 3.6f)); break;
    case 2: if (pi == 0) snprintf(b, sizeof b, "%d seg", std::max(3, (int)std::round(3 + v * 0.09f))); else snprintf(b, sizeof b, "%d\xC2\xB0", (int)std::round(v * 3.6f)); break;
    case 4: if (pi == 0) snprintf(b, sizeof b, "%.1f hz", 0.5f + v * 0.12f); else snprintf(b, sizeof b, "%d %%", (int)std::round(10 + v * 0.6f)); break;
    case 5: if (pi == 0) snprintf(b, sizeof b, "%d %%", (int)std::round(v)); else snprintf(b, sizeof b, "%.3f\xC3\x97", 1 + v * 0.004f); break;
    case 6: snprintf(b, sizeof b, "%d px", std::max(2, (int)std::round(2 + v * 0.5f))); break;
    case 7: if (pi == 0) snprintf(b, sizeof b, "%d\xC2\xB0", (int)std::round(v * 3.6f)); else snprintf(b, sizeof b, "%d %%", (int)std::round(v * 2)); break;
    default: b[0] = 0;
  }
  return b;
}
static Fx MkFx(int kind) {
  Fx f; f.kind = kind; f.p[0] = FX_LIB[kind].def[0]; f.p[1] = FX_LIB[kind].def[1]; return f;
}
void App::addFx(int kind) { auto& c = fxChain(); c.push_back(MkFx(kind)); fxSel = (int)c.size() - 1; tab = 2; }
void App::removeFx(int i) { auto& c = fxChain(); if (i >= 0 && i < (int)c.size()) c.erase(c.begin() + i); fxSel = std::max(0, i - 1); }
void App::dupFx(int i) { auto& c = fxChain(); if (i >= 0 && i < (int)c.size()) { Fx f = c[i]; c.insert(c.begin() + i + 1, f); fxSel = i + 1; } }
void App::moveFx(int i, int d) { auto& c = fxChain(); int j = i + d; if (i < 0 || j < 0 || j >= (int)c.size()) return; std::swap(c[i], c[j]); fxSel = j; }
void App::resetFx(int i) { auto& c = fxChain(); if (i >= 0 && i < (int)c.size()) { bool on = c[i].on; c[i] = MkFx(c[i].kind); c[i].on = on; } }
void App::insertCol(int at) {
  int n = colCount();
  colNames.resize(n);
  int k = n + 1;
  auto has = [&](const std::string& s) { for (auto& x : colNames) if (x == s) return true; return false; };
  while (has("Column " + std::to_string(k))) ++k;
  colNames.insert(colNames.begin() + std::clamp(at, 0, n), "Column " + std::to_string(k));
  for (auto& l : layers) l.clips.insert(l.clips.begin() + std::clamp(at, 0, (int)l.clips.size()), Clip());
  activeCol = std::min(activeCol, colCount() - 1);
}
void App::deleteCol(int ci) {
  if (colCount() < 2) return;
  colNames.resize(colCount());
  colNames.erase(colNames.begin() + ci);
  for (auto& l : layers) { l.clips.erase(l.clips.begin() + ci); l.live = false; for (auto& c : l.clips) if (c.isLive()) l.live = true; }
  activeCol = std::clamp(activeCol, 0, colCount() - 1);
  std::vector<std::pair<int, int>> ns;
  for (auto& c : selectedCells) if (c.second != ci) ns.push_back({c.first, c.second > ci ? c.second - 1 : c.second});
  selectedCells = ns;
}
void App::beginRename(int kind, int idx, ImVec2 pos, const std::string& cur) {
  rename.open = true; rename.fresh = true; rename.kind = kind; rename.idx = idx; rename.pos = pos;
  snprintf(rename.buf, sizeof rename.buf, "%s", cur.c_str());
}
void App::commitRename(const char* text) {
  std::string t = text; size_t a = t.find_first_not_of(" \t"), b = t.find_last_not_of(" \t");
  if (a == std::string::npos) return;   // empty name: keep the old one
  t = t.substr(a, b - a + 1);
  if (rename.kind == 0 && rename.idx >= 0 && rename.idx < (int)layers.size()) layers[rename.idx].name = t;
  else if (rename.kind == 1 && rename.idx >= 0 && rename.idx < colCount()) { colNames.resize(colCount()); colNames[rename.idx] = t; }
  else if (rename.kind == 3 && rename.idx >= 0 && rename.idx < (int)groups.size()) groups[rename.idx].name = t;
  else if (rename.kind == 4 && rename.idx >= 0 && rename.idx < (int)decks.size()) decks[rename.idx].name = t;
  else if (rename.kind == 2) {   // clip: idx = layer * 1000 + column
    int li = rename.idx / 1000, ci = rename.idx % 1000;
    if (li >= 0 && li < (int)layers.size() && ci < (int)layers[li].clips.size() && layers[li].clips[ci].st != Clip::Empty) {
      Clip& c = layers[li].clips[ci];
      if (c.style < 0) c.style = ClipStyleOf(c.name);   // pin the look before the name (its source of truth) changes
      c.name = t;
    }
  }
}
void App::moveColTo(int from, int to) {
  int n = colCount();
  if (from == to || from < 0 || to < 0 || to >= n) return;
  auto remap = [&](int i) { return i == from ? to : (from < to ? (i > from && i <= to ? i - 1 : i) : (i >= to && i < from ? i + 1 : i)); };
  colNames.resize(n);
  std::string nm = colNames[from]; colNames.erase(colNames.begin() + from); colNames.insert(colNames.begin() + to, nm);
  for (auto& l : layers) { Clip c = l.clips[from]; l.clips.erase(l.clips.begin() + from); l.clips.insert(l.clips.begin() + to, c); }
  activeCol = remap(activeCol);
  for (auto& c : selectedCells) c.second = remap(c.second);
}
void App::loadClip(int li, int ci, const std::string& name, const std::string& dur, const std::string& media) {
  Clip c; c.st = Clip::Loaded; c.name = name; c.media = media; if (MediaKindOf(media) == MEDIA_IMAGE) PreloadMedia(media); c.dur = dur.empty() ? "\xE2\x88\x9E" : dur;
  layers[li].clips[ci] = c;
  selLayer = li; selLi = li; selCi = ci; selectedCells = {{li, ci}}; selMode = 1;
}
void App::addLayer() {
  Layer nl; nl.name = "Layer " + std::to_string(layers.size() + 1); nl.blend = "Normal"; nl.blendTime = 0; nl.opacity = 100;
  nl.clips.assign(colCount() > 0 ? colCount() : 8, Clip());   // as many cells as the deck has columns
  layers.push_back(nl);
  EnsureLayerIds(layers);
}
void App::setLayerColor(int li, int color) {
  if (li < 0 || li >= (int)layers.size()) return;
  color = std::clamp(color, 0, 5);
  Layer& l = layers[li];
  int old = l.color;
  if (old == color) return;
  for (auto& c : l.clips) if (c.color == old) c.color = color;   // a clip the user coloured differently keeps its own colour
  l.color = color;
}
void App::groupSelectedLayer() {
  if (layers.empty()) return;
  int li = std::clamp(selLayer, 0, (int)layers.size() - 1);
  Group ng; ng.id = uid("g"); ng.name = "Group " + std::to_string(groups.size() + 1); ng.role = 2; ng.open = true;
  groups.push_back(ng);
  layers[li].group = ng.id;
}
void App::toggleSync() {
  quantize = !quantize; pending.clear();
  notify(quantize ? "Sync on â triggers wait for the next beat" : "Sync off â triggers fire immediately", 2.5);
}
// Files dropped from the OS onto a clip cell: the first goes into that cell, each further file into the next
// empty cell of the same layer (so dropping a folder's worth of stills fills a row); files with no free cell
// still land in the Browser. Everything dropped is also remembered in the Browser's Media list.
void App::dropFilesOnCell(int li, int ci, const std::vector<std::string>& paths) {
  if (li < 0 || li >= (int)layers.size()) return;
  std::vector<std::string> ok;
  for (auto& p : paths) if (MediaKindOf(p) != MEDIA_NONE) ok.push_back(p);
  if (ok.empty()) { notify("Unsupported file type \xE2\x80\x94 drop an image, video or audio file"); return; }
  ImportMedia(ok);
  int placed = 0, c = ci;
  for (auto& p : ok) {
    while (c < (int)layers[li].clips.size() && placed > 0 && layers[li].clips[c].st != Clip::Empty && layers[li].clips[c].st != Clip::Armed) ++c;
    if (c >= (int)layers[li].clips.size()) break;
    loadClip(li, c, std::filesystem::path(p).filename().string(), "", p);
    ++placed; ++c;
  }
  MediaKind k = MediaKindOf(ok[0]);
  std::string msg = "Loaded " + std::to_string(placed) + (placed == 1 ? " clip" : " clips");
  if (k != MEDIA_IMAGE && placed) msg += std::string(" \xE2\x80\x94 ") + (k == MEDIA_VIDEO ? "video" : "audio") + " playback isn't available yet";
  notify(msg);
}

// ───────────────────────── multi-deck ─────────────────────────
static void CloseDeckPopups(App& A) { A.pop.open = A.layerMenu.open = A.colMenu.open = A.ctx.open = A.blendDD.open = A.rename.open = A.deckMenu.open = false; }
static void ResetDeckSelection(App& A) { A.selLi = A.selCi = A.selLayer = 0; A.selectedCells.clear(); A.selMode = 2; }

void App::switchDeck(int idx) {
  if (idx < 0 || idx >= (int)decks.size() || idx == curDeckIdx) return;
  decks[curDeckIdx] = {decks[curDeckIdx].name, groups, layers, colNames, activeCol};
  curDeckIdx = idx;
  Deck& d = decks[curDeckIdx];
  groups = d.groups; layers = d.layers; colNames = d.colNames; activeCol = d.activeCol;
  ResetDeckSelection(*this);   // a selection index from the old deck means nothing on one with different layers/columns
  CloseDeckPopups(*this);
}

void App::addDeck() {
  decks[curDeckIdx] = {decks[curDeckIdx].name, groups, layers, colNames, activeCol};
  static const char* letters = "BCDEFGHIJKLMNOPQRSTUVWXYZ";
  size_t li = decks.size() - 1;
  Deck d; d.name = "Deck " + std::string(1, li < 25 ? letters[li] : 'X');
  for (int i = 0; i < 3; ++i) { Layer l; l.name = "Layer " + std::to_string(i + 1); l.blend = "Normal"; l.opacity = 100; l.clips.assign(8, Clip()); d.layers.push_back(l); }
  EnsureLayerIds(d.layers);
  decks.push_back(d);
  curDeckIdx = (int)decks.size() - 1;
  groups = d.groups; layers = d.layers; colNames = d.colNames; activeCol = d.activeCol;
  ResetDeckSelection(*this);
  CloseDeckPopups(*this);
}

void App::duplicateDeck(int idx) {
  if (idx < 0 || idx >= (int)decks.size()) return;
  if (idx == curDeckIdx) decks[curDeckIdx] = {decks[curDeckIdx].name, groups, layers, colNames, activeCol};
  Deck copy = decks[idx]; copy.name += " copy";
  decks.insert(decks.begin() + idx + 1, copy);
  if (idx + 1 <= curDeckIdx) ++curDeckIdx;   // keep pointing at the same (now-shifted) deck
}

void App::deleteDeck(int idx) {
  if (idx < 0 || idx >= (int)decks.size() || decks.size() < 2) return;
  bool wasCurrent = idx == curDeckIdx;
  decks.erase(decks.begin() + idx);
  if (wasCurrent) {
    curDeckIdx = std::clamp(idx, 0, (int)decks.size() - 1);
    Deck& d = decks[curDeckIdx];
    groups = d.groups; layers = d.layers; colNames = d.colNames; activeCol = d.activeCol;
    ResetDeckSelection(*this);
  } else if (idx < curDeckIdx) {
    --curDeckIdx;
  }
}

void App::moveDeckTo(int from, int to) {
  int n = (int)decks.size();
  if (from == to || from < 0 || from >= n || to < 0 || to >= n) return;
  decks[curDeckIdx] = {decks[curDeckIdx].name, groups, layers, colNames, activeCol};   // flush live edits before reshuffling the vector
  Deck d = decks[from]; decks.erase(decks.begin() + from); decks.insert(decks.begin() + to, d);
  auto remap = [&](int i) { return i == from ? to : (from < to ? (i > from && i <= to ? i - 1 : i) : (i >= to && i < from ? i + 1 : i)); };
  curDeckIdx = remap(curDeckIdx);
  Deck& cd = decks[curDeckIdx];   // same content either way, but re-point in case "current" is now at a different index
  groups = cd.groups; layers = cd.layers; colNames = cd.colNames; activeCol = cd.activeCol;
}

// ───────────────────────── Timeline run mode ─────────────────────────
// Read-only re-projection of the SAME grid clips: each layer's non-empty clips play back-to-back, in column
// order, each sized by its own real duration (ClipSeconds), looping over one shared 0..100 playhead. There is
// no separate "timeline block" — tlSync() below just flips the grid's own Clip::st, so Grid mode shows exactly
// what Timeline mode had playing if you switch back mid-show.
std::vector<std::vector<App::TlLayoutBlock>> App::tlLayout() const {
  std::vector<std::vector<TlLayoutBlock>> out;
  for (auto& l : layers) {
    std::vector<TlLayoutBlock> row;
    std::vector<std::pair<int, float>> durs;
    float total = 0;
    for (int ci = 0; ci < (int)l.clips.size(); ++ci) {
      const Clip& c = l.clips[ci];
      if (c.st == Clip::Empty || c.st == Clip::Armed) continue;
      float s = ClipSeconds(c);
      durs.push_back({ci, s});
      total += s;
    }
    if (total > 0.f) {
      float acc = 0;
      for (auto& [ci, s] : durs) { float start = acc / total * 100.f; acc += s; row.push_back({ci, start, acc / total * 100.f}); }
    }
    out.push_back(std::move(row));
  }
  return out;
}

void App::tlSync(float pct) {
  auto lay = tlLayout();
  for (int li = 0; li < (int)layers.size(); ++li) {
    Layer& l = layers[li];
    int hitCi = -1;
    for (auto& b : lay[li]) if (pct >= b.start && pct < b.end) { hitCi = b.ci; break; }
    bool changed = false;
    for (int ci = 0; ci < (int)l.clips.size(); ++ci) {
      Clip& c = l.clips[ci];
      if (c.st == Clip::Empty || c.st == Clip::Armed) continue;
      bool wantLive = ci == hitCi, isLive = c.st == Clip::Live || c.st == Clip::LiveSel;
      if (wantLive == isLive) continue;
      changed = true;
      bool sel = c.st == Clip::Selected || c.st == Clip::LiveSel;
      c.st = wantLive ? (sel ? Clip::LiveSel : Clip::Live) : (sel ? Clip::Selected : Clip::Loaded);
    }
    if (changed) { l.live = false; for (auto& c : l.clips) if (c.isLive()) l.live = true; }
  }
}
// ───────────────────────── clip cell ─────────────────────────
const uint32_t CLIP_COLORS[6] = {0xff7f50, 0x118ab2, 0x06d6a0, 0xffd166, 0xef4444, 0xb388ff};
const char* CLIP_COLOR_NAMES[6] = {"Amber", "Cyan", "Mint", "Yellow", "Red", "Violet"};

// The cell has two independently clickable zones: `bar` (name strip, top) only selects/cues and arms a
// possible move-drag; `body` (gradient area, bottom) triggers the clip immediately and never starts a drag.
struct CellOut { bool hover = false, barPress = false, barRelease = false, barRclick = false, bodyClick = false; };

static uint32_t Dark45(uint32_t c) {  // color-mix(in srgb, c 45%, #000)
  return (((c >> 16) & 255) * 45 / 100) << 16 | (((c >> 8) & 255) * 45 / 100) << 8 | ((c & 255) * 45 / 100);
}

static void DashedRect(ImRect r, ImU32 c, float th) {
  ImVec2 p[4] = {{r.Min.x, r.Min.y}, {r.Max.x, r.Min.y}, {r.Max.x, r.Max.y}, {r.Min.x, r.Max.y}};
  DashedPoly(p, 4, c, th, 5, 3);
}

// compact = the layer is collapsed (or its group is): a short cell that shows just the name strip + play mode/duration,
// with no thumbnail (nothing to see in a 16px sliver, and it spares the per-frame thumbnail budget).
static CellOut ClipCell(ImRect r, Clip& cl, bool selectedCell, bool dragged, bool dropT, float progress, bool compact = false) {
  bool empty = cl.st == Clip::Empty || cl.st == Clip::Armed;
  // Design tokens §3.2 give the clip cell THREE distinct looks, not "selected = coral / else = warm brown":
  // loaded-idle (warm amber, already at rest), cued-for-preview (cool cyan — about to show, not showing yet),
  // and live-on-output (hot burnt-orange). LiveSel (cued AND live at once) must read as live above all — an
  // operator mistaking "what's live" for "what's merely selected" is the one mistake this can't afford.
  bool live = cl.st == Clip::Live || cl.st == Clip::LiveSel;
  bool preview = cl.st == Clip::Selected;
  Hit h = HitR(r);
  uint32_t col = CLIP_COLORS[std::clamp(cl.color, 0, 5)];
  uint32_t ringHex = col;   // the selection ring is the clip's own colour, not a fixed cyan/coral
  float prevA = g.alpha; if (dragged) g.alpha *= 0.4f;

  // wrapper fill + glow
  ImU32 wrapFill = 0;
  if (empty) { if (selectedCell) wrapFill = K(ringHex, 0.12f); }
  else wrapFill = K(col, selectedCell ? 0.18f : 0.07f);
  Fill(r, K(pal::g12), 3);
  if (wrapFill) Fill(r, wrapFill, 3);

  ImU32 border = K(pal::g22), bar = K(pal::g1c), barFg = K(pal::te0), body = K(pal::clipLoadedBg), foot = K(pal::t88);
  bool barBold = false;
  // All three states are shades of the CLIP'S OWN colour (Clip color in the clip popover): not selected = a dark shade
  // (amber -> brown, cyan -> deep blue-black), selected/cued = the colour itself, live = the colour at full strength
  // with the glow. The default amber reproduces the old brown / orange look.
  auto mix = [&](uint32_t base, float t) { return MixHex(base, col, t); };
  // The name bar is as bright as the border in every state (selected/live: the colour itself, with dark or white text
  // chosen for contrast — white on yellow would be unreadable).
  const float lum = (0.299f * ((col >> 16) & 0xFF) + 0.587f * ((col >> 8) & 0xFF) + 0.114f * (col & 0xFF)) / 255.f;
  const ImU32 onCol = lum > 0.62f ? K(0x101010) : K(0xffffff);
  // Border and bar answer two different questions. BORDER = "this clip is playing" (live only). BAR = "this is the clip
  // selected / previewed". So in one layer: click a body -> that clip is live AND selected, border and bar both bright;
  // then preview another clip -> its bar lights up but its border stays plain, and the playing clip keeps its bright
  // border while its bar goes dark.
  const bool sel = selectedCell || preview;
  const ImU32 idleBorder = mix(0x1a1a1a, 0.30f), idleFg = mix(0xd0d0d0, 0.30f);
  if (live) {
    border = K(col); barBold = true; body = mix(0x080808, 0.22f); foot = mix(0xffffff, 0.30f);
    if (sel) { bar = K(col); barFg = onCol; } else { bar = idleBorder; barFg = idleFg; }
  } else if (!empty) {
    border = idleBorder;
    if (sel) { bar = K(col); barFg = onCol; body = mix(0x080808, 0.14f); foot = mix(0x777777, 0.5f); }
    else { bar = idleBorder; barFg = idleFg; body = mix(0x080808, 0.06f); foot = mix(0x555555, 0.45f); }
  }
  else if (h.hover) border = K(pal::g33);

  if (live) Glow(r, col, 0.20f, 6, 4);
  Fill(r, empty ? K(pal::g10) : body, 4);
  ImRect in = Inset(r, 1);
  float cx0 = in.Min.x, cx1 = in.Max.x;
  // Same 22px split used whether or not the cell has a clip, so the two hit-zones below stay predictable.
  ImRect barR(in.Min.x, in.Min.y, cx1, in.Min.y + 22), bodyR(cx0, in.Min.y + 22, cx1, in.Max.y);
  if (!empty) {
    g.dl->AddRectFilled(bodyR.Min, bodyR.Max, Ca(body), 3, ImDrawFlags_RoundCornersBottom);
    g.dl->PushClipRect(bodyR.Min, bodyR.Max, true);
    // A4: cached thumbnail. Drawing the generated art into every one of ~40 cells every frame cost
    // ~16 s/frame (S_STARS worst) — so each Clip gets its own small texture, redrawn into an FBO at most a
    // few times a second, budgeted to a handful of clips per frame (ResetThumbBudget in DrawDeck). Falls
    // back to the flat gradient until the first render lands, or if the FBO functions never loaded.
    // Only the cued/live clip actually animates; every other cell renders once and then freezes — a still
    // frame, not a second copy of the same motion playing out of sync in 40 places at once.
    if (!compact) {
      bool active = live || preview;
      bool stale = cl.thumbTex == 0 || (active && g.time - cl.thumbAt > 0.2);
      if (stale && ThumbBudgetLeft()) RenderClipThumbnail(cl);
      if (cl.thumbTex != 0) g.dl->AddImage((ImTextureID)(intptr_t)cl.thumbTex, bodyR.Min, bodyR.Max, ImVec2(0, 1), ImVec2(1, 0), Ca(IM_COL32_WHITE));
      else GradDiag(bodyR, col, Dark45(col), live ? 0.45f : 0.22f);
    }
    g.dl->PopClipRect();
    g.dl->AddRectFilled(barR.Min, ImVec2(cx1, in.Min.y + 22), Ca(bar), 3, ImDrawFlags_RoundCornersTop);
    TextEll(cx0 + 10, in.Min.y + 11, (cx1 - cx0) - 20, barBold ? UI_B : UI_S, 10, barFg, cl.name.c_str());
    float fy = in.Max.y - 16;
    if (selectedCell && progress > 0)
      g.dl->AddRectFilled(ImVec2(cx0, in.Max.y - 3), ImVec2(cx0 + (cx1 - cx0) * std::clamp(progress, 0.f, 100.f) / 100.f, in.Max.y), Ca(K(pal::coral)));
    std::string m = PlayModeName(cl.playMode);
    FontId ff = live ? MONO_B : MONO_M;
    Text(cx0 + 10, fy + 8, ff, 9, foot, m.c_str(), 0.1f);
    if (!cl.dur.empty()) TextR(cx1 - 10, fy + 8, ff, 9, foot, Upper(cl.dur).c_str(), 0.1f);
  } else {
    // An empty slot has the same two-part structure as a loaded clip — a bar and a body — but only as colour: no name, no
    // thumbnail, no footer. Selecting an empty cell then shows the same lit bar / tinted body as any clip instead of a lone ring.
    ImU32 eBar = sel ? K(col, 0.55f) : h.hover ? K(pal::g22) : K(pal::g1c);
    ImU32 eBody = sel ? mix(0x080808, 0.14f) : K(pal::g0f);
    g.dl->AddRectFilled(bodyR.Min, bodyR.Max, Ca(eBody), 3, ImDrawFlags_RoundCornersBottom);
    g.dl->AddRectFilled(barR.Min, ImVec2(cx1, in.Min.y + 22), Ca(eBar), 3, ImDrawFlags_RoundCornersTop);
  }
  Border(r, border, 4);
  // selection outlines from the wrapper
  if (dropT) DashedRect(Inset(r, 1), K(pal::yellow), 2);
  else if (live) Border(r, K(col), 4, 2);                                  // playing: thick bright border
  else if (selectedCell && empty) Border(r, K(ringHex, 0.6f), 4, 1);        // an empty selected slot still needs a marker
  else if (!empty) Border(r, K(col, 0.35f), 4);
  g.alpha = prevA;

  CellOut o;
  o.hover = h.hover;
  if (h.hover) {
    if (empty) {
      // An empty cell now draws a colour-only bar and body too, but there is still no name or clip behind either, so it stays
      // one hit zone: any click, on the bar or the body, stops/clears the layer (the "top edge only cues" surprise that made
      // us do this must not come back). Right-click still opens the popover (its items are already disabled/no-ops for an
      // empty slot where irrelevant).
      o.bodyClick = h.click;
      o.barRclick = h.rclick;
    } else {
      Hit bh = HitR(barR), bo = HitR(bodyR);
      if (bh.hover) { o.barPress = bh.click; o.barRelease = bh.release; o.barRclick = bh.rclick; }
      else if (bo.hover) o.bodyClick = bo.click;
    }
    if (!A.dragging) CursorHand();
  }
  return o;
}

// ───────────────────────── layer row ─────────────────────────
const char* BLEND_NAMES[8] = {"Normal", "Add", "Screen", "Multiply", "Overlay", "Difference", "Lighten", "Darken"};

static void LayerRow(ImRect r, int li, ScrollArea&) {
  Layer& l = A.layers[li];
  bool selected = A.selLayer == li, collapsed = l.collapsed;
  Group* grp = l.group.empty() ? nullptr : A.group(l.group);
  if (grp && !grp->open) collapsed = true;
  int indent = l.group.empty() ? 0 : 1;
  Hit row = HitR(r);

  float prevAlpha = g.alpha;
  if (l.bypassed) g.alpha = 0.45f;
  Fill(r, selected ? K(pal::g18) : K(pal::g12));
  HLine(r.Min.x, r.Max.x, r.Max.y - 1, K(pal::g2a));
  if (grp) VLine(r.Min.x, r.Min.y, r.Max.y - 1, K(RoleHex(grp->role)));
  const uint32_t acc = CLIP_COLORS[std::clamp(l.color, 0, 5)];   // Properties > Layer > Color
  if (selected) {
    for (int k = 3; k >= 1; --k) g.dl->AddRectFilled(ImVec2(r.Min.x - k, r.Min.y), ImVec2(r.Min.x + 3 + k, r.Max.y), Ca(K(acc, 0.08f)));
    Fill(Rc(r.Min.x, r.Min.y, 3, r.GetHeight()), K(acc));
  } else Fill(Rc(r.Min.x, r.Min.y, 3, r.GetHeight()), K(acc, 0.35f));   // faint, so a layer's colour reads even when it isn't selected
  float padL = 6 + indent * 8 + r.Min.x, padR = r.Max.x - 6;
  float top = r.Min.y + 6;
  float headCy = collapsed ? (r.Min.y + r.Max.y - 1) * 0.5f : top + 8;
  bool consumed = false;

  // chevron
  ImRect chev(padL, headCy - 7, padL + 14, headCy + 7);
  if (HitR(chev).click) { l.collapsed = !l.collapsed; consumed = true; }
  Icon(collapsed ? "chevron-right" : "chevron-down", ImVec2(chev.Min.x + 7, headCy), 10,
       l.bypassed ? K(pal::t66) : l.live ? K(acc) : K(pal::t88));

  // layer options button
  ImRect ob(padR - 16, headCy - 8, padR, headCy + 8);
  {
    Hit oh = HitR(ob);
    Box(ob, K(pal::g1c), oh.hover ? K(pal::g33) : K(pal::g22), 2);
    Icon("sliders-vertical", ImVec2((ob.Min.x + ob.Max.x) * 0.5f, headCy), 10, K(oh.hover ? pal::white : pal::t88));
    if (oh.hover) CursorHand();
    if (oh.click) { A.layerMenu.open = true; A.layerMenu.li = li; A.layerMenu.pos = ImVec2(std::min(ob.Min.x, ImGui::GetIO().DisplaySize.x - 190), ob.Max.y + 4); consumed = true; }
  }
  TextEll(chev.Max.x + 4, headCy, ob.Min.x - 6 - (chev.Max.x + 4), UI_B, 11, K(selected ? pal::white : pal::te0), l.name.c_str());

  if (!collapsed) {
    float y = top + 16 + 4;
    // clip name + S M B
    std::string cn;
    {
      bool found = false;
      for (auto& sc : A.selectedCells) if (sc.first == li) {
        const Clip& c = l.clips[sc.second];
        if (c.st != Clip::Empty && !c.name.empty()) { cn = c.name; found = true; }
        break;
      }
      if (!found) for (auto& c : l.clips) if (c.isLive()) { cn = c.name; break; }
    }
    float ry = y + 8;
    struct B { const char* lab; Tone t; bool* v; } bs[3] = {{"S", T_PREVIEW, &l.solo}, {"M", T_ALERT, &l.muted}, {"B", T_STANDBY, &l.bypassed}};
    float x = padR;
    for (int i = 2; i >= 0; --i) {
      x -= 16;
      if (ToggleBtn(Rc(x, ry - 8, 16, 16), bs[i].lab, bs[i].t, *bs[i].v)) { *bs[i].v = !*bs[i].v; consumed = true; }
      x -= 2;
    }
    if (!cn.empty()) TextEll(padL, ry, x - 2 - padL, MONO_M, 10, K(pal::clipLoadedText), cn.c_str());
    y += 16 + 4;
    auto sliderRow = [&](const char* lab, uint32_t hex, float& v, uint32_t id) {
      float cy2 = y + 5;
      TextC(padL + 5, cy2, MONO_B, 10, K(hex), lab);
      float valW = 26;
      ImRect tr(padL + 10 + 6, cy2 - 3, padR - valW - 6, cy2 + 3);
      if (Slider(id, tr, v, hex)) consumed = true;
      char b[16]; snprintf(b, sizeof b, "%d%%", (int)std::round(v));
      TextR(padR, cy2, MONO_M, 10, K(pal::tcc), b);
      y += 10 + 4;
    };
    sliderRow("V", acc, l.opacity, 0x1000 + li * 4);
    sliderRow("A", pal::mint, l.audio, 0x1001 + li * 4);
    float fy = y;
    float timeW = 38 + 6 + 8 + 10 + 2;
    ImRect sel(padL, fy, padR - timeW - 2, fy + 18);
    Hit sh = HitR(sel);
    Box(sel, sh.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 2);
    std::string bl = Upper(l.blend);
    TextEll(sel.Min.x + 4, fy + 9, sel.GetWidth() - 8, UI_S, 10, K(pal::tcc), bl.c_str(), 0.09f);
    if (sh.click) { A.blendDD.open = true; A.blendDD.layer = li; A.blendDD.anchor = sel; consumed = true; }
    ImRect tb(padR - timeW, fy, padR, fy + 18);
    Box(tb, K(pal::g1c), K(pal::g22), 2);
    {
      char buf[16]; snprintf(buf, sizeof buf, "%.1f", l.blendTime);
      static std::string edit;
      char idb[32]; snprintf(idb, sizeof idb, "##bt%d", li);
      ImGuiID gid = ImGui::GetID(idb);
      if (ImGui::GetActiveID() != gid) edit = buf;
      ImGui::SetCursorScreenPos(ImVec2(tb.Min.x + 4, std::floor(tb.Min.y + (tb.GetHeight() - TextPx(9) - 2) * 0.5f)));   // centred: font + 2x1px padding
      ImGui::PushFont(F(MONO_B), TextPx(9));
      ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
      ImGui::PushStyleColor(ImGuiCol_Text, K(pal::cyan));
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 1));
      ImGui::SetNextItemWidth(38);
      char eb[16]; snprintf(eb, sizeof eb, "%s", edit.c_str());
      if (ImGui::InputText(idb, eb, sizeof eb, ImGuiInputTextFlags_CharsDecimal)) { edit = eb; l.blendTime = std::max(0.f, (float)atof(eb)); }
      ImGui::PopStyleVar(); ImGui::PopStyleColor(2); ImGui::PopFont();
      if (ImGui::IsItemHovered() || ImGui::IsItemActive()) consumed = true;
      TextR(tb.Max.x - 6, fy + 9, MONO_M, 10, K(pal::t66), "s");
    }
  }
  g.alpha = prevAlpha;
  if (row.click && !consumed) { A.selLayer = li; A.tab = 1; A.selMode = 0; }
  if (row.dbl && !consumed) A.beginRename(0, li, ImVec2(r.Min.x + 20, r.Min.y + 4), l.name);
}
// ───────────────────────── monitors ─────────────────────────

// ───────────────────────── Preview Cue transform editor ─────────────────────────
// The Preview Cue monitor edits the previewed clip's transform directly: drag inside the frame to move, the small squares
// to scale (uniform — clips have one scale), the big corner circles to rotate. Right-click for the quick presets. It edits
// the very same Clip fields as Properties > Clip > Transform (and the live output, if that clip is live).
static float gPvFitW = 0.f;   // width of the canvas at Fit in the Preview Cue monitor (the zoom menu's percentages are relative to it)
static void ClipContentExtent(const Clip& c, float& w, float& h) {   // art units (the composite is 960 wide)
  w = 960.f; h = 540.f;
  int iw = 0, ih = 0;
  if (MediaKindOf(c.media) == MEDIA_IMAGE && MediaImageSize(c.media, iw, ih) && iw > 0 && ih > 0) {
    float s0 = std::min(960.f / iw, 540.f / ih); w = iw * s0; h = ih * s0;   // images are aspect-fitted into the canvas
  }
}

void PreviewTransformMenu(int li, int ci, ImVec2 at) {
  auto clipAt = [li, ci]() -> Clip* {
    if (li < 0 || li >= (int)A.layers.size() || ci < 0 || ci >= (int)A.layers[li].clips.size()) return nullptr;
    Clip& c = A.layers[li].clips[ci];
    return (c.st == Clip::Empty || c.st == Clip::Armed) ? nullptr : &c;
  };
  // fit the clip into a rectangle of the composite (art units): no rotation, uniform scale, centred there
  auto fitTo = [clipAt](float cx, float cy, float tw, float th) {
    if (Clip* c = clipAt()) {
      float w, h; ClipContentExtent(*c, w, h);
      c->rotation = 0; c->anchorX = c->anchorY = 0;
      c->scale = std::clamp(std::min(tw / w, th / h), 0.05f, 8.f); c->posX = cx; c->posY = cy;
    }
  };
  std::vector<MenuItem> mi;
  auto add = [&](const char* label, std::function<void()> fn, bool divider = false) {
    if (divider) { MenuItem d; d.label = ""; d.disabled = true; d.divider = true; mi.push_back(d); }
    MenuItem it; it.label = label; it.run = fn; mi.push_back(it);
  };
  add("Center X", [clipAt] { if (Clip* c = clipAt()) { float px, py; ClipEffectivePos(*c, 960.f, px, py); c->posX -= px; } });
  add("Center Y", [clipAt] { if (Clip* c = clipAt()) { float px, py; ClipEffectivePos(*c, 960.f, px, py); c->posY -= py; } });
  add("Mirror X", [clipAt] { if (Clip* c = clipAt()) c->flipH = !c->flipH; });
  add("Mirror Y", [clipAt] { if (Clip* c = clipAt()) c->flipV = !c->flipV; });
  add("Left Half", [fitTo] { fitTo(-240.f, 0.f, 480.f, 540.f); }, true);
  add("Top Half", [fitTo] { fitTo(0.f, -135.f, 960.f, 270.f); });
  add("Right Half", [fitTo] { fitTo(240.f, 0.f, 480.f, 540.f); });
  add("Bottom Half", [fitTo] { fitTo(0.f, 135.f, 960.f, 270.f); });
  add("Reset", [clipAt] { if (Clip* c = clipAt()) { c->posX = c->posY = c->rotation = c->anchorX = c->anchorY = 0; c->scale = 1; c->flipH = c->flipV = false; } }, true);
  A.openCtx(at, mi);
}

static void PreviewTransformEditor(ImRect well, ImRect cv, ImRect ctl, int li, int ci) {
  Clip& c = A.layers[li].clips[ci];
  if (c.st == Clip::Empty || c.st == Clip::Armed) return;
  if (A.pvHand) return;   // hand tool: left-drag pans the view instead of editing the clip
  ImGuiIO& io = ImGui::GetIO();
  const bool uiFree = !(A.ctx.open || A.pop.open || A.layerMenu.open || A.colMenu.open || A.deckMenu.open || A.blendDD.open ||
                        A.projectMenu || A.rename.open || A.settingsOpen || A.openDialog || A.helpOpen);
  float cw, ch; ClipContentExtent(c, cw, ch);
  float px, py; ClipEffectivePos(c, 960.f, px, py);
  const float k = cv.GetWidth() / 960.f;
  const ImVec2 C((cv.Min.x + cv.Max.x) * 0.5f, (cv.Min.y + cv.Max.y) * 0.5f);
  const float sc = std::max(0.01f, c.scale), rot = c.rotation * 3.14159265f / 180.f, cs = std::cos(rot), sn = std::sin(rot);
  auto toScreen = [&](float lx, float ly) { float x = lx * sc, y = ly * sc; return ImVec2(C.x + (x * cs - y * sn + px) * k, C.y + (x * sn + y * cs + py) * k); };
  ImVec2 corner[4] = {toScreen(-cw / 2, -ch / 2), toScreen(cw / 2, -ch / 2), toScreen(cw / 2, ch / 2), toScreen(-cw / 2, ch / 2)};
  ImVec2 mid[4] = {toScreen(0, -ch / 2), toScreen(cw / 2, 0), toScreen(0, ch / 2), toScreen(-cw / 2, 0)};
  const ImVec2 ctr = toScreen(0, 0);
  auto near = [](ImVec2 a, ImVec2 b, float r) { return std::hypot(a.x - b.x, a.y - b.y) <= r; };
  auto inQuad = [&](ImVec2 p) {   // convex quad: p on the same side of all four edges
    int pos = 0, neg = 0;
    for (int i = 0; i < 4; ++i) {
      ImVec2 a = corner[i], b = corner[(i + 1) % 4];
      float cr = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
      (cr >= 0 ? pos : neg)++;
    }
    return pos == 4 || neg == 4;
  };

  static int dk = 0;   // 1 rotate, 2 scale, 3 move
  static ImVec2 dm, dctr; static float dpx, dpy, dsc, drot, dang, ddist;
  const ImVec2 m = io.MousePos;
  const bool over = well.Contains(m) && uiFree && !ctl.Contains(m);
  int hk = 0;
  if (over) {
    for (int i = 0; i < 4 && !hk; ++i) if (near(m, corner[i], 6.f) || near(m, mid[i], 6.f)) hk = 2;   // small squares: scale
    for (int i = 0; i < 4 && !hk; ++i) if (near(m, corner[i], 14.f)) hk = 1;                         // ring around a corner: rotate
    if (!hk && inQuad(m)) hk = 3;
  }
  if (dk == 0 && hk && ImGui::IsMouseClicked(0)) {
    dk = hk; dm = m; dctr = ctr; dpx = c.posX; dpy = c.posY; dsc = c.scale; drot = c.rotation;
    dang = std::atan2(m.y - ctr.y, m.x - ctr.x); ddist = std::max(4.f, std::hypot(m.x - ctr.x, m.y - ctr.y));
  }
  if (dk) {
    if (ImGui::IsMouseDown(0)) {
      if (dk == 3) { c.posX = dpx + (m.x - dm.x) / k; c.posY = dpy + (m.y - dm.y) / k; }
      else if (dk == 2) c.scale = std::clamp(dsc * std::hypot(m.x - dctr.x, m.y - dctr.y) / ddist, 0.05f, 8.f);
      else {
        float r = drot + (std::atan2(m.y - dctr.y, m.x - dctr.x) - dang) * 180.f / 3.14159265f;
        c.rotation = r - 360.f * std::floor((r + 180.f) / 360.f);   // keep within -180..180
      }
      ImGui::SetMouseCursor(dk == 3 ? ImGuiMouseCursor_Hand : ImGuiMouseCursor_ResizeAll);
    } else dk = 0;
  } else if (hk) ImGui::SetMouseCursor(hk == 3 ? ImGuiMouseCursor_Hand : ImGuiMouseCursor_ResizeAll);
  if (over && dk == 0 && ImGui::IsMouseClicked(1)) PreviewTransformMenu(li, ci, m);

  // drawing: the frame only shows while the pointer is over the monitor (or a drag is under way)
  if (!over && dk == 0) return;
  g.dl->PushClipRect(well.Min, well.Max, true);
  const ImU32 line = Ca(K(pal::mint)), dark = Ca(K(0x0a0a0a));
  g.dl->AddPolyline(corner, 4, line, ImDrawFlags_Closed, 1.5f);
  for (int i = 0; i < 4; ++i) {
    for (const ImVec2& p : {corner[i], mid[i]}) {
      g.dl->AddRectFilled(ImVec2(p.x - 4, p.y - 4), ImVec2(p.x + 4, p.y + 4), dark);
      g.dl->AddRect(ImVec2(p.x - 4, p.y - 4), ImVec2(p.x + 4, p.y + 4), line, 0.f, 0, 1.5f);
    }
    bool hot = (dk == 1 || (!dk && hk == 1)) && near(m, corner[i], 14.f);
    g.dl->AddCircle(corner[i], 11.f, hot ? Ca(K(pal::white)) : line, 24, 1.5f);
  }
  g.dl->PopClipRect();
}

static void Monitor(ImRect r, bool live) {
  Fill(r, K(pal::g12), 3);
  ImRect hd(r.Min.x + 1, r.Min.y + 1, r.Max.x - 1, r.Min.y + 25);
  PanelHeader(hd, live ? "Live Output" : "Preview Cue", live ? pal::coral : pal::cyan, K(pal::g1c));
  float cy = (hd.Min.y + hd.Max.y - 1) * 0.5f;
  ImRect pvHandB, pvZoomB;   // Preview Cue only: the zoom dropdown + hand tool live in the header, right of the resolution
  // header content: pulse dot precedes title, so re-draw title with offset
  Fill(ImRect(hd.Min.x, hd.Min.y, hd.Min.x + 130, hd.Max.y - 1), K(pal::g1c));
  bool blk = live && A.blackout;
  float pulse = (live && !blk) ? 0.7f + 0.3f * cosf((float)g.time * 2.f * 3.14159f / 1.4f) : 1.f;
  Dot(ImVec2(hd.Min.x + 9, cy), 6, live ? pal::coral : pal::cyan, true, pulse);
  std::string t = Upper(live ? "Live Output" : "Preview Cue");
  Text(hd.Min.x + 9 + 3 + 4, cy, UI_B, 9, K(live ? pal::coral : pal::cyan), t.c_str(), 0.14f);
  float xr = hd.Max.x - 6;
  if (live) {
    // ValueReadout pair
    auto rd = [&](const char* label, const char* val, const char* unit) {
      float w = TextW(MONO_B, 10, val) + (unit ? TextW(MONO_R, 10, unit) + 1 : 0);
      w = std::max(w, TextW(UI_B, 9, Upper(label).c_str(), 0.14f));
      TextR(xr, cy - 5, UI_B, 9, K(pal::t66), Upper(label).c_str(), 0.14f);
      float vx = xr;
      if (unit) { TextR(vx, cy + 5, MONO_R, 10, K(pal::t66), unit); vx -= TextW(MONO_R, 10, unit) + 1; }
      TextR(vx, cy + 5, MONO_B, 10, K(pal::tf3), val);
      xr -= w + 8;
    };
    char cres[32]; snprintf(cres, sizeof cres, "%d\xC3\x97%d", A.canvasW, A.canvasH);
    char cfps[16]; snprintf(cfps, sizeof cfps, "%.1f", PerfFps());
    rd("Output", cres, nullptr);
    rd("Rate", cfps, "fps");
  } else {
    ImRect hb(hd.Max.x - 6 - 24, cy - 9, hd.Max.x - 6, cy + 9);
    pvHandB = hb; pvZoomB = ImRect(hb.Min.x - 4 - 56, hb.Min.y, hb.Min.x - 4, hb.Max.y);
    char pres[32]; snprintf(pres, sizeof pres, "%d\xC3\x97%d", A.canvasW, A.canvasH);
    TextR(pvZoomB.Min.x - 8, cy, MONO_M, 10, K(pal::t66), pres);
  }
  ImRect well(r.Min.x + 1, hd.Max.y, r.Max.x - 1, r.Max.y - 1);
  Fill(well, K(pal::g050));
  if (blk) {
    TextC((well.Min.x + well.Max.x) * 0.5f, (well.Min.y + well.Max.y) * 0.5f, MONO_M, 10, K(pal::red), "OUTPUT MUTED", 0.14f);
  } else if (live) {
    {
      Fill(well, K(0x050505));
      // A1: the composition has its own virtual resolution; letterbox the monitor to that aspect.
      ImRect cv = CanvasRect(well);
      Fill(cv, K(0x0a0a0a));
      g.dl->PushClipRect(cv.Min, cv.Max, true);
      DrawComposite(cv, (float)g.time * 1.2f, 1.f);
      g.dl->PopClipRect();
      Border(cv, K(pal::g2a));
      Text(well.Min.x + 6, well.Max.y - 10, MONO_M, 10, K(pal::coral), "COMPOSITE", 0.09f);
      char cr[32]; snprintf(cr, sizeof cr, "%d\xC3\x97%d", A.canvasW, A.canvasH);
      TextR(well.Max.x - 6, well.Max.y - 10, MONO_M, 10, K(pal::t66), cr);
    }
  } else {
    int sli = std::clamp(A.selLi, 0, (int)A.layers.size() - 1);
    const Clip& sc = A.layers[sli].clips[std::clamp(A.selCi, 0, (int)A.layers[sli].clips.size() - 1)];
    const std::vector<Fx>& chain = sc.fx;
    int act = 0; for (auto& f : chain) if (f.on) act++;
    Fill(well, K(0x080808));
    // same default size as Live Output: letterbox to the canvas aspect and use the same
    // 960 base width (the old full-well rect + 480 base zoomed the cue ~2x vs live).
    // Time multiplier must match every other "live motion" draw call (DrawComposite above, the
    // deck thumbnails) so the same clip animates at the same phase/speed everywhere it's shown —
    // this used to be 1.5f here alone, which desynced Preview Cue from Live Output for a live clip.
    ImRect cvFit = CanvasRect(well);
    gPvFitW = cvFit.GetWidth();
    ImGuiIO& pio = ImGui::GetIO();
    const ImVec2 pm = pio.MousePos;
    const bool pvFree = !(A.ctx.open || A.pop.open || A.layerMenu.open || A.colMenu.open || A.deckMenu.open || A.blendDD.open ||
                          A.projectMenu || A.rename.open || A.settingsOpen || A.openDialog || A.helpOpen);
    // zoom + hand buttons, top-right of the monitor
    const ImRect handB = pvHandB, zoomB = pvZoomB;
    ImRect ctlR(zoomB.Min.x, zoomB.Min.y, handB.Max.x, handB.Max.y);
    const bool pvOver = well.Contains(pm) && pvFree;
    static int panBtn = -1;   // -1 none, 0 left (hand tool), 2 middle
    if (pvOver && !ctlR.Contains(pm)) {
      if (pio.MouseWheel != 0.f) {   // wheel: zoom about the pointer (the point under it stays put)
        ImVec2 wc((well.Min.x + well.Max.x) * 0.5f, (well.Min.y + well.Max.y) * 0.5f);
        float z0 = A.pvZoom, z1 = std::clamp(z0 * (pio.MouseWheel > 0 ? 1.15f : 1.f / 1.15f), 0.1f, 16.f);
        float dx = pm.x - (wc.x + A.pvPanX), dy = pm.y - (wc.y + A.pvPanY);
        A.pvPanX += dx * (1.f - z1 / z0); A.pvPanY += dy * (1.f - z1 / z0);
        A.pvZoom = z1;
      }
      if (panBtn < 0 && ImGui::IsMouseClicked(2)) panBtn = 2;
      else if (panBtn < 0 && A.pvHand && ImGui::IsMouseClicked(0)) panBtn = 0;
    }
    if (panBtn >= 0) {
      if (ImGui::IsMouseDown(panBtn)) { A.pvPanX += pio.MouseDelta.x; A.pvPanY += pio.MouseDelta.y; ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeAll); }
      else panBtn = -1;
    } else if (pvOver && A.pvHand && !ctlR.Contains(pm)) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    {
      float lim = cvFit.GetWidth() * A.pvZoom + well.GetWidth();   // keep the picture reachable
      A.pvPanX = std::clamp(A.pvPanX, -lim, lim); A.pvPanY = std::clamp(A.pvPanY, -lim, lim);
    }
    ImVec2 wcn((well.Min.x + well.Max.x) * 0.5f, (well.Min.y + well.Max.y) * 0.5f);
    ImVec2 ccn(wcn.x + A.pvPanX, wcn.y + A.pvPanY);
    ImRect cv(ccn.x - cvFit.GetWidth() * 0.5f * A.pvZoom, ccn.y - cvFit.GetHeight() * 0.5f * A.pvZoom,
              ccn.x + cvFit.GetWidth() * 0.5f * A.pvZoom, ccn.y + cvFit.GetHeight() * 0.5f * A.pvZoom);
    ImRect vis(std::max(cv.Min.x, well.Min.x), std::max(cv.Min.y, well.Min.y), std::min(cv.Max.x, well.Max.x), std::min(cv.Max.y, well.Max.y));
    if (vis.Max.x > vis.Min.x && vis.Max.y > vis.Min.y) {
      Fill(vis, K(0x0a0a0a));
      g.dl->PushClipRect(vis.Min, vis.Max, true);
      DrawClipContent(cv, sc, (float)g.time * 1.2f, 960.f, 1.f);
      g.dl->PopClipRect();
    }
    g.dl->PushClipRect(well.Min, well.Max, true);
    Border(cv, K(pal::g2a));
    g.dl->PopClipRect();
    PreviewTransformEditor(well, cv, ctlR, sli, std::clamp(A.selCi, 0, (int)A.layers[sli].clips.size() - 1));
    {
      // zoom dropdown (percent of the canvas's real pixel size) and the hand tool
      Hit zh = pvFree ? HitR(zoomB) : Hit(), hh = pvFree ? HitR(handB) : Hit();
      char zt[16]; snprintf(zt, sizeof zt, "%d%%", (int)std::round(cv.GetWidth() / std::max(1, A.canvasW) * 100.f));
      Box(zoomB, zh.hover ? K(pal::ctrlHover) : K(0x101010, 0.92f), K(pal::g2a), 3);
      Text(zoomB.Min.x + 8, (zoomB.Min.y + zoomB.Max.y) * 0.5f, MONO_B, 10, K(pal::tcc), zt);
      Icon("chevron-down", ImVec2(zoomB.Max.x - 10, (zoomB.Min.y + zoomB.Max.y) * 0.5f), 9, K(pal::t88));
      Box(handB, A.pvHand ? K(pal::cyan, 0.2f) : hh.hover ? K(pal::ctrlHover) : K(0x101010, 0.92f), A.pvHand ? K(pal::cyan) : K(pal::g2a), 3);
      Icon("hand", ImVec2((handB.Min.x + handB.Max.x) * 0.5f, (handB.Min.y + handB.Max.y) * 0.5f), 13, K(A.pvHand ? pal::cyan : pal::tcc));
      if (zh.hover || hh.hover) CursorHand();
      if (hh.click) A.pvHand = !A.pvHand;
      if (zh.click) {
        std::vector<MenuItem> mi;
        MenuItem f; f.label = "Fit"; f.run = [] { A.pvZoom = 1.f; A.pvPanX = A.pvPanY = 0.f; }; mi.push_back(f);
        MenuItem d; d.divider = true; mi.push_back(d);
        for (int pct : {12, 25, 50, 100, 200, 400}) {
          char lb[16]; snprintf(lb, sizeof lb, "%d%%", pct);
          MenuItem it; it.label = lb;
          it.run = [pct] { if (gPvFitW > 1.f) A.pvZoom = std::clamp(A.canvasW * pct / 100.f / gPvFitW, 0.1f, 16.f); };   // percent of the canvas's real pixel size
          mi.push_back(it);
        }
        A.openCtx(ImVec2(zoomB.Min.x, zoomB.Max.y + 4), mi);
      }
    }
    std::string nm = sc.name.empty() ? "no cue" : Upper(sc.name);
    Text(well.Min.x + 6, well.Max.y - 10, MONO_M, 10, K(pal::cyan), nm.c_str(), 0.09f);
    char fx[16]; snprintf(fx, sizeof fx, act ? "FX %d/%d" : "FX DRY", act, (int)chain.size());
    TextR(well.Max.x - 6, well.Max.y - 10, MONO_B, 9, K(pal::t88), fx, 0.09f);
  }
  Border(well, K(live ? pal::coral : pal::cyan, 0.2f));
  Border(r, K(pal::g2a), 3);
}

// ───────────────────────── inspector ─────────────────────────
static const char* PROP_TABS[3][2] = {{"Comp", "sliders-horizontal"}, {"Layer", "layers"}, {"Clip", "film"}};

static void Inspector(ImRect r) {
  Fill(r, K(pal::g16));
  VLine(r.Min.x, r.Min.y, r.Max.y, K(pal::g2a));
  PanelHeader(Rc(r.Min.x + 1, r.Min.y, r.GetWidth() - 1, 24), "Properties", pal::t88);
  float w = r.GetWidth() - 1, x0 = r.Min.x + 1;
  // tabs
  float ty = r.Min.y + 24;
  Fill(Rc(x0, ty, w, 52), K(pal::g16));
  HLine(x0, r.Max.x, ty + 51, K(pal::g2a));
  float tw = (w - 12 - 8) / 3.f;
  for (int i = 0; i < 3; ++i) {
    ImRect tr(x0 + 6 + i * (tw + 4), ty + 5, x0 + 6 + i * (tw + 4) + tw, ty + 5 + 42);
    bool on = A.tab == i;
    Hit h = HitR(tr);
    if (on) { Glow(tr, pal::cyan, 0.30f, 12, 3); Fill(tr, K(pal::g16), 3); }
    Box(tr, on ? K(pal::cyan, 0.15f) : 0, on ? K(pal::cyan) : 0, 3);
    ImU32 fg = K(on ? pal::cyan : pal::t66);
    Icon(PROP_TABS[i][1], ImVec2((tr.Min.x + tr.Max.x) * 0.5f, tr.Min.y + 15), 13, fg);
    std::string u = Upper(PROP_TABS[i][0]);
    TextC((tr.Min.x + tr.Max.x) * 0.5f, tr.Min.y + 32, UI_B, 10, fg, u.c_str(), 0.09f);
    if (h.hover) CursorHand();
    if (h.click) A.tab = i;
  }
  ImRect body(x0, ty + 52, r.Max.x, r.Max.y);
  static ScrollArea sa;
  sa.Begin("##inspector", body);
  if (gTestInspScroll > 0.f) ImGui::SetScrollY(gTestInspScroll);
  float ox = sa.origin.x, oy = sa.origin.y, W = body.GetWidth() - (ImGui::GetCurrentWindow()->ScrollbarY ? 8.f : 0.f);
  float y = 0;
  const Clip& cell = A.layers[A.selLi].clips[A.selCi];
  bool cellLive = cell.isLive();
  char b1[64];

  int sid = 0, sidBase = 0x2100;   // shared by the Comp and Layer tabs' slider rows (each tab restarts sid)
  auto section = [&](const char* title, uint32_t hex, const char* note) {
    HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 8;
    Text(ox + 8, oy + y + 5, UI_B, 9, K(hex), title, 0.14f);
    if (note) TextR(ox + W - 8, oy + y + 5, MONO_R, 8, K(pal::t66), note);
    y += 9 + 6;
  };
  auto sliderRow = [&](const char* label, const char* val, float& v, float mn, float mx, uint32_t hex) -> bool {
    Text(ox + 8, oy + y + 5, UI_S, 10, K(pal::t88), label);
    TextR(ox + W - 8, oy + y + 5, MONO_B, 10, K(pal::tf3), val);
    y += 10 + 4;
    bool ch = Slider(sidBase + sid++, Rc(ox + 8, oy + y + 4, W - 16, 6), v, hex, mn, mx);
    y += 14 + 4;
    return ch;
  };
  auto dropRow = [&](const char* label, std::vector<const char*> opts, int& idx) {
    Text(ox + 8, oy + y + 5, UI_S, 10, K(pal::t88), label);
    y += 10 + 4;
    ImRect dr(ox + 8, oy + y, ox + W - 8, oy + y + 24);
    Hit dh = HitR(dr);
    Box(dr, dh.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
    TextEll(dr.Min.x + 8, (dr.Min.y + dr.Max.y) * 0.5f, dr.GetWidth() - 30, UI_S, 10, K(pal::tf3), opts[std::clamp(idx, 0, (int)opts.size() - 1)]);
    Icon("chevron-down", ImVec2(dr.Max.x - 12, (dr.Min.y + dr.Max.y) * 0.5f), 10, K(pal::t66));
    if (dh.hover) CursorHand();
    if (dh.click) {
      int* target = &idx;   // A is a global whose address never changes, so this stays valid until the menu item runs
      std::vector<MenuItem> mi;
      for (int i = 0; i < (int)opts.size(); ++i) {
        MenuItem it; it.label = opts[i]; it.toneHex = i == idx ? pal::cyan : 0; it.run = [target, i] { *target = i; };
        mi.push_back(it);
      }
      A.openCtx(ImVec2(dr.Min.x, dr.Max.y + 4), mi);
    }
    y += 24 + 6;
  };
  if (A.tab == 1) {
    // Layer properties, laid out like Resolume's Layer panel. LIVE: Master, Audio volume (the strip's A bar), Blend Mode,
    // Opacity, Transition duration and the Transform. Stored only: Pan, Size/Auto Size, Transition blend mode.
    Layer& ml = A.layers[A.selLayer];
    sid = 0; sidBase = 0x2200;
    {
      // Layer name, editable right here (double-click on the layer strip still works too). Applies as you type, but
      // never to an empty/blank name — that keeps the old one, same rule as the rename popup (App::commitRename).
      y += 6;
      Text(ox + 8, oy + y + 5, UI_S, 10, K(pal::t88), "Name");
      y += 10 + 4;
      static std::string nameEdit;
      ImGuiID idN = ImGui::GetID("##lyname");
      if (ImGui::GetActiveID() != idN) nameEdit = ml.name;
      if (TextField("##lyname", Rc(ox + 8, oy + y, W - 16, 26), nameEdit)) {
        size_t n0 = nameEdit.find_first_not_of(" \t"), n1 = nameEdit.find_last_not_of(" \t");
        if (n0 != std::string::npos) ml.name = nameEdit.substr(n0, n1 - n0 + 1);
      }
      y += 26 + 8;
    }
    section("LAYER", pal::coral, nullptr);
    snprintf(b1, sizeof b1, "%d %%", (int)std::round(ml.master));            sliderRow("Master", b1, ml.master, 0, 100, pal::coral);
    // ── AUDIO ──
    section("AUDIO", pal::mint, "NO AUDIO ENGINE YET");
    {
      // The layer strip's A bar stores a 0..100 % gain; the panel shows and edits the same value in dB (100 % = 0 dB).
      float db = ml.audio <= 0.001f ? -60.f : std::clamp(20.f * std::log10(ml.audio / 100.f), -60.f, 0.f);
      if (db <= -59.5f) snprintf(b1, sizeof b1, "-inf dB"); else snprintf(b1, sizeof b1, "%d dB", (int)std::round(db));
      if (sliderRow("Volume", b1, db, -60, 0, pal::mint)) ml.audio = db <= -59.5f ? 0.f : 100.f * std::pow(10.f, db / 20.f);
    }
    snprintf(b1, sizeof b1, "%d", (int)std::round(ml.pan));                  sliderRow("Pan", b1, ml.pan, -100, 100, pal::mint);
    // ── VIDEO ──
    section("VIDEO", pal::cyan, nullptr);
    {
      int bi = std::max(0, BlendIndex(ml.blend));
      std::vector<const char*> names(BLEND_NAMES, BLEND_NAMES + BLEND_COUNT);
      Text(ox + 8, oy + y + 5, UI_S, 10, K(pal::t88), "Blend Mode");
      y += 10 + 4;
      ImRect dr(ox + 8, oy + y, ox + W - 8, oy + y + 24);
      Hit dh = HitR(dr);
      Box(dr, dh.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
      TextEll(dr.Min.x + 8, (dr.Min.y + dr.Max.y) * 0.5f, dr.GetWidth() - 30, UI_S, 10, K(pal::tf3), names[bi]);
      Icon("chevron-down", ImVec2(dr.Max.x - 12, (dr.Min.y + dr.Max.y) * 0.5f), 10, K(pal::t66));
      if (dh.hover) CursorHand();
      if (dh.click) {
        int li = A.selLayer; std::vector<MenuItem> mi;
        for (int i = 0; i < BLEND_COUNT; ++i) {
          MenuItem it; it.label = BLEND_NAMES[i]; it.toneHex = i == bi ? pal::cyan : 0;
          it.run = [li, i] { if (li < (int)A.layers.size()) A.layers[li].blend = BLEND_NAMES[i]; };   // same 8 names as the layer-row dropdown, so it always matches what is rendered
          mi.push_back(it);
        }
        A.openCtx(ImVec2(dr.Min.x, dr.Max.y + 4), mi);
      }
      y += 24 + 6;
    }
    snprintf(b1, sizeof b1, "%d %%", (int)std::round(ml.opacity));           sliderRow("Opacity", b1, ml.opacity, 0, 100, pal::coral);
    {
      Text(ox + 8, oy + y + 5, UI_S, 10, K(pal::t88), "Size (W \xC3\x97 H)");
      TextR(ox + W - 8, oy + y + 5, MONO_R, 8, K(pal::t66), "NOT ACTIVE YET");
      y += 10 + 4;
      static int lw = 0, lh = 0;   // applied on focus loss, like the composition resolution
      ImGuiID idW = ImGui::GetID("##lyw"), idH = ImGui::GetID("##lyh");
      if (ImGui::GetActiveID() != idW) lw = ml.width > 0 ? ml.width : A.canvasW;
      if (ImGui::GetActiveID() != idH) lh = ml.height > 0 ? ml.height : A.canvasH;
      float fw = (W - 16 - 22) / 2.f;
      IntField("##lyw", Rc(ox + 8, oy + y, fw, 24), lw);
      if (ImGui::IsItemDeactivatedAfterEdit()) ml.width = std::clamp(lw, 1, 16384);
      TextC(ox + 8 + fw + 11, oy + y + 12, MONO_B, 11, K(pal::t66), "\xC3\x97");
      IntField("##lyh", Rc(ox + 8 + fw + 22, oy + y, fw, 24), lh);
      if (ImGui::IsItemDeactivatedAfterEdit()) ml.height = std::clamp(lh, 1, 16384);
      y += 24 + 6;
    }
    dropRow("Auto Size", {"Off", "Fit", "Fill", "Stretch"}, ml.autoSize);
    // ── TRANSITION ──
    section("TRANSITION", pal::yellow, "BLEND MODE NOT ACTIVE");
    dropRow("Blend Mode", {"Alpha", "Add", "Multiply", "Screen"}, ml.transBlend);
    {
      float t10 = std::round(ml.blendTime * 10.f);   // the slider is integer-stepped: tenths of a second
      snprintf(b1, sizeof b1, "%.1f s", ml.blendTime);
      if (sliderRow("Duration", b1, t10, 0, 50, pal::yellow)) ml.blendTime = t10 / 10.f;
    }
    // ── TRANSFORM: applies to this layer's clips ──
    HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 8;
    Text(ox + 8, oy + y + 5, UI_B, 9, K(pal::t88), "TRANSFORM", 0.14f);
    {
      ImRect rb(ox + W - 8 - 40, oy + y - 3, ox + W - 8, oy + y + 13);
      Hit hh = HitR(rb);
      Box(rb, hh.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
      TextC((rb.Min.x + rb.Max.x) * 0.5f, (rb.Min.y + rb.Max.y) * 0.5f, MONO_B, 9, K(hh.hover ? pal::white : pal::t88), "RESET", 0.09f);
      if (hh.hover) CursorHand();
      if (hh.click) { ml.posX = ml.posY = ml.rotation = ml.anchorX = ml.anchorY = 0; ml.scale = 100; }
    }
    y += 9 + 6;
    float hw = (float)A.canvasW, hh2 = (float)A.canvasH;
    snprintf(b1, sizeof b1, "%d px", (int)std::round(ml.posX));              sliderRow("Position X", b1, ml.posX, -hw, hw, pal::cyan);
    snprintf(b1, sizeof b1, "%d px", (int)std::round(ml.posY));              sliderRow("Position Y", b1, ml.posY, -hh2, hh2, pal::cyan);
    snprintf(b1, sizeof b1, "%d %%", (int)std::round(ml.scale));             sliderRow("Scale", b1, ml.scale, 1, 400, pal::coral);
    snprintf(b1, sizeof b1, "%d\xC2\xB0", (int)std::round(ml.rotation));     sliderRow("Rotation", b1, ml.rotation, -180, 180, pal::yellow);
    snprintf(b1, sizeof b1, "%d px", (int)std::round(ml.anchorX));           sliderRow("Anchor X", b1, ml.anchorX, -hw * 0.5f, hw * 0.5f, pal::t88);
    snprintf(b1, sizeof b1, "%d px", (int)std::round(ml.anchorY));           sliderRow("Anchor Y", b1, ml.anchorY, -hh2 * 0.5f, hh2 * 0.5f, pal::t88);
    // ── ROUTING ──
    HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 8;
    Text(ox + 8, oy + y + 5, UI_B, 9, K(pal::t88), "ROUTING", 0.14f);
    y += 9 + 6;
    float sw = (W - 16 - 8) / 3.f;
    struct SW { const char* n; uint32_t hex; bool* v; } sws[3] = {{"Solo", pal::yellow, &ml.solo}, {"Mute", pal::red, &ml.muted}, {"Bypass", pal::t88, &ml.bypassed}};
    for (int i = 0; i < 3; ++i) {
      ImRect br(ox + 8 + i * (sw + 4), oy + y, ox + 8 + i * (sw + 4) + sw, oy + y + 26);
      bool on = *sws[i].v;
      Hit h = HitR(br);
      if (on) { Glow(br, sws[i].hex, 0.4f, 10, 3); Fill(br, K(pal::g16), 3); }
      Box(br, on ? K(sws[i].hex, 0.14f) : K(pal::g1c), on ? K(sws[i].hex) : K(pal::g22), 3);
      std::string u = Upper(sws[i].n);
      TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, UI_B, 10, K(on ? sws[i].hex : pal::t77), u.c_str(), 0.09f);
      if (h.hover) CursorHand();
      if (h.click) *sws[i].v = !*sws[i].v;
    }
    y += 26 + 8;
    // ── COLOR: the layer's accent. Changing it recolours the clips that still have the layer's OLD colour, so the layer and
    //    its clips stay one colour; a clip the user coloured differently keeps its own. Swatches are the six fixed colours at
    //    full brightness, no outline; the current one carries a small dot.
    HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 8;
    Text(ox + 8, oy + y + 5, UI_B, 9, K(pal::t88), "COLOR", 0.14f);
    y += 9 + 8;
    {
      float sw = (W - 16 - 5 * 4) / 6.f;
      for (int k = 0; k < 6; ++k) {
        ImRect sr(ox + 8 + k * (sw + 4), oy + y, ox + 8 + k * (sw + 4) + sw, oy + y + 22);
        Hit sh = HitR(sr);
        Fill(sr, MixHex(CLIP_COLORS[k], 0xffffff, sh.hover ? 0.22f : 0.f), 3);
        if (ml.color == k) g.dl->AddCircleFilled(ImVec2((sr.Min.x + sr.Max.x) * 0.5f, (sr.Min.y + sr.Max.y) * 0.5f), 3.f, Ca(K(0xffffff, 0.9f)));
        if (sh.hover) CursorHand();
        if (sh.click) A.setLayerColor(A.selLayer, k);
      }
      y += 22 + 8;
    }
  } else if (A.tab == 0) {
    // Composition properties. Resolution, Master, Speed, Video opacity and Transform are LIVE (they change what is
    // rendered / how fast clips play); Audio and CrossFader are stored with the project but nothing consumes them yet.
    CompProps& k = A.comp;
    sid = 0; sidBase = 0x2100;
    PropertyRow(Rc(ox, oy + y, W, 22), "Composition", A.projectName.c_str(), "", pal::tcc); y += 22;   // name first, then the controls
    // ── COMPOSITION: resolution / master / speed ──
    section("COMPOSITION", pal::cyan, nullptr);
    {
      Text(ox + 8, oy + y + 5, UI_S, 10, K(pal::t88), "Resolution");
      int gd = std::gcd(A.canvasW, A.canvasH); char ar[48];
      if (gd > 0 && A.canvasW / gd <= 32 && A.canvasH / gd <= 32) snprintf(ar, sizeof ar, "%d:%d \xC2\xB7 %.1f MP", A.canvasW / gd, A.canvasH / gd, A.canvasW * (double)A.canvasH / 1e6);
      else snprintf(ar, sizeof ar, "%.2f:1 \xC2\xB7 %.1f MP", A.canvasW / (double)std::max(1, A.canvasH), A.canvasW * (double)A.canvasH / 1e6);
      TextR(ox + W - 8, oy + y + 5, MONO_R, 9, K(pal::t66), ar);
      y += 10 + 4;
      // Typed values apply when the field loses focus (Enter / Tab / click away), not per keystroke: "3840" passes
      // through 3, 38, 384 and each of those would otherwise resize the canvas and rescale every slice.
      static int pw = 0, ph = 0;
      ImGuiID idW = ImGui::GetID("##compw"), idH = ImGui::GetID("##comph");
      if (ImGui::GetActiveID() != idW) pw = A.canvasW;
      if (ImGui::GetActiveID() != idH) ph = A.canvasH;
      float fw = (W - 16 - 22) / 2.f;
      IntField("##compw", Rc(ox + 8, oy + y, fw, 24), pw);
      bool doneW = ImGui::IsItemDeactivatedAfterEdit();
      TextC(ox + 8 + fw + 11, oy + y + 12, MONO_B, 11, K(pal::t66), "\xC3\x97");
      IntField("##comph", Rc(ox + 8 + fw + 22, oy + y, fw, 24), ph);
      bool doneH = ImGui::IsItemDeactivatedAfterEdit();
      if (doneW || doneH) A.setCanvasSize(pw, ph);
      y += 24 + 6;
      struct Pre { const char* n; int w, h; } pres[4] = {{"720p", 1280, 720}, {"1080p", 1920, 1080}, {"1440p", 2560, 1440}, {"4K", 3840, 2160}};
      float pwid = (W - 16 - 3 * 4) / 4.f;
      for (int i = 0; i < 4; ++i) {
        ImRect br(ox + 8 + i * (pwid + 4), oy + y, ox + 8 + i * (pwid + 4) + pwid, oy + y + 20);
        bool on = A.canvasW == pres[i].w && A.canvasH == pres[i].h;
        Hit h = HitR(br);
        if (on) { Glow(br, pal::cyan, 0.3f, 10, 3); Fill(br, K(pal::g16), 3); }
        Box(br, on ? K(pal::cyan, 0.15f) : K(pal::g1c), on ? K(pal::cyan) : K(pal::g22), 3);
        TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, MONO_B, 9, K(on ? pal::cyan : pal::t77), pres[i].n, 0.09f);
        if (h.hover) CursorHand();
        if (h.click) A.setCanvasSize(pres[i].w, pres[i].h);
      }
      y += 20 + 8;
    }
    snprintf(b1, sizeof b1, "%d %%", (int)std::round(k.master));           sliderRow("Master", b1, k.master, 0, 100, pal::coral);
    snprintf(b1, sizeof b1, "%.2f\xC3\x97", k.speed / 100.f);              sliderRow("Speed", b1, k.speed, 0, 400, pal::cyan);
    // ── AUDIO (stored only) ──
    section("AUDIO", pal::mint, "NO AUDIO ENGINE YET");
    snprintf(b1, sizeof b1, "%d dB", (int)std::round(k.volume));           sliderRow("Volume", b1, k.volume, -60, 12, pal::mint);
    snprintf(b1, sizeof b1, "%d", (int)std::round(k.pan));                 sliderRow("Pan", b1, k.pan, -100, 100, pal::mint);
    // ── VIDEO ──
    section("VIDEO", pal::coral, nullptr);
    snprintf(b1, sizeof b1, "%d %%", (int)std::round(k.opacity));          sliderRow("Opacity", b1, k.opacity, 0, 100, pal::coral);
    // ── CROSSFADER (stored only) ──
    section("CROSSFADER", pal::yellow, "NOT IMPLEMENTED YET");
    dropRow("Blend Mode", {"Alpha", "Add", "Multiply", "Screen"}, k.xfBlend);
    dropRow("Behaviour", {"Cut", "Fade", "Wipe"}, k.xfBehaviour);
    dropRow("Curve", {"Linear", "Ease in", "Ease out"}, k.xfCurve);
    // ── TRANSFORM: applies to the whole composite ──
    HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 8;
    Text(ox + 8, oy + y + 5, UI_B, 9, K(pal::t88), "TRANSFORM", 0.14f);
    {
      ImRect rb(ox + W - 8 - 40, oy + y - 3, ox + W - 8, oy + y + 13);
      Hit hh = HitR(rb);
      Box(rb, hh.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
      TextC((rb.Min.x + rb.Max.x) * 0.5f, (rb.Min.y + rb.Max.y) * 0.5f, MONO_B, 9, K(hh.hover ? pal::white : pal::t88), "RESET", 0.09f);
      if (hh.hover) CursorHand();
      if (hh.click) { k.posX = k.posY = k.rotation = k.anchorX = k.anchorY = 0; k.scale = 100; }
    }
    y += 9 + 6;
    float hw = (float)A.canvasW, hh2 = (float)A.canvasH;
    snprintf(b1, sizeof b1, "%d px", (int)std::round(k.posX));             sliderRow("Position X", b1, k.posX, -hw, hw, pal::cyan);
    snprintf(b1, sizeof b1, "%d px", (int)std::round(k.posY));             sliderRow("Position Y", b1, k.posY, -hh2, hh2, pal::cyan);
    snprintf(b1, sizeof b1, "%d %%", (int)std::round(k.scale));            sliderRow("Scale", b1, k.scale, 1, 400, pal::coral);
    snprintf(b1, sizeof b1, "%d\xC2\xB0", (int)std::round(k.rotation));    sliderRow("Rotation", b1, k.rotation, -180, 180, pal::yellow);
    snprintf(b1, sizeof b1, "%d px", (int)std::round(k.anchorX));          sliderRow("Anchor X", b1, k.anchorX, -hw * 0.5f, hw * 0.5f, pal::t88);
    snprintf(b1, sizeof b1, "%d px", (int)std::round(k.anchorY));          sliderRow("Anchor Y", b1, k.anchorY, -hh2 * 0.5f, hh2 * 0.5f, pal::t88);
    y += 8;   // (the Layers/Groups/Columns/BPM/Rate/Latency/Output readout that used to sit here duplicated the deck and status bar)
  } else {
    // Clip properties, laid out like Resolume's Clip panel: Name / preview / Transport / Autopilot / Audio / Video /
    // Transform, then ONE section per effect on the clip (an effect dragged in gets its own parameters here).
    // LIVE: name, transport (play/pause/reverse, in/out range, scrub, play mode, speed, duration), R/G/B channels,
    // opacity, blend mode, transform incl. anchor, every effect parameter. Stored only (panel says so): transport mode,
    // autopilot, audio volume/pan, video size.
    Clip& mc = A.layers[std::clamp(A.selLi, 0, (int)A.layers.size() - 1)].clips[std::clamp(A.selCi, 0, A.colCount() - 1)];
    const bool editable = mc.st != Clip::Empty && mc.st != Clip::Armed;
    sid = 0; sidBase = 0x2300;
    const MediaKind mk = MediaKindOf(mc.media);
    auto chip = [&](ImRect br, const char* lab, bool on, uint32_t hex, bool enabled = true) -> bool {
      Hit h = enabled ? HitR(br) : Hit();
      if (on) { Glow(br, hex, 0.28f, 10, 3); Fill(br, K(pal::g16), 3); }
      Box(br, on ? K(hex, 0.18f) : K(pal::g1c), on ? K(hex) : K(pal::g22), 3);
      TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, MONO_B, 9, K(on ? hex : enabled ? pal::t88 : pal::g33), lab, 0.09f);
      if (h.hover) CursorHand();
      return h.click;
    };
    y += 6;
    // ── name ──
    Text(ox + 8, oy + y + 5, UI_S, 10, K(pal::t88), "Name");
    y += 10 + 4;
    if (editable) {
      static std::string clipNameEdit;
      ImGuiID idC = ImGui::GetID("##clipname");
      if (ImGui::GetActiveID() != idC) clipNameEdit = mc.name;
      if (TextField("##clipname", Rc(ox + 8, oy + y, W - 16, 26), clipNameEdit)) {
        size_t n0 = clipNameEdit.find_first_not_of(" \t"), n1 = clipNameEdit.find_last_not_of(" \t");
        if (n0 != std::string::npos) {
          if (mc.style < 0) mc.style = ClipStyleOf(mc.name);   // pin the look before the name (its source of truth) changes
          mc.name = clipNameEdit.substr(n0, n1 - n0 + 1);
        }
      }
    } else {
      ImRect nr(ox + 8, oy + y, ox + W - 8, oy + y + 26);
      Box(nr, K(pal::g050), K(pal::g22), 3);
      Text(nr.Min.x + 8, (nr.Min.y + nr.Max.y) * 0.5f, UI_S, 10, K(pal::t66), "Empty slot");
    }
    y += 26 + 8;
    // ── preview ──
    {
      ImRect th(ox + 8, oy + y, ox + W - 8, oy + y + 76);
      g.dl->PushClipRect(th.Min, th.Max, true);
      Fill(th, K(0x050505));
      DrawClipContent(th, cell, (float)g.time * 1.2f, 300.f, 1.f, 0.4f);
      g.dl->AddRectFilledMultiColor(th.Min, th.Max, Ca(K(0x050505, 0.15f)), Ca(K(0x050505, 0.15f)), Ca(K(0x050505, 0.82f)), Ca(K(0x050505, 0.82f)));
      g.dl->PopClipRect();
      std::string srcLab = mc.media.empty() ? std::string("GENERATOR") : Upper(std::filesystem::path(mc.media).filename().string());
      if (editable) TextEll(th.Min.x + 6, th.Min.y + 10, th.GetWidth() - 12, MONO_M, 10, K(pal::tcc), srcLab.c_str(), 0.09f);
      Text(th.Min.x + 6, th.Max.y - 10, UI_B, 11, K(pal::white), editable ? mc.name.c_str() : "Empty slot");
      if (editable) TextR(th.Max.x - 6, th.Max.y - 10, MONO_B, 9, K(cellLive ? pal::coral : pal::cyan), cellLive ? "LIVE" : "CUED");
      Border(th, K(pal::g2a), 3);
      y += 76 + 8;
    }
    if (!editable) {
      Text(ox + 8, oy + y + 7, UI_S, 10, K(pal::t66), "Drag a source from the Browser, or drop a file here.", 0.01f);
      y += 22;
    } else {
    // ── TRANSPORT ──
    HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 8;
    Text(ox + 8, oy + y + 5, UI_B, 9, K(pal::cyan), "TRANSPORT", 0.14f);
    {
      const char* tm = mc.tMode == 1 ? "BPM Sync (inactive)" : "Timeline";
      float cw = TextW(UI_S, 10, tm) + 30;
      ImRect cr(ox + W - 8 - cw, oy + y - 4, ox + W - 8, oy + y + 16);
      Hit ch = HitR(cr);
      Box(cr, ch.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
      Text(cr.Min.x + 8, (cr.Min.y + cr.Max.y) * 0.5f, UI_S, 10, K(pal::tf3), tm);
      Icon("chevron-down", ImVec2(cr.Max.x - 10, (cr.Min.y + cr.Max.y) * 0.5f), 9, K(pal::t66));
      if (ch.hover) CursorHand();
      if (ch.click) {
        int* target = &mc.tMode; std::vector<MenuItem> mi;
        const char* nm2[2] = {"Timeline", "BPM Sync (not active yet)"};
        for (int i = 0; i < 2; ++i) { MenuItem it; it.label = nm2[i]; it.toneHex = i == mc.tMode ? pal::cyan : 0; it.run = [target, i] { *target = i; }; mi.push_back(it); }
        A.openCtx(ImVec2(cr.Min.x - 60, cr.Max.y + 4), mi);
      }
    }
    y += 9 + 8;
    {
      // timeline track: dimmed outside the in/out range, playhead + green in/out markers. Drag the playhead to scrub,
      // the markers to set the playback range. No waveform: there is no audio decoder yet.
      float total = ClipSeconds(mc);
      char tt[24]; snprintf(tt, sizeof tt, "%06.3f", mc.progress / 100.f * total);
      TextR(ox + W - 8, oy + y + 5, MONO_B, 10, K(pal::tf3), tt);
      y += 10 + 6;
      ImRect tr(ox + 14, oy + y, ox + W - 14, oy + y + 22);
      Box(tr, K(pal::g050), K(pal::g22), 3);
      auto px = [&](float pct) { return tr.Min.x + tr.GetWidth() * std::clamp(pct, 0.f, 100.f) / 100.f; };
      Fill(ImRect(px(mc.inPt), tr.Min.y + 1, px(mc.outPt), tr.Max.y - 1), K(pal::cyan, 0.16f), 2);
      Fill(ImRect(px(mc.progress) - 1, tr.Min.y - 2, px(mc.progress) + 1, tr.Max.y + 2), K(pal::coral));
      g.dl->AddTriangleFilled(ImVec2(px(mc.progress) - 4, tr.Min.y - 5), ImVec2(px(mc.progress) + 4, tr.Min.y - 5), ImVec2(px(mc.progress), tr.Min.y + 1), Ca(K(pal::coral)));
      g.dl->AddTriangleFilled(ImVec2(px(mc.inPt), tr.Max.y + 1), ImVec2(px(mc.inPt) + 7, tr.Max.y + 8), ImVec2(px(mc.inPt) - 1, tr.Max.y + 8), Ca(K(pal::mint)));
      g.dl->AddTriangleFilled(ImVec2(px(mc.outPt), tr.Max.y + 1), ImVec2(px(mc.outPt) - 7, tr.Max.y + 8), ImVec2(px(mc.outPt) + 1, tr.Max.y + 8), Ca(K(pal::mint)));
      ImRect grab(tr.Min.x - 8, tr.Min.y - 6, tr.Max.x + 8, tr.Max.y + 10);
      static int dragK = 0;   // 1 playhead, 2 in, 3 out
      Hit gh = HitR(grab);
      if (gh.hover) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
      float mxp = ImGui::GetIO().MousePos.x;
      if (gh.click) {
        bool lower = ImGui::GetIO().MousePos.y > tr.Max.y - 2;
        if (lower && std::fabs(mxp - px(mc.inPt)) <= 9) dragK = 2;
        else if (lower && std::fabs(mxp - px(mc.outPt)) <= 9) dragK = 3;
        else dragK = 1;
        g.active = 0x2390;
      }
      if (dragK && g.active == 0x2390) {
        if (ImGui::IsMouseDown(0)) {
          float pct = std::clamp((mxp - tr.Min.x) / std::max(1.f, tr.GetWidth()) * 100.f, 0.f, 100.f);
          if (dragK == 1) mc.progress = pct;
          else if (dragK == 2) mc.inPt = std::clamp(pct, 0.f, mc.outPt - 1.f);
          else mc.outPt = std::clamp(pct, mc.inPt + 1.f, 100.f);
        } else { dragK = 0; g.active = 0; }
      }
      y += 22 + 12;
    }
    {
      // ◀ reverse-play · ❚❚ pause · ▶ play (this clip's own transport), then the loop mode
      bool rev = !mc.paused && mc.dir < 0, fwd = !mc.paused && mc.dir >= 0;
      if (chip(ImRect(ox + 8, oy + y, ox + 8 + 28, oy + y + 24), "<", rev, pal::mint)) { mc.dir = -1; mc.paused = false; }
      if (chip(ImRect(ox + 8 + 32, oy + y, ox + 8 + 60, oy + y + 24), "||", mc.paused, pal::yellow)) mc.paused = true;
      if (chip(ImRect(ox + 8 + 64, oy + y, ox + 8 + 92, oy + y + 24), ">", fwd, pal::mint)) { mc.dir = 1; mc.paused = false; }
      static const char* pmNames[4] = {"Loop", "Bounce", "Hold", "Once"};
      ImRect dr(ox + 8 + 100, oy + y, ox + W - 8, oy + y + 24);
      Hit dh = HitR(dr);
      Box(dr, dh.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
      Icon("repeat", ImVec2(dr.Min.x + 12, (dr.Min.y + dr.Max.y) * 0.5f), 10, K(pal::cyan));
      Text(dr.Min.x + 24, (dr.Min.y + dr.Max.y) * 0.5f, UI_S, 10, K(pal::tf3), pmNames[std::clamp(mc.playMode, 0, 3)]);
      Icon("chevron-down", ImVec2(dr.Max.x - 10, (dr.Min.y + dr.Max.y) * 0.5f), 9, K(pal::t66));
      if (dh.hover) CursorHand();
      if (dh.click) {
        Clip* target = &mc; std::vector<MenuItem> mi;
        for (int i = 0; i < 4; ++i) { MenuItem it; it.label = pmNames[i]; it.toneHex = i == mc.playMode ? pal::cyan : 0; it.run = [target, i] { target->playMode = i; target->dir = 1; }; mi.push_back(it); }
        A.openCtx(ImVec2(dr.Min.x, dr.Max.y + 4), mi);
      }
      y += 24 + 8;
    }
    snprintf(b1, sizeof b1, "%.2f\xC3\x97", mc.speed / 100.f);   sliderRow("Speed", b1, mc.speed, 0, 400, pal::cyan);
    {
      float sec = ClipSeconds(mc);
      Text(ox + 8, oy + y + 6, UI_S, 10, K(pal::t88), "Duration");
      float xr = ox + W - 8;
      auto half = [&](const char* lab, float k) {
        ImRect br(xr - 28, oy + y, xr, oy + y + 18);
        if (chip(br, lab, false, pal::cyan)) {
          float ns = std::clamp(sec * k, 0.5f, 3600.f);
          char d[16]; snprintf(d, sizeof d, "%gs", std::round(ns * 100.f) / 100.f); mc.dur = d;
        }
        xr -= 32;
      };
      half("\xC3\x97" "2", 2.f);
      half("/2", 0.5f);
      float dv = 0; bool finite = std::sscanf(mc.dur.c_str(), "%f", &dv) == 1 && dv > 0.f;   // generators and images have an infinity duration
      char ds[24]; if (!finite) snprintf(ds, sizeof ds, "\xE2\x88\x9E"); else snprintf(ds, sizeof ds, "%g s", sec);
      TextR(xr - 4, oy + y + 9, MONO_B, 10, K(pal::tf3), ds);
      y += 18 + 8;
    }
    // ── AUTOPILOT (stored only) ──
    section("AUTOPILOT", pal::yellow, "NOT ACTIVE YET");
    dropRow("Action", {"Layer Determined", "None", "Play next clip", "Play previous clip", "Play random clip"}, mc.autoAction);
    { float lp = (float)mc.autoLoops; snprintf(b1, sizeof b1, "%d", mc.autoLoops); if (sliderRow("Loops", b1, lp, 1, 16, pal::yellow)) mc.autoLoops = (int)lp; }
    // ── AUDIO (only for audio/video files; stored only) ──
    if (mk == MEDIA_VIDEO || mk == MEDIA_AUDIO) {
      section("AUDIO", pal::mint, "NO AUDIO ENGINE YET");
      TextEll(ox + 8, oy + y + 6, W - 16, UI_B, 10, K(pal::tcc), std::filesystem::path(mc.media).filename().string().c_str());
      Text(ox + 8, oy + y + 20, MONO_R, 9, K(pal::t66), "audio decoder not available yet");
      y += 30;
      snprintf(b1, sizeof b1, "%d dB", (int)std::round(mc.volume));   sliderRow("Volume", b1, mc.volume, -60, 12, pal::mint);
      snprintf(b1, sizeof b1, "%d", (int)std::round(mc.pan));         sliderRow("Pan", b1, mc.pan, -100, 100, pal::mint);
    }
    // ── VIDEO ──
    section("VIDEO", pal::coral, nullptr);
    {
      int iw = 0, ih = 0; bool isImg = mk == MEDIA_IMAGE && MediaImageSize(mc.media, iw, ih);
      std::string nm = mc.media.empty() ? mc.name : std::filesystem::path(mc.media).filename().string();
      TextEll(ox + 8, oy + y + 6, W - 16, UI_B, 10, K(pal::tcc), nm.c_str());
      char info[96];
      if (isImg) snprintf(info, sizeof info, "Image, %dx%d", iw, ih);
      else if (mk == MEDIA_VIDEO) snprintf(info, sizeof info, "Video file \xC2\xB7 decoder not available yet");
      else snprintf(info, sizeof info, "Procedural generator, %dx%d", A.canvasW, A.canvasH);
      Text(ox + 8, oy + y + 20, MONO_R, 9, K(pal::t66), info);
      char du[32]; int ts = (int)std::round(ClipSeconds(mc)); snprintf(du, sizeof du, "00:%02d:%02d", ts / 60, ts % 60);
      Text(ox + 8, oy + y + 32, MONO_R, 9, K(pal::t66), du);
      y += 44;
      // R G B render channels (real); A is greyed — clips have no alpha channel to switch
      Text(ox + 8, oy + y + 9, UI_S, 10, K(pal::t88), "Channels");
      const char* cl[4] = {"R", "G", "B", "A"}; uint32_t chx[4] = {pal::red, pal::mint, pal::cyan, pal::t66};
      for (int i = 0; i < 4; ++i) {
        ImRect br(ox + W - 8 - (4 - i) * 30 + 2, oy + y, ox + W - 8 - (4 - i) * 30 + 28, oy + y + 18);
        if (chip(br, cl[i], i < 3 ? (mc.chan >> i & 1) != 0 : false, chx[i], i < 3)) mc.chan ^= 1 << i;
      }
      y += 18 + 8;
    }
    snprintf(b1, sizeof b1, "%d %%", (int)std::round(mc.opacity));   sliderRow("Opacity", b1, mc.opacity, 0, 100, pal::coral);
    {
      Text(ox + 8, oy + y + 5, UI_S, 10, K(pal::t88), "Size (W \xC3\x97 H)");
      TextR(ox + W - 8, oy + y + 5, MONO_R, 8, K(pal::t66), "NOT ACTIVE YET");
      y += 10 + 4;
      static int cwid = 0, chei = 0;
      ImGuiID idW = ImGui::GetID("##clw"), idH = ImGui::GetID("##clh");
      int iw = 0, ih = 0; bool isImg = mk == MEDIA_IMAGE && MediaImageSize(mc.media, iw, ih);
      if (ImGui::GetActiveID() != idW) cwid = mc.width > 0 ? mc.width : isImg ? iw : A.canvasW;
      if (ImGui::GetActiveID() != idH) chei = mc.height > 0 ? mc.height : isImg ? ih : A.canvasH;
      float fw = (W - 16 - 22) / 2.f;
      IntField("##clw", Rc(ox + 8, oy + y, fw, 24), cwid);
      if (ImGui::IsItemDeactivatedAfterEdit()) mc.width = std::clamp(cwid, 1, 16384);
      TextC(ox + 8 + fw + 11, oy + y + 12, MONO_B, 11, K(pal::t66), "\xC3\x97");
      IntField("##clh", Rc(ox + 8 + fw + 22, oy + y, fw, 24), chei);
      if (ImGui::IsItemDeactivatedAfterEdit()) mc.height = std::clamp(chei, 1, 16384);
      y += 24 + 6;
    }
    dropRow("Blend Mode", {"Layer Determined", "Normal", "Add", "Screen", "Multiply", "Overlay", "Difference", "Lighten", "Darken"}, mc.blend);
    // ── TRANSFORM ──
    HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 8;
    Text(ox + 8, oy + y + 5, UI_B, 9, K(pal::t88), "TRANSFORM", 0.14f);
    {
      ImRect rb(ox + W - 8 - 40, oy + y - 3, ox + W - 8, oy + y + 13);
      Hit hh = HitR(rb);
      Box(rb, hh.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
      TextC((rb.Min.x + rb.Max.x) * 0.5f, (rb.Min.y + rb.Max.y) * 0.5f, MONO_B, 9, K(hh.hover ? pal::white : pal::t88), "RESET", 0.09f);
      if (hh.hover) CursorHand();
      if (hh.click) { mc.posX = mc.posY = mc.rotation = mc.anchorX = mc.anchorY = 0; mc.scale = 1; mc.flipH = mc.flipV = false; }
    }
    y += 9 + 6;
    {
      // Position is stored in art units (the composite is 960 wide) but shown in canvas pixels like the rest of the panel.
      float kx = A.canvasW / 960.f, ky = A.canvasH / 540.f, hw = (float)A.canvasW, hh2 = (float)A.canvasH;
      float px = mc.posX * kx, py = mc.posY * ky, scp = mc.scale * 100.f;
      snprintf(b1, sizeof b1, "%d px", (int)std::round(px));   if (sliderRow("Position X", b1, px, -hw * 0.5f, hw * 0.5f, pal::cyan)) mc.posX = px / kx;
      snprintf(b1, sizeof b1, "%d px", (int)std::round(py));   if (sliderRow("Position Y", b1, py, -hh2 * 0.5f, hh2 * 0.5f, pal::cyan)) mc.posY = py / ky;
      snprintf(b1, sizeof b1, "%d %%", (int)std::round(scp));  if (sliderRow("Scale", b1, scp, 10, 400, pal::coral)) mc.scale = scp / 100.f;
      snprintf(b1, sizeof b1, "%d\xC2\xB0", (int)std::round(mc.rotation)); sliderRow("Rotation", b1, mc.rotation, -180, 180, pal::yellow);
      snprintf(b1, sizeof b1, "%d px", (int)std::round(mc.anchorX)); sliderRow("Anchor X", b1, mc.anchorX, -hw * 0.5f, hw * 0.5f, pal::t88);
      snprintf(b1, sizeof b1, "%d px", (int)std::round(mc.anchorY)); sliderRow("Anchor Y", b1, mc.anchorY, -hh2 * 0.5f, hh2 * 0.5f, pal::t88);
      float bw2 = (W - 16 - 4) / 2.f;
      if (chip(ImRect(ox + 8, oy + y, ox + 8 + bw2, oy + y + 22), "FLIP H", mc.flipH, pal::cyan)) mc.flipH = !mc.flipH;
      if (chip(ImRect(ox + 8 + bw2 + 4, oy + y, ox + W - 8, oy + y + 22), "FLIP V", mc.flipV, pal::cyan)) mc.flipV = !mc.flipV;
      y += 22 + 8;
    }
    // ── EFFECTS: one section per effect on this clip ──
    {
      std::vector<Fx>& chain = mc.fx;
      HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 8;
      float addW = TextW(MONO_B, 9, "ADD", 0.09f) + 11 + 4 + 12;
      Text(ox + 8, oy + y + 9, UI_B, 9, K(pal::t88), "EFFECTS", 0.14f);
      ImRect addB(ox + W - 8 - addW, oy + y, ox + W - 8, oy + y + 20);
      Hit ah = HitR(addB);
      Box(addB, K(pal::g1c), ah.hover ? K(pal::cyan) : K(pal::g22), 3);
      Icon("plus", ImVec2(addB.Min.x + 6 + 5.5f, oy + y + 10), 11, K(ah.hover ? pal::cyan : pal::tcc));
      Text(addB.Min.x + 6 + 11 + 4, oy + y + 10, MONO_B, 9, K(ah.hover ? pal::cyan : pal::tcc), "ADD", 0.09f);
      if (ah.hover) CursorHand();
      if (ah.click) {
        std::vector<ui::MenuItem> items;
        for (int k2 = 0; k2 < FX_COUNT; ++k2) { ui::MenuItem m; m.label = FX_LIB[k2].name; m.icon = FX_LIB[k2].icon; int kk = k2; m.run = [kk] { A.addFx(kk); }; items.push_back(std::move(m)); }
        A.openCtx(ImGui::GetIO().MousePos, std::move(items));
      }
      y += 20 + 6;
      if (chain.empty()) {
        Text(ox + 8, oy + y + 7, UI_S, 10, K(pal::t66), "Drag an effect here from the Browser, or use ADD.", 0.01f);
        y += 15 + 6;
      }
      int removeIdx = -1;
      for (int i = 0; i < (int)chain.size(); ++i) {
        Fx& f = chain[i];
        const FxDef& d = FX_LIB[std::clamp(f.kind, 0, FX_COUNT - 1)];
        uint32_t tn = FxTone(d.tone);
        HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 6;
        // header: icon + name, then bypass (eye) and remove (x); right-click for the full menu
        ImRect hr(ox + 8, oy + y, ox + W - 8, oy + y + 22);
        Hit hh = HitR(hr);
        float cy2 = oy + y + 11;
        float prevA = g.alpha; if (!f.on) g.alpha *= 0.45f;
        char nn[8]; snprintf(nn, sizeof nn, "%02d", i + 1);
        Text(hr.Min.x, cy2, MONO_B, 9, K(pal::t66), nn);
        Icon(d.icon, ImVec2(hr.Min.x + 22 + 6, cy2), 12, K(tn));
        Text(hr.Min.x + 22 + 16, cy2, UI_B, 10, K(tn), Upper(d.name).c_str(), 0.14f);
        g.alpha = prevA;
        ImRect xr(hr.Max.x - 16, cy2 - 8, hr.Max.x, cy2 + 8), er(hr.Max.x - 36, cy2 - 8, hr.Max.x - 20, cy2 + 8);
        Hit xh = HitR(xr), eh = HitR(er);
        Icon("x", ImVec2((xr.Min.x + xr.Max.x) * 0.5f, cy2), 10, K(xh.hover ? pal::red : pal::t66));
        Icon(f.on ? "eye" : "eye-off", ImVec2((er.Min.x + er.Max.x) * 0.5f, cy2), 11, K(eh.hover ? pal::white : pal::t88));
        if (xh.hover || eh.hover) CursorHand();
        if (xh.click) removeIdx = i;
        else if (eh.click) f.on = !f.on;
        else if (hh.rclick) {
          int ii = i;
          std::vector<ui::MenuItem> items;
          auto mkI = [&](const char* l, const char* ic, bool dis, bool dg, std::function<void()> fn) { ui::MenuItem m; m.label = l; m.icon = ic; m.disabled = dis; m.danger = dg; m.run = fn; items.push_back(std::move(m)); };
          mkI(f.on ? "Bypass effect" : "Enable effect", f.on ? "eye-off" : "eye", false, false, [ii] { auto& c = A.fxChain(); if (ii < (int)c.size()) c[ii].on = !c[ii].on; });
          mkI("Move up", "arrow-up", ii <= 0, false, [ii] { A.moveFx(ii, -1); });
          mkI("Move down", "arrow-down", ii >= (int)chain.size() - 1, false, [ii] { A.moveFx(ii, 1); });
          mkI("Duplicate", "copy", false, false, [ii] { A.dupFx(ii); });
          mkI("Reset parameters", "rotate-ccw", false, false, [ii] { A.resetFx(ii); });
          mkI("Remove effect", "trash-2", false, true, [ii] { A.removeFx(ii); });
          A.openCtx(ImGui::GetIO().MousePos, std::move(items));
        }
        y += 22 + 4;
        for (int pi = 0; pi < d.nparams; ++pi) {
          std::string vl = FxFmt(f.kind, pi, f.p[pi]);
          Text(ox + 8, oy + y + 5, UI_S, 10, K(pal::t88), d.pl[pi] ? d.pl[pi] : "");
          TextR(ox + W - 8, oy + y + 5, MONO_B, 10, K(pal::tf3), vl.c_str());
          y += 10 + 4;
          Slider(0x3000 + i * 8 + pi, Rc(ox + 8, oy + y + 4, W - 16, 6), f.p[pi], tn);
          y += 14 + 4;
        }
        if (d.enumLabel) {
          Text(ox + 8, oy + y + 5, UI_S, 10, K(pal::t88), d.enumLabel);
          y += 10 + 4;
          float ew = (W - 16 - 8) / 3.f;
          for (int oi = 0; oi < 3 && d.opts[oi]; ++oi)
            if (chip(ImRect(ox + 8 + oi * (ew + 4), oy + y, ox + 8 + oi * (ew + 4) + ew, oy + y + 22), d.opts[oi], f.en == oi, tn)) f.en = oi;
          y += 22 + 6;
        }
        snprintf(b1, sizeof b1, "%d %%", (int)std::round(f.mix));
        Text(ox + 8, oy + y + 5, UI_S, 10, K(pal::t88), "Dry / Wet");
        TextR(ox + W - 8, oy + y + 5, MONO_B, 10, K(pal::tf3), b1);
        y += 10 + 4;
        Slider(0x3800 + i, Rc(ox + 8, oy + y + 4, W - 16, 6), f.mix, pal::coral);
        y += 14 + 4;
        float tw3 = (W - 16 - 4) / 2.f;
        if (chip(ImRect(ox + 8, oy + y, ox + 8 + tw3, oy + y + 22), "BEAT", f.beat, pal::yellow)) f.beat = !f.beat;
        if (chip(ImRect(ox + 8 + tw3 + 4, oy + y, ox + W - 8, oy + y + 22), "AUDIO", f.react, pal::mint)) f.react = !f.react;
        y += 22 + 8;
      }
      if (removeIdx >= 0) A.removeFx(removeIdx);
    }
    }   // editable
  }
  sa.End(W, y + 4);
}

// ───────────────────────── browser ─────────────────────────
// Matches HTML browserItems: Sources (4) · Generators (2, ∞) · Effects (8 FX_LIB, "FX") · Composition (1).
// Folders collapse via A.browserOpen (absent = open); FX double-click adds to selected layer.
static void Browser(ImRect r) {
  Fill(r, K(pal::g14));
  VLine(r.Max.x - 1, r.Min.y, r.Max.y, K(pal::g2a));
  PanelHeader(Rc(r.Min.x, r.Min.y, r.GetWidth() - 1, 24), "Browser", pal::t88);
  static ScrollArea sa;
  sa.Begin("##browser", ImRect(r.Min.x, r.Min.y + 24, r.Max.x - 1, r.Max.y));
  float ox = sa.origin.x, oy = sa.origin.y, W = r.GetWidth() - 1;
  float y = 2;
  // Build rows dynamically so Effects always mirrors FX_LIB (8 items, correct icons).
  struct Row { std::string name, icon, dur; int depth; int fxKind; bool folder; bool open; int count; std::string path; };
  std::vector<Row> rows;
  auto folder = [&](const char* n, const char* ic) {
    bool open = A.browserOpen.find(n) == A.browserOpen.end() ? true : A.browserOpen[n];
    rows.push_back({n, ic, "", 0, -1, true, open, 0, ""});
    return (int)rows.size() - 1;
  };
  auto item = [&](int fi, const char* n, const char* ic, const char* d, int fx) {
    rows.push_back({n, ic, d ? d : "", 1, fx, false, true, 0, ""});
    rows[fi].count++;
  };
  // Files dragged in from the OS: dropping anywhere on the Browser adds them to the Media list (referenced in place).
  if (A.osDrop.pending && r.Contains(A.osDrop.pos)) {
    int n = ImportMedia(A.osDrop.paths);
    A.osDrop.handled = true;
    A.browserOpen["Media"] = true;
    A.notify(n > 0 ? "Added " + std::to_string(n) + (n == 1 ? " file" : " files") + " to Media" : "Nothing new to add \xE2\x80\x94 drop an image, video or audio file");
  }
  if (A.mediaStale) RebuildMediaList();
  int fMed = folder("Media", "folder");   // B2: images/video/audio from Documents/MikMap/media + anything dropped in
  for (auto& p : A.mediaList) {
    MediaKind mk = MediaKindOf(p);
    item(fMed, std::filesystem::path(p).filename().string().c_str(), mk == MEDIA_VIDEO ? "film" : mk == MEDIA_AUDIO ? "audio-waveform" : "frame", "\xE2\x88\x9E", -1);
    rows.back().path = p;
  }
  if (A.mediaList.empty()) rows.push_back({"Drop image / video / audio files here", "info", "", 1, -3, false, true, 0, ""});
  int fSrc = folder("Sources", "folder");
  item(fSrc, "Particle Vortex", "video", "10s", -1); item(fSrc, "Cyber Hex Grid", "video", "8s", -1);
  item(fSrc, "Plasma Waves 01", "video", "12s", -1); item(fSrc, "Strobe Tunnel", "video", "6s", -1);
  item(fSrc, "Aurora Flow", "video", "16s", -1); item(fSrc, "Starfield Warp", "video", "14s", -1); item(fSrc, "Concentric Tunnel", "video", "9s", -1);
  int fGen = folder("Generators", "folder");
  item(fGen, "Spectrum Bars", "audio-waveform", "\xE2\x88\x9E", -1); item(fGen, "Beat Pulse", "activity", "\xE2\x88\x9E", -1); item(fGen, "Neon Rain", "video", "\xE2\x88\x9E", -1);
  int fFx = folder("Effects", "folder");
  for (int k = 0; k < FX_COUNT; ++k) item(fFx, FX_LIB[k].name, FX_LIB[k].icon, "FX", k);
  int fComp = folder("Composition", "folder");
  item(fComp, A.projectName.c_str(), "layers", "", -1);
  bool parentOpen = true;
  for (const Row& it : rows) {
    if (it.folder) { parentOpen = it.open; }
    else if (!parentOpen) continue;
    ImRect rr(ox, oy + y, ox + W, oy + y + 22);
    bool on = !it.folder && A.browserSel == it.name;
    Hit h = HitR(rr);
    if (on) Fill(rr, K(pal::g18));
    Fill(Rc(rr.Min.x, rr.Min.y, 2, 22), on ? K(pal::cyan) : 0);
    float px = rr.Min.x + 6 + it.depth * 12;
    if (it.folder) {
      Icon(it.open ? "chevron-down" : "chevron-right", ImVec2(rr.Min.x + 6 + 5, rr.Min.y + 11), 10, K(pal::t66));
      px += 10;
      Icon(it.open ? "folder-open" : "folder", ImVec2(px + 5, rr.Min.y + 11), 10, K(pal::t88));
    } else {
      Icon(it.icon.c_str(), ImVec2(px + 5, rr.Min.y + 11), 10, K(pal::t77));
    }
    std::string nm = it.depth ? it.name : Upper(it.name);
    ImU32 fg = K(on ? pal::white : it.depth ? pal::tcc : pal::t88);
    char cnt[8] = {};
    if (it.folder) snprintf(cnt, sizeof cnt, "%d", it.count);
    float durW = 0;
    if (it.folder) durW = TextW(MONO_M, 10, cnt) + 6;
    else if (!it.dur.empty()) durW = TextW(MONO_M, 10, it.dur.c_str()) + 6;
    TextEll(px + 10 + 6, rr.Min.y + 11, rr.Max.x - (px + 16) - durW - 6, it.depth ? UI_S : UI_B, 10, fg, nm.c_str(), it.depth ? 0.01f : 0.09f);
    if (it.folder) TextR(rr.Max.x - 6, rr.Min.y + 11, MONO_M, 10, K(pal::t66), cnt);
    else if (!it.dur.empty()) TextR(rr.Max.x - 6, rr.Min.y + 11, MONO_M, 10, K(pal::t66), it.dur.c_str());
    if (h.hover) CursorHand();
    if (h.click) {
      if (it.folder) {
        bool cur = A.browserOpen.find(it.name) == A.browserOpen.end() ? true : A.browserOpen[it.name];
        A.browserOpen[it.name] = !cur;
        if (it.name == "Media") A.mediaStale = true;   // rescan the folder next frame
      } else if (it.fxKind != -3) A.browserSel = it.name;
    }
    if (!it.folder && it.fxKind >= 0 && h.dbl) A.addFx(it.fxKind);
    if (!it.folder && it.fxKind != -3 && h.click) { A.browserPress = true; A.browserPressPos = ImGui::GetIO().MousePos; A.dragSrcCand.name = it.name; A.dragSrcCand.dur = it.dur; A.dragSrcCand.fxKind = it.fxKind; A.dragSrcCand.media = it.path; }
    if (!it.folder && h.rclick && it.fxKind >= 0) { int fk = it.fxKind; std::vector<MenuItem> mi; MenuItem a; a.label = "Add to " + (A.layers[A.selLi].clips[A.selCi].name.empty() ? std::string("clip") : A.layers[A.selLi].clips[A.selCi].name); a.icon = "plus"; a.run = [fk] { A.addFx(fk); }; mi.push_back(a); MenuItem b; b.label = "Show FX chain"; b.icon = "wand-sparkles"; b.run = [] { A.tab = 2; }; mi.push_back(b); A.openCtx(ImGui::GetIO().MousePos, mi); }
    y += 22;
  }
  sa.End(W, y + 2);
}

// ───────────────────────── deck ─────────────────────────
static void ColumnHeader(ImRect r, int i, bool active, int layerCount, bool inert = false) {
  Hit h = inert ? Hit() : HitR(r);   // inert: this header is half-hidden under the pinned Layers strip, so the strip owns the click
  bool isDragCol = A.dragCol == i, isDropCol = A.dragCol >= 0 && A.dropCol == i && A.dragCol != i;
  float prevA = g.alpha; if (isDragCol && A.dragging) g.alpha *= 0.45f;
  // `active` only means "this is the current column" (last click / arrow keys) — it can be true for an empty
  // column (e.g. activeCol's default on a brand new project). The glow, filled badge and pulsing dot must be
  // reserved for a column that is actually LIVE (>=1 layer playing there); otherwise an empty "current" column
  // renders identically to one that is really live, which reads as "something is playing" when nothing is.
  bool live = active && layerCount > 0;
  if (live) { Glow(r, pal::coral, 0.30f, 12, 4); Fill(r, K(pal::g12), 4); }
  Box(r, live ? K(pal::coral, 0.15f) : K(pal::g1c), live ? K(pal::coral) : active ? K(pal::coral, 0.5f) : h.hover ? K(pal::g33) : K(pal::g22), 4);
  ImU32 fg = K(active ? pal::coral : h.hover ? pal::white : pal::tcc);
  float cy = (r.Min.y + r.Max.y) * 0.5f;
  Icon("play", ImVec2(r.Min.x + 8 + 4.5f, cy), 9, fg);
  std::string labS = A.colName(i); const char* lab = labS.c_str();
  float right = r.Max.x - 8;
  if (live) {
    float pulse = 0.7f + 0.3f * cosf((float)g.time * 2.f * 3.14159f / (A.beat ? 0.5f : 1.4f));
    Dot(ImVec2(right - 3.5f, cy), 7, pal::mint, true, pulse);
    right -= 7 + 4;
    char lc[8]; snprintf(lc, sizeof lc, "%dL", layerCount);
    float bw = TextW(MONO_B, 8, lc, 0.09f) + 8 + 2;
    ImRect br(right - bw, cy - 7, right, cy + 7);
    Box(br, K(pal::coral, 0.2f), K(pal::coral), 2);
    Text(br.Min.x + 5, cy, MONO_B, 8, K(pal::coral), lc, 0.09f);
    right = br.Min.x - 6;
  } else {
    float bw = TextW(MONO_B, 8, "TRIG", 0.09f) + 8 + 2;
    ImRect br(right - bw, cy - 7, right, cy + 7);
    Box(br, K(pal::g1c), K(pal::g22), 2);
    Text(br.Min.x + 5, cy, MONO_B, 8, K(h.hover ? pal::tcc : pal::t66), "TRIG", 0.09f);
    right = br.Min.x - 6;
  }
  float labX = r.Min.x + 8 + 9 + 6;
  if (A.autoStartCol == i) {   // Setting: this column fires automatically when the project is opened
    Icon("zap", ImVec2(labX + 5, cy), 10, K(pal::yellow));
    labX += 14;
  }
  TextEll(labX, cy, right - labX, UI_B, 11, fg, lab);
  g.alpha = prevA;
  if (isDropCol) DashedRect(Rc(r.Min.x - 1, r.Min.y - 1, r.GetWidth() + 2, r.GetHeight() + 2), K(pal::yellow), 2);
  if (h.hover) CursorHand();
  if (h.click) { A.dragCol = -2 - i; A.dragStart = ImGui::GetIO().MousePos; }
  if (h.hover && A.dragging && A.dragCol >= 0) A.dropCol = i;
  if (h.rclick) { A.colMenu.open = true; A.colMenu.ci = i; A.colMenu.pos = ImGui::GetIO().MousePos; }
  // A6: a plain click (press+release, no drag) fires every non-empty clip in the column across all layers;
  // fireColumn() also selects the column, so this replaces the old select-only click / double-click-to-fire split.
  if (h.hover && h.release && !A.dragging && A.dragCol == -2 - i) A.fireColumn(i);
}

// Add-Layer/Group/Column + Sync buttons — the design's RUN MODE row reserves this slot for mode-specific
// controls (loop switches in Timeline mode, see TimelineView); Grid mode's own controls live here instead,
// since the reference has no other spot for "add a layer/column" and these were already working features.
// The Grid|Timeline toggle sits at the top-left of the grid (where the old DECK TOOLS button was); the deck's structure actions
// (add layer / group / column, Sync) are in the DECK tab's menu (right-click a tab, or its little arrow), see main.cpp.
static bool gDeckMenuInBar = false;                 // the deck tabs live in the transport bar (else Deck() draws its own rows)
static void SystemTimeStr(char (&out)[16]) {
  std::snprintf(out, sizeof out, "--:--:--");
  std::time_t tt = std::time(nullptr); std::tm tmv{};
#if defined(_WIN32)
  localtime_s(&tmv, &tt);
#else
  localtime_r(&tt, &tmv);
#endif
  std::strftime(out, sizeof out, "%H:%M:%S", &tmv);
}
static float DeckModeToggle(float x, float cy, float h);

static void DeckGrid(ImRect r) {
  Fill(r, K(pal::g12));
  if (A.dragLi >= 0 && !A.dragging && ImGui::IsMouseDown(0)) {
    ImVec2 m = ImGui::GetIO().MousePos;
    if (std::hypot(m.x - A.dragStart.x, m.y - A.dragStart.y) > 5.f) A.dragging = true;
  }
  if (A.dragCol <= -2 && ImGui::IsMouseDown(0)) {
    ImVec2 m = ImGui::GetIO().MousePos;
    if (std::hypot(m.x - A.dragStart.x, m.y - A.dragStart.y) > 5.f) { A.dragCol = -2 - A.dragCol; A.dragging = true; A.dropCol = -1; }
  }
  if (A.dragCol >= 0) A.dropCol = -1;
  if (A.dragging || A.dragSrc.active) { A.dropLi = A.dropCi = -1; ImGui::SetMouseCursor(ImGuiMouseCursor_Hand); }
  static ScrollArea sa;
  ImRect area(r.Min.x, r.Min.y, r.Max.x, r.Max.y);   // the tabs/run-mode header now lives above `r`, drawn by the Deck() wrapper
  sa.Begin("##deck", area, true);
  float ox = sa.origin.x, oy = sa.origin.y;
  const float P = 4, LW = 178, CW = 128, GAP = 4;
  auto colX = [&](int i) { return ox + P + LW + GAP + i * (CW + GAP); };
  const int NC = A.colCount();
  // The pinned Layers strip occupies [pinX - P, pinRight] on screen. Everything that scrolls horizontally is
  // drawn BEFORE the strip and clipped to `pinRight`, so a half-scrolled column is cut cleanly at the strip's
  // edge instead of painting over the layer/group boxes; `hiddenByPin` skips the ones entirely behind it, and
  // `mouseUnderPin` stops a click on the strip from also reaching whatever scrolled underneath that spot.
  const float pinX = std::max(ox + P, area.Min.x + P);
  const float pinRight = pinX + LW + P;
  const bool pinned = pinX > ox + P + 0.5f;   // actually scrolled: content is passing under the strip
  const bool mouseUnderPin = pinned && ImGui::GetIO().MousePos.x < pinRight;
  auto hiddenByPin = [&](int i) { return colX(i) + CW <= pinRight; };

  // Layout pass: just the Y position/height of every row (group header or layer), no drawing. Clip cells use
  // it directly below (they never move horizontally); the pinned Layers strip replays it further down at a
  // clamped X so it can float on top of whatever has scrolled underneath, like a spreadsheet's frozen column.
  struct RowEntry { bool isGroup; std::string groupId; int li; float y, h; };
  std::vector<RowEntry> rowsLayout;
  {
    float yy = P + 30 + GAP;
    std::string lastGroup = "?";
    for (int li = 0; li < (int)A.layers.size(); ++li) {
      Layer& l = A.layers[li];
      if (l.group != lastGroup) {
        lastGroup = l.group;
        if (!l.group.empty()) { rowsLayout.push_back({true, l.group, -1, yy, 26.f}); yy += 26 + GAP; }
      }
      Group* gp = l.group.empty() ? nullptr : A.group(l.group);
      bool coll = l.collapsed || (gp && !gp->open);
      float rowH = coll ? 38.f : 92.f;
      rowsLayout.push_back({false, "", li, yy, rowH});
      yy += rowH + GAP;
    }
    (void)yy;
  }
  float y = rowsLayout.empty() ? (P + 30 + GAP) : (rowsLayout.back().y + rowsLayout.back().h);

  // Scrolling pass: clip cells AND the per-column group cue boxes, at their normal (horizontally scrolling)
  // position, clipped to the right of the pinned strip. The cue boxes used to be drawn down in the pinned pass
  // right after the group's own identity box, so a column caught half-under the strip painted its "Cue N" box
  // straight over the group's name and fader.
  g.dl->PushClipRect(ImVec2(pinRight, area.Min.y), ImVec2(area.Max.x, area.Max.y), true);
  for (auto& re : rowsLayout) {
    if (re.isGroup) {
      Group* gp = A.group(re.groupId);
      if (!gp) continue;
      for (int ci = 0; ci < NC; ++ci) {
        if (hiddenByPin(ci)) continue;   // fully behind the pinned strip — the strip covers it, and skipping keeps it from eating the click too
        ImRect cr(colX(ci), oy + re.y, colX(ci) + CW, oy + re.y + 26);
        Hit ch = mouseUnderPin ? Hit() : HitR(cr);
        bool act = gp->activeCol == ci;
        // blur kept under the 4px GAP to the next Cue box — the default blur=12 used for clip cells swallows
        // that gap and bleeds over its neighbours
        if (act) { Glow(cr, pal::coral, 0.30f, 3, 2); Fill(cr, K(pal::g12), 2); }
        Box(cr, act ? K(pal::coral, 0.2f) : K(pal::g1c), act ? K(pal::coral) : ch.hover ? K(pal::g33) : K(pal::g22), 2);
        char lab[16]; snprintf(lab, sizeof lab, "Cue %d", ci + 1);
        TextC((cr.Min.x + cr.Max.x) * 0.5f, (cr.Min.y + cr.Max.y) * 0.5f, MONO_B, 9, K(act ? pal::coral : ch.hover ? pal::white : pal::t66), lab, 0.09f);
        if (ch.hover) CursorHand();
        if (ch.click) A.selectGroupCue(gp->id, ci);
      }
      continue;
    }
    int li = re.li; Layer& l = A.layers[li];
    for (int ci = 0; ci < NC; ++ci) {
      if (hiddenByPin(ci)) continue;
      ImRect cr(colX(ci), oy + re.y, colX(ci) + CW, oy + re.y + re.h);
      bool selc = false;
      for (auto& sc : A.selectedCells) if (sc.first == li && sc.second == ci) selc = true;
      Clip& c = l.clips[ci];
      bool isDrag = A.dragging && A.dragLi == li && A.dragCi == ci;
      bool isDrop = (A.dragging || A.dragSrc.active) && A.dropLi == li && A.dropCi == ci;
      CellOut o = ClipCell(cr, c, selc, isDrag, isDrop, c.progress, re.h < 60.f);   // collapsed layer row: no thumbnail
      if (A.osDrop.pending && !A.osDrop.handled && !mouseUnderPin && cr.Contains(A.osDrop.pos) && area.Contains(A.osDrop.pos)) {
        A.dropFilesOnCell(li, ci, A.osDrop.paths);   // file dragged in from the OS straight onto this clip
        A.osDrop.handled = true;
      }
      if (o.hover && !mouseUnderPin) {
        // bar: press arms a possible move-drag and, on plain release, only cues (selects/previews, never plays).
        if (o.barPress) { A.pressLi = li; A.pressCi = ci; A.dragStart = ImGui::GetIO().MousePos; if (c.st != Clip::Empty && c.st != Clip::Armed) { A.dragLi = li; A.dragCi = ci; } }
        if (A.dragging || A.dragSrc.active) { A.dropLi = li; A.dropCi = ci; }
        if (o.barRclick) { A.pop.open = true; A.pop.pos = ImGui::GetIO().MousePos; A.pop.li = li; A.pop.ci = ci; }
        else if (o.bodyClick) { A.cue(li, ci); A.trigger(li, ci); A.pressLi = -1; A.tab = 2; }   // body: plays immediately, no drag/right-click
        else if (o.barRelease && !A.dragging && A.pressLi == li && A.pressCi == ci) { A.cue(li, ci); A.pressLi = -1; A.tab = 2; }   // selecting a clip shows ITS properties, not whatever tab was open
      }
    }
  }
  g.dl->PopClipRect();

  float contentW = P + LW + GAP + NC * (CW + GAP) + P - GAP, contentH = y + P;

  // Pinned Layers strip (design request: never let the layer boxes scroll out of view horizontally) — a
  // background wash the full height of the visible area, then group headers + LayerRow redrawn on top at a
  // clamped X so they occlude whatever cells have scrolled underneath, while still scrolling normally on Y.
  {
    Fill(ImRect(pinX - P, area.Min.y, pinX + LW + P, area.Max.y), K(pal::g12));
    for (auto& re : rowsLayout) {
      if (re.isGroup) {
        Group* gp = A.group(re.groupId);
        if (!gp) continue;
        ImRect gr(pinX, oy + re.y, pinX + LW, oy + re.y + 26);
        Hit gh = HitR(gr);
        Fill(gr, MixHex(pal::g12, RoleHex(gp->role), 0.10f));
        HLine(gr.Min.x, gr.Max.x, gr.Max.y - 1, K(pal::g2a));
        Fill(Rc(gr.Min.x, gr.Min.y, 3, 26), K(RoleHex(gp->role)));
        float cy = (gr.Min.y + gr.Max.y) * 0.5f;
        Icon(gp->open ? "chevron-down" : "chevron-right", ImVec2(gr.Min.x + 6 + 3 + 5, cy), 10, K(RoleHex(gp->role)));
        std::string nm = Upper(gp->name);
        int nInGroup = 0; for (auto& q : A.layers) if (q.group == re.groupId) ++nInGroup;
        char cn[8]; snprintf(cn, sizeof cn, "%d", nInGroup);
        // Name + member count end before the master fader (gr.Max.x - 66); a long name is shortened with "…" instead
        // of running under the fader.
        float nx = gr.Min.x + 3 + 6 + 10 + 4;
        float nameMax = (gr.Max.x - 66 - 8) - TextW(MONO_M, 10, cn) - 4 - nx;
        TextEll(nx, cy, nameMax, UI_B, 11, K(pal::white), nm.c_str(), 0.09f);
        float nw = std::min(TextW(UI_B, 11, nm.c_str(), 0.09f), nameMax);
        Text(nx + nw + 4, cy, MONO_M, 10, K(pal::t77), cn);
        if (gh.hover) CursorHand();
        {   // A13: master fader on the right of the header; pressing it must not collapse the group
          ImRect sr(gr.Max.x - 66, cy - 3, gr.Max.x - 30, cy + 3);
          bool overFader = ImRect(sr.Min.x - 4, gr.Min.y, gr.Max.x, gr.Max.y).Contains(ImGui::GetIO().MousePos);
          Slider(0x5000 + (uint32_t)(gp - &A.groups[0]), sr, gp->opacity, RoleHex(gp->role));
          char gpb[8]; snprintf(gpb, sizeof gpb, "%d%%", (int)std::round(gp->opacity));
          TextR(gr.Max.x - 6, cy, MONO_M, 9, K(pal::tcc), gpb);
          if (gh.click && !overFader) gp->open = !gp->open;
        }
        if (gh.rclick) {   // group menu: rename, cycle the colour role, or dissolve the group (layers are kept)
          std::string gid = gp->id; std::vector<MenuItem> mi; ImVec2 mp = ImGui::GetIO().MousePos;
          MenuItem a; a.label = "Rename group"; a.icon = "pencil";
          a.run = [gid, mp] { for (int k = 0; k < (int)A.groups.size(); ++k) if (A.groups[k].id == gid) A.beginRename(3, k, mp, A.groups[k].name); };
          mi.push_back(a);
          MenuItem b; b.label = "Change color"; b.icon = "palette";
          b.run = [gid] { if (Group* q = A.group(gid)) q->role = (q->role + 1) % 3; };
          mi.push_back(b);
          MenuItem c; c.label = "Ungroup"; c.icon = "trash-2"; c.danger = true;
          c.run = [gid] {
            for (auto& q : A.layers) if (q.group == gid) q.group.clear();
            A.groups.erase(std::remove_if(A.groups.begin(), A.groups.end(), [&](const Group& x) { return x.id == gid; }), A.groups.end());
          };
          mi.push_back(c);
          A.openCtx(mp, mi);
        }
      } else {
        ImRect lr(pinX, oy + re.y, pinX + LW, oy + re.y + re.h);
        LayerRow(lr, re.li, sa);
      }
    }
    // soft edge while scrolled, so the cut-off column reads as passing UNDER the strip rather than being
    // glued to the layer box next to it
    if (pinned)
      for (int k = 0; k < 6; ++k)
        g.dl->AddRectFilled(ImVec2(pinRight + k, area.Min.y), ImVec2(pinRight + k + 1, area.Max.y),
                            Ca(K(0x000000, 0.22f * (1.f - k / 6.f))));
  }

  // sticky column header row (pinned on Y the same way the Layers strip above is pinned on X)
  {
    float sy = std::max(oy + P, area.Min.y + 0.f);
    ImRect strip(area.Min.x, sy, area.Max.x + 9999, sy + 30 + GAP);
    Fill(ImRect(ox, sy - P, ox + contentW, sy + 30 + GAP), K(pal::g12));
    g.dl->PushClipRect(ImVec2(pinRight, area.Min.y), ImVec2(area.Max.x, area.Max.y), true);   // cut at the strip, same as the cells below
    for (int i = 0; i < NC; ++i) {
      if (hiddenByPin(i)) continue;   // scrolled under the pinned "LAYERS" label — skip so it can't also eat this click
      bool act = A.activeCol == i;
      int nLive = 0; for (auto& lyr : A.layers) if (i < (int)lyr.clips.size() && lyr.clips[i].isLive()) ++nLive;
      ColumnHeader(ImRect(colX(i), sy, colX(i) + CW, sy + 30), i, act, nLive, mouseUnderPin);
    }
    g.dl->PopClipRect();
    // pinned "LAYERS" label drawn LAST so it stays on top of any column header that has
    // scrolled underneath it (same draw-order trick as the pinned Layers strip below)
    Fill(ImRect(pinX - P, sy - P, pinX + LW + P, sy + 30 + GAP), K(pal::g12));   // re-cover where it crosses the pinned Layers strip
    if (gDeckMenuInBar) DeckModeToggle(pinX + 2, sy + 15, 24);   // otherwise Deck() shows it in its own row
    (void)strip;
  }
  sa.End(contentW, contentH);
  if (A.dragSrc.active && ImGui::IsMouseReleased(0)) {
    if (A.dropLi >= 0) {
      if (A.dragSrc.fxKind >= 0) { A.cue(A.dropLi, A.dropCi); A.addFx(A.dragSrc.fxKind); }
      else A.loadClip(A.dropLi, A.dropCi, A.dragSrc.name, A.dragSrc.dur, A.dragSrc.media);
    }
    A.dragSrc = DragSrc(); A.browserPress = false; A.dropLi = A.dropCi = -1;
  }
  if (A.dragCol != -1 && ImGui::IsMouseReleased(0)) {
    if (A.dragging && A.dragCol >= 0 && A.dropCol >= 0) A.moveColTo(A.dragCol, A.dropCol);
    A.dragging = false; A.dragCol = -1; A.dropCol = -1;
  }
  if (A.dragLi >= 0 && ImGui::IsMouseReleased(0)) {
    if (A.dragging && A.dropLi >= 0) A.moveClip(A.dragLi, A.dragCi, A.dropLi, A.dropCi);
    A.dragging = false; A.dragLi = A.dragCi = A.dropLi = A.dropCi = A.pressLi = -1;
  }
}

static void TimelineView(ImRect r);  // implemented further down — Deck() below picks it or DeckGrid per A.deckMode

// The deck tabs live in the transport bar under the monitors (gDeckMenuInBar): one row less above the grid. When that bar is
// hidden or too short (Settings > Layout > timeline height) Deck() draws them itself in a row above the grid, so they can
// never disappear.

// Deck tabs (switch / rename on double-click / menu on right-click) drawn inside [x0, xMax] on the row centred at cy.
// They shrink (label ellipsised) when they don't fit; tabs that still don't fit are left out — right-click Add deck
// and Move left/right are on each tab's menu.
static void DeckTabs(float x0, float xMax, float cy) {
  // Same look as the top navigation (Composition / Advanced Mapping / Sensor I/O): one dark rounded group, the active
  // tab outlined and glowing in coral, the others plain grey text. The active tab carries a small arrow that opens the
  // deck menu (right-click any tab opens it too).
  int n = (int)A.decks.size();
  if (n == 0) return;
  const float pad = 3, gap = 3, tabH = 18, arrowW = 14;
  std::vector<float> wv(n); float tot = 0;
  for (int i = 0; i < n; ++i) { wv[i] = TextW(UI_B, 10, Upper(A.decks[i].name).c_str()) + 20 + (i == A.curDeckIdx ? arrowW : 0); tot += wv[i]; }
  float avail = xMax - x0 - 2 * pad - gap * (n - 1);
  float k = tot > avail ? std::max(0.3f, avail / tot) : 1.f;
  float gw = 2 * pad + gap * (n - 1) + tot * k;
  ImRect grp(x0, cy - tabH * 0.5f - pad, std::min(xMax, x0 + gw), cy + tabH * 0.5f + pad);
  Box(grp, K(pal::g12), K(pal::g2a), 4);
  float x = grp.Min.x + pad;
  for (int i = 0; i < n; ++i) {
    bool cur = i == A.curDeckIdx;
    float w = wv[i] * k;
    if (x + w > grp.Max.x) break;
    std::string lab = Upper(A.decks[i].name);
    ImRect tr(x, cy - tabH * 0.5f, x + w, cy + tabH * 0.5f);
    Hit h = HitR(tr);
    if (cur) { Glow(tr, pal::coral, 0.30f, 10, 3); Fill(tr, K(pal::g12), 3); Box(tr, K(pal::coral, 0.12f), K(pal::coral), 3); }
    ImU32 fg = K(cur ? pal::coral : h.hover ? pal::white : pal::t77);
    const float lw = w - (cur ? arrowW : 0.f);
    if (k < 1.f) TextEll(tr.Min.x + 8, cy, lw - 12, UI_B, 10, fg, lab.c_str());
    else Text(tr.Min.x + 10, cy, UI_B, 10, fg, lab.c_str());
    ImRect arrow(tr.Max.x - arrowW - 2, tr.Min.y, tr.Max.x, tr.Max.y);
    Hit ah2 = cur ? HitR(arrow) : Hit();
    if (cur) Icon("chevron-down", ImVec2(tr.Max.x - 10, cy), 9, K(ah2.hover ? pal::white : pal::coral));
    if (h.hover) CursorHand();
    if (cur && ah2.click) { A.deckMenu.open = true; A.deckMenu.idx = i; A.deckMenu.pos = ImVec2(tr.Min.x, tr.Max.y + 4); }
    else if (h.click) A.switchDeck(i);
    if (h.dbl) A.beginRename(4, i, ImVec2(tr.Min.x, tr.Max.y + 4), A.decks[i].name);
    if (h.rclick) { A.deckMenu.open = true; A.deckMenu.idx = i; A.deckMenu.pos = ImGui::GetIO().MousePos; }
    x += w + gap;
  }
}

// Grid | Timeline run-mode toggle, left edge at x, vertically centred on cy, `h` tall. Returns its width.
static float DeckModeToggle(float x, float cy, float h) {
  static const char* names[2] = {"Grid", "Timeline"};
  static const char* icons[2] = {"grid-3x3", "film"};
  float segW[2], segTotal = 4;   // each segment sized to fit its own label — "Timeline" is almost 2x "Grid"
  for (int i = 0; i < 2; ++i) { segW[i] = 12 + 8 + TextW(UI_B, 10, Upper(names[i]).c_str(), 0.09f) + 10; segTotal += segW[i]; }
  ImRect seg(x, cy - h * 0.5f, x + segTotal, cy + h * 0.5f);
  Box(seg, K(pal::g050), K(pal::g2a), 4);
  float segX = seg.Min.x + 2;
  for (int i = 0; i < 2; ++i) {
    ImRect mr(segX, seg.Min.y + 2, segX + segW[i], seg.Max.y - 2);
    bool cur = A.deckMode == i;
    Hit hh = HitR(mr);
    if (cur) { Fill(mr, K(pal::g12), 3); Box(mr, K(pal::coral, 0.15f), K(pal::coral), 3); }
    ImU32 fg = K(cur ? pal::coral : hh.hover ? pal::white : pal::tcc);
    Icon(icons[i], ImVec2(mr.Min.x + 12, (mr.Min.y + mr.Max.y) * 0.5f), 10, fg);
    Text(mr.Min.x + 20, (mr.Min.y + mr.Max.y) * 0.5f, UI_B, 10, fg, Upper(names[i]).c_str(), 0.09f);
    if (hh.hover) CursorHand();
    if (hh.click) A.deckMode = i;
    segX += segW[i];
  }
  return segTotal;
}

// Multiple decks let one show keep several independent layer/column sets (e.g. a "warm-up" deck and a "main set"
// deck) without them fighting over the same grid. Then whichever view is armed (Grid or Timeline).
static void Deck(ImRect r) {
  Fill(r, K(pal::g12));
  float rowY = r.Min.y;
  if (!gDeckMenuInBar) {   // normally the clock and the tabs sit in the transport bar; without the bar they get their own rows here
    ImRect clockRow(r.Min.x, rowY, r.Max.x, rowY + 26);
    Fill(clockRow, K(pal::g18));
    HLine(clockRow.Min.x, clockRow.Max.x, clockRow.Max.y - 1, K(pal::g2a));
    char sysTm[16]; SystemTimeStr(sysTm);
    float cyc = (clockRow.Min.y + clockRow.Max.y - 1) * 0.5f;
    Text(clockRow.Min.x + 10, cyc, UI_B, 9, K(pal::t88), "SYSTEM TIME", 0.14f);
    Text(clockRow.Min.x + 10 + TextW(UI_B, 9, "SYSTEM TIME", 0.14f) + 12, cyc, MONO_B, 12, K(pal::tf3), sysTm, 0.04f);
    rowY = clockRow.Max.y;
    ImRect tabsRow(r.Min.x, rowY, r.Max.x, rowY + 30);
    Fill(tabsRow, K(pal::g18));
    HLine(tabsRow.Min.x, tabsRow.Max.x, tabsRow.Max.y - 1, K(pal::g2a));
    DeckTabs(tabsRow.Min.x + 6, tabsRow.Max.x - 6, (tabsRow.Min.y + tabsRow.Max.y - 1) * 0.5f);
    rowY = tabsRow.Max.y;
  }
  // A row above the grid/timeline exists only when the tabs are NOT in the transport bar: then it holds the Grid|Timeline toggle.
  // (With the tabs in the bar the toggle sits inside the view itself — grid: its top-left corner, timeline: the ruler's left cell.)
  float bodyY = rowY;
  if (!gDeckMenuInBar) {
    ImRect runRow(r.Min.x, rowY, r.Max.x, rowY + 34);
    Fill(runRow, K(pal::g18));
    HLine(runRow.Min.x, runRow.Max.x, runRow.Max.y - 1, K(pal::g2a));
    DeckModeToggle(runRow.Min.x + 6, (runRow.Min.y + runRow.Max.y - 1) * 0.5f, 24);
    bodyY = runRow.Max.y;
  }

  ImRect body(r.Min.x, bodyY, r.Max.x, r.Max.y);
  if (A.deckMode == 0) DeckGrid(body); else TimelineView(body);
}

// Timeline run mode (design ref: tlLanes/tlTicks/blocks) — one lane per layer, its non-empty clips laid out
// back-to-back per A::tlLayout(), a shared 0..100 playhead, drag-from-Browser drops into the first empty slot
// (or a freshly appended column) for that layer. Clicking a block cues it; playback advances the playhead
// itself (see TlAdvance in main.cpp), which is what actually flips clips live via A::tlSync().
static void TimelineView(ImRect r) {
  Fill(r, K(pal::g050));
  const float sideW = 150, rulerH = 26, laneH = 40;
  ImRect side(r.Min.x, r.Min.y, r.Min.x + sideW, r.Max.y);
  ImRect main(r.Min.x + sideW, r.Min.y, r.Max.x, r.Max.y);
  Fill(side, K(pal::g14));

  ImRect sideHd(side.Min.x, side.Min.y, side.Max.x, side.Min.y + rulerH);
  Fill(sideHd, K(pal::g1c)); HLine(sideHd.Min.x, sideHd.Max.x, sideHd.Max.y - 1, K(pal::g2a));
  if (gDeckMenuInBar) DeckModeToggle(sideHd.Min.x + 1, (sideHd.Min.y + sideHd.Max.y - 1) * 0.5f, 22);   // the Grid|Timeline toggle takes the spot of the old "BAR nn" label
  else {   // tabs not in the bar: Deck() shows the toggle in its own row, so keep the bar counter here
    int bar = (int)(A.tlProgress / 6.25f) + 1;
    char barb[16]; snprintf(barb, sizeof barb, "BAR %02d", bar);
    Text(sideHd.Min.x + 8, (sideHd.Min.y + sideHd.Max.y) * 0.5f, MONO_B, 8, K(pal::t66), barb, 0.09f);
  }

  auto lay = A.tlLayout();
  ImVec2 mouse = ImGui::GetIO().MousePos;
  float y = sideHd.Max.y;
  for (int li = 0; li < (int)A.layers.size(); ++li) {
    Layer& l = A.layers[li];
    ImRect laneHead(side.Min.x, y, side.Max.x, y + laneH);
    Hit lh = HitR(laneHead);
    Fill(laneHead, li == A.selLayer ? K(pal::g18) : K(pal::g14));
    HLine(laneHead.Min.x, laneHead.Max.x, laneHead.Max.y - 1, K(pal::g22));
    VLine(laneHead.Max.x, laneHead.Min.y, laneHead.Max.y, K(pal::g2a));
    float lcy = (laneHead.Min.y + laneHead.Max.y) * 0.5f;
    TextEll(laneHead.Min.x + 8, lcy - 6, sideW - 26, UI_S, 10, K(l.live ? pal::coral : pal::tcc), l.name.c_str());
    Text(laneHead.Min.x + 8, lcy + 7, MONO_M, 8, K(pal::t66), Upper(l.blend).c_str(), 0.09f);
    if (l.live) Dot(ImVec2(laneHead.Max.x - 10, lcy), 5, pal::coral, true);
    if (lh.hover) CursorHand();
    if (lh.click) { A.selLayer = li; A.selMode = 0; A.tab = 1; }

    ImRect lane(main.Min.x, y, main.Max.x, y + laneH);
    Fill(lane, K(pal::g10));
    HLine(lane.Min.x, lane.Max.x, lane.Max.y - 1, K(pal::g22));
    g.dl->PushClipRect(lane.Min, lane.Max, true);
    if (lay[li].empty()) TextEll(lane.Min.x + 8, (lane.Min.y + lane.Max.y) * 0.5f, lane.GetWidth() - 16, MONO_M, 9, K(pal::t66), "K\xC3\xA9o clip t\xE1\xBB\xAB Browser v\xC3\xA0o \xC4\x91\xC3\xA2y");
    for (auto& b : lay[li]) {
      Clip& c = l.clips[b.ci];
      bool sel = false; for (auto& sc : A.selectedCells) if (sc.first == li && sc.second == b.ci) sel = true;
      bool hot = c.st == Clip::Live || c.st == Clip::LiveSel;
      float bx0 = lane.Min.x + lane.GetWidth() * b.start / 100.f, bx1 = lane.Min.x + lane.GetWidth() * b.end / 100.f;
      ImRect br(bx0 + 1, lane.Min.y + 3, bx1 - 1, lane.Max.y - 3);
      uint32_t col = CLIP_COLORS[std::clamp(c.color, 0, 5)];
      Hit bh = HitR(br);
      Fill(br, K(col, hot ? 0.30f : 0.14f), 3);
      Box(br, 0, sel ? K(pal::cyan) : hot ? K(col) : K(col, 0.4f), 3);
      if (hot) Glow(br, col, 0.30f, 8, 3);
      TextEll(br.Min.x + 5, (br.Min.y + br.Max.y) * 0.5f, br.GetWidth() - 10, UI_S, 9, K(hot ? pal::white : pal::tcc), c.name.c_str());
      if (bh.hover) CursorHand();
      if (bh.click) { A.cue(li, b.ci); A.tab = 2; }   // selecting a block shows ITS clip's properties
    }
    g.dl->PopClipRect();
    if (A.osDrop.pending && !A.osDrop.handled && lane.Contains(A.osDrop.pos)) {   // OS file dropped onto a timeline lane: first free cell, like a Browser drag
      int emptyCi = -1;
      for (int ci = 0; ci < (int)l.clips.size(); ++ci) if (l.clips[ci].st == Clip::Empty || l.clips[ci].st == Clip::Armed) { emptyCi = ci; break; }
      if (emptyCi < 0) { emptyCi = A.colCount(); A.insertCol(emptyCi); }
      A.dropFilesOnCell(li, emptyCi, A.osDrop.paths);
      A.osDrop.handled = true;
    }
    if (A.dragSrc.active && Hover(lane)) {
      Border(lane, K(pal::coral), 0, 2);
      if (ImGui::IsMouseReleased(0)) {
        int emptyCi = -1;
        for (int ci = 0; ci < (int)l.clips.size(); ++ci) if (l.clips[ci].st == Clip::Empty || l.clips[ci].st == Clip::Armed) { emptyCi = ci; break; }
        if (emptyCi < 0) { emptyCi = A.colCount(); A.insertCol(emptyCi); }
        if (A.dragSrc.fxKind >= 0) { A.cue(li, emptyCi); A.addFx(A.dragSrc.fxKind); }
        else A.loadClip(li, emptyCi, A.dragSrc.name, A.dragSrc.dur, A.dragSrc.media);
        A.dragSrc = DragSrc();
      }
    }
    y += laneH;
  }

  ImRect ruler(main.Min.x, r.Min.y, main.Max.x, r.Min.y + rulerH);
  Fill(ruler, K(pal::g1c)); HLine(ruler.Min.x, ruler.Max.x, ruler.Max.y - 1, K(pal::g2a));
  for (int i = 0; i <= 10; ++i) {
    float px = ruler.Min.x + ruler.GetWidth() * i / 10.f;
    bool major = i % 5 == 0;
    VLine(px, ruler.Min.y, ruler.Max.y, K(pal::g2a, major ? 1.f : 0.5f));
    char lb[8]; snprintf(lb, sizeof lb, "%d%%", i * 10);
    Text(px + 3, (ruler.Min.y + ruler.Max.y) * 0.5f, MONO_M, 8, K(pal::t66), lb, 0.09f);
  }
  if (A.tlLoopOn) {
    ImRect lp(main.Min.x + main.GetWidth() * A.tlIn / 100.f, r.Min.y, main.Min.x + main.GetWidth() * A.tlOut / 100.f, y);
    Fill(lp, K(pal::cyan, 0.08f));
    VLine(lp.Min.x, lp.Min.y, lp.Max.y, K(pal::cyan));
    VLine(lp.Max.x, lp.Min.y, lp.Max.y, K(pal::cyan));
  }
  float px = main.Min.x + main.GetWidth() * std::clamp(A.tlProgress, 0.f, 100.f) / 100.f;
  VLine(px, r.Min.y, y, K(pal::coral));
  g.dl->AddTriangleFilled(ImVec2(px - 4, r.Min.y), ImVec2(px + 4, r.Min.y), ImVec2(px, r.Min.y + 6), Ca(K(pal::coral)));

  ImRect scrubZone(main.Min.x, r.Min.y, main.Max.x, y);
  static bool scrubbing = false;
  Hit sh = HitR(scrubZone);
  if (sh.click) scrubbing = true;
  if (scrubbing) {
    if (ImGui::IsMouseDown(0)) A.tlProgress = std::clamp((mouse.x - main.Min.x) / std::max(1.f, main.GetWidth()) * 100.f, 0.f, 100.f);
    else scrubbing = false;
  }
}

// Column splitters: the 4px gaps either side of the monitors resize the Browser / Properties columns by dragging.
// Same prefs and limits as Settings > Layout (140–320 / 180–360px), saved to settings.json the same way; the monitors
// never get narrower than kMinMonitorsW — below that the Preview Cue header (title, resolution, zoom, hand tool)
// starts overlapping itself. The grab zone is the gap plus the panel's own 1px border line — never further
// in, where the Browser/Properties scroll areas (and their scrollbars) start.
static constexpr float kMinMonitorsW = 560.f;

static ImRect ColumnGap(int side, float x0, float x1, float y0, float bandH) {
  if (side == 1) { float bx = x0 + A.prefs.browserW; return ImRect(bx - 1, y0, bx + 4, y0 + bandH); }
  float ix = x1 - A.prefs.inspectorW;
  return ImRect(ix - 4, y0, ix + 1, y0 + bandH);
}

// Runs before the panels are laid out, so a drag moves them this frame rather than the next.
static void ColumnSplittersInput(float x0, float x1, float y0, float bandH) {
  bool idle = A.resizingCol == 0 && !A.resizingDeck && !A.dragSrc.active;
  for (int side = 1; side <= 2; ++side) {
    Hit h = HitR(ColumnGap(side, x0, x1, y0, bandH));
    if (idle && h.hover) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
    if (idle && h.click) A.resizingCol = side;
  }
  if (A.resizingCol == 0) return;
  if (!ImGui::IsMouseDown(0)) { A.resizingCol = 0; return; }
  ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
  float mx = ImGui::GetIO().MousePos.x, W = x1 - x0;
  if (A.resizingCol == 1) {
    int hi = std::max(140, std::min(320, (int)(W - A.prefs.inspectorW - 8 - kMinMonitorsW)));
    A.prefs.browserW = std::clamp((int)std::round(mx - x0 - 2), 140, hi);   // pointer stays on the middle of the gap
  } else {
    int hi = std::max(180, std::min(360, (int)(W - A.prefs.browserW - 8 - kMinMonitorsW)));
    A.prefs.inspectorW = std::clamp((int)std::round(x1 - mx - 2), 180, hi);
  }
}

// Drawn after the panels, over their border lines — the same look as the deck/band handle below.
static void ColumnSplittersDraw(float x0, float x1, float y0, float bandH) {
  for (int side = 1; side <= 2; ++side) {
    ImRect z = ColumnGap(side, x0, x1, y0, bandH);
    bool hot = A.resizingCol == side || (A.resizingCol == 0 && !A.resizingDeck && !A.dragSrc.active && Hover(z));
    if (hot) {
      Fill(z, K(0xff7f50, 0.15f));
      VLine(z.Min.x, z.Min.y, z.Max.y, K(pal::coral));
      VLine(z.Max.x - 1, z.Min.y, z.Max.y, K(pal::coral));
    }
    float gx = side == 1 ? z.Min.x + 3 : z.Max.x - 3;   // middle of the 4px gap
    Fill(Rc(gx - 0.5f, (z.Min.y + z.Max.y) * 0.5f - 17, 1, 34), K(pal::t66));
  }
}

void DrawDeck(ImRect body) {
  ResetThumbBudget(3);   // A4: at most 3 deck-cell thumbnails redraw (FBO round-trip) per frame — see ClipCell
  float H = ImGui::GetIO().DisplaySize.y;
  float bandH = A.topBandPx > 0 ? A.topBandPx : A.prefs.bandPct / 100.f * H;
  bandH = std::max(bandH, 180.f);
  float x0 = body.Min.x, x1 = body.Max.x, y0 = body.Min.y;
  ImRect band(x0, y0, x1, y0 + bandH);
  Fill(body, K(pal::g0f));

  { int n = A.colCount(); bool need = false; for (auto& l : A.layers) if (!l.clips.empty() && l.clips.back().st != Clip::Empty && l.clips.back().st != Clip::Armed) need = true; if (need) A.insertCol(n); }
  if (A.browserPress && !A.dragSrc.active && ImGui::IsMouseDown(0)) {
    ImVec2 m = ImGui::GetIO().MousePos;
    if (std::hypot(m.x - A.browserPressPos.x, m.y - A.browserPressPos.y) > 5.f) { A.dragSrc = A.dragSrcCand; A.dragSrc.active = true; }
  }
  if (!ImGui::IsMouseDown(0)) A.browserPress = false;
  ColumnSplittersInput(x0, x1, y0, bandH);
  Browser(ImRect(x0, y0, x0 + A.prefs.browserW, y0 + bandH));
  Inspector(ImRect(x1 - A.prefs.inspectorW, y0, x1, y0 + bandH));

  // monitors + timeline
  float mx0 = x0 + A.prefs.browserW + 4, mx1 = x1 - A.prefs.inspectorW - 4;
  float timelineH = (float)A.prefs.timelineH;
  float monH = bandH - 8 - 4 - timelineH;
  float mw = (mx1 - mx0 - 4) / 2.f;
  Monitor(Rc(mx0, y0 + 4, mw, monH), false);
  Monitor(Rc(mx0 + mw + 4, y0 + 4, mw, monH), true);
  gDeckMenuInBar = false;
  if (timelineH > 0.5f) {
    ImRect tl(mx0, y0 + 4 + monH + 4, mx1, y0 + bandH - 4);
    Box(tl, K(pal::g18), K(pal::g2a), 3);
    const Clip* tc0 = A.topClip();
    float total = tc0 ? ClipSeconds(*tc0) : 10.f, t = A.topProgress() / 100.f * total;
    char tc[32]; snprintf(tc, sizeof tc, "00:%02d:%02d:%02d", (int)(t / 60), (int)fmodf(t, 60.f), (int)(fmodf(t, 1.f) * 25));
    char tot[32]; snprintf(tot, sizeof tot, "/ 00:%02d:%02d:00", (int)(total / 60), (int)fmodf(total, 60.f));
    float rx = tl.Max.x - 8;
    TextR(rx, tl.Min.y + 15, UI_B, 9, K(pal::t88), "TIMELINE", 0.14f);
    float totW = TextW(MONO_B, 9, tot, 0.09f);
    TextR(rx, tl.Min.y + 35, MONO_B, 9, K(pal::t66), tot, 0.09f);
    TextR(rx - totW - 6, tl.Min.y + 33, MONO_B, 13, K(pal::coral), tc, 0.04f);
    // Grid mode: skip-back/play/pause/stop/skip-forward (unchanged). Timeline mode adds "step 1 bar"
    // chevrons and its buttons drive the shared tlProgress playhead instead of column selection.
    struct TB { const char* ico; Tone t; bool on; int action; };
    TB tbGrid[5] = {{"skip-back", T_LIVE, false, 0}, {"play", T_LIVE, A.playing, 1}, {"pause", T_STANDBY, !A.playing, 2}, {"square", T_ALERT, false, 3}, {"skip-forward", T_LIVE, false, 4}};
    TB tbTl[7] = {{"skip-back", T_LIVE, false, 5}, {"chevron-left", T_LIVE, false, 6}, {"play", T_LIVE, A.playing, 1}, {"pause", T_STANDBY, !A.playing, 2},
                  {"square", T_ALERT, false, 7}, {"chevron-right", T_LIVE, false, 8}, {"skip-forward", T_LIVE, false, 9}};
    TB* tb = A.deckMode == 1 ? tbTl : tbGrid;
    int nb = A.deckMode == 1 ? 7 : 5;
    float grpW = nb * 29.f + 4.f;
    float mid = (tl.Min.x + tl.Max.x) * 0.5f;
    ImRect grp(mid - grpW * 0.5f, tl.Min.y + 9, mid + grpW * 0.5f, tl.Min.y + 39);
    Box(grp, K(pal::g050), K(pal::g22), 2);
    for (int i = 0; i < nb; ++i) {
      ImRect br(grp.Min.x + 2 + i * 29, grp.Min.y + 1, grp.Min.x + 2 + i * 29 + 28, grp.Min.y + 1 + 28);
      Hit h = HitR(br);
      uint32_t hex = ToneHex(tb[i].t);
      if (tb[i].on) { Glow(br, hex, 0.3f, 10, 2); Fill(br, K(pal::g1c), 2); }
      Box(br, tb[i].on ? K(hex, 0.15f) : K(pal::g1c), tb[i].on ? K(hex) : h.hover ? K(pal::g33) : K(pal::g22), 2);
      Icon(tb[i].ico, ImVec2((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f), 14, K(tb[i].on ? hex : h.hover ? pal::white : pal::t77));
      if (h.hover) CursorHand();
      if (h.click) {
        switch (tb[i].action) {
          case 0: A.stepFireColumn(-1); break;
          case 1: A.playing = true; break;
          case 2: A.playing = false; break;
          case 3: A.playing = false; A.setTopProgress(0.f); break;   // C1 stop: pause and rewind the playhead
          case 4: A.stepFireColumn(1); break;
          case 5: A.tlProgress = 0.f; break;                         // jump to timeline start
          case 6: A.tlProgress = std::max(0.f, A.tlProgress - 6.25f); break;   // back 1 bar
          case 7: A.playing = false; A.tlProgress = 0.f; break;       // stop + rewind
          case 8: A.tlProgress = std::min(100.f, A.tlProgress + 6.25f); break; // forward 1 bar
          case 9: A.tlProgress = 99.9f; break;                        // jump to timeline end
        }
      }
    }
    // Left block, two rows like the TIMELINE block on the right: row 1 = SYSTEM TIME + the clock, row 2 = the deck menu (styled
    // like the top navigation). Needs a bar tall enough for two rows; otherwise Deck() draws its own clock row and tabs row.
    if (tl.GetHeight() >= 44.f) {
      float zx0 = tl.Min.x + 8, zx1 = grp.Min.x - 14;
      if (zx1 - zx0 >= 130.f) {
        gDeckMenuInBar = true;
        char sysTm[16]; SystemTimeStr(sysTm);
        Text(zx0, tl.Min.y + 13, UI_B, 9, K(pal::t88), "SYSTEM TIME", 0.14f);
        Text(zx0 + TextW(UI_B, 9, "SYSTEM TIME", 0.14f) + 10, tl.Min.y + 13, MONO_B, 12, K(pal::tf3), sysTm, 0.04f);
        DeckTabs(zx0, zx1, tl.Min.y + 33.f);
      }
    }
  }

  ColumnSplittersDraw(x0, x1, y0, bandH);

  // resize handle
  {
    ImRect rh(x0, y0 + bandH, x1, y0 + bandH + 6);
    Hit h = HitR(rh);
    bool hot = h.hover || A.resizingDeck;
    Fill(rh, K(hot ? 0xff7f50 : pal::g0f, hot ? 0.15f : 1.f));
    if (!hot) Fill(rh, K(pal::g0f));
    HLine(x0, x1, rh.Min.y, hot ? K(pal::coral) : K(pal::g2a));
    HLine(x0, x1, rh.Max.y - 1, hot ? K(pal::coral) : K(pal::g2a));
    Fill(Rc((x0 + x1) * 0.5f - 17, rh.Min.y + 2, 34, 1), K(pal::t66));
    if (h.hover) ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
    if (h.click) A.resizingDeck = true;
    if (A.resizingDeck) {
      if (ImGui::IsMouseDown(0)) {
        float mxv = std::max(180.f, H - (40 + 20 + 6) - 220.f);
        A.topBandPx = std::clamp(ImGui::GetIO().MousePos.y - 40.f, 180.f, mxv);
        A.prefs.bandPct = std::clamp((int)std::round(A.topBandPx / H * 100.f), 25, 70);
        ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeNS);
      } else A.resizingDeck = false;
    }
  }
  Deck(ImRect(x0, y0 + bandH + 6, x1, body.Max.y));
}















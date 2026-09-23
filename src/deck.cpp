// Composition screen: browser · monitors · properties · layer/clip deck.
#include "app.h"
#include <array>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <ctime>
#include <filesystem>

using namespace ui;


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
    Mask m; m.id = id; m.name = n; m.inverted = inv; m.feather = f; m.pts[0] = a; m.pts[1] = b; m.pts[2] = c; m.pts[3] = d; return m;
  };
  auto slice = [&](const char* id, const char* n, int warp, int ix, int iy, int iw, int ih) {
    Slice s; s.id = id; s.name = n; s.warp = warp; s.ix = ix; s.iy = iy; s.iw = iw; s.ih = ih; quad(s, (float)ix, (float)iy, (float)iw, (float)ih); return s;
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

static std::string BpmStr() { char b[16]; snprintf(b, sizeof b, "%.1f", A.bpm); return b; }
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
    l.live = true;
  }
  selMode = 2;
  activeCol = ci;
  for (auto& g : groups) g.activeCol = ci;   // column select also selects every group's cue, so the Cue N highlight follows the column
  selectedCells.clear();
  for (int li = 0; li < (int)layers.size(); ++li) selectedCells.push_back({li, ci});
  // Preview Cue (Monitor reads selLi/selCi directly) follows the topmost layer that actually has a clip here,
  // same "layer 0 draws on top" convention as DrawComposite. Firing an all-empty column leaves it unchanged.
  for (int li = 0; li < (int)layers.size(); ++li)
    if (ci < (int)layers[li].clips.size() && layers[li].clips[ci].st != Clip::Empty && layers[li].clips[ci].st != Clip::Armed) {
      selLi = li; selCi = ci; selLayer = li;
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
  Clip c; c.st = Clip::Loaded; c.name = name; c.media = media; if (!media.empty()) PreloadMedia(media); c.dur = dur.empty() ? "\xE2\x88\x9E" : dur;
  layers[li].clips[ci] = c;
  selLayer = li; selLi = li; selCi = ci; selectedCells = {{li, ci}}; selMode = 1;
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

static CellOut ClipCell(ImRect r, Clip& cl, bool selectedCell, bool dragged, bool dropT, float progress) {
  bool empty = cl.st == Clip::Empty || cl.st == Clip::Armed;
  // Design tokens §3.2 give the clip cell THREE distinct looks, not "selected = coral / else = warm brown":
  // loaded-idle (warm amber, already at rest), cued-for-preview (cool cyan — about to show, not showing yet),
  // and live-on-output (hot burnt-orange). LiveSel (cued AND live at once) must read as live above all — an
  // operator mistaking "what's live" for "what's merely selected" is the one mistake this can't afford.
  bool live = cl.st == Clip::Live || cl.st == Clip::LiveSel;
  bool preview = cl.st == Clip::Selected;
  uint32_t ringHex = live ? pal::coral : pal::cyan;  // outer selection ring: coral only for an actually-live cell
  Hit h = HitR(r);
  uint32_t col = CLIP_COLORS[std::clamp(cl.color, 0, 5)];
  float prevA = g.alpha; if (dragged) g.alpha *= 0.4f;

  // wrapper fill + glow
  if (selectedCell) Glow(r, ringHex, 0.35f, 12, 3);
  ImU32 wrapFill = 0;
  if (empty) { if (selectedCell) wrapFill = K(ringHex, 0.12f); }
  else wrapFill = K(col, selectedCell ? 0.18f : 0.07f);
  Fill(r, K(pal::g12), 3);
  if (wrapFill) Fill(r, wrapFill, 3);

  ImU32 border = K(pal::g22), bar = K(pal::g1c), barFg = K(pal::te0), body = K(pal::clipLoadedBg), foot = K(pal::t88);
  bool barBold = false;
  if (live) { border = K(pal::coral); bar = K(pal::clipBarLive); barFg = K(pal::clipLiveText); barBold = true; body = K(pal::clipBodyLive); foot = K(pal::clipFootLive); }
  else if (preview) { border = K(pal::cyan); bar = K(pal::clipBarPreview); barFg = K(pal::clipPreviewText); body = K(pal::clipBodyPreview); foot = K(pal::clipFootPreview); }
  else if (!empty) { border = K(pal::clipLoadedBorder); bar = K(pal::clipBarLoaded); barFg = K(pal::clipLoadedText); body = K(pal::clipBodyLoaded); foot = K(pal::clipFootLoaded); }
  else if (h.hover) border = K(pal::g33);

  if (live) Glow(r, pal::coral, 0.40f, 12, 4);
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
    bool active = live || preview;
    bool stale = cl.thumbTex == 0 || (active && g.time - cl.thumbAt > 0.2);
    if (stale && ThumbBudgetLeft()) RenderClipThumbnail(cl);
    if (cl.thumbTex != 0) g.dl->AddImage((ImTextureID)(intptr_t)cl.thumbTex, bodyR.Min, bodyR.Max, ImVec2(0, 1), ImVec2(1, 0), Ca(IM_COL32_WHITE));
    else GradDiag(bodyR, col, Dark45(col), live ? 0.45f : 0.22f);
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
  }
  Border(r, border, 4);
  // selection outlines from the wrapper
  if (dropT) DashedRect(Inset(r, 1), K(pal::yellow), 2);
  else if (selectedCell) Border(r, K(ringHex), 4, 2);
  else if (!empty) Border(r, K(col, 0.35f), 4);
  g.alpha = prevA;

  CellOut o;
  o.hover = h.hover;
  if (h.hover) {
    if (empty) {
      // An empty cell draws no name strip, so there is nothing on screen telling the user where "bar" ends and
      // "body" begins — the bar/body split above only exists once a clip is loaded. Until then, treat the whole
      // cell as body: any click here stops/clears the layer, from wherever inside the cell it lands. Right-click
      // still opens the popover (its items are already disabled/no-ops for an empty slot where irrelevant).
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
  if (selected) {
    for (int k = 3; k >= 1; --k) g.dl->AddRectFilled(ImVec2(r.Min.x - k, r.Min.y), ImVec2(r.Min.x + 3 + k, r.Max.y), Ca(K(pal::coral, 0.08f)));
    Fill(Rc(r.Min.x, r.Min.y, 3, r.GetHeight()), K(pal::coral));
  }
  float padL = 6 + indent * 8 + r.Min.x, padR = r.Max.x - 6;
  float top = r.Min.y + 6;
  float headCy = collapsed ? (r.Min.y + r.Max.y - 1) * 0.5f : top + 8;
  bool consumed = false;

  // chevron
  ImRect chev(padL, headCy - 7, padL + 14, headCy + 7);
  if (HitR(chev).click) { l.collapsed = !l.collapsed; consumed = true; }
  Icon(collapsed ? "chevron-right" : "chevron-down", ImVec2(chev.Min.x + 7, headCy), 10,
       l.bypassed ? K(pal::t66) : l.live ? K(pal::coral) : K(pal::t88));

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
    sliderRow("V", pal::coral, l.opacity, 0x1000 + li * 4);
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
      ImGui::SetCursorScreenPos(ImVec2(tb.Min.x + 4, tb.Min.y + 3));
      ImGui::PushFont(F(MONO_B), 9);
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

static void TestCard(ImRect r) {
  static const uint32_t bars[7] = {0xc0c0c0, 0xc0c000, 0x00c0c0, 0x00c000, 0xc000c0, 0xc00000, 0x0000c0};
  float w = r.GetWidth() / 7.f;
  for (int i = 0; i < 7; ++i) Fill(ImRect(r.Min.x + w * i, r.Min.y, r.Min.x + w * (i + 1), r.Min.y + r.GetHeight() * 0.7f), K(bars[i]));
  Fill(ImRect(r.Min.x, r.Min.y + r.GetHeight() * 0.7f, r.Max.x, r.Max.y), K(0x101010));
}


static void Monitor(ImRect r, bool live) {
  Fill(r, K(pal::g12), 3);
  ImRect hd(r.Min.x + 1, r.Min.y + 1, r.Max.x - 1, r.Min.y + 25);
  PanelHeader(hd, live ? "Live Output" : "Preview Cue", live ? pal::coral : pal::cyan, K(pal::g1c));
  float cy = (hd.Min.y + hd.Max.y - 1) * 0.5f;
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
    TextR(xr, cy, MONO_M, 10, K(pal::t66), "1920\xC3\x97" "1080");
  }
  ImRect well(r.Min.x + 1, hd.Max.y, r.Max.x - 1, r.Max.y - 1);
  Fill(well, K(pal::g050));
  if (blk) {
    TextC((well.Min.x + well.Max.x) * 0.5f, (well.Min.y + well.Max.y) * 0.5f, MONO_M, 10, K(pal::red), "OUTPUT MUTED", 0.14f);
  } else if (live) {
    if (A.testCard) TestCard(well);
    else {
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
    ImRect cv = CanvasRect(well);
    Fill(cv, K(0x0a0a0a));
    g.dl->PushClipRect(cv.Min, cv.Max, true);
    DrawClipContent(cv, sc, (float)g.time * 1.5f, 960.f, 1.f);
    for (float x = cv.Min.x + 13; x < cv.Max.x; x += 14) VLine(std::floor(x), cv.Min.y, cv.Max.y, K(0xffffff, 0.045f));
    for (float y = cv.Min.y + 13; y < cv.Max.y; y += 14) HLine(cv.Min.x, cv.Max.x, std::floor(y), K(0xffffff, 0.045f));
    g.dl->PopClipRect();
    Border(cv, K(pal::g2a));
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
  float ox = sa.origin.x, oy = sa.origin.y, W = body.GetWidth() - (ImGui::GetCurrentWindow()->ScrollbarY ? 8.f : 0.f);
  float y = 0;
  auto rowsOf = [&](std::vector<std::array<std::string, 4>>& rows) {
    for (auto& rw : rows) {
      uint32_t hex = pal::tcc;
      if (rw[3] == "preview") hex = pal::cyan; else if (rw[3] == "audio") hex = pal::mint; else if (rw[3] == "live") hex = pal::coral; else if (rw[3] == "alert") hex = pal::red;
      PropertyRow(Rc(ox, oy + y, W, 22), rw[0].c_str(), rw[1].c_str(), rw[2].c_str(), hex);
      y += 22;
    }
  };
  const Layer& sl = A.layers[std::clamp(A.selLayer, 0, (int)A.layers.size() - 1)];
  const Clip& cell = A.layers[A.selLi].clips[A.selCi];
  bool cellLive = cell.isLive();
  char b1[64];

  if (A.tab == 1) {
    // opacity
    y += 8;
    Text(ox + 8, oy + y + 5, UI_B, 9, K(pal::coral), "VISUAL OPACITY (V)", 0.14f);
    snprintf(b1, sizeof b1, "%d%%", (int)sl.opacity);
    TextR(ox + W - 8, oy + y + 5, MONO_B, 10, K(pal::tf3), b1);
    y += 10 + 6;
    Layer& ml = A.layers[A.selLayer];
    Slider(0x2001, Rc(ox + 8, oy + y + 4, W - 16, 6), ml.opacity, pal::coral);
    y += 14 + 6;
    Text(ox + 8, oy + y + 5, UI_B, 9, K(pal::mint), "AUDIO VOLUME (A)", 0.14f);
    snprintf(b1, sizeof b1, "%d%%", (int)sl.audio);
    TextR(ox + W - 8, oy + y + 5, MONO_B, 10, K(pal::tf3), b1);
    y += 10 + 6;
    Slider(0x2002, Rc(ox + 8, oy + y + 4, W - 16, 6), ml.audio, pal::mint);
    y += 14 + 6;
    Text(ox + 8, oy + y + 5, UI_B, 9, K(pal::t88), "BLEND MODE", 0.14f);
    y += 9 + 6;
    const char** modes = BLEND_NAMES;   // same 8 names as the layer-row dropdown, so the chip always matches what is rendered
    float bw = (W - 16 - 4) / 2.f;
    for (int i = 0; i < BLEND_COUNT; ++i) {
      ImRect br(ox + 8 + (i % 2) * (bw + 4), oy + y + (i / 2) * 28, ox + 8 + (i % 2) * (bw + 4) + bw, oy + y + (i / 2) * 28 + 24);
      bool on = (ml.blend.empty() ? std::string("Normal") : ml.blend) == modes[i];
      Hit h = HitR(br);
      if (on) { Glow(br, pal::coral, 0.3f, 12, 3); Fill(br, K(pal::g16), 3); }
      Box(br, on ? K(pal::coral, 0.15f) : K(pal::g1c), on ? K(pal::coral) : K(pal::g22), 3);
      std::string u = Upper(modes[i]);
      TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, UI_B, 10, K(on ? pal::coral : pal::t88), u.c_str(), 0.09f);
      if (h.hover) CursorHand();
      if (h.click) ml.blend = modes[i];
    }
    y += (BLEND_COUNT / 2) * 28 - 4 + 8;
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
    HLine(ox, ox + W, oy + y, K(pal::g2a));
    Group* gp = sl.group.empty() ? nullptr : A.group(sl.group);
    char bt[16]; snprintf(bt, sizeof bt, "%.1f", sl.blendTime);
    std::vector<std::array<std::string, 4>> rows = {
        {"Layer", sl.name, "", ""}, {"Group", gp ? gp->name : "No group", "", ""},
        {"Play mode", PlayModeName(cell.playMode), "", ""}, {"Blend time", bt, "s", "preview"},
        {"Solo", sl.solo ? "On" : "Off", "", ""}, {"Mute", sl.muted ? "On" : "Off", "", ""},
        {"Bypass", sl.bypassed ? "On" : "Off", "", ""}, {"State", sl.live ? "Live" : "Idle", "", sl.live ? "live" : ""}};
    rowsOf(rows);
  } else if (A.tab == 0) {
    char fb2[16]; snprintf(fb2, sizeof fb2, "%.1f", PerfFps());
    char pb2[16]; snprintf(pb2, sizeof pb2, "%.1f", PerfP99());
    std::vector<std::array<std::string, 4>> rows = {
        {"Composition", A.projectName, "", ""}, {"Canvas", std::to_string(A.canvasW) + "\xC3\x97" + std::to_string(A.canvasH), "", ""},
        {"Layers", std::to_string(A.layers.size()), "", ""},
        {"Groups", std::to_string(A.groups.size()), "", ""}, {"Columns", std::to_string(A.colCount()), "", ""}, {"BPM", BpmStr(), "", "audio"},
        {"Beat sync", "1/4", "", "audio"}, {"Rate", fb2, "fps", ""}, {"Latency", pb2, "ms", PerfP99() > 20.f ? "alert" : "audio"},
        {"Output", A.blackout ? "Blackout" : "Live", "", A.blackout ? "alert" : "live"}};
    rowsOf(rows);
  } else {
    y += 6;
    ImRect th(ox + 6, oy + y, ox + W - 6, oy + y + 76);
    g.dl->PushClipRect(th.Min, th.Max, true);
    Fill(th, K(0x050505));
    DrawClipContent(th, cell, (float)g.time * 1.2f, 300.f, 1.f, 0.4f);
    g.dl->AddRectFilledMultiColor(th.Min, th.Max, Ca(K(0x050505, 0.15f)), Ca(K(0x050505, 0.15f)), Ca(K(0x050505, 0.82f)), Ca(K(0x050505, 0.82f)));
    g.dl->PopClipRect();
    Text(th.Min.x + 6, th.Min.y + 10, MONO_M, 10, K(pal::tcc), "TUNNEL_04.MOV", 0.09f);
    Text(th.Min.x + 6, th.Max.y - 10, UI_B, 11, K(pal::white), cell.name.empty() ? "Empty slot" : cell.name.c_str());
    TextR(th.Max.x - 6, th.Max.y - 10, MONO_B, 9, K(cellLive ? pal::coral : pal::cyan), cellLive ? "LIVE" : "CUED");
    Border(th, K(pal::g2a), 3);
    y += 76 + 6;
    Text(ox + 6, oy + y + 5, UI_B, 9, K(pal::t88), "PLAYHEAD", 0.14f);
    int h1 = (int)(A.topProgress() / 100 * 212), sec = h1;
    char tc[32]; snprintf(tc, sizeof tc, "00:%02d:%02d:%02d", sec / 60, sec % 60, (int)(fmodf(A.topProgress() / 100 * 212, 1.f) * 25));
    TextR(ox + W - 6, oy + y + 5, MONO_B, 10, K(pal::coral), tc);
    y += 10 + 4;
    ImRect bar(ox + 6, oy + y, ox + W - 6, oy + y + 6);
    Box(bar, K(pal::meterTrack), K(pal::g22), 999);
    float fw = (bar.GetWidth() - 2) * std::clamp(A.topProgress() / 100.f, 0.f, 1.f);
    if (fw > 1) { Fill(ImRect(bar.Min.x + 1, bar.Min.y + 1, bar.Min.x + 1 + fw, bar.Max.y - 1), K(pal::coral), 999); }
    y += 6 + 6;
    Text(ox + 6, oy + y + 5, UI_B, 9, K(pal::t88), "PLAY MODE", 0.14f);
    y += 9 + 4;
    Clip& mc = A.layers[std::clamp(A.selLi, 0, (int)A.layers.size() - 1)]
                   .clips[std::clamp(A.selCi, 0, A.colCount() - 1)];
    bool editable = mc.st != Clip::Empty;
    float pw = (W - 12 - 12) / 4.f;
    for (int i = 0; i < 4; ++i) {
      ImRect br(ox + 6 + i * (pw + 4), oy + y, ox + 6 + i * (pw + 4) + pw, oy + y + 22);
      bool on = mc.playMode == i;
      Hit h = HitR(br);
      if (on) { Glow(br, pal::coral, 0.3f, 12, 3); Fill(br, K(pal::g16), 3); }
      Box(br, on ? K(pal::coral, 0.15f) : K(pal::g1c), on ? K(pal::coral) : K(pal::g22), 3);
      TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, MONO_B, 9, K(on ? pal::coral : pal::t77), PlayModeName(i), 0.09f);
      if (h.hover) CursorHand();
      if (h.click && editable) { mc.playMode = i; mc.dir = 1; }
    }
    y += 22 + 6;
    // C5 speed + C4 direction
    Text(ox + 6, oy + y + 5, UI_B, 9, K(pal::cyan), "SPEED", 0.14f);
    snprintf(b1, sizeof b1, "%.2f\xC3\x97", mc.speed / 100.f);
    TextR(ox + W - 6, oy + y + 5, MONO_B, 10, K(pal::tf3), b1);
    y += 10 + 4;
    {
      float sv = mc.speed * 0.5f;  // 0..200% mapped onto the 0..100 slider
      if (Slider(0x2003, Rc(ox + 6, oy + y + 4, W - 40, 6), sv, pal::cyan) && editable) mc.speed = sv * 2.f;
      ImRect rv(ox + W - 6 - 28, oy + y, ox + W - 6, oy + y + 16);
      bool rev = mc.dir < 0;
      Hit rh = HitR(rv);
      Box(rv, rev ? K(pal::yellow, 0.2f) : K(pal::g1c), rev ? K(pal::yellow) : K(pal::g22), 3);
      TextC((rv.Min.x + rv.Max.x) * 0.5f, (rv.Min.y + rv.Max.y) * 0.5f, MONO_B, 9, K(rev ? pal::yellow : pal::t77), "REV", 0.09f);
      if (rh.hover) CursorHand();
      if (rh.click && editable) mc.dir = -mc.dir;
    }
    y += 16 + 8;
    // ── D1/D2/D3/D5/D6 transform ──
    HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 6;
    Text(ox + 6, oy + y + 5, UI_B, 9, K(pal::t88), "TRANSFORM", 0.14f);
    {
      ImRect rb(ox + W - 6 - 40, oy + y - 3, ox + W - 6, oy + y + 13);
      Hit hh = HitR(rb);
      Box(rb, hh.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
      TextC((rb.Min.x + rb.Max.x) * 0.5f, (rb.Min.y + rb.Max.y) * 0.5f, MONO_B, 9, K(hh.hover ? pal::white : pal::t88), "RESET", 0.09f);
      if (hh.hover) CursorHand();
      if (hh.click && editable) { mc.posX = mc.posY = mc.rotation = 0; mc.scale = 1; mc.opacity = 100; mc.flipH = mc.flipV = false; }
    }
    y += 10 + 6;
    struct TR { const char* lab; float* v; float mn, mx; const char* unit; uint32_t hex; };
    TR trs[5] = {{"Position X", &mc.posX, -480, 480, "px", pal::cyan}, {"Position Y", &mc.posY, -270, 270, "px", pal::cyan},
                 {"Scale", &mc.scale, 0.1f, 3.f, "\xC3\x97", pal::coral}, {"Rotation", &mc.rotation, -180, 180, "\xC2\xB0", pal::yellow},
                 {"Opacity", &mc.opacity, 0, 100, "%", pal::coral}};
    for (int i = 0; i < 5; ++i) {
      TR& tr = trs[i];
      Text(ox + 6, oy + y + 5, UI_S, 10, K(pal::t88), tr.lab);
      char vb[24];
      if (i == 2) snprintf(vb, sizeof vb, "%.2f%s", *tr.v, tr.unit);
      else snprintf(vb, sizeof vb, "%d%s", (int)std::round(*tr.v), tr.unit);
      TextR(ox + W - 6, oy + y + 5, MONO_B, 10, K(pal::tf3), vb);
      y += 10 + 4;
      float norm = (*tr.v - tr.mn) / (tr.mx - tr.mn) * 100.f;
      if (Slider(0x2010 + i, Rc(ox + 6, oy + y + 4, W - 12, 6), norm, tr.hex) && editable)
        *tr.v = tr.mn + norm / 100.f * (tr.mx - tr.mn);
      y += 14 + 4;
    }
    {
      float bw2 = (W - 12 - 4) / 2.f;
      const char* fl[2] = {"FLIP H", "FLIP V"};
      bool* fv[2] = {&mc.flipH, &mc.flipV};
      for (int i = 0; i < 2; ++i) {
        ImRect br(ox + 6 + i * (bw2 + 4), oy + y, ox + 6 + i * (bw2 + 4) + bw2, oy + y + 22);
        bool on = *fv[i];
        Hit h = HitR(br);
        if (on) { Glow(br, pal::cyan, 0.3f, 10, 3); Fill(br, K(pal::g16), 3); }
        Box(br, on ? K(pal::cyan, 0.15f) : K(pal::g1c), on ? K(pal::cyan) : K(pal::g22), 3);
        TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, MONO_B, 9, K(on ? pal::cyan : pal::t77), fl[i], 0.09f);
        if (h.hover) CursorHand();
        if (h.click && editable) *fv[i] = !*fv[i];
      }
      y += 22 + 8;
    }
    char pr[16]; snprintf(pr, sizeof pr, "%d", (int)std::round(A.topProgress()));
    char spd[16]; snprintf(spd, sizeof spd, "%.2f", mc.speed / 100.f);
    std::vector<std::array<std::string, 4>> rows = {
        {"Clip", cell.name.empty() ? "\xE2\x80\x94" : cell.name, "", ""}, {"Source", "generator", "", ""},
        {"Duration", cell.dur.empty() ? "\xE2\x80\x94" : cell.dur, "", ""}, {"Play mode", PlayModeName(mc.playMode), "", ""},
        {"Speed", spd, "\xC3\x97", ""}, {"Playhead", pr, "%", "live"}, {"Resolution", "1920\xC3\x97" "1080", "", ""},
        {"Codec", "procedural", "", ""}, {"Beat sync", "1/4", "", "audio"}, {"State", cellLive ? "Live" : "Cued", "", cellLive ? "live" : "preview"}};
    rowsOf(rows);
    // ── FX chain (matches HTML fxRows / fxParams / fxMix / fxToggles / fxMeta) ──
    {
      std::vector<Fx>& chain = A.fxChain();
      A.fxSel = chain.empty() ? 0 : std::clamp(A.fxSel, 0, (int)chain.size() - 1);
      y += 4; HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 6;
      float addW = TextW(MONO_B, 9, "ADD", 0.09f) + 11 + 4 + 12;
      TextEll(ox + 6, oy + y + 9, W - 12 - addW - 6, UI_B, 9, K(pal::t88), ("FX CHAIN \xC2\xB7 " + Upper(cell.name.empty() ? std::string("EMPTY SLOT") : cell.name)).c_str(), 0.14f);
      ImRect addB(ox + W - 6 - addW, oy + y, ox + W - 6, oy + y + 20);
      Hit ah = HitR(addB);
      Box(addB, K(pal::g1c), ah.hover ? K(pal::cyan) : K(pal::g22), 3);
      Icon("plus", ImVec2(addB.Min.x + 6 + 5.5f, oy + y + 10), 11, K(ah.hover ? pal::cyan : pal::tcc));
      Text(addB.Min.x + 6 + 11 + 4, oy + y + 10, MONO_B, 9, K(ah.hover ? pal::cyan : pal::tcc), "ADD", 0.09f);
      if (ah.hover) CursorHand();
      if (ah.click) {
        std::vector<ui::MenuItem> items;
        for (int k = 0; k < FX_COUNT; ++k) { ui::MenuItem m; m.label = FX_LIB[k].name; m.icon = FX_LIB[k].icon; int kk = k; m.run = [kk] { A.addFx(kk); }; items.push_back(std::move(m)); }
        A.openCtx(ImGui::GetIO().MousePos, std::move(items));
      }
      y += 20 + 6;
      if (chain.empty()) {
        Text(ox + 6, oy + y + 7, UI_S, 10, K(pal::t66), "No effects on this layer. Use ADD.", 0.01f);
        y += 15 + 6;
      } else {
        for (int i = 0; i < (int)chain.size(); ++i) {
          Fx& f = chain[i];
          const FxDef& d = FX_LIB[std::clamp(f.kind, 0, FX_COUNT - 1)];
          uint32_t tn = FxTone(d.tone);
          bool on = A.fxSel == i;
          ImRect fr(ox + 6, oy + y, ox + W - 6, oy + y + 24);
          Hit fh = HitR(fr);
          if (on) { Glow(fr, tn, 0.30f, 12, 3); Fill(fr, K(pal::g16), 3); }
          Box(fr, on ? K(tn, 0.15f) : K(pal::g1c), on ? K(tn) : K(pal::g22), 3);
          float prev = g.alpha; if (!f.on) g.alpha *= 0.45f;
          float cy = oy + y + 12;
          char nn[8]; snprintf(nn, sizeof nn, "%02d", i + 1);
          Text(fr.Min.x + 5, cy, MONO_B, 9, K(pal::t66), nn);
          Icon(d.icon, ImVec2(fr.Min.x + 5 + TextW(MONO_B, 9, nn) + 4 + 5.5f, cy), 11, K(on ? tn : pal::tcc));
          char mx[16]; snprintf(mx, sizeof mx, "%d%%", (int)std::round(f.mix));
          float mxW = TextW(MONO_B, 9, mx);
          ImRect by(fr.Max.x - 5 - 10, cy - 8, fr.Max.x - 5, cy + 8);
          Icon(f.on ? "eye" : "eye-off", ImVec2((by.Min.x + by.Max.x) * 0.5f, cy), 10, K(pal::t88));
          TextR(by.Min.x - 4, cy, MONO_B, 9, K(pal::t77), mx);
          TextEll(fr.Min.x + 5 + TextW(MONO_B, 9, nn) + 4 + 11 + 4, cy, by.Min.x - 4 - mxW - 4 - (fr.Min.x + 5 + TextW(MONO_B, 9, nn) + 4 + 11 + 4), UI_B, 10, K(on ? tn : pal::tcc), d.name);
          g.alpha = prev;
          if (fh.hover) CursorHand();
          if (fh.click) A.fxSel = i;
          if (fh.rclick) {
            int ii = i;
            std::vector<ui::MenuItem> items;
            auto mk = [&](const char* l, const char* ic, bool dis, bool dg, std::function<void()> fn) { ui::MenuItem m; m.label = l; m.icon = ic; m.disabled = dis; m.danger = dg; m.run = fn; items.push_back(std::move(m)); };
            mk(f.on ? "Bypass effect" : "Enable effect", f.on ? "eye-off" : "eye", false, false, [ii] { auto& c = A.fxChain(); if (ii < (int)c.size()) c[ii].on = !c[ii].on; });
            mk("Move up", "arrow-up", ii <= 0, false, [ii] { A.moveFx(ii, -1); });
            mk("Move down", "arrow-down", ii >= (int)A.fxChain().size() - 1, false, [ii] { A.moveFx(ii, 1); });
            mk("Duplicate", "copy", false, false, [ii] { A.dupFx(ii); });
            mk("Reset parameters", "rotate-ccw", false, false, [ii] { A.resetFx(ii); });
            mk("Remove effect", "trash-2", false, true, [ii] { A.removeFx(ii); });
            A.openCtx(ImGui::GetIO().MousePos, std::move(items));
          }
          // bypass toggle on click of eye icon
          if (HitR(by).click) f.on = !f.on;
          y += 24 + 2;
        }
        y += 4;
        // selected FX editor
        Fx& cur = chain[A.fxSel];
        const FxDef& dd = FX_LIB[std::clamp(cur.kind, 0, FX_COUNT - 1)];
        uint32_t tn = FxTone(dd.tone);
        HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 6;
        float rsW = TextW(MONO_B, 9, "RESET", 0.09f) + 10 + 4 + 12;
        Icon(dd.icon, ImVec2(ox + 6 + 6, oy + y + 9), 12, K(tn));
        TextEll(ox + 6 + 12 + 4, oy + y + 9, W - 12 - 12 - 4 - rsW - 6, UI_B, 9, K(tn), Upper(dd.name).c_str(), 0.14f);
        ImRect rsB(ox + W - 6 - rsW, oy + y, ox + W - 6, oy + y + 18);
        Hit rh = HitR(rsB);
        Box(rsB, K(pal::g1c), rh.hover ? K(pal::yellow) : K(pal::g22), 3);
        Icon("rotate-ccw", ImVec2(rsB.Min.x + 5 + 5, oy + y + 9), 10, K(rh.hover ? pal::yellow : pal::t88));
        Text(rsB.Min.x + 5 + 10 + 4, oy + y + 9, MONO_B, 9, K(rh.hover ? pal::yellow : pal::t88), "RESET", 0.09f);
        if (rh.hover) CursorHand();
        if (rh.click) A.resetFx(A.fxSel);
        y += 18 + 6;
        for (int pi = 0; pi < dd.nparams; ++pi) {
          std::string lb = Upper(dd.pl[pi] ? dd.pl[pi] : "");
          std::string vl = FxFmt(cur.kind, pi, cur.p[pi]);
          Text(ox + 6, oy + y + 5, UI_B, 9, K(pal::t88), lb.c_str(), 0.09f);
          TextR(ox + W - 6, oy + y + 5, MONO_B, 10, K(pal::tf3), vl.c_str());
          y += 10 + 4;
          Slider(0x3000 + A.fxSel * 8 + pi, Rc(ox + 6, oy + y + 4, W - 12, 6), cur.p[pi], tn);
          y += 14 + 6;
        }
        if (dd.enumLabel) {
          Text(ox + 6, oy + y + 5, UI_B, 9, K(pal::t88), Upper(dd.enumLabel).c_str(), 0.09f);
          y += 9 + 4;
          float ew = (W - 12 - 8) / 3.f;
          for (int oi = 0; oi < 3 && dd.opts[oi]; ++oi) {
            ImRect er(ox + 6 + oi * (ew + 4), oy + y, ox + 6 + oi * (ew + 4) + ew, oy + y + 22);
            bool on = cur.en == oi;
            Hit eh = HitR(er);
            if (on) { Glow(er, tn, 0.28f, 10, 3); Fill(er, K(pal::g16), 3); }
            Box(er, on ? K(tn, 0.18f) : K(pal::g1c), on ? K(tn) : K(pal::g22), 3);
            TextC((er.Min.x + er.Max.x) * 0.5f, (er.Min.y + er.Max.y) * 0.5f, MONO_B, 9, K(on ? tn : pal::t88), dd.opts[oi], 0.09f);
            if (eh.hover) CursorHand();
            if (eh.click) cur.en = oi;
          }
          y += 22 + 6;
        }
        {
          char ml2[16]; snprintf(ml2, sizeof ml2, "%d %%", (int)std::round(cur.mix));
          Text(ox + 6, oy + y + 5, UI_B, 9, K(pal::coral), "DRY / WET MIX", 0.09f);
          TextR(ox + W - 6, oy + y + 5, MONO_B, 10, K(pal::tf3), ml2);
          y += 10 + 4;
          Slider(0x3800 + A.fxSel, Rc(ox + 6, oy + y + 4, W - 12, 6), cur.mix, pal::coral);
          y += 14 + 6;
        }
        {
          struct TG { const char* n; bool on; uint32_t hx; std::function<void()> fn; };
          int ii = A.fxSel;
          TG tgs[3] = {{"B", !cur.on, pal::red, [ii] { auto& c = A.fxChain(); if (ii < (int)c.size()) c[ii].on = !c[ii].on; }},
                       {"BEAT", cur.beat, pal::yellow, [ii] { auto& c = A.fxChain(); if (ii < (int)c.size()) c[ii].beat = !c[ii].beat; }},
                       {"AUDIO", cur.react, pal::mint, [ii] { auto& c = A.fxChain(); if (ii < (int)c.size()) c[ii].react = !c[ii].react; }}};
          float tw3 = (W - 12 - 8) / 3.f;
          for (int ti = 0; ti < 3; ++ti) {
            ImRect tr(ox + 6 + ti * (tw3 + 4), oy + y, ox + 6 + ti * (tw3 + 4) + tw3, oy + y + 22);
            Hit th = HitR(tr);
            if (tgs[ti].on) { Glow(tr, tgs[ti].hx, 0.28f, 10, 3); Fill(tr, K(pal::g16), 3); }
            Box(tr, tgs[ti].on ? K(tgs[ti].hx, 0.18f) : K(pal::g1c), tgs[ti].on ? K(tgs[ti].hx) : K(pal::g22), 3);
            TextC((tr.Min.x + tr.Max.x) * 0.5f, (tr.Min.y + tr.Max.y) * 0.5f, MONO_B, 9, K(tgs[ti].on ? tgs[ti].hx : pal::t88), tgs[ti].n, 0.09f);
            if (th.hover) CursorHand();
            if (th.click) tgs[ti].fn();
          }
          y += 22 + 6;
        }
      }
      // fx meta rows
      {
        int act = 0; std::string names;
        for (auto& f : chain) { if (f.on) act++; const FxDef& d = FX_LIB[std::clamp(f.kind, 0, FX_COUNT - 1)]; if (!names.empty()) names += " \xE2\x80\xBA "; names += d.name; }
        if (names.empty()) names = "none";
        char gpu[16], lat[16]; snprintf(gpu, sizeof gpu, "%d", 4 + act * 7); snprintf(lat, sizeof lat, "%d", act ? act * 2 : 0);
        std::vector<std::array<std::string, 4>> mrows = {
            {"Chain", names, "", ""}, {"GPU load", gpu, "%", "audio"},
            {"Render", "per-layer, pre-mask", "", ""}, {"Latency", lat, "ms", ""}};
        HLine(ox, ox + W, oy + y, K(pal::g2a));
        rowsOf(mrows);
      }
    }
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
  if (A.mediaStale) { A.mediaList = ListMedia(); A.mediaStale = false; }
  int fMed = folder("Media", "folder");   // B2: images from Documents/MikMap/media
  for (auto& p : A.mediaList) { item(fMed, std::filesystem::path(p).filename().string().c_str(), "video", "\xE2\x88\x9E", -1); rows.back().path = p; }
  if (A.mediaList.empty()) rows.push_back({"Add PNG/JPG to Documents/MikMap/media", "info", "", 1, -3, false, true, 0, ""});
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
static void DeckGridTools(ImRect r) {
  float cy = (r.Min.y + r.Max.y) * 0.5f, xr = r.Max.x - 6;
  struct TB { const char* l; Tone t; bool act; const char* ico; } tb[4] = {{"Layer", T_LIVE, false, "plus"}, {"Group", T_LIVE, false, "folder-plus"}, {"Column", T_LIVE, false, "plus"}, {"Sync", T_AUDIO, A.quantize, nullptr}};
  for (int i = 3; i >= 0; --i) {
    float w = ButtonW(tb[i].l, 0, tb[i].ico != nullptr);
    ImRect br(xr - w, cy - 8, xr, cy + 8);
    bool cl = Button(br, tb[i].l, tb[i].t, tb[i].act, 0, true, tb[i].ico);
    if (cl && i == 0) {
      Layer nl; nl.name = "Layer " + std::to_string(A.layers.size() + 1); nl.blend = "Normal"; nl.blendTime = 0; nl.opacity = 100;
      nl.clips.assign(8, Clip());
      A.layers.push_back(nl);
      EnsureLayerIds(A.layers);
    }
    if (cl && i == 1 && !A.layers.empty()) {   // Group: put the selected layer into a new group
      int li = std::clamp(A.selLayer, 0, (int)A.layers.size() - 1);
      Group ng; ng.id = A.uid("g"); ng.name = "Group " + std::to_string(A.groups.size() + 1); ng.role = 2; ng.open = true;
      A.groups.push_back(ng);
      A.layers[li].group = ng.id;
    }
    if (cl && i == 2) A.insertCol(A.colCount());
    if (cl && i == 3) { A.quantize = !A.quantize; A.pending.clear(); A.notify(A.quantize ? "Sync on \xE2\x80\x94 triggers wait for the next beat" : "Sync off \xE2\x80\x94 triggers fire immediately", 2.5); }   // Column: append an empty column
    xr = br.Min.x - 4;
  }
}

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
      CellOut o = ClipCell(cr, c, selc, isDrag, isDrop, c.progress);
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
        Text(gr.Min.x + 3 + 6 + 10 + 4, cy, UI_B, 11, K(pal::white), nm.c_str(), 0.09f);
        float nw = TextW(UI_B, 11, nm.c_str(), 0.09f);
        int nInGroup = 0; for (auto& q : A.layers) if (q.group == re.groupId) ++nInGroup;
        char cn[8]; snprintf(cn, sizeof cn, "%d", nInGroup);
        Text(gr.Min.x + 3 + 6 + 10 + 4 + nw + 4, cy, MONO_M, 10, K(pal::t77), cn);
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
    Text(pinX, sy + 15, UI_B, 9, K(pal::t66), "LAYERS", 0.14f);
    Icon("chevron-down", ImVec2(pinX + TextW(UI_B, 9, "LAYERS", 0.14f) + 9, sy + 15), 8, K(pal::t66));
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

// Deck tabs (switch/rename/duplicate/delete, design ref: deckTabs) + Grid/Timeline run-mode toggle, then
// whichever view is armed. Multiple decks let one show keep several independent layer/column sets (e.g. a
// "warm-up" deck and a "main set" deck) without them fighting over the same grid.
static void Deck(ImRect r) {
  Fill(r, K(pal::g12));
  ImRect tabsRow(r.Min.x, r.Min.y, r.Max.x, r.Min.y + 26);
  Fill(tabsRow, K(pal::g18));
  HLine(tabsRow.Min.x, tabsRow.Max.x, tabsRow.Max.y - 1, K(pal::g2a));
  {
    float cy = (tabsRow.Min.y + tabsRow.Max.y - 1) * 0.5f, x = tabsRow.Min.x + 6;
    for (int i = 0; i < (int)A.decks.size(); ++i) {
      bool cur = i == A.curDeckIdx;
      std::string lab = Upper(A.decks[i].name);
      float w = TextW(UI_B, 10, lab.c_str(), 0.09f) + 16;
      ImRect tr(x, cy - 10, x + w, cy + 10);
      Hit h = HitR(tr);
      if (cur) { Glow(tr, pal::coral, 0.25f, 8, 2); Fill(tr, K(pal::g12), 2); }
      Box(tr, cur ? K(pal::coral, 0.15f) : K(pal::g1c), cur ? K(pal::coral) : h.hover ? K(pal::g33) : K(pal::g22), 2);
      TextC((tr.Min.x + tr.Max.x) * 0.5f, cy, UI_B, 10, K(cur ? pal::coral : h.hover ? pal::white : pal::tcc), lab.c_str(), 0.09f);
      if (h.hover) CursorHand();
      if (h.click) A.switchDeck(i);
      if (h.dbl) A.beginRename(4, i, ImVec2(tr.Min.x, tr.Max.y + 4), A.decks[i].name);
      if (h.rclick) { A.deckMenu.open = true; A.deckMenu.idx = i; A.deckMenu.pos = ImGui::GetIO().MousePos; }
      x += w + 4;
    }
    // no trailing "+ DECK" button anymore — right-click any tab's menu now has "Add deck" + "Move left/right"
  }

  ImRect runRow(r.Min.x, tabsRow.Max.y, r.Max.x, tabsRow.Max.y + 34);
  Fill(runRow, K(pal::g18));
  HLine(runRow.Min.x, runRow.Max.x, runRow.Max.y - 1, K(pal::g2a));
  {
    float cy = (runRow.Min.y + runRow.Max.y - 1) * 0.5f;
    static const char* names[2] = {"Grid", "Timeline"};
    static const char* icons[2] = {"grid-3x3", "film"};
    float segW[2], segTotal = 4;   // each segment sized to fit its own label — "Timeline" is almost 2x "Grid"
    for (int i = 0; i < 2; ++i) { segW[i] = 12 + 8 + TextW(UI_B, 10, Upper(names[i]).c_str(), 0.09f) + 10; segTotal += segW[i]; }
    ImRect seg(runRow.Min.x + 6, cy - 12, runRow.Min.x + 6 + segTotal, cy + 12);
    Box(seg, K(pal::g050), K(pal::g2a), 4);
    float segX = seg.Min.x + 2;
    for (int i = 0; i < 2; ++i) {
      ImRect mr(segX, seg.Min.y + 2, segX + segW[i], seg.Max.y - 2);
      bool cur = A.deckMode == i;
      Hit h = HitR(mr);
      if (cur) { Fill(mr, K(pal::g12), 3); Box(mr, K(pal::coral, 0.15f), K(pal::coral), 3); }
      ImU32 fg = K(cur ? pal::coral : h.hover ? pal::white : pal::tcc);
      Icon(icons[i], ImVec2(mr.Min.x + 12, (mr.Min.y + mr.Max.y) * 0.5f), 10, fg);
      Text(mr.Min.x + 20, (mr.Min.y + mr.Max.y) * 0.5f, UI_B, 10, fg, Upper(names[i]).c_str(), 0.09f);
      if (h.hover) CursorHand();
      if (h.click) A.deckMode = i;
      segX += segW[i];
    }
    if (A.deckMode == 1) {
      float xr = runRow.Max.x - 6;
      const char* lab = A.tlLoopOn ? "LOOP ON" : "LOOP OFF";
      float w = ButtonW(lab, 0);
      ImRect br(xr - w, cy - 10, xr, cy + 10);
      if (Button(br, lab, T_PREVIEW, A.tlLoopOn)) A.tlLoopOn = !A.tlLoopOn;
    } else {
      DeckGridTools(runRow);
    }
  }

  ImRect body(r.Min.x, runRow.Max.y, r.Max.x, r.Max.y);
  if (A.deckMode == 0) DeckGrid(body); else TimelineView(body);
}

// Timeline run mode (design ref: tlLanes/tlTicks/blocks) — one lane per layer, its non-empty clips laid out
// back-to-back per A::tlLayout(), a shared 0..100 playhead, drag-from-Browser drops into the first empty slot
// (or a freshly appended column) for that layer. Clicking a block cues it; playback advances the playhead
// itself (see TlAdvance in main.cpp), which is what actually flips clips live via A::tlSync().
static void TimelineView(ImRect r) {
  Fill(r, K(pal::g050));
  const float sideW = 150, rulerH = 18, laneH = 40;
  ImRect side(r.Min.x, r.Min.y, r.Min.x + sideW, r.Max.y);
  ImRect main(r.Min.x + sideW, r.Min.y, r.Max.x, r.Max.y);
  Fill(side, K(pal::g14));

  ImRect sideHd(side.Min.x, side.Min.y, side.Max.x, side.Min.y + rulerH);
  Fill(sideHd, K(pal::g1c)); HLine(sideHd.Min.x, sideHd.Max.x, sideHd.Max.y - 1, K(pal::g2a));
  int bar = (int)(A.tlProgress / 6.25f) + 1;
  char barb[16]; snprintf(barb, sizeof barb, "BAR %02d", bar);
  Text(sideHd.Min.x + 8, (sideHd.Min.y + sideHd.Max.y) * 0.5f, MONO_B, 8, K(pal::t66), barb, 0.09f);

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
  Browser(ImRect(x0, y0, x0 + A.prefs.browserW, y0 + bandH));
  Inspector(ImRect(x1 - A.prefs.inspectorW, y0, x1, y0 + bandH));

  // monitors + timeline
  float mx0 = x0 + A.prefs.browserW + 4, mx1 = x1 - A.prefs.inspectorW - 4;
  float timelineH = (float)A.prefs.timelineH;
  float monH = bandH - 8 - 4 - timelineH;
  float mw = (mx1 - mx0 - 4) / 2.f;
  Monitor(Rc(mx0, y0 + 4, mw, monH), false);
  Monitor(Rc(mx0 + mw + 4, y0 + 4, mw, monH), true);
  if (timelineH > 0.5f) {
    ImRect tl(mx0, y0 + 4 + monH + 4, mx1, y0 + bandH - 4);
    Box(tl, K(pal::g18), K(pal::g2a), 3);
    Text(tl.Min.x + 8, tl.Min.y + 15, UI_B, 9, K(pal::t88), "SYSTEM TIME", 0.14f);
    char sysTm[16] = "--:--:--";
    { std::time_t tt = std::time(nullptr); std::tm tmv{};
#if defined(_WIN32)
      localtime_s(&tmv, &tt);
#else
      localtime_r(&tt, &tmv);
#endif
      std::strftime(sysTm, sizeof sysTm, "%H:%M:%S", &tmv); }
    Text(tl.Min.x + 8, tl.Min.y + 33, MONO_B, 13, K(pal::tf3), sysTm, 0.04f);
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
  }

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















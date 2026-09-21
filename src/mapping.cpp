// Advanced Mapping screen: mapping tree · stage canvas · slice/mask/screen properties.
#include "app.h"
#include <cmath>
#include <cstring>
#include <algorithm>

using namespace ui;

// ───────────────────────── model helpers ─────────────────────────
Screen* App::curScreen() {
  for (auto& s : screens) if (s.id == selSc) return &s;
  return screens.empty() ? nullptr : &screens[0];
}
Slice* App::curSlice() {
  Screen* sc = curScreen();
  if (!sc) return nullptr;
  for (auto& s : sc->slices) if (s.id == selSl) return &s;
  return sc->slices.empty() ? nullptr : &sc->slices[0];
}
Mask* App::curMask() {
  Slice* sl = curSlice();
  if (!sl || selMk.empty()) return nullptr;
  for (auto& m : sl->masks) if (m.id == selMk) return &m;
  return nullptr;
}

static void QuadOf(Slice& s, float x, float y, float w, float h) {
  s.q[0] = {x, y}; s.q[1] = {x + w, y}; s.q[2] = {x + w, y + h}; s.q[3] = {x, y + h};
}

static std::vector<float> Uni(int n) { std::vector<float> v; for (int i = 1; i < std::max(2, n); ++i) v.push_back((float)i / n); return v; }
static void MeshUV(const Slice& s, std::vector<float>& us, std::vector<float>& vs) {
  us = s.meshU.empty() ? Uni(s.meshCols) : s.meshU; vs = s.meshV.empty() ? Uni(s.meshRows) : s.meshV;
  std::sort(us.begin(), us.end()); std::sort(vs.begin(), vs.end());
}
static std::vector<std::vector<ImVec2>> MeshGrid(const Slice& s) {
  std::vector<float> uu, vv; MeshUV(s, uu, vv);
  std::vector<float> us = {0}, vs = {0};
  us.insert(us.end(), uu.begin(), uu.end()); us.push_back(1); vs.insert(vs.end(), vv.begin(), vv.end()); vs.push_back(1);
  if (s.meshPts.size() == vs.size() && !s.meshPts.empty() && s.meshPts[0].size() == us.size()) return s.meshPts;
  std::vector<std::vector<ImVec2>> g;
  for (float v : vs) { std::vector<ImVec2> row; for (float u : us) row.push_back(ImVec2(
      (1 - u) * (1 - v) * s.q[0].x + u * (1 - v) * s.q[1].x + u * v * s.q[2].x + (1 - u) * v * s.q[3].x,
      (1 - u) * (1 - v) * s.q[0].y + u * (1 - v) * s.q[1].y + u * v * s.q[2].y + (1 - u) * v * s.q[3].y)); g.push_back(row); }
  return g;
}

// unit square (u,v) → a point in the screen's 1920x1080 output space, honouring corner pin or mesh warp
ImVec2 SliceMapUV(const Slice& s, float u, float v) {
  if (s.warp != 0) {
    auto gr = MeshGrid(s);
    std::vector<float> uu, vv;
    MeshUV(s, uu, vv);
    std::vector<float> us = {0}, vs = {0};
    us.insert(us.end(), uu.begin(), uu.end()); us.push_back(1);
    vs.insert(vs.end(), vv.begin(), vv.end()); vs.push_back(1);
    int ci = 0, ri = 0;
    while (ci + 2 < (int)us.size() && u >= us[ci + 1]) ++ci;
    while (ri + 2 < (int)vs.size() && v >= vs[ri + 1]) ++ri;
    float lu = (u - us[ci]) / std::max(1e-6f, us[ci + 1] - us[ci]);
    float lv = (v - vs[ri]) / std::max(1e-6f, vs[ri + 1] - vs[ri]);
    const ImVec2& a = gr[ri][ci], & b = gr[ri][ci + 1], & c2 = gr[ri + 1][ci + 1], & d = gr[ri + 1][ci];
    return ImVec2((1 - lu) * (1 - lv) * a.x + lu * (1 - lv) * b.x + lu * lv * c2.x + (1 - lu) * lv * d.x,
                  (1 - lu) * (1 - lv) * a.y + lu * (1 - lv) * b.y + lu * lv * c2.y + (1 - lu) * lv * d.y);
  }
  return ImVec2((1 - u) * (1 - v) * s.q[0].x + u * (1 - v) * s.q[1].x + u * v * s.q[2].x + (1 - u) * v * s.q[3].x,
                (1 - u) * (1 - v) * s.q[0].y + u * (1 - v) * s.q[1].y + u * v * s.q[2].y + (1 - u) * v * s.q[3].y);
}

ImVec2 WarpMap::Map(float canvasX, float canvasY) const {
  if (!slice) return ImVec2(ox + canvasX * sx, oy + canvasY * sy);
  // canvas pixels → the slice's input rect in unit coordinates
  float u = (canvasX - slice->ix) / std::max(1, slice->iw);
  float v = (canvasY - slice->iy) / std::max(1, slice->ih);
  ImVec2 o = SliceMapUV(*slice, u, v);
  return ImVec2(ox + o.x * sx, oy + o.y * sy);
}

void App::addScreen() {
  pushHist();
  int n = (int)screens.size() + 1;
  Screen sc; sc.id = uid("screen"); sc.name = "Screen " + std::to_string(n); sc.outDev = "Projector / Display " + std::to_string(n);
  sc.role = 2;
  Slice sl; sl.id = uid("slice"); sl.name = "Slice 1"; QuadOf(sl, 100, 100, 1720, 880);
  sc.slices.push_back(sl);
  selSc = sc.id; selSl = sl.id; selMk.clear(); selKind = -1;
  screens.push_back(sc);
}
void App::addSlice() { pushHist();
  Screen* sc = curScreen(); if (!sc) return;
  Slice sl; sl.id = uid("slice"); sl.name = "Slice " + std::to_string(sc->slices.size() + 1);
  sl.ix = 200; sl.iy = 100; sl.iw = 800; sl.ih = 600; QuadOf(sl, 200, 100, 800, 600);
  selSl = sl.id; selMk.clear(); selKind = -1;
  sc->slices.push_back(sl);
}
void App::addMask() { pushHist();
  Slice* sl = curSlice(); if (!sl) return;
  Mask m; m.id = uid("mask"); m.name = "Mask " + std::to_string(sl->masks.size() + 1); m.inverted = true; m.feather = 4;
  m.pts[0] = {620, 380}; m.pts[1] = {1100, 380}; m.pts[2] = {1020, 720}; m.pts[3] = {700, 720};
  selSl = sl->id; selMk = m.id;
  sl->masks.push_back(m);
}
void App::deleteMask() { pushHist();
  Slice* sl = curSlice(); Mask* mk = curMask(); if (!sl || !mk) return;
  std::string id = mk->id;
  sl->masks.erase(std::remove_if(sl->masks.begin(), sl->masks.end(), [&](const Mask& m) { return m.id == id; }), sl->masks.end());
  selMk.clear(); selKind = -1;
}
void App::deleteSlice() { pushHist();
  Screen* sc = curScreen(); Slice* sl = curSlice();
  if (!sc || !sl || sc->slices.size() <= 1) return;
  std::string id = sl->id;
  sc->slices.erase(std::remove_if(sc->slices.begin(), sc->slices.end(), [&](const Slice& s) { return s.id == id; }), sc->slices.end());
  selSl = sc->slices[0].id; selMk.clear(); selKind = -1;
}
void App::resetWarp() { pushHist(); if (Slice* s = curSlice()) QuadOf(*s, (float)s->ix, (float)s->iy, (float)s->iw, (float)s->ih); }
void App::deleteScreen(const std::string& id) { pushHist();
  if (screens.size() <= 1) return;
  screens.erase(std::remove_if(screens.begin(), screens.end(), [&](const Screen& s) { return s.id == id; }), screens.end());
  if (selSc == id) { selSc = screens[0].id; selSl = screens[0].slices.empty() ? "" : screens[0].slices[0].id; selMk.clear(); selKind = -1; }
}
void App::removeSlice(const std::string& scId, const std::string& slId) {
  for (auto& sc : screens) if (sc.id == scId && sc.slices.size() > 1) {
    sc.slices.erase(std::remove_if(sc.slices.begin(), sc.slices.end(), [&](const Slice& s) { return s.id == slId; }), sc.slices.end());
    if (selSl == slId) { selSc = scId; selSl = sc.slices[0].id; selMk.clear(); selKind = -1; }
  }
}
void App::dupScreen(const std::string& id) { pushHist();
  for (size_t i = 0; i < screens.size(); ++i) if (screens[i].id == id) {
    Screen c = screens[i]; c.id = uid("screen"); c.name += " copy";
    for (auto& s : c.slices) { s.id = uid("slice"); for (auto& m : s.masks) m.id = uid("mask"); }
    selSc = c.id; selSl = c.slices.empty() ? "" : c.slices[0].id; selMk.clear(); selKind = -1;
    screens.insert(screens.begin() + i + 1, c);
    return;
  }
}
void App::dupSlice(const std::string& scId, const std::string& slId) { pushHist();
  for (auto& sc : screens) if (sc.id == scId) for (size_t i = 0; i < sc.slices.size(); ++i) if (sc.slices[i].id == slId) {
    Slice c = sc.slices[i]; c.id = uid("slice"); c.name += " copy";
    for (auto& m : c.masks) m.id = uid("mask");
    selSc = scId; selSl = c.id; selMk.clear(); selKind = -1;
    sc.slices.insert(sc.slices.begin() + i + 1, c);
    return;
  }
}
void App::moveScreen(const std::string& id, int dir) {
  for (int i = 0; i < (int)screens.size(); ++i) if (screens[i].id == id) {
    int j = i + dir; if (j < 0 || j >= (int)screens.size()) return;
    std::swap(screens[i], screens[j]); return;
  }
}
void App::moveSlice(const std::string& scId, const std::string& slId, int dir) {
  for (auto& sc : screens) if (sc.id == scId) for (int i = 0; i < (int)sc.slices.size(); ++i) if (sc.slices[i].id == slId) {
    int j = i + dir; if (j < 0 || j >= (int)sc.slices.size()) return;
    std::swap(sc.slices[i], sc.slices[j]); return;
  }
}

std::vector<MenuItem> App::screenMenu(const std::string& scId) {
  int i = 0; Screen* sc = nullptr;
  for (int k = 0; k < (int)screens.size(); ++k) if (screens[k].id == scId) { i = k; sc = &screens[k]; }
  std::vector<MenuItem> m;
  if (!sc) return m;
  bool vis = sc->visible;
  auto mk = [](const char* l, const char* ic, bool dis, bool danger, std::function<void()> f) { MenuItem x; x.label = l; x.icon = ic; x.disabled = dis; x.danger = danger; x.run = f; return x; };
  m.push_back(mk(vis ? "Hide from output" : "Show on output", vis ? "eye-off" : "eye", false, false, [this, scId] { for (auto& s : screens) if (s.id == scId) s.visible = !s.visible; }));
  m.push_back(mk("Move up", "arrow-up", i <= 0, false, [this, scId] { moveScreen(scId, -1); }));
  m.push_back(mk("Move down", "arrow-down", i >= (int)screens.size() - 1, false, [this, scId] { moveScreen(scId, 1); }));
  m.push_back(mk("Duplicate screen", "copy", false, false, [this, scId] { dupScreen(scId); }));
  m.push_back(mk("Add slice", "plus", false, false, [this, scId] {
    for (auto& s : screens) if (s.id == scId) { selSc = scId; selSl = s.slices.empty() ? "" : s.slices[0].id; selMk.clear(); selKind = -1; }
    addSlice(); }));
  m.push_back(mk("Delete screen", "trash-2", screens.size() <= 1, true, [this, scId] { deleteScreen(scId); }));
  return m;
}
std::vector<MenuItem> App::sliceMenu(const std::string& scId, const std::string& slId) {
  std::vector<MenuItem> m;
  for (auto& sc : screens) if (sc.id == scId) for (int i = 0; i < (int)sc.slices.size(); ++i) if (sc.slices[i].id == slId) {
    bool vis = sc.slices[i].visible; int n = (int)sc.slices.size();
    auto mk = [](const char* l, const char* ic, bool dis, bool danger, std::function<void()> f) { MenuItem x; x.label = l; x.icon = ic; x.disabled = dis; x.danger = danger; x.run = f; return x; };
    auto sel = [this, scId, slId] { selSc = scId; selSl = slId; selMk.clear(); selKind = -1; };
    m.push_back(mk(vis ? "Hide from output" : "Show on output", vis ? "eye-off" : "eye", false, false, [this, scId, slId] {
      for (auto& s : screens) if (s.id == scId) for (auto& l : s.slices) if (l.id == slId) l.visible = !l.visible; }));
    m.push_back(mk("Move up", "arrow-up", i <= 0, false, [this, scId, slId] { moveSlice(scId, slId, -1); }));
    m.push_back(mk("Move down", "arrow-down", i >= n - 1, false, [this, scId, slId] { moveSlice(scId, slId, 1); }));
    m.push_back(mk("Duplicate slice", "copy", false, false, [this, scId, slId] { dupSlice(scId, slId); }));
    m.push_back(mk("Reset warp", "rotate-ccw", false, false, [this, sel] { sel(); resetWarp(); }));
    m.push_back(mk("Add mask", "scissors", false, false, [this, sel] { sel(); addMask(); }));
    m.push_back(mk("Delete slice", "trash-2", n <= 1, true, [this, scId, slId] { removeSlice(scId, slId); }));
  }
  return m;
}
std::vector<MenuItem> App::maskMenu(const std::string& scId, const std::string& slId, const std::string& mkId) {
  std::vector<MenuItem> m;
  auto mk = [](const char* l, const char* ic, bool danger, std::function<void()> f) { MenuItem x; x.label = l; x.icon = ic; x.danger = danger; x.run = f; return x; };
  bool inv = true;
  for (auto& sc : screens) if (sc.id == scId) for (auto& sl : sc.slices) if (sl.id == slId) for (auto& k : sl.masks) if (k.id == mkId) inv = k.inverted;
  m.push_back(mk(inv ? "Make additive" : "Invert (cut hole)", "scissors", false, [this, scId, slId, mkId] {
    selSc = scId; selSl = slId; selMk = mkId; if (Mask* k = curMask()) k->inverted = !k->inverted; }));
  m.push_back(mk("Duplicate mask", "copy", false, [this, scId, slId, mkId] {
    for (auto& sc : screens) if (sc.id == scId) for (auto& sl : sc.slices) if (sl.id == slId) for (auto k : sl.masks) if (k.id == mkId) {
      k.id = uid("mask"); k.name += " copy"; sl.masks.push_back(k); return; } }));
  m.push_back(mk("Delete mask", "trash-2", true, [this, scId, slId, mkId] {
    for (auto& sc : screens) if (sc.id == scId) for (auto& sl : sc.slices) if (sl.id == slId)
      sl.masks.erase(std::remove_if(sl.masks.begin(), sl.masks.end(), [&](const Mask& k) { return k.id == mkId; }), sl.masks.end()); }));
  return m;
}

// ───────────────────────── tree ─────────────────────────
struct Node {
  enum Kind { ScreenN, SliceN, MaskN } kind;
  std::string sc, sl, mk, name, meta;
  float h, pad;
  bool open = true, canHide = true, vis = true;
  uint32_t color = pal::coral;
};

static void TreeRow(ImRect r, const Node& n, bool railMode) {
  Screen* cs = A.curScreen(); Slice* csl = A.curSlice(); Mask* cmk = A.curMask();
  bool scOn = cs && n.sc == cs->id;
  bool on;
  uint32_t hexOn;
  if (n.kind == Node::ScreenN) { on = scOn; hexOn = n.color; }
  else if (n.kind == Node::SliceN) { on = scOn && csl && n.sl == csl->id && !cmk; hexOn = pal::coral; }
  else { on = cmk && cmk->id == n.mk; hexOn = pal::yellow; }
  Hit h = HitR(r);
  float prev = g.alpha;
  if (!n.vis) g.alpha = 0.5f;
  float mixA = n.kind == Node::ScreenN ? 0.12f : 0.15f, borA = n.kind == Node::ScreenN ? 0.55f : 0.40f;
  if (on) Box(r, K(hexOn, mixA), n.kind == Node::MaskN ? 0 : K(hexOn, borA), 3);
  float cy = (r.Min.y + r.Max.y) * 0.5f;
  float x = r.Min.x + n.pad;
  bool chevHit = false;
  if (n.kind == Node::ScreenN && !railMode) {
    ImRect cr(x, cy - 8, x + 12, cy + 8);
    Icon(n.open ? "chevron-down" : "chevron-right", ImVec2(x + 6, cy), 12, K(pal::t88));
    if (HitR(cr).click) { A.collapsedScreens[n.sc] = n.open; chevHit = true; }
    x += 12 + 6;
  } else if (!railMode) x += 12 + 6;
  const char* ico = n.kind == Node::ScreenN ? "monitor" : n.kind == Node::SliceN ? "maximize" : "scissors";
  float isz = n.kind == Node::MaskN ? 10.f : n.kind == Node::ScreenN ? 12.f : 11.f;
  uint32_t icol = n.kind == Node::ScreenN ? (scOn ? n.color : pal::t88) : n.kind == Node::SliceN ? pal::cyan : pal::yellow;
  Icon(ico, ImVec2(x + isz * 0.5f, cy), isz, K(icol));
  x += isz + 6;
  uint32_t fg = on ? hexOn : n.kind == Node::ScreenN ? pal::tcc : n.kind == Node::SliceN ? pal::t88 : pal::t77;
  float right = r.Max.x - 6;
  bool visHit = false;
  if (n.canHide) {
    ImRect er(right - 11, cy - 8, right, cy + 8);
    Icon(n.vis ? "eye" : "eye-off", ImVec2(right - 5.5f, cy), 11, K(n.vis ? pal::t88 : pal::t66));
    if (HitR(er).click) {
      visHit = true;
      for (auto& s : A.screens) if (s.id == n.sc) {
        if (n.kind == Node::ScreenN) s.visible = !s.visible;
        else for (auto& l : s.slices) if (l.id == n.sl) l.visible = !l.visible;
      }
    }
    right -= 11 + 6;
  }
  if (!n.meta.empty()) { TextR(right, cy, MONO_R, 9, K(pal::t66), n.meta.c_str()); right -= TextW(MONO_R, 9, n.meta.c_str()) + 6; }
  TextEll(x, cy, right - x, n.kind == Node::ScreenN ? UI_B : UI_S, 10, K(fg), n.name.c_str());
  g.alpha = prev;
  if (h.hover) CursorHand();
  if (h.click && !chevHit && !visHit) {
    if (n.kind == Node::ScreenN) {
      Screen* sc = nullptr; for (auto& s : A.screens) if (s.id == n.sc) sc = &s;
      bool keep = false; if (sc) for (auto& sl : sc->slices) if (sl.id == A.selSl) keep = true;
      A.selSc = n.sc; A.selSl = keep ? A.selSl : (sc && !sc->slices.empty() ? sc->slices[0].id : ""); A.selMk.clear(); A.selKind = 0;
    } else { A.selSc = n.sc; A.selSl = n.sl; A.selMk = n.kind == Node::MaskN ? n.mk : ""; A.selKind = n.kind == Node::MaskN ? 2 : 1; }
  }
  if (h.rclick) {
    if (n.kind == Node::ScreenN) A.openCtx(ImGui::GetIO().MousePos, A.screenMenu(n.sc));
    else if (n.kind == Node::SliceN) A.openCtx(ImGui::GetIO().MousePos, A.sliceMenu(n.sc, n.sl));
    else A.openCtx(ImGui::GetIO().MousePos, A.maskMenu(n.sc, n.sl, n.mk));
  }
}

static std::vector<Node> BuildTree(bool onlyScreen, const std::string& only) {
  std::vector<Node> out;
  for (auto& sc : A.screens) {
    if (onlyScreen && sc.id != only) continue;
    bool open = !(A.collapsedScreens.count(sc.id) && A.collapsedScreens[sc.id]);
    if (!onlyScreen) {
      Node n; n.kind = Node::ScreenN; n.sc = sc.id; n.name = sc.name; n.meta = std::to_string(sc.w) + "x" + std::to_string(sc.h);
      n.h = 28; n.pad = 6; n.open = open; n.vis = sc.visible; n.color = RoleHex(sc.role); out.push_back(n);
      if (!open) continue;
    }
    for (auto& sl : sc.slices) {
      Node s; s.kind = Node::SliceN; s.sc = sc.id; s.sl = sl.id; s.name = sl.name; s.h = onlyScreen ? 24 : 26; s.pad = onlyScreen ? 6 : 16; s.vis = sl.visible; out.push_back(s);
      for (auto& m : sl.masks) {
        Node k; k.kind = Node::MaskN; k.sc = sc.id; k.sl = sl.id; k.mk = m.id; k.name = m.name; k.h = 24; k.pad = onlyScreen ? 20 : 28; k.canHide = false; out.push_back(k);
      }
    }
  }
  return out;
}

static void TreePanel(ImRect r) {
  Fill(r, K(pal::g12));
  VLine(r.Max.x - 1, r.Min.y, r.Max.y, K(pal::g2a));
  PanelHeader(Rc(r.Min.x, r.Min.y, r.GetWidth() - 1, 24), "Mapping tree", pal::t88);
  {
    float cy = r.Min.y + 11.5f, xr = r.Max.x - 1 - 6;
    Icon("panel-left-close", ImVec2(xr - 6, cy), 12, K(pal::t88));
    if (HitR(ImRect(xr - 14, cy - 9, xr + 2, cy + 9)).click) { A.treeCollapsed = true; A.railScreen = A.selSc; }
    xr -= 12 + 8;
    std::string c = std::to_string(A.screens.size()) + " screens";
    TextR(xr, cy, MONO_M, 10, K(pal::t66), c.c_str());
  }
  float footH = 46;
  static ScrollArea sa;
  sa.Begin("##tree", ImRect(r.Min.x, r.Min.y + 24, r.Max.x - 1, r.Max.y - footH));
  float ox = sa.origin.x, oy = sa.origin.y, y = 4;
  auto nodes = BuildTree(false, "");
  for (auto& n : nodes) {
    ImRect rr(ox + 6, oy + y, ox + r.GetWidth() - 1 - 6, oy + y + n.h);
    TreeRow(rr, n, false);
    y += n.h + 2;
  }
  sa.End(r.GetWidth() - 1, y + 4);
  // footer actions
  ImRect fr(r.Min.x, r.Max.y - footH, r.Max.x - 1, r.Max.y);
  Fill(fr, K(pal::g14));
  HLine(fr.Min.x, fr.Max.x, fr.Min.y, K(pal::g2a));
  float bw = (fr.GetWidth() - 12 - 12) / 3.f;
  struct Ac { const char* l; const char* ic; uint32_t fg; bool primary; } acts[3] = {{"Screen", "monitor", pal::coral, true}, {"Slice", "plus", pal::white, false}, {"Mask", "scissors", pal::yellow, false}};
  for (int i = 0; i < 3; ++i) {
    ImRect br(fr.Min.x + 6 + i * (bw + 6), fr.Min.y + 9, fr.Min.x + 6 + i * (bw + 6) + bw, fr.Min.y + 9 + 28);
    Hit h = HitR(br);
    Box(br, acts[i].primary ? K(pal::coral, 0.15f) : h.hover ? K(pal::ctrlHover) : K(pal::g1c), acts[i].primary ? K(pal::coral, 0.4f) : K(pal::g22), 3);
    float tw = TextW(UI_B, 10, acts[i].l), cx = (br.Min.x + br.Max.x) * 0.5f, cy = (br.Min.y + br.Max.y) * 0.5f;
    Icon(acts[i].ic, ImVec2(cx - tw * 0.5f - 8, cy), 11, K(acts[i].fg));
    Text(cx - tw * 0.5f + 4, cy, UI_B, 10, K(acts[i].fg), acts[i].l);
    if (h.hover) CursorHand();
    if (h.click) { if (i == 0) A.addScreen(); else if (i == 1) A.addSlice(); else A.addMask(); }
  }
}

static void RailPanel(ImRect r) {
  Fill(r, K(pal::g12));
  VLine(r.Max.x - 1, r.Min.y, r.Max.y, K(pal::g2a));
  float cx = (r.Min.x + r.Max.x - 1) * 0.5f, y = r.Min.y + 6;
  ImRect eb(cx - 14, y, cx + 14, y + 26);
  Hit eh = HitR(eb);
  Box(eb, K(pal::g1c), K(pal::g22), 3);
  Icon("panel-left-open", ImVec2(cx, y + 13), 13, K(pal::t88));
  if (eh.hover) CursorHand();
  if (eh.click) { A.treeCollapsed = false; A.railScreen.clear(); }
  y += 26 + 6;
  Fill(Rc(cx - 11, y, 22, 1), K(pal::g2a));
  y += 1 + 6;
  Screen* cs = A.curScreen();
  float railTop = 0;
  for (size_t i = 0; i < A.screens.size(); ++i) {
    Screen& sc = A.screens[i];
    ImRect br(cx - 14, y, cx + 14, y + 30);
    Hit h = HitR(br);
    bool on = cs && sc.id == cs->id;
    Box(br, on ? K(pal::coral, 0.18f) : K(pal::g1c), on ? K(pal::coral, 0.55f) : K(pal::g22), 3);
    ImU32 fg = K(on ? pal::coral : pal::t88);
    Icon("monitor", ImVec2(cx, y + 11), 12, fg);
    TextC(cx, y + 23, MONO_B, 8, fg, std::to_string(i + 1).c_str());
    if (A.railScreen == sc.id) railTop = y;
    if (h.hover) CursorHand();
    if (h.click) { A.selSc = sc.id; A.selSl = sc.slices.empty() ? "" : sc.slices[0].id; A.selMk.clear(); A.selKind = 0; A.railScreen = A.railScreen == sc.id ? "" : sc.id; }
    if (h.rclick) A.openCtx(ImGui::GetIO().MousePos, A.screenMenu(sc.id));
    y += 30 + 6;
  }
  (void)railTop;
}

static void RailPopover(ImRect rail) {
  if (A.railScreen.empty()) return;
  int idx = 0; Screen* sc = nullptr;
  for (int i = 0; i < (int)A.screens.size(); ++i) if (A.screens[i].id == A.railScreen) { idx = i; sc = &A.screens[i]; }
  if (!sc) return;
  auto nodes = BuildTree(true, sc->id);
  float listH = std::min(260.f, (float)nodes.size() * 26 + 8);
  float w = 212, h = 24 + listH + 42;
  ImRect r(rail.Max.x + 6, rail.Min.y + 58 + idx * 36, rail.Max.x + 6 + w, rail.Min.y + 58 + idx * 36 + h);
  ImDrawList* dl = g.dl;
  dl->AddRectFilled(ImVec2(r.Min.x - 1, r.Min.y - 1), ImVec2(r.Max.x + 1, r.Max.y + 1), Ca(K(0x000000, 0.4f)), 5);
  Box(r, K(pal::g16), K(pal::g3a), 4);
  ImRect hd(r.Min.x + 1, r.Min.y + 1, r.Max.x - 1, r.Min.y + 25);
  Fill(hd, K(pal::g18)); HLine(hd.Min.x, hd.Max.x, hd.Max.y - 1, K(pal::g2a));
  std::string t = Upper(sc->name);
  TextEll(hd.Min.x + 6, (hd.Min.y + hd.Max.y) * 0.5f, w - 40, UI_B, 9, K(pal::tcc), t.c_str(), 0.09f);
  ImRect xb(hd.Max.x - 22, hd.Min.y, hd.Max.x, hd.Max.y);
  Icon("x", ImVec2(hd.Max.x - 12, (hd.Min.y + hd.Max.y) * 0.5f), 12, K(pal::t66));
  if (HitR(xb).click) A.railScreen.clear();
  float y = r.Min.y + 24 + 4;
  for (auto& n : nodes) {
    ImRect rr(r.Min.x + 5, y, r.Max.x - 5, y + 24);
    TreeRow(rr, n, true);
    y += 26;
  }
  ImRect fr(r.Min.x + 1, r.Max.y - 43, r.Max.x - 1, r.Max.y - 1);
  Fill(fr, K(pal::g14)); HLine(fr.Min.x, fr.Max.x, fr.Min.y, K(pal::g2a));
  for (int i = 0; i < 2; ++i) {
    float bw = (fr.GetWidth() - 18) / 2.f;
    ImRect br(fr.Min.x + 6 + i * (bw + 6), fr.Min.y + 9, fr.Min.x + 6 + i * (bw + 6) + bw, fr.Min.y + 9 + 24);
    Hit h = HitR(br);
    Box(br, h.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
    TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, UI_B, 10, K(i ? pal::yellow : pal::white), i ? "Mask" : "Slice");
    if (h.hover) CursorHand();
    if (h.click) { if (i) A.addMask(); else A.addSlice(); }
  }
}

// ───────────────────────── stage ─────────────────────────
static int dragKind = 0, dragIdx = 0;  // 1 corner, 2 input, 3 mask point

static bool PointInPoly(ImVec2 p, const ImVec2* v, int n) {
  bool in = false;
  for (int i = 0, j = n - 1; i < n; j = i++)
    if (((v[i].y > p.y) != (v[j].y > p.y)) && (p.x < (v[j].x - v[i].x) * (p.y - v[i].y) / (v[j].y - v[i].y) + v[i].x)) in = !in;
  return in;
}

static void FillPoly(const ImVec2* p, int n, ImU32 c) {
  // fan triangulation (quads are convex; masks are user-edited quads)
  for (int i = 1; i + 1 < n; ++i) g.dl->AddTriangleFilled(p[0], p[i], p[i + 1], Ca(c));
}

static void Stage(ImRect r) {
  Fill(r, K(pal::g050));
  Screen* sc = A.curScreen(); Slice* sl = A.curSlice(); Mask* mk = A.curMask();
  ImVec2 m = ImGui::GetIO().MousePos;
  // toolbar
  ImRect tb(r.Min.x, r.Min.y, r.Max.x, r.Min.y + 44);
  Fill(tb, K(pal::g12)); HLine(tb.Min.x, tb.Max.x, tb.Max.y - 1, K(pal::g2a));
  float cy = r.Min.y + 21.5f;
  const char* pages[2] = {"Input selection", "Output routing"};
  uint32_t pcol[2] = {pal::cyan, pal::coral};
  float pw[2]; float tot = 4;
  for (int i = 0; i < 2; ++i) { pw[i] = TextW(UI_B, 10, Upper(pages[i]).c_str(), 0.09f) + 18; tot += pw[i] + (i ? 0 : 2); }
  ImRect grp(tb.Min.x + 6, cy - 14, tb.Min.x + 6 + tot + 2, cy + 14);
  Box(grp, K(0x000000), K(pal::g2a), 4);
  float px = grp.Min.x + 3;
  for (int i = 0; i < 2; ++i) {
    ImRect pr(px, cy - 11, px + pw[i], cy + 11);
    Hit h = HitR(pr);
    bool on = A.mpage == i;
    if (on) Fill(pr, K(pcol[i]), 3);
    TextC((pr.Min.x + pr.Max.x) * 0.5f, cy, UI_B, 10, K(on ? pal::white : pal::t77), Upper(pages[i]).c_str(), 0.09f);
    if (h.hover) CursorHand();
    if (h.click) A.mpage = i;
    px += pw[i] + 2;
  }
  float leftEnd = grp.Max.x;
  if (sl && A.mpage != 0) {
    ImRect wg(grp.Max.x + 4, cy - 14, grp.Max.x + 4 + 4 + 21 * 2 + 2 + 2, cy + 14);
    Box(wg, K(0x000000), K(pal::g2a), 4);
    const char* wi[2] = {"frame", "grid-3x3"};
    for (int i = 0; i < 2; ++i) {
      ImRect br(wg.Min.x + 3 + i * 23, cy - 11, wg.Min.x + 3 + i * 23 + 21, cy + 11);
      bool on = (i == 0) == (sl->warp == 0);
      Hit h = HitR(br);
      Box(br, on ? K(pal::coral, 0.2f) : 0, on ? K(pal::coral, 0.5f) : 0, 3);
      Icon(wi[i], ImVec2((br.Min.x + br.Max.x) * 0.5f, cy), 11, K(on ? pal::coral : pal::t77));
      if (h.hover) CursorHand();
      if (h.click && !on) { A.pushHist(); sl->warp = i; }
    }
    leftEnd = wg.Max.x;
  }
  // right side: zoom group, history/reset group, readout
  float xr = tb.Max.x - 6;
  {
    const char* zi[5] = {"zoom-in", "zoom-out", "scan-search", "maximize", A.mapFocus ? "minimize-2" : "expand"};
    char zl[16]; snprintf(zl, sizeof zl, "%d%%", (int)std::round(A.mapZ * 100));
    float gw = 2 + 5 * 20 + 26 + 4 + 2;
    ImRect zg(xr - gw, cy - 13, xr, cy + 13);
    Box(zg, K(pal::g1c), K(pal::g22), 3);
    float zx = zg.Min.x + 3;
    for (int i = 0; i < 5; ++i) {
      ImRect br(zx, cy - 10, zx + 18, cy + 10);
      Hit h = HitR(br);
      if (h.hover) Fill(br, K(pal::g1c), 2);
      uint32_t hex = i == 2 ? pal::coral : (i == 4 && A.mapFocus) ? pal::coral : h.hover ? pal::white : pal::tcc;
      Icon(zi[i], ImVec2((br.Min.x + br.Max.x) * 0.5f, cy), 11, K(hex));
      if (h.hover) CursorHand();
      if (h.click) {
        if (i == 0) A.setZoom(A.mapZ * 1.4f); else if (i == 1) A.setZoom(A.mapZ / 1.4f);
        else if (i == 2 && sl) {
          float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
          for (int k = 0; k < 4; ++k) { mnx = std::min(mnx, sl->q[k].x); mxx = std::max(mxx, sl->q[k].x); mny = std::min(mny, sl->q[k].y); mxy = std::max(mxy, sl->q[k].y); }
          float w = (mxx - mnx) / 1920.f, hh = (mxy - mny) / 1080.f;
          A.setZoom(std::clamp(0.85f / std::max(w, hh), 2.f, 6.f), (mnx + mxx) * 0.5f / 1920.f, (mny + mxy) * 0.5f / 1080.f);
        } else if (i == 3) A.setZoom(1, 0.5f, 0.5f);
        else if (i == 4) A.mapFocus = !A.mapFocus;
      }
      zx += 20;
    }
    TextC(zx + 11, cy, MONO_B, 9, K(pal::tcc), zl);
    xr = zg.Min.x - 4;
    float hw = 2 + 20 * 2 + 2 + 1 + 2 + 20 + 2 + 2;
    ImRect hg(xr - hw, cy - 13, xr, cy + 13);
    Box(hg, K(pal::g1c), K(pal::g22), 3);
    float hx = hg.Min.x + 3;
    for (int i = 0; i < 2; ++i) {
      ImRect br(hx, cy - 10, hx + 20, cy + 10);
      bool can = i == 0 ? !A.undoStack.empty() : !A.redoStack.empty();
      Hit h = HitR(br);
      if (h.hover && can) Fill(br, K(pal::g1c), 2);
      float prev = g.alpha; if (!can) g.alpha *= 0.4f;
      Icon(i == 0 ? "undo-2" : "redo-2", ImVec2((br.Min.x + br.Max.x) * 0.5f, cy), 11, K(can ? (h.hover ? pal::white : pal::tcc) : pal::t66));
      g.alpha = prev;
      if (h.hover && can) CursorHand();
      if (h.click) { if (i == 0) A.undoMap(); else A.redoMap(); }
      hx += 22;
    }
    VLine(hx, cy - 7, cy + 7, K(pal::g2a)); hx += 3;
    ImRect rb(hx, cy - 10, hx + 20, cy + 10);
    Hit rh = HitR(rb);
    if (rh.hover) Fill(rb, K(pal::g1c), 2);
    Icon("rotate-ccw", ImVec2((rb.Min.x + rb.Max.x) * 0.5f, cy), 12, K(rh.hover ? pal::white : pal::t88));
    if (rh.hover) CursorHand();
    if (rh.click) {
      std::vector<MenuItem> mi;
      auto mk = [](const char* l, const char* ic, bool dis, std::function<void()> f) { MenuItem x; x.label = l; x.icon = ic; x.disabled = dis; x.run = f; return x; };
      mi.push_back(mk("Reset 4 corner pins", "frame", false, [] { A.pushHist(); A.resetWarp(); }));
      bool noMesh = !A.curSlice() || A.curSlice()->warp == 0;
      mi.push_back(mk("Reset mesh warp", "grid-3x3", noMesh, [] { if (Slice* s = A.curSlice()) { A.pushHist(); s->meshPts.clear(); s->meshU.clear(); s->meshV.clear(); } }));
      mi.push_back(mk("Reset all warping", "rotate-ccw", false, [] { A.pushHist(); A.resetWarp(); if (Slice* s = A.curSlice()) { s->meshPts.clear(); s->meshU.clear(); s->meshV.clear(); } }));
      A.openCtx(ImVec2(rb.Min.x - 60, rb.Max.y + 4), mi);
    }
    xr = hg.Min.x - 6;
    std::string ro = A.mpage == 0 ? "Source Content: 1920x1080" : (sc ? sc->name + " (" + sc->outDev + ")" : "");
    float lim = leftEnd + 8;
    float rw = std::min(TextW(MONO_R, 10, ro.c_str()), std::max(0.f, xr - lim));
    TextEll(xr - rw, cy, rw, MONO_R, 10, K(pal::t88), ro.c_str());
  }  // canvas viewport
  ImRect area(r.Min.x, r.Min.y + 44, r.Max.x, r.Max.y);
  float innerW = area.GetWidth() - 20, innerH = area.GetHeight() - 20;
  if (innerW < 60 || !sc) return;
  // apply pending zoom request (centres the requested point, like the prototype)
  auto canvasW = [&](float z) {
    if (z > 1.001f) return z * innerW;
    float w = A.mapFocus ? innerW : std::min(innerW, 960.f);
    return std::min(w, innerH * 16.f / 9.f);
  };
  if (A.mapReq.valid) {
    A.mapZ = std::clamp(A.mapReq.z, 1.f, 6.f);
    A.mapCx = A.mapReq.cx; A.mapCy = A.mapReq.cy;
    float cwn = canvasW(A.mapZ), chn = cwn * 9.f / 16.f;
    A.mapScrollX = cwn * A.mapCx - area.GetWidth() * 0.5f + 10;
    A.mapScrollY = chn * A.mapCy - area.GetHeight() * 0.5f + 10;
    A.mapReq.valid = false;
  }
  float cw = canvasW(A.mapZ), ch = cw * 9.f / 16.f;
  float contentW = cw + 20, contentH = ch + 20;
  float maxSX = std::max(0.f, contentW - area.GetWidth()), maxSY = std::max(0.f, contentH - area.GetHeight());
  bool inArea = area.Contains(m) && !g.blocked;
  ImGuiIO& io = ImGui::GetIO();
  if (inArea && !ImGui::GetIO().KeyAlt && io.MouseWheel != 0.f) A.mapScrollY -= io.MouseWheel * 48.f;
  if (inArea && !ImGui::GetIO().KeyAlt && io.MouseWheelH != 0.f) A.mapScrollX -= io.MouseWheelH * 48.f;
  A.mapScrollX = std::clamp(A.mapScrollX, 0.f, maxSX); A.mapScrollY = std::clamp(A.mapScrollY, 0.f, maxSY);
  ImRect cv;
  cv.Min.x = maxSX > 0 ? area.Min.x + 10 - A.mapScrollX : (area.Min.x + area.Max.x) * 0.5f - cw * 0.5f;
  cv.Min.y = maxSY > 0 ? area.Min.y + 10 - A.mapScrollY : (area.Min.y + area.Max.y) * 0.5f - ch * 0.5f;
  cv.Max = ImVec2(cv.Min.x + cw, cv.Min.y + ch);
  float s = cw / 1920.f;
  // alt+wheel zoom about the cursor, right-drag pan
  if (inArea && io.KeyAlt && io.MouseWheel != 0.f) {
    float cxn = std::clamp((m.x - cv.Min.x) / cw, 0.f, 1.f), cyn = std::clamp((m.y - cv.Min.y) / ch, 0.f, 1.f);
    A.setZoom(A.mapZ * (io.MouseWheel > 0 ? 1.15f : 1.f / 1.15f), cxn, cyn);
  }
  static bool panning = false; static ImVec2 panM; static float panX, panY;
  if (inArea && ImGui::IsMouseClicked(1) && !panning) { panning = true; panM = m; panX = A.mapScrollX; panY = A.mapScrollY; }
  if (panning) {
    if (ImGui::IsMouseDown(1)) { A.mapScrollX = std::clamp(panX - (m.x - panM.x), 0.f, maxSX); A.mapScrollY = std::clamp(panY - (m.y - panM.y), 0.f, maxSY); }
    else panning = false;
  }
  auto toPx = [&](ImVec2 p) { return ImVec2(cv.Min.x + p.x * s, cv.Min.y + p.y * s); };
  ImVec2 mu(std::clamp(std::round((m.x - cv.Min.x) / cw * 1920.f), 0.f, 1920.f), std::clamp(std::round((m.y - cv.Min.y) / ch * 1080.f), 0.f, 1080.f));

  g.dl->PushClipRect(area.Min, area.Max, true);
  Box(cv, K(0x0d0d0d), 0, 3);
  g.dl->PushClipRect(cv.Min, cv.Max, true);
  for (float x = cv.Min.x + 39; x < cv.Max.x; x += 40) VLine(std::floor(x), cv.Min.y, cv.Max.y, K(0xffffff, 0.045f));
  for (float y = cv.Min.y + 39; y < cv.Max.y; y += 40) HLine(cv.Min.x, cv.Max.x, std::floor(y), K(0xffffff, 0.045f));

  // ---- drag processing ----
  if (dragKind && !ImGui::IsMouseDown(0)) dragKind = 0;
  if (dragKind && sl) {
    if (dragKind == 4) {
      auto gr = MeshGrid(*sl); sl->meshPts = gr;
      int rr = dragIdx / 100, cc = dragIdx % 100;
      if (rr < (int)sl->meshPts.size() && cc < (int)sl->meshPts[rr].size()) sl->meshPts[rr][cc] = mu;
    }
    else if (dragKind == 1) sl->q[dragIdx] = mu;
    else if (dragKind == 3 && mk) mk->pts[dragIdx] = mu;
    else if (dragKind == 2) {
      int x = (int)mu.x, y = (int)mu.y, ix = sl->ix, iy = sl->iy, iw = sl->iw, ih = sl->ih, nx = ix, ny = iy, nw = iw, nh = ih;
      if (dragIdx == 0) { nx = x; ny = y; nw = ix + iw - x; nh = iy + ih - y; }
      else if (dragIdx == 1) { ny = y; nw = x - ix; nh = iy + ih - y; }
      else if (dragIdx == 2) { nw = x - ix; nh = y - iy; }
      else { nx = x; nw = ix + iw - x; nh = y - iy; }
      if (nw > 20 && nh > 20) { sl->ix = nx; sl->iy = ny; sl->iw = nw; sl->ih = nh; }
    }
  }

  bool scVis = sc->visible;
  bool overCanvas = inArea && cv.Contains(m) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem);
  bool clickPending = ImGui::IsMouseClicked(0) && overCanvas && !dragKind;

  if (A.mpage == 0) {
    if (sl && sl->visible && scVis) {
      ImRect ir(cv.Min.x + sl->ix * s, cv.Min.y + sl->iy * s, cv.Min.x + (sl->ix + sl->iw) * s, cv.Min.y + (sl->iy + sl->ih) * s);
      Fill(ir, K(pal::cyan, 0.15f));
      Border(ir, K(pal::cyan), 0, 2);
      char dm[48]; snprintf(dm, sizeof dm, "%d \xC3\x97 %d", sl->iw, sl->ih);
      float mxc = (ir.Min.x + ir.Max.x) * 0.5f, myc = (ir.Min.y + ir.Max.y) * 0.5f;
      TextC(mxc, myc - 7, MONO_B, 11, K(pal::cyan), sl->name.c_str());
      TextC(mxc, myc + 7, MONO_R, 9, K(pal::tcc), dm);
      ImVec2 hp[4] = {{ir.Min.x, ir.Min.y}, {ir.Max.x, ir.Min.y}, {ir.Max.x, ir.Max.y}, {ir.Min.x, ir.Max.y}};
      for (int i = 0; i < 4; ++i) {
        ImRect hr(hp[i].x - 6, hp[i].y - 6, hp[i].x + 6, hp[i].y + 6);
        Box(hr, K(pal::white), K(pal::cyan), 2);
        Border(hr, K(pal::cyan), 2, 2);
        ImRect big(hr.Min.x - 3, hr.Min.y - 3, hr.Max.x + 3, hr.Max.y + 3);
        if (inArea && big.Contains(m)) { CursorHand(); if (ImGui::IsMouseClicked(0)) { A.pushHist(); dragKind = 2; dragIdx = i; } }
      }
    }
  } else if (scVis) {
    bool consumed = false;
    auto polyPx = [&](const ImVec2* q, int n, ImVec2* out) { for (int i = 0; i < n; ++i) out[i] = toPx(q[i]); };
    // zoom-to-zone chip next to the selected slice label
    ImRect zoomBtn; bool haveZoom = false;
    if (sl && sl->visible) {
      float cxq = 0, cyq = 0; for (int i = 0; i < 4; ++i) { cxq += sl->q[i].x; cyq += sl->q[i].y; }
      cxq /= 4; cyq /= 4;
      float xp = cxq / 1920.f, maxW = std::min(xp, 1 - xp) * 2 * cw - 12;
      ImVec2 c = toPx(ImVec2(cxq, cyq));
      float nw = std::min(TextW(MONO_B, 11, sl->name.c_str()), std::max(0.f, maxW - 20));
      float total = nw + 4 + 16;
      zoomBtn = ImRect(c.x - total * 0.5f + nw + 4, c.y - 8, c.x - total * 0.5f + nw + 20, c.y + 8);
      haveZoom = true;
    }
    if (clickPending && haveZoom && zoomBtn.Contains(m)) {
      float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
      for (int k = 0; k < 4; ++k) { mnx = std::min(mnx, sl->q[k].x); mxx = std::max(mxx, sl->q[k].x); mny = std::min(mny, sl->q[k].y); mxy = std::max(mxy, sl->q[k].y); }
      float w = (mxx - mnx) / 1920.f, hh = (mxy - mny) / 1080.f;
      A.setZoom(std::clamp(0.85f / std::max(w, hh), 2.f, 6.f), (mnx + mxx) * 0.5f / 1920.f, (mny + mxy) * 0.5f / 1080.f);
      consumed = true;
    }
    if (sl && sl->visible && clickPending && !consumed) {
      for (int i = 0; i < 4 && !consumed; ++i) if (std::hypot(m.x - toPx(sl->q[i]).x, m.y - toPx(sl->q[i]).y) <= 16 * s + 2) { A.pushHist(); dragKind = 1; dragIdx = i; consumed = true; }
      if (mk) for (int i = 0; i < 4 && !consumed; ++i) if (std::hypot(m.x - toPx(mk->pts[i]).x, m.y - toPx(mk->pts[i]).y) <= 13 * s + 2) { A.pushHist(); dragKind = 3; dragIdx = i; consumed = true; }
    } else if (mk && clickPending && !consumed) {
      for (int i = 0; i < 4 && !consumed; ++i) if (std::hypot(m.x - toPx(mk->pts[i]).x, m.y - toPx(mk->pts[i]).y) <= 13 * s + 2) { A.pushHist(); dragKind = 3; dragIdx = i; consumed = true; }
    }
    if (clickPending && !consumed && sl && sl->visible && sl->warp != 0) {
      auto gr = MeshGrid(*sl);
      for (int rr = 0; rr < (int)gr.size() && !consumed; ++rr) for (int cc = 0; cc < (int)gr[rr].size() && !consumed; ++cc) {
        bool corner = (rr == 0 || rr == (int)gr.size() - 1) && (cc == 0 || cc == (int)gr[rr].size() - 1);
        if (corner) continue;
        bool edge = rr == 0 || rr == (int)gr.size() - 1 || cc == 0 || cc == (int)gr[rr].size() - 1;
        ImVec2 p = toPx(gr[rr][cc]);
        if (std::hypot(m.x - p.x, m.y - p.y) <= (edge ? 7 : 8) * s + 2) { A.pushHist(); dragKind = 4; dragIdx = rr * 100 + cc; consumed = true; }
      }
    }
    if (clickPending && !consumed) {
      ImVec2 pp[4];
      for (auto& S : sc->slices) if (S.visible) for (auto& M : S.masks) { polyPx(M.pts, 4, pp); if (!consumed && PointInPoly(m, pp, 4)) { A.selSc = sc->id; A.selSl = S.id; A.selMk = M.id; A.selKind = 2; consumed = true; } }
      for (auto it = sc->slices.rbegin(); it != sc->slices.rend() && !consumed; ++it) if (it->visible) { polyPx(it->q, 4, pp); if (PointInPoly(m, pp, 4)) {
        bool wasOn = sl && it->id == sl->id;
        A.selSc = sc->id; A.selSl = it->id; A.selMk.clear(); A.selKind = 1; consumed = true;
        if (wasOn && it->warp != 0) {
          float mnx = 1e9f, mxx = -1e9f, mny = 1e9f, mxy = -1e9f;
          for (int k = 0; k < 4; ++k) { mnx = std::min(mnx, it->q[k].x); mxx = std::max(mxx, it->q[k].x); mny = std::min(mny, it->q[k].y); mxy = std::max(mxy, it->q[k].y); }
          float u = std::clamp((mu.x - mnx) / std::max(1.f, mxx - mnx), 0.02f, 0.98f), v = std::clamp((mu.y - mny) / std::max(1.f, mxy - mny), 0.02f, 0.98f);
          if (A.meshArm) {
            std::vector<float> us, vs; MeshUV(*it, us, vs);
            std::vector<float>& lst = A.meshArm == 'u' ? us : vs; float p = A.meshArm == 'u' ? u : v;
            bool dup = false; for (float x : lst) if (std::fabs(x - p) < 0.01f) dup = true;
            if (!dup) { A.pushHist(); lst.push_back(p); std::sort(lst.begin(), lst.end()); it->meshU = us; it->meshV = vs; it->warp = 1; }
            A.meshArm = 0;
          }
          A.meshPickOn = true; A.meshPickU = u; A.meshPickV = v;
        }
      } }
    }
    sl = A.curSlice(); mk = A.curMask();
    for (auto& S : sc->slices) {
      if (!S.visible) continue;
      bool on = sl && S.id == sl->id;
      ImVec2 pp[4]; polyPx(S.q, 4, pp);
      FillPoly(pp, 4, on ? K(pal::coral, 0.22f) : K(0xffffff, 0.05f));
      if (on) g.dl->AddPolyline(pp, 4, Ca(K(pal::coral)), ImDrawFlags_Closed, 3.f * std::max(0.6f, s * 2));
      else DashedPoly(pp, 4, K(0x444444), 1.5f, 10 * s * 2, 6 * s * 2);
    }
    if (sl && sl->visible && sl->warp != 0) {
      auto gr = MeshGrid(*sl);
      for (int rr = 1; rr + 1 < (int)gr.size(); ++rr) { std::vector<ImVec2> pts; for (auto& p : gr[rr]) pts.push_back(toPx(p)); for (size_t k = 0; k + 1 < pts.size(); ++k) DashedPoly(std::vector<ImVec2>{pts[k], pts[k + 1]}.data(), 2, K(pal::coral, 0.55f), 1.5f, 8 * s * 2, 6 * s * 2); }
      for (int cc = 1; cc + 1 < (int)gr[0].size(); ++cc) { std::vector<ImVec2> pts; for (auto& row : gr) pts.push_back(toPx(row[cc])); for (size_t k = 0; k + 1 < pts.size(); ++k) DashedPoly(std::vector<ImVec2>{pts[k], pts[k + 1]}.data(), 2, K(pal::coral, 0.55f), 1.5f, 8 * s * 2, 6 * s * 2); }
      for (int rr = 0; rr < (int)gr.size(); ++rr) for (int cc = 0; cc < (int)gr[rr].size(); ++cc) {
        bool corner = (rr == 0 || rr == (int)gr.size() - 1) && (cc == 0 || cc == (int)gr[rr].size() - 1);
        if (corner) continue;
        bool edge = rr == 0 || rr == (int)gr.size() - 1 || cc == 0 || cc == (int)gr[rr].size() - 1;
        ImVec2 p = toPx(gr[rr][cc]);
        g.dl->AddCircleFilled(p, (edge ? 7 : 8) * s, Ca(K(edge ? pal::yellow : pal::coral)), 20); g.dl->AddCircle(p, (edge ? 7 : 8) * s, Ca(K(0x050505)), 20, 2.f);
        if (std::hypot(m.x - p.x, m.y - p.y) <= (edge ? 7 : 8) * s + 2 && overCanvas) CursorHand();
      }
      if (A.meshPickOn) {
        float u = A.meshPickU, v = A.meshPickV;
        ImVec2 mp = toPx(ImVec2((1 - u) * (1 - v) * sl->q[0].x + u * (1 - v) * sl->q[1].x + u * v * sl->q[2].x + (1 - u) * v * sl->q[3].x,
                                (1 - u) * (1 - v) * sl->q[0].y + u * (1 - v) * sl->q[1].y + u * v * sl->q[2].y + (1 - u) * v * sl->q[3].y));
        g.dl->AddCircleFilled(mp, 9 * s, Ca(K(pal::yellow)), 20); g.dl->AddCircle(mp, 9 * s, Ca(K(0x050505)), 20, 2.f);
      }
    }
    for (auto& S : sc->slices) {
      if (!S.visible) continue;
      for (auto& M : S.masks) {
        bool on = mk && mk->id == M.id;
        ImVec2 pp[4]; polyPx(M.pts, 4, pp);
        FillPoly(pp, 4, on ? K(pal::yellow, 0.30f) : K(pal::red, 0.20f));
        DashedPoly(pp, 4, on ? K(pal::yellow) : K(pal::red), 2.f, 10 * s * 2, 8 * s * 2);
      }
    }
    if (sl && sl->visible && haveZoom) {
      float cxq = 0, cyq = 0; for (int i = 0; i < 4; ++i) { cxq += sl->q[i].x; cyq += sl->q[i].y; }
      cxq /= 4; cyq /= 4;
      float xp = cxq / 1920.f, maxW = std::min(xp, 1 - xp) * 2 * cw - 12;
      ImVec2 c = toPx(ImVec2(cxq, cyq));
      float nw = std::min(TextW(MONO_B, 11, sl->name.c_str()), std::max(0.f, maxW - 20));
      float total = nw + 4 + 16;
      TextEll(c.x - total * 0.5f, c.y, nw, MONO_B, 11, K(pal::coral), sl->name.c_str());
      ImRect zb = ImRect(c.x - total * 0.5f + nw + 4, c.y - 8, c.x - total * 0.5f + nw + 20, c.y + 8);
      Box(zb, K(pal::g1c, 0.8f), K(pal::g22), 2);
      Icon("scan-search", ImVec2((zb.Min.x + zb.Max.x) * 0.5f, c.y), 10, K(pal::coral));
      if (inArea && zb.Contains(m)) CursorHand();
    }
    if (mk) for (int i = 0; i < 4; ++i) {
      ImVec2 p = toPx(mk->pts[i]);
      g.dl->AddCircleFilled(p, 13 * s, Ca(K(pal::yellow)), 24); g.dl->AddCircle(p, 13 * s, Ca(K(pal::white)), 24, 2.f);
      if (std::hypot(m.x - p.x, m.y - p.y) <= 13 * s + 2 && overCanvas) CursorHand();
    }
    if (sl && sl->visible) for (int i = 0; i < 4; ++i) {
      ImVec2 p = toPx(sl->q[i]);
      g.dl->AddCircleFilled(p, 16 * s, Ca(K(pal::coral)), 24); g.dl->AddCircle(p, 16 * s, Ca(K(pal::white)), 24, 3.f);
      if (std::hypot(m.x - p.x, m.y - p.y) <= 16 * s + 2 && overCanvas) CursorHand();
    }
  }
  g.dl->PopClipRect();
  Border(cv, K(pal::g2a), 3);
  // scrollbars (indicators)
  if (maxSX > 0) {
    float tw = area.GetWidth() - 8, th = std::max(24.f, tw * area.GetWidth() / contentW);
    float tx = area.Min.x + (tw - th) * (A.mapScrollX / maxSX);
    g.dl->AddRectFilled(ImVec2(tx, area.Max.y - 9), ImVec2(tx + th, area.Max.y - 1), Ca(K(pal::g33)), 8);
  }
  if (maxSY > 0) {
    float tw = area.GetHeight() - 8, th = std::max(24.f, tw * area.GetHeight() / contentH);
    float ty = area.Min.y + (tw - th) * (A.mapScrollY / maxSY);
    g.dl->AddRectFilled(ImVec2(area.Max.x - 9, ty), ImVec2(area.Max.x - 1, ty + th), Ca(K(pal::g33)), 8);
  }
  g.dl->PopClipRect();
}
// ───────────────────────── properties ─────────────────────────
static void Label(float x, float y, const char* t, uint32_t hex = pal::t88) {
  Text(x, y + 4.5f, MONO_R, 9, K(hex), Upper(t).c_str(), 0.09f);
}

static void PropsPanel(ImRect r) {
  Fill(r, K(pal::g12));
  VLine(r.Min.x, r.Min.y, r.Max.y, K(pal::g2a));
  Screen* sc = A.curScreen(); Slice* sl = A.curSlice(); Mask* mk = A.curMask();
  int kind = A.MapKind();
  if (kind == 2 && !mk) kind = 1;
  if (kind == 1 && !sl) kind = 0;
  PanelHeader(Rc(r.Min.x + 1, r.Min.y, r.GetWidth() - 1, 24), kind == 0 ? "Screen properties" : kind == 2 ? "Mask properties" : "Slice properties", pal::t88);
  Badge(r.Max.x - 6, r.Min.y + 11.5f, kind == 0 ? "Screen" : kind == 2 ? "Mask" : "Slice", kind == 0 ? T_AUDIO : kind == 2 ? T_STANDBY : T_LIVE, true);
  static ScrollArea sa;
  sa.Begin("##mapprops", ImRect(r.Min.x + 1, r.Min.y + 24, r.Max.x, r.Max.y));
  float ox = sa.origin.x + 0, oy = sa.origin.y, W = r.GetWidth() - 1 - 8, x = ox + 8, w = W - 16, y = 8;
  bool output = A.mpage != 0;
  if (kind == 1) {
    Label(x, oy + y, "Slice name"); y += 9 + 4;
    TextField("##slicename", Rc(x, oy + y, w, 28), sl->name);
    y += 28 + 8;
    if (sl->warp != 0) {
      HLine(ox + 8, ox + 8 + w, oy + y, K(pal::g2a)); y += 1 + 6;
      std::vector<float> uu, vv; MeshUV(*sl, uu, vv);
      int ncol = (int)uu.size() + 1, nrow = (int)vv.size() + 1;
      bool custom = !sl->meshU.empty() || !sl->meshV.empty();
      Text(x, oy + y + 4.5f, MONO_R, 9, K(pal::t88), "OUTPUT MESH GRID", 0.09f);
      char ml[64]; snprintf(ml, sizeof ml, "%d \xC3\x97 %d patches%s", ncol, nrow, custom ? " \xC2\xB7 custom" : "");
      TextR(x + w, oy + y + 4.5f, MONO_B, 9, K(pal::coral), ml);
      y += 9 + 4;
      float fw = (w - 12) / 2.f;
      for (int i = 0; i < 2; ++i) {
        float fx = x + i * (fw + 12);
        Text(fx, oy + y + 4.5f, MONO_R, 9, K(pal::t66), i ? "ROWS" : "COLUMNS", 0.09f);
        int n = i ? nrow : ncol;
        ImRect mb(fx, oy + y + 11, fx + 22, oy + y + 11 + 26), pb(fx + fw - 22, oy + y + 11, fx + fw, oy + y + 11 + 26);
        ImRect vb(fx + 24, oy + y + 11, fx + fw - 24, oy + y + 11 + 26);
        int nv = n; bool chg = false;
        Hit hm = HitR(mb), hp = HitR(pb);
        Box(mb, K(pal::g1c), hm.hover ? K(pal::coral) : K(pal::g22), 3); TextC((mb.Min.x + mb.Max.x) * 0.5f, (mb.Min.y + mb.Max.y) * 0.5f, MONO_B, 11, K(hm.hover ? pal::coral : pal::tcc), "\xE2\x88\x92");
        Box(pb, K(pal::g1c), hp.hover ? K(pal::coral) : K(pal::g22), 3); TextC((pb.Min.x + pb.Max.x) * 0.5f, (pb.Min.y + pb.Max.y) * 0.5f, MONO_B, 11, K(hp.hover ? pal::coral : pal::tcc), "+");
        Box(vb, K(pal::g050), K(pal::g22), 3);
        char nb[8]; snprintf(nb, sizeof nb, "%d", n);
        TextC((vb.Min.x + vb.Max.x) * 0.5f, (vb.Min.y + vb.Max.y) * 0.5f, MONO_B, 11, K(pal::tf3), nb);
        if (hm.hover || hp.hover) CursorHand();
        if (hm.click) { nv = n - 1; chg = true; } if (hp.click) { nv = n + 1; chg = true; }
        if (chg) { A.pushHist(); nv = std::clamp(nv, 2, 16); if (i) { sl->meshRows = nv; sl->meshV.clear(); } else { sl->meshCols = nv; sl->meshU.clear(); } sl->meshPts.clear(); }
      }
      y += 11 + 26 + 6;
      {
        const char* pl = A.meshArm ? (A.meshArm == 'u' ? "CLICK CANVAS TO PLACE COLUMN" : "CLICK CANVAS TO PLACE ROW") : A.meshPickOn ? "" : "PICK ADD COL / ADD ROW FIRST";
        char pk[64]; if (!A.meshArm && A.meshPickOn) snprintf(pk, sizeof pk, "LAST POINT \xC2\xB7 U %d%% \xC2\xB7 V %d%%", (int)std::round(A.meshPickU * 100), (int)std::round(A.meshPickV * 100)); else snprintf(pk, sizeof pk, "%s", pl);
        uint32_t ph = A.meshArm ? pal::yellow : A.meshPickOn ? pal::coral : pal::t66;
        float bw1 = TextW(MONO_B, 9, "FLATTEN", 0.09f) + 12 + 2, bw2 = TextW(MONO_B, 9, "UNIFORM", 0.09f) + 12 + 2;
        ImRect ub(x + w - bw2, oy + y, x + w, oy + y + 20), fb(x + w - bw2 - 4 - bw1, oy + y, x + w - bw2 - 4, oy + y + 20);
        TextEll(x, oy + y + 10, fb.Min.x - 4 - x, MONO_R, 9, K(ph), pk, 0.09f);
        Hit hf = HitR(fb), hu = HitR(ub);
        Box(fb, K(pal::g1c), hf.hover ? K(pal::coral) : K(pal::g22), 3); TextC((fb.Min.x + fb.Max.x) * 0.5f, oy + y + 10, MONO_B, 9, K(hf.hover ? pal::coral : pal::t88), "FLATTEN", 0.09f);
        Box(ub, K(pal::g1c), hu.hover ? K(pal::yellow) : K(pal::g22), 3); TextC((ub.Min.x + ub.Max.x) * 0.5f, oy + y + 10, MONO_B, 9, K(hu.hover ? pal::yellow : pal::t88), "UNIFORM", 0.09f);
        if (hf.hover || hu.hover) CursorHand();
        if (hf.click) { A.pushHist(); sl->meshPts.clear(); }
        if (hu.click) { A.pushHist(); sl->meshU.clear(); sl->meshV.clear(); sl->meshPts.clear(); }
        y += 20 + 6;
      }
      {
        float bw = (w - 6) / 2.f;
        for (int i = 0; i < 2; ++i) {
          ImRect br(x + i * (bw + 6), oy + y, x + i * (bw + 6) + bw, oy + y + 24);
          bool armed = A.meshArm == (i ? 'v' : 'u');
          Hit h = HitR(br);
          if (h.hover) Glow(br, pal::coral, 0.3f, 10, 3);
          Box(br, armed ? K(pal::yellow, 0.22f) : K(pal::coral, 0.15f), armed ? K(pal::yellow) : K(pal::coral, 0.45f), 3);
          TextC((br.Min.x + br.Max.x) * 0.5f, (br.Min.y + br.Max.y) * 0.5f, MONO_B, 9, K(armed ? pal::yellow : pal::coral), i ? "+ ADD ROW" : "+ ADD COL", 0.09f);
          if (h.hover) CursorHand();
          if (h.click) { char want = i ? 'v' : 'u'; A.meshArm = A.meshArm == want ? 0 : want; }
        }
        y += 24 + 8;
      }
    }
    if (output) {
      HLine(ox + 8, ox + 8 + w, oy + y, K(pal::g2a)); y += 1 + 6;
      Label(x, oy + y, "Corner pins"); y += 9 + 4;
      const char* ck[4] = {"TL", "TR", "BR", "BL"};
      for (int i = 0; i < 4; ++i) {
        ImRect cr(x, oy + y, x + w, oy + y + 24);
        Box(cr, K(pal::g18), K(pal::g2a), 3);
        Text(cr.Min.x + 8, cr.Min.y + 12, MONO_B, 10, K(pal::coral), ck[i]);
        char v[64]; snprintf(v, sizeof v, "X: %d  |  Y: %d", (int)sl->q[i].x, (int)sl->q[i].y);
        TextR(cr.Max.x - 8, cr.Min.y + 12, MONO_M, 10, K(pal::tcc), v);
        y += 24 + 4;
      }
      y += 4;
    } else {
      HLine(ox + 8, ox + 8 + w, oy + y, K(pal::g2a)); y += 1 + 6;
      Label(x, oy + y, "Input rectangle (px)"); y += 9 + 4;
      const char* fl[4] = {"X", "Y", "Width", "Height"};
      int* vals[4] = {&sl->ix, &sl->iy, &sl->iw, &sl->ih};
      float fw = (w - 6) / 2.f;
      for (int i = 0; i < 4; ++i) {
        float fx = x + (i % 2) * (fw + 6), fy = y + (i / 2) * (35 + 6);
        Text(fx, oy + fy + 4.5f, MONO_R, 9, K(pal::t66), fl[i]);
        char id[32]; snprintf(id, sizeof id, "##rect%d", i);
        IntField(id, Rc(fx, oy + fy + 11, fw, 24), *vals[i]);
      }
      y += 35 * 2 + 6 + 8;
    }
  }
  if (kind == 2) {
    Label(x, oy + y, "Mask name"); y += 9 + 6;
    TextField("##maskname", Rc(x, oy + y, w, 28), mk->name);
    y += 28 + 6;
    HLine(x, x + w, oy + y, K(pal::g2a)); y += 1 + 4;
    ImRect ir(x, oy + y, x + w, oy + y + 24);
    Hit ih = HitR(ir);
    Text(x, ir.Min.y + 12, UI_S, 10, K(pal::te0), "Invert mask (cut hole)");
    ImRect cb(ir.Max.x - 14, ir.Min.y + 5, ir.Max.x, ir.Min.y + 19);
    Box(cb, mk->inverted ? K(pal::yellow) : K(pal::g050), mk->inverted ? K(pal::yellow) : K(pal::g22), 2);
    if (mk->inverted) Check(ImVec2((cb.Min.x + cb.Max.x) * 0.5f, (cb.Min.y + cb.Max.y) * 0.5f), 12, K(0x0f0f0f));
    if (ih.hover) CursorHand();
    if (ih.click) mk->inverted = !mk->inverted;
    y += 24 + 6;
    Text(x, oy + y + 5, UI_S, 10, K(pal::t88), "Feather");
    char fb[16]; snprintf(fb, sizeof fb, "%dpx", mk->feather);
    TextR(x + w, oy + y + 5, MONO_B, 10, K(pal::yellow), fb);
    y += 10 + 6;
    float fv = mk->feather / 40.f * 100.f;
    if (Slider(0x3001, Rc(x, oy + y + 4, w, 6), fv, pal::yellow)) mk->feather = (int)std::round(fv / 100.f * 40.f);
    y += 14 + 6;
    if (output) {
      HLine(x, x + w, oy + y, K(pal::g2a)); y += 1 + 4;
      Label(x, oy + y, "Mask points (output)"); y += 9 + 4;
      for (int i = 0; i < 4; ++i) {
        ImRect cr(x, oy + y, x + w, oy + y + 22);
        Box(cr, K(pal::g18), K(pal::g2a), 3);
        char k[8]; snprintf(k, sizeof k, "P%d", i + 1);
        Text(cr.Min.x + 8, cr.Min.y + 11, MONO_B, 10, K(pal::yellow), k);
        char v[64]; snprintf(v, sizeof v, "X: %d  |  Y: %d", (int)mk->pts[i].x, (int)mk->pts[i].y);
        TextR(cr.Max.x - 8, cr.Min.y + 11, MONO_R, 10, K(pal::tcc), v);
        y += 22 + 6;
      }
    } else {
      HLine(x, x + w, oy + y, K(pal::g2a)); y += 1 + 4;
      for (auto& ln : std::vector<std::string>{"Mask geometry is edited in Output mode. Switch the", "stage to Output to move mask points."}) {
        Text(x, oy + y + 8, UI_S, 10, K(pal::t66), ln.c_str(), 0.01f); y += 16;
      }
      y += 2;
    }
    ImRect dr(x, oy + y, x + w, oy + y + 28);
    Hit dh = HitR(dr);
    Box(dr, K(pal::red, 0.14f), K(pal::red, 0.5f), 3);
    TextC((dr.Min.x + dr.Max.x) * 0.5f, dr.Min.y + 14, UI_B, 10, K(pal::red), "Delete mask");
    if (dh.hover) CursorHand();
    if (dh.click) A.deleteMask();
    y += 28 + 8;
    HLine(ox, ox + W, oy + y - 1, K(pal::g2a));
  }
  if (kind == 0 && sc) {
    HLine(ox, ox + W, oy + y, K(pal::g2a));
    y += 1 + 8;
    Text(x, oy + y + 5, MONO_R, 9, K(pal::t66), "OUTPUT TARGET", 0.09f);
    TextR(x + w, oy + y + 5, MONO_R, 9, K(pal::mint), sc->name.c_str());
    y += 10 + 6;
    float bx = y;
    ImRect box(x, oy + bx, x + w, oy + bx + 6 + 9 + 2 + 26 + 6 + 9 + 2 + 26 + 6 + 9 + 6 + 4 + 22 + 6);
    Box(box, K(pal::g18), K(pal::g2a), 3);
    float ix = x + 6, iw = w - 12, yy = bx + 6;
    Text(ix, oy + yy + 4.5f, MONO_R, 9, K(pal::t66), "SCREEN NAME", 0.09f); yy += 9 + 2;
    TextField("##scname", Rc(ix, oy + yy, iw, 26), sc->name); yy += 26 + 6;
    Text(ix, oy + yy + 4.5f, MONO_R, 9, K(pal::t66), "OUTPUT DEVICE", 0.09f); yy += 9 + 2;
    TextField("##scout", Rc(ix, oy + yy, iw, 26), sc->outDev); yy += 26 + 6;
    char rl[64]; snprintf(rl, sizeof rl, "RES: %d x %d @ %dHz", sc->w, sc->h, sc->fps);
    Text(ix, oy + yy + 4.5f, MONO_R, 9, K(pal::tcc), rl); yy += 9 + 6;
    HLine(ix, ix + iw, oy + yy, K(pal::g2a)); yy += 1 + 4;
    ImRect er(ix, oy + yy, ix + iw, oy + yy + 22);
    Hit eh = HitR(er);
    Text(ix, er.Min.y + 11, UI_S, 10, K(pal::t88), "Edge blending");
    const char* el = sc->edgeBlend ? "ENABLED" : "DISABLED";
    float bw2 = TextW(MONO_B, 9, el, 0.09f) + 12 + 2;
    ImRect bb(er.Max.x - bw2, er.Min.y + 2, er.Max.x, er.Min.y + 20);
    Box(bb, sc->edgeBlend ? K(pal::mint, 0.2f) : K(pal::g1c), sc->edgeBlend ? K(pal::mint, 0.4f) : K(pal::g22), 3);
    Text(bb.Min.x + 7, (bb.Min.y + bb.Max.y) * 0.5f, MONO_B, 9, K(sc->edgeBlend ? pal::mint : pal::t66), el, 0.09f);
    if (eh.hover) CursorHand();
    if (eh.click) sc->edgeBlend = !sc->edgeBlend;
    y = bx + box.GetHeight() + 8;
    // ── F2/I1: send this screen to a physical display ──
    HLine(ox, ox + W, oy + y, K(pal::g2a)); y += 1 + 8;
    Text(x, oy + y + 5, MONO_R, 9, K(pal::t88), "PROJECTOR OUTPUT", 0.09f);
    y += 9 + 4;
    {
      ImRect mr(x, oy + y, x + w, oy + y + 26);
      Hit mh = HitR(mr);
      Box(mr, mh.hover ? K(pal::ctrlHover) : K(pal::g1c), K(pal::g22), 3);
      std::string mn = MonitorName(A.outMonitor);
      TextEll(mr.Min.x + 8, (mr.Min.y + mr.Max.y) * 0.5f, w - 30, MONO_R, 10, K(pal::tcc), mn.c_str());
      Icon("chevron-down", ImVec2(mr.Max.x - 12, (mr.Min.y + mr.Max.y) * 0.5f), 10, K(pal::t66));
      if (mh.hover) CursorHand();
      if (mh.click && MonitorCount() > 0) {          // cycle through the connected displays
        A.outMonitor = (A.outMonitor + 1) % MonitorCount();
        if (OutputOpen()) OpenOutput(glfwWin(), A.outMonitor);
      }
      y += 26 + 6;
      ImRect ob(x, oy + y, x + w, oy + y + 30);
      Hit oh = HitR(ob);
      bool on = OutputOpen();
      if (on) Glow(ob, pal::coral, 0.35f, 12, 3);
      Box(ob, on ? K(pal::coral, 0.2f) : K(pal::g1c), on ? K(pal::coral) : K(pal::g22), 3);
      const char* lb = on ? "\xC4\x90\xC3\x93NG OUTPUT (F11)" : "M\xE1\xBB\x9E OUTPUT (F11)";
      TextC((ob.Min.x + ob.Max.x) * 0.5f, (ob.Min.y + ob.Max.y) * 0.5f, UI_B, 10, K(on ? pal::coral : pal::tcc), lb, 0.09f);
      if (oh.hover) CursorHand();
      if (oh.click) ToggleOutput(glfwWin(), A.outMonitor);
      y += 30 + 8;
    }
  }
  sa.End(W, y + 8);
}

void DrawMapping(ImRect body) {
  float x0 = body.Min.x, x1 = body.Max.x;
  Fill(body, K(pal::g0f));
  float lw = A.treeCollapsed ? 46.f : (A.mapFocus ? 0.f : 236.f);
  float rw = A.mapFocus ? 0.f : 280.f;
  ImRect left(x0, body.Min.y, x0 + lw, body.Max.y);
  if (lw > 0) { if (A.treeCollapsed) RailPanel(left); else TreePanel(left); }
  if (rw > 0) PropsPanel(ImRect(x1 - rw, body.Min.y, x1, body.Max.y));
  Stage(ImRect(x0 + lw, body.Min.y, x1 - rw, body.Max.y));
  if (A.treeCollapsed) RailPopover(left);
}




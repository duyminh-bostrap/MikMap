// Project + settings persistence (X1). A project (.mikmap, JSON) describes ONE show: deck, mapping, sensor calibration.
// Settings describe THIS machine and user (language, fonts, theme, output display) and live in the OS config folder —
// the two are never mixed, so opening a colleague's project does not change your language or move your output window.
#include "app.h"

#include "core/util/Json.h"

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace fs = std::filesystem;
using mikmap::JsonValue;

namespace {

const int kFormat = 1;
// While set, Serialize() drops values that change every frame during a show (playhead, live/cued highlight) so that
// "unsaved changes" only reflects real edits, not the show simply running.
bool gForDirty = false;

fs::path HomeDir() {
#ifdef _WIN32
  if (const char* h = std::getenv("USERPROFILE")) return h;
#else
  if (const char* h = std::getenv("HOME")) return h;
#endif
  return fs::current_path();
}

JsonValue V2(const ImVec2& p) { JsonValue a = JsonValue::array(); a.push(p.x); a.push(p.y); return a; }
ImVec2 ReadV2(const JsonValue& v, ImVec2 def = ImVec2(0, 0)) {
  if (!v.isArray() || v.size() < 2) return def;
  return ImVec2((float)v.at(0).asNumber(def.x), (float)v.at(1).asNumber(def.y));
}
float F(const JsonValue& v, const char* k, float def) { return (float)v[k].asNumber(def); }

JsonValue FxJ(const Fx& f) {
  JsonValue o = JsonValue::object();
  o.set("kind", f.kind); o.set("on", f.on); o.set("beat", f.beat); o.set("react", f.react);
  o.set("mix", f.mix); o.set("p0", f.p[0]); o.set("p1", f.p[1]); o.set("en", f.en);
  return o;
}
Fx ReadFx(const JsonValue& o) {
  Fx f;
  f.kind = std::clamp(o["kind"].asInt(0), 0, FX_COUNT - 1);
  f.on = o["on"].asBool(true); f.beat = o["beat"].asBool(false); f.react = o["react"].asBool(false);
  f.mix = F(o, "mix", 100); f.p[0] = F(o, "p0", 0); f.p[1] = F(o, "p1", 0); f.en = o["en"].asInt(0);
  return f;
}

JsonValue ClipJ(const Clip& c) {
  JsonValue o = JsonValue::object();
  // The cue/live *selection* highlight is UI state, not show content: store Selected as Loaded, LiveSel as Live.
  int st = c.st == Clip::Selected ? Clip::Loaded : c.st == Clip::LiveSel ? Clip::Live : c.st;
  if (gForDirty && st == Clip::Live) st = Clip::Loaded;
  o.set("st", st); o.set("name", c.name); o.set("dur", c.dur); o.set("color", c.color); o.set("style", c.style); o.set("media", c.media); o.set("progress", gForDirty ? 0.f : c.progress);
  o.set("playMode", c.playMode); o.set("speed", c.speed); o.set("dir", c.dir);
  o.set("posX", c.posX); o.set("posY", c.posY); o.set("scale", c.scale); o.set("rotation", c.rotation); o.set("opacity", c.opacity);
  o.set("flipH", c.flipH); o.set("flipV", c.flipV);
  o.set("inPt", c.inPt); o.set("outPt", c.outPt); o.set("blend", c.blend); o.set("chan", c.chan);
  o.set("anchorX", c.anchorX); o.set("anchorY", c.anchorY); o.set("tMode", c.tMode); o.set("autoAction", c.autoAction); o.set("autoLoops", c.autoLoops);
  o.set("width", c.width); o.set("height", c.height); o.set("volume", c.volume); o.set("pan", c.pan);
  JsonValue fx = JsonValue::array(); for (auto& f : c.fx) fx.push(FxJ(f));
  o.set("fx", fx);
  return o;
}
Clip ReadClip(const JsonValue& o) {
  Clip c;
  c.st = (Clip::St)std::clamp(o["st"].asInt(0), 0, (int)Clip::Armed);
  if (c.st == Clip::Selected) c.st = Clip::Loaded; else if (c.st == Clip::LiveSel) c.st = Clip::Live;
  c.name = o["name"].asString(); c.dur = o["dur"].asString(); c.color = std::clamp(o["color"].asInt(0), 0, 5); c.style = o["style"].asInt(-1); c.media = o["media"].asString();
  c.progress = F(o, "progress", 0); c.playMode = std::clamp(o["playMode"].asInt(PM_LOOP), 0, (int)PM_ONCE);
  c.speed = F(o, "speed", 100); c.dir = o["dir"].asInt(1) < 0 ? -1 : 1;
  c.posX = F(o, "posX", 0); c.posY = F(o, "posY", 0); c.scale = F(o, "scale", 1); c.rotation = F(o, "rotation", 0); c.opacity = F(o, "opacity", 100);
  c.flipH = o["flipH"].asBool(); c.flipV = o["flipV"].asBool();
  c.inPt = std::clamp(F(o, "inPt", 0), 0.f, 99.f); c.outPt = std::clamp(F(o, "outPt", 100), c.inPt + 1.f, 100.f);
  c.blend = std::clamp(o["blend"].asInt(0), 0, BLEND_COUNT); c.chan = std::clamp(o["chan"].asInt(7), 0, 7);
  c.anchorX = std::clamp(F(o, "anchorX", 0), -16384.f, 16384.f); c.anchorY = std::clamp(F(o, "anchorY", 0), -16384.f, 16384.f);
  c.tMode = std::clamp(o["tMode"].asInt(0), 0, 1); c.autoAction = std::clamp(o["autoAction"].asInt(0), 0, 4); c.autoLoops = std::clamp(o["autoLoops"].asInt(1), 1, 999);
  c.width = std::clamp(o["width"].asInt(0), 0, 16384); c.height = std::clamp(o["height"].asInt(0), 0, 16384);
  c.volume = std::clamp(F(o, "volume", 0), -60.f, 12.f); c.pan = std::clamp(F(o, "pan", 0), -100.f, 100.f);
  if (o["fx"].isArray()) for (auto& f : o["fx"].arrayItems()) c.fx.push_back(ReadFx(f));
  return c;
}

// Properties > Layer extras (master/pan/size/transition/transform), shared by every place a layer is written or read.
void LayerPropsJ(JsonValue& lo, const Layer& l) {
  lo.set("master", l.master); lo.set("pan", l.pan); lo.set("width", l.width); lo.set("height", l.height);
  lo.set("autoSize", l.autoSize); lo.set("transBlend", l.transBlend); lo.set("color", l.color);
  lo.set("posX", l.posX); lo.set("posY", l.posY); lo.set("scale", l.scale); lo.set("rotation", l.rotation);
  lo.set("anchorX", l.anchorX); lo.set("anchorY", l.anchorY);
}
void ReadLayerProps(const JsonValue& lo, Layer& l) {
  l.master = std::clamp(F(lo, "master", 100), 0.f, 100.f); l.pan = std::clamp(F(lo, "pan", 0), -100.f, 100.f);
  l.width = std::clamp(lo["width"].asInt(0), 0, 16384); l.height = std::clamp(lo["height"].asInt(0), 0, 16384);
  l.autoSize = std::clamp(lo["autoSize"].asInt(0), 0, 3); l.transBlend = std::clamp(lo["transBlend"].asInt(0), 0, 3); l.color = std::clamp(lo["color"].asInt(0), 0, 5);
  l.posX = std::clamp(F(lo, "posX", 0), -16384.f, 16384.f); l.posY = std::clamp(F(lo, "posY", 0), -16384.f, 16384.f);
  l.scale = std::clamp(F(lo, "scale", 100), 1.f, 1000.f); l.rotation = std::clamp(F(lo, "rotation", 0), -360.f, 360.f);
  l.anchorX = std::clamp(F(lo, "anchorX", 0), -16384.f, 16384.f); l.anchorY = std::clamp(F(lo, "anchorY", 0), -16384.f, 16384.f);
}

JsonValue FloatsJ(const std::vector<float>& v) { JsonValue a = JsonValue::array(); for (float x : v) a.push(x); return a; }
std::vector<float> ReadFloats(const JsonValue& a) { std::vector<float> v; if (a.isArray()) for (auto& x : a.arrayItems()) v.push_back((float)x.asNumber()); return v; }

// Shared by the project serializer and the G8 calibration-profile file.
JsonValue CalibJ(const Calib& c) { JsonValue co = JsonValue::object(); co.set("tx", c.tx); co.set("ty", c.ty); co.set("mx", c.mx); co.set("my", c.my); return co; }
Calib ReadCalibPoint(const JsonValue& co) { return {F(co, "tx", 0), F(co, "ty", 0), F(co, "mx", 0), F(co, "my", 0)}; }

JsonValue SliceJ(const Slice& s) {
  JsonValue o = JsonValue::object();
  o.set("id", s.id); o.set("name", s.name); o.set("visible", s.visible); o.set("solo", s.solo); o.set("warp", s.warp);
  o.set("meshCols", s.meshCols); o.set("meshRows", s.meshRows);
  o.set("meshU", FloatsJ(s.meshU)); o.set("meshV", FloatsJ(s.meshV));
  JsonValue mp = JsonValue::array();
  for (auto& row : s.meshLocal) { JsonValue r = JsonValue::array(); for (auto& p : row) r.push(V2(p)); mp.push(r); }
  o.set("meshLocal", mp);
  o.set("srcKind", s.srcKind); o.set("srcRef", s.srcRef);
  o.set("ix", s.ix); o.set("iy", s.iy); o.set("iw", s.iw); o.set("ih", s.ih);
  JsonValue q = JsonValue::array(); for (int i = 0; i < 4; ++i) q.push(V2(s.q[i]));
  o.set("q", q);
  JsonValue ms = JsonValue::array();
  for (auto& m : s.masks) {
    JsonValue mo = JsonValue::object();
    mo.set("id", m.id); mo.set("name", m.name); mo.set("inverted", m.inverted); mo.set("feather", m.feather);
    JsonValue pts = JsonValue::array(); for (int i = 0; i < 4; ++i) pts.push(V2(m.pts[i]));
    mo.set("pts", pts);
    ms.push(mo);
  }
  o.set("masks", ms);
  return o;
}
Slice ReadSlice(const JsonValue& o) {
  Slice s;
  s.id = o["id"].asString(); s.name = o["name"].asString(); s.visible = o["visible"].asBool(true); s.solo = o["solo"].asBool(false);
  s.warp = std::clamp(o["warp"].asInt(0), 0, 1);
  s.meshCols = std::clamp(o["meshCols"].asInt(4), 2, 64); s.meshRows = std::clamp(o["meshRows"].asInt(3), 2, 64);
  s.meshU = ReadFloats(o["meshU"]); s.meshV = ReadFloats(o["meshV"]);
  auto readGrid = [](const JsonValue& a) {
    std::vector<std::vector<ImVec2>> g;
    if (a.isArray()) for (auto& r : a.arrayItems()) { std::vector<ImVec2> row; if (r.isArray()) for (auto& p : r.arrayItems()) row.push_back(ReadV2(p)); g.push_back(row); }
    return g;
  };
  s.meshLocal = readGrid(o["meshLocal"]);
  // a dangling ref is kept, not cleared: the slice shows the composition and the UI warns, and reappearing
  // layer/group (e.g. an undo) restores the routing
  s.srcKind = std::clamp(o["srcKind"].asInt(0), 0, 2); s.srcRef = o["srcRef"].asString();
  s.ix = o["ix"].asInt(0); s.iy = o["iy"].asInt(0); s.iw = std::max(20, o["iw"].asInt(1920)); s.ih = std::max(20, o["ih"].asInt(1080));
  ImVec2 def[4] = {{(float)s.ix, (float)s.iy}, {(float)(s.ix + s.iw), (float)s.iy}, {(float)(s.ix + s.iw), (float)(s.iy + s.ih)}, {(float)s.ix, (float)(s.iy + s.ih)}};
  for (int i = 0; i < 4; ++i) s.q[i] = o["q"].isArray() && o["q"].size() > (size_t)i ? ReadV2(o["q"].at(i), def[i]) : def[i];
  if (o["masks"].isArray()) for (auto& mo : o["masks"].arrayItems()) {
    Mask m; m.id = mo["id"].asString(); m.name = mo["name"].asString(); m.inverted = mo["inverted"].asBool(true); m.feather = mo["feather"].asInt(4);
    for (int i = 0; i < 4; ++i) m.pts[i] = mo["pts"].isArray() && mo["pts"].size() > (size_t)i ? ReadV2(mo["pts"].at(i)) : ImVec2(0, 0);
    s.masks.push_back(m);
  }
  // files/presets from before meshLocal: absolute output-pixel mesh, converted once here (needs q, so after reading it)
  if (!o["meshLocal"].isArray() && o["meshPts"].isArray()) MigrateAbsoluteMesh(s, readGrid(o["meshPts"]));
  return s;
}

// Shared by the project serializer and the F8 output-preset file — one Screen, never duplicated field-by-field.
JsonValue ScreenJ(const Screen& s) {
  JsonValue so = JsonValue::object();
  so.set("id", s.id); so.set("name", s.name); so.set("outDev", s.outDev); so.set("w", s.w); so.set("h", s.h); so.set("fps", s.fps);
  so.set("edgeBlend", s.edgeBlend); so.set("visible", s.visible); so.set("role", s.role);
  JsonValue sl = JsonValue::array(); for (auto& x : s.slices) sl.push(SliceJ(x));
  so.set("slices", sl);
  return so;
}
Screen ReadScreen(const JsonValue& so) {
  Screen s; s.id = so["id"].asString(); s.name = so["name"].asString(); s.outDev = so["outDev"].asString();
  s.w = std::max(16, so["w"].asInt(1920)); s.h = std::max(16, so["h"].asInt(1080)); s.fps = so["fps"].asInt(60);
  s.edgeBlend = so["edgeBlend"].asBool(); s.visible = so["visible"].asBool(true); s.role = std::clamp(so["role"].asInt(0), 0, 2);
  if (so["slices"].isArray()) for (auto& x : so["slices"].arrayItems()) s.slices.push_back(ReadSlice(x));
  return s;
}

// Shared by the flat composition.{groups,layers,colNames} (kept for older-file/tool compatibility, always a
// mirror of the CURRENT deck) and by each entry of the new "decks" array below.
static void WriteDeckContent(JsonValue& obj, const std::vector<Group>& groups, const std::vector<Layer>& layers, const std::vector<std::string>& colNames) {
  int cc = layers.empty() ? 8 : (int)layers[0].clips.size();
  JsonValue cn = JsonValue::array();
  for (int i = 0; i < cc; ++i) cn.push(i < (int)colNames.size() ? colNames[i] : std::string());
  obj.set("colNames", cn);
  JsonValue gs = JsonValue::array();
  for (auto& g : groups) {
    JsonValue go = JsonValue::object(); go.set("id", g.id); go.set("name", g.name); go.set("role", g.role); go.set("open", g.open); go.set("activeCol", g.activeCol); go.set("opacity", g.opacity);
    gs.push(go);
  }
  obj.set("groups", gs);
  JsonValue ls = JsonValue::array();
  for (auto& l : layers) {
    JsonValue lo = JsonValue::object();
    lo.set("id", l.id); lo.set("name", l.name); lo.set("group", l.group); lo.set("blend", l.blend); lo.set("blendTime", l.blendTime);
    lo.set("opacity", l.opacity); lo.set("audio", l.audio); LayerPropsJ(lo, l);
    lo.set("solo", l.solo); lo.set("muted", l.muted); lo.set("bypassed", l.bypassed); lo.set("collapsed", l.collapsed);
    JsonValue cs = JsonValue::array(); for (auto& c : l.clips) cs.push(ClipJ(c));
    lo.set("clips", cs);
    ls.push(lo);
  }
  obj.set("layers", ls);
}

JsonValue Serialize(const App& a) {
  JsonValue root = JsonValue::object();
  root.set("format", kFormat);
  root.set("app", "MikMap");
  JsonValue comp = JsonValue::object();
  comp.set("canvasW", a.canvasW); comp.set("canvasH", a.canvasH); comp.set("bpm", a.bpm); comp.set("quantize", a.quantize); comp.set("autoStartCol", a.autoStartCol);
  {   // Properties > Comp
    JsonValue cp = JsonValue::object(); const CompProps& k = a.comp;
    cp.set("master", k.master); cp.set("speed", k.speed); cp.set("volume", k.volume); cp.set("pan", k.pan); cp.set("opacity", k.opacity);
    cp.set("xfBlend", k.xfBlend); cp.set("xfBehaviour", k.xfBehaviour); cp.set("xfCurve", k.xfCurve);
    cp.set("posX", k.posX); cp.set("posY", k.posY); cp.set("scale", k.scale); cp.set("rotation", k.rotation);
    cp.set("anchorX", k.anchorX); cp.set("anchorY", k.anchorY);
    comp.set("props", cp);
  }
  WriteDeckContent(comp, a.groups, a.layers, a.colNames);   // current deck, flat — kept for backward/tool compatibility
  root.set("composition", comp);

  // Multi-deck (X: deck tabs) — every deck, with the CURRENT one's live state (decks[curDeckIdx] on disk can be
  // stale between switches). curDeckIdx/deckMode are saved so reopening a project resumes on the same deck/view.
  JsonValue decks = JsonValue::array();
  for (int i = 0; i < (int)a.decks.size(); ++i) {
    const Deck& d = (i == a.curDeckIdx) ? Deck{a.decks[i].name, a.groups, a.layers, a.colNames, a.activeCol} : a.decks[i];
    JsonValue dj = JsonValue::object();
    dj.set("name", d.name); dj.set("activeCol", d.activeCol);
    WriteDeckContent(dj, d.groups, d.layers, d.colNames);
    decks.push(dj);
  }
  root.set("decks", decks);
  root.set("curDeckIdx", a.curDeckIdx);
  root.set("deckMode", a.deckMode);

  JsonValue scs = JsonValue::array();
  for (auto& s : a.screens) scs.push(ScreenJ(s));
  root.set("screens", scs);

  JsonValue sensor = JsonValue::object();
  JsonValue cal = JsonValue::array();
  for (auto& c : a.calib) cal.push(CalibJ(c));
  sensor.set("calib", cal);
  JsonValue roi = JsonValue::array(); for (int i = 0; i < 4; ++i) roi.push(V2(a.roi[i]));
  sensor.set("roi", roi);
  sensor.set("noise", a.noise); sensor.set("blobSize", a.blobSize);
  JsonValue rs = JsonValue::array();
  for (auto& r : a.routes) { JsonValue ro = JsonValue::object(); ro.set("id", r.id); ro.set("source", r.source); ro.set("target", r.target); ro.set("active", r.active); rs.push(ro); }
  sensor.set("routes", rs);
  JsonValue ds = JsonValue::array();
  for (auto& d : a.devices) {
    JsonValue dob = JsonValue::object(); dob.set("id", d.id); dob.set("name", d.name); dob.set("endpoint", d.endpoint); dob.set("connected", d.connected);
    dob.set("fps", d.fps); dob.set("latency", d.latency); dob.set("packets", d.packets); ds.push(dob);
  }
  sensor.set("devices", ds);
  root.set("sensor", sensor);
  return root;
}

// One entry of the "decks" array — same field set/validation as the flat composition.{groups,layers,colNames}
// parse below, just scoped to its own deck instead of `out` directly (a dangling group ref only looks within
// the SAME deck's own groups, not the whole file).
static bool ReadDeckContent(const JsonValue& dj, Deck& d) {
  if (!dj["layers"].isArray() || dj["layers"].size() == 0) return false;
  d.name = dj["name"].asString("Deck");
  d.groups.clear();
  if (dj["groups"].isArray()) for (auto& go : dj["groups"].arrayItems()) {
    Group g; g.id = go["id"].asString(); g.name = go["name"].asString(); g.role = std::clamp(go["role"].asInt(2), 0, 2);
    g.open = go["open"].asBool(true); g.activeCol = go["activeCol"].asInt(0); g.opacity = std::clamp(F(go, "opacity", 100), 0.f, 100.f);
    d.groups.push_back(g);
  }
  d.layers.clear();
  size_t cols = 1;
  for (auto& lo : dj["layers"].arrayItems()) {
    Layer l; l.id = lo["id"].asString(); l.name = lo["name"].asString("Layer"); l.group = lo["group"].asString(); l.blend = lo["blend"].asString("Normal");
    if (BlendIndex(l.blend) == 0) l.blend = "Normal";
    l.blendTime = std::max(0.f, F(lo, "blendTime", 0)); l.opacity = std::clamp(F(lo, "opacity", 100), 0.f, 100.f); l.audio = std::clamp(F(lo, "audio", 0), 0.f, 100.f); ReadLayerProps(lo, l);
    l.solo = lo["solo"].asBool(); l.muted = lo["muted"].asBool(); l.bypassed = lo["bypassed"].asBool(); l.collapsed = lo["collapsed"].asBool();
    if (lo["clips"].isArray()) for (auto& co : lo["clips"].arrayItems()) l.clips.push_back(ReadClip(co));
    cols = std::max(cols, l.clips.size());
    d.layers.push_back(std::move(l));
  }
  EnsureLayerIds(d.layers);   // files from before layer ids, or hand-edited duplicates
  for (auto& l : d.layers) {
    l.clips.resize(cols);
    l.live = false; for (auto& c : l.clips) if (c.isLive()) l.live = true;
    bool found = false; for (auto& g : d.groups) if (g.id == l.group) found = true;
    if (!l.group.empty() && !found) l.group.clear();
  }
  d.colNames.clear();
  if (dj["colNames"].isArray()) for (auto& n : dj["colNames"].arrayItems()) d.colNames.push_back(n.asString());
  d.colNames.resize(cols);
  d.activeCol = std::clamp(dj["activeCol"].asInt(0), 0, (int)cols - 1);
  return true;
}

// Reads into a scratch App so a half-broken file can never leave the live app in a half-loaded state.
bool Deserialize(const JsonValue& root, App& out, std::string& err) {
  if (!root.isObject()) { err = "not a JSON object"; return false; }
  if (root["format"].asInt(0) > kFormat) { err = "file was saved by a newer MikMap"; return false; }
  const JsonValue& comp = root["composition"];
  if (!comp["layers"].isArray() || comp["layers"].size() == 0) { err = "no layers in file"; return false; }
  out.canvasW = std::clamp(comp["canvasW"].asInt(1920), 64, 16384); out.canvasH = std::clamp(comp["canvasH"].asInt(1080), 64, 16384);
  {
    const JsonValue& cp = comp["props"]; CompProps k;
    k.master = std::clamp(F(cp, "master", 100), 0.f, 100.f); k.speed = std::clamp(F(cp, "speed", 100), 0.f, 400.f);
    k.volume = std::clamp(F(cp, "volume", 0), -60.f, 12.f); k.pan = std::clamp(F(cp, "pan", 0), -100.f, 100.f);
    k.opacity = std::clamp(F(cp, "opacity", 100), 0.f, 100.f);
    k.xfBlend = std::clamp(cp["xfBlend"].asInt(0), 0, 3); k.xfBehaviour = std::clamp(cp["xfBehaviour"].asInt(0), 0, 2); k.xfCurve = std::clamp(cp["xfCurve"].asInt(0), 0, 2);
    k.posX = std::clamp(F(cp, "posX", 0), -16384.f, 16384.f); k.posY = std::clamp(F(cp, "posY", 0), -16384.f, 16384.f);
    k.scale = std::clamp(F(cp, "scale", 100), 1.f, 1000.f); k.rotation = std::clamp(F(cp, "rotation", 0), -360.f, 360.f);
    k.anchorX = std::clamp(F(cp, "anchorX", 0), -16384.f, 16384.f); k.anchorY = std::clamp(F(cp, "anchorY", 0), -16384.f, 16384.f);
    out.comp = k;
  }
  out.bpm = std::clamp(F(comp, "bpm", 128.f), 40.f, 240.f); out.quantize = comp["quantize"].asBool(false);
  out.autoStartCol = comp["autoStartCol"].asInt(-1);   // clamped against real column count once `cols` is known below
  out.groups.clear();
  if (comp["groups"].isArray()) for (auto& go : comp["groups"].arrayItems()) {
    Group g; g.id = go["id"].asString(); g.name = go["name"].asString(); g.role = std::clamp(go["role"].asInt(2), 0, 2);
    g.open = go["open"].asBool(true); g.activeCol = go["activeCol"].asInt(0); g.opacity = std::clamp(F(go, "opacity", 100), 0.f, 100.f); out.groups.push_back(g);
  }
  out.layers.clear();
  size_t cols = 1;
  for (auto& lo : comp["layers"].arrayItems()) {
    Layer l; l.id = lo["id"].asString(); l.name = lo["name"].asString("Layer"); l.group = lo["group"].asString(); l.blend = lo["blend"].asString("Normal");
    if (BlendIndex(l.blend) == 0) l.blend = "Normal";
    l.blendTime = std::max(0.f, F(lo, "blendTime", 0)); l.opacity = std::clamp(F(lo, "opacity", 100), 0.f, 100.f); l.audio = std::clamp(F(lo, "audio", 0), 0.f, 100.f); ReadLayerProps(lo, l);
    l.solo = lo["solo"].asBool(); l.muted = lo["muted"].asBool(); l.bypassed = lo["bypassed"].asBool(); l.collapsed = lo["collapsed"].asBool();
    if (lo["clips"].isArray()) for (auto& co : lo["clips"].arrayItems()) l.clips.push_back(ReadClip(co));
    cols = std::max(cols, l.clips.size());
    out.layers.push_back(std::move(l));
  }
  EnsureLayerIds(out.layers);
  for (auto& l : out.layers) {                      // every layer must have the same number of columns
    l.clips.resize(cols);
    l.live = false; for (auto& c : l.clips) if (c.isLive()) l.live = true;
    if (!l.group.empty() && !out.group(l.group)) l.group.clear();   // dangling group reference: fall back to ungrouped
  }
  out.colNames.clear();
  if (comp["colNames"].isArray()) for (auto& n : comp["colNames"].arrayItems()) out.colNames.push_back(n.asString());
  out.colNames.resize(cols);
  if (out.autoStartCol < 0 || out.autoStartCol >= (int)cols) out.autoStartCol = -1;

  // Multi-deck: "decks" is authoritative when present (a file saved by this build always has it); a file from
  // before this feature has none, so synthesize a single deck from the flat composition just parsed above.
  out.decks.clear();
  if (root["decks"].isArray()) for (auto& dj : root["decks"].arrayItems()) { Deck d; if (ReadDeckContent(dj, d)) out.decks.push_back(std::move(d)); }
  if (out.decks.empty()) out.decks.push_back({"Deck A", out.groups, out.layers, out.colNames, out.activeCol});
  out.curDeckIdx = std::clamp(root["curDeckIdx"].asInt(0), 0, (int)out.decks.size() - 1);
  { Deck& d = out.decks[out.curDeckIdx]; out.groups = d.groups; out.layers = d.layers; out.colNames = d.colNames; out.activeCol = d.activeCol; }
  out.deckMode = std::clamp(root["deckMode"].asInt(0), 0, 1);

  out.screens.clear();
  if (root["screens"].isArray()) for (auto& so : root["screens"].arrayItems()) out.screens.push_back(ReadScreen(so));
  if (out.screens.empty()) { err = "no screens in file"; return false; }

  const JsonValue& sn = root["sensor"];
  if (sn["calib"].isArray() && sn["calib"].size() == 4) {
    out.calib.clear();
    for (auto& co : sn["calib"].arrayItems()) out.calib.push_back(ReadCalibPoint(co));
  }
  if (sn["roi"].isArray() && sn["roi"].size() == 4) for (int i = 0; i < 4; ++i) out.roi[i] = ReadV2(sn["roi"].at(i));
  out.noise = F(sn, "noise", out.noise); out.blobSize = F(sn, "blobSize", out.blobSize);
  if (sn["routes"].isArray()) { out.routes.clear(); for (auto& r : sn["routes"].arrayItems()) out.routes.push_back({r["id"].asString(), r["source"].asString(), r["target"].asString(), r["active"].asBool()}); }
  if (sn["devices"].isArray()) {
    out.devices.clear();
    for (auto& d : sn["devices"].arrayItems()) out.devices.push_back({d["id"].asString(), d["name"].asString(), d["endpoint"].asString(), d["connected"].asBool(), d["fps"].asInt(), d["latency"].asInt(), d["packets"].asString("0")});
  }
  return true;
}

std::string ReadFile(const fs::path& p, bool& ok) {
  std::ifstream f(p, std::ios::binary); ok = (bool)f;
  std::stringstream ss; ss << f.rdbuf(); return ss.str();
}
// Write to a temp file then rename, so a crash mid-write cannot destroy the previous good file.
bool WriteAtomic(const fs::path& p, const std::string& data, std::string& err) {
  std::error_code ec; fs::create_directories(p.parent_path(), ec);
  fs::path tmp = p; tmp += ".tmp";
  { std::ofstream f(tmp, std::ios::binary | std::ios::trunc); if (!f) { err = "cannot write " + tmp.string(); return false; } f << data; if (!f) { err = "write failed"; return false; } }
  fs::rename(tmp, p, ec);
  if (ec) { err = ec.message(); fs::remove(tmp, ec); return false; }
  return true;
}

std::string Stamp() { char b[32]; std::time_t t = std::time(nullptr); std::tm tmv{};
#ifdef _WIN32
  localtime_s(&tmv, &t);
#else
  localtime_r(&t, &tmv);
#endif
  std::strftime(b, sizeof b, "%Y%m%d-%H%M%S", &tmv); return b; }

}  // namespace

// ───────────── locations ─────────────
std::string ProjectsDir() { return (HomeDir() / "Documents" / "MikMap").string(); }
static fs::path ConfigDir() {
#ifdef _WIN32
  if (const char* a = std::getenv("APPDATA")) return fs::path(a) / "MikMap";
  return HomeDir() / "AppData" / "Roaming" / "MikMap";
#elif defined(__APPLE__)
  return HomeDir() / "Library" / "Application Support" / "MikMap";
#else
  if (const char* x = std::getenv("XDG_CONFIG_HOME")) return fs::path(x) / "MikMap";
  return HomeDir() / ".config" / "MikMap";
#endif
}

std::string MediaDir() { return (fs::path(ProjectsDir()) / "media").string(); }
MediaKind MediaKindOf(const std::string& path) {
  std::string ext = fs::path(path).extension().string();
  for (auto& ch : ext) ch = (char)std::tolower((unsigned char)ch);
  for (const char* e : {".png", ".jpg", ".jpeg", ".bmp", ".tga"}) if (ext == e) return MEDIA_IMAGE;
  for (const char* e : {".mov", ".mp4", ".m4v", ".avi", ".mkv", ".webm", ".wmv"}) if (ext == e) return MEDIA_VIDEO;
  for (const char* e : {".wav", ".mp3", ".ogg", ".flac", ".aac", ".m4a", ".aif", ".aiff"}) if (ext == e) return MEDIA_AUDIO;
  return MEDIA_NONE;
}
std::vector<std::string> ListMedia() {
  std::vector<std::string> out; std::error_code ec;
  for (auto& e : fs::directory_iterator(MediaDir(), ec)) {
    if (!e.is_regular_file(ec)) continue;
    if (MediaKindOf(e.path().string()) != MEDIA_NONE) out.push_back(e.path().string());
  }
  std::sort(out.begin(), out.end());
  return out;
}
void RebuildMediaList() {
  A.mediaList = ListMedia();
  std::error_code ec;
  for (auto& p : A.mediaExtra) {
    if (!fs::is_regular_file(p, ec)) continue;   // a dropped file that was moved/deleted quietly disappears from the Browser
    if (std::find(A.mediaList.begin(), A.mediaList.end(), p) == A.mediaList.end()) A.mediaList.push_back(p);
  }
  A.mediaStale = false;
}
static bool gPersistSettings = true;   // not an App member: NewProject()/LoadProject() rebuild App and would reset it
void SetSettingsPersistence(bool on) { gPersistSettings = on; }
int ImportMedia(const std::vector<std::string>& paths) {
  int added = 0; std::error_code ec;
  for (auto& p : paths) {
    if (MediaKindOf(p) == MEDIA_NONE || !fs::is_regular_file(p, ec)) continue;
    if (std::find(A.mediaExtra.begin(), A.mediaExtra.end(), p) != A.mediaExtra.end()) continue;
    if (fs::path(p).parent_path() == fs::path(MediaDir())) continue;   // already listed by the folder scan
    A.mediaExtra.push_back(p); ++added;
  }
  A.mediaStale = true;
  if (added && gPersistSettings) SaveSettings();
  return added;
}

// Shared by ListProjects/ListPresets/ListCalibProfiles — same "newest first" file listing, only the folder and
// extension change. Extension must include the leading dot (e.g. ".mikmap").
std::vector<ProjectFile> ListDirByExt(const std::string& dir, const std::string& ext) {
  std::vector<ProjectFile> out;
  std::error_code ec;
  for (auto& e : fs::directory_iterator(dir, ec)) {
    if (!e.is_regular_file(ec) || e.path().extension() != ext) continue;
    ProjectFile pf; pf.path = e.path().string(); pf.name = e.path().stem().string();
    auto t = fs::last_write_time(e.path(), ec);
    // file_clock's epoch differs between standard libraries; rebase onto system_clock so the date is right everywhere
    auto sys = std::chrono::time_point_cast<std::chrono::system_clock::duration>(t - fs::file_time_type::clock::now() + std::chrono::system_clock::now());
    pf.mtime = (long long)std::chrono::system_clock::to_time_t(sys);
    out.push_back(pf);
  }
  std::sort(out.begin(), out.end(), [](const ProjectFile& a, const ProjectFile& b) { return a.mtime > b.mtime; });
  return out;
}

std::vector<ProjectFile> ListProjects() { return ListDirByExt(ProjectsDir(), ".mikmap"); }

// F8/G8: presets and calibration profiles are portable show config, not project content and not machine settings
// — so they get their own subfolders under ProjectsDir() (not ConfigDir(), which is machine-specific) and their
// own extensions, distinct from ".mikmap" so ListProjects()/the Open-project dialog never picks them up.
std::string PresetsDir() { return (fs::path(ProjectsDir()) / "Presets").string(); }
std::vector<ProjectFile> ListPresets() { return ListDirByExt(PresetsDir(), ".mikmap-preset"); }
std::string CalibDir() { return (fs::path(ProjectsDir()) / "Calibration").string(); }
std::vector<ProjectFile> ListCalibProfiles() { return ListDirByExt(CalibDir(), ".mikmap-calib"); }

// ───────────── project ─────────────
static std::string Snapshot() { gForDirty = true; std::string s = Serialize(A).dump(0); gForDirty = false; return s; }
static std::vector<std::string> gUndo, gRedo;
static std::string gLast;               // snapshot the undo history is currently based on
static double gLastCheck = 0;
static bool gPending = false;         // set by user input; a snapshot (~5 ms) is only taken after input, never while the UI is idle
void UndoNote() { gPending = true; }
static void UndoReset() { gUndo.clear(); gRedo.clear(); gLast = Snapshot(); }
void MarkSaved() { A.savedSnapshot = Snapshot(); UndoReset(); }
void App::notify(const std::string& s, double secs) { toast = s; toastUntil = ui::g.time + secs; }

bool SaveProject(const std::string& path, std::string& err) {
  if (!WriteAtomic(path, Serialize(A).dump(2), err)) return false;
  A.projectPath = path; A.projectName = fs::path(path).stem().string();
  A.savedSnapshot = Snapshot();
  return true;
}

bool LoadProject(const std::string& path, std::string& err) {
  bool ok; std::string text = ReadFile(path, ok);
  if (!ok) { err = "cannot open file"; return false; }
  JsonValue root; std::string perr;
  if (!JsonValue::parse(text, root, perr)) { err = "invalid JSON: " + perr; return false; }
  App tmp = A;                       // start from current app so absent optional sections keep sane values
  if (!Deserialize(root, tmp, err)) return false;
  Prefs keepPrefs = A.prefs; int keepMon = A.outMonitor; int keepScreen = A.screen; auto keepMedia = A.mediaExtra; bool keepBlack = A.blackout;
  A = tmp;
  A.prefs = keepPrefs; A.outMonitor = keepMon; A.screen = keepScreen; A.mediaExtra = keepMedia; A.mediaStale = true; A.blackout = keepBlack;
  // fresh selection/UI state: nothing carried over from the previous show
  A.selectedCells.clear(); A.selMode = 2; A.activeCol = 0; A.selLi = A.selLayer = A.selCi = 0; A.fxSel = 0;
  A.pop.open = A.layerMenu.open = A.colMenu.open = A.deckMenu.open = A.ctx.open = A.blendDD.open = A.rename.open = false;
  A.tlProgress = 0; A.tlLoopOn = false; A.tlIn = 0; A.tlOut = 100;   // timeline playhead/loop are runtime-only, never saved
  A.selSc = A.screens[0].id; A.selSl = A.screens[0].slices.empty() ? "" : A.screens[0].slices[0].id; A.selMk.clear(); A.selKind = -1;
  A.railScreen.clear(); A.mapScrollX = A.mapScrollY = 0;
  A.wizardStep = 0; A.editRoi = false; A.touchPts.clear(); A.pending.clear();
  A.idCounter = 1000;
  for (auto& l : A.layers) for (auto& c : l.clips) if (!c.media.empty()) PreloadMedia(c.media);
  A.cue(0, 0);
  if (A.autoStartCol >= 0 && A.autoStartCol < A.colCount()) {
    bool wasFlushing = A.flushing; A.flushing = true;   // fire immediately on open, never queued behind Sync/quantize
    A.fireColumn(A.autoStartCol);
    A.flushing = wasFlushing;
  }
  A.projectPath = path; A.projectName = fs::path(path).stem().string();
  MarkSaved();
  return true;
}

// F8: one Screen (device/resolution/slices/masks) as its own file, independent of any project — load it into a
// screen in a different show later. Own "format" version: this is a different file format from .mikmap, not a
// slice of it, so it must not be assumed to move in lockstep with kFormat.
bool SaveOutputPreset(const std::string& path, const Screen& s, std::string& err) {
  JsonValue root = JsonValue::object();
  root.set("format", 1); root.set("app", "MikMap-preset");
  root.set("screen", ScreenJ(s));
  return WriteAtomic(path, root.dump(2), err);
}
bool LoadOutputPreset(const std::string& path, Screen& out, std::string& err) {
  bool ok; std::string text = ReadFile(path, ok);
  if (!ok) { err = "cannot open file"; return false; }
  JsonValue root; std::string perr;
  if (!JsonValue::parse(text, root, perr)) { err = "invalid JSON: " + perr; return false; }
  if (!root["screen"].isObject()) { err = "not a MikMap output preset"; return false; }
  out = ReadScreen(root["screen"]);
  return true;
}

// G8: the 4 calibration correspondence points + ROI/noise, as their own file. src/calib.cpp refits H_s from these
// points at point of use (never caches a matrix), so a "calibration profile" is the points themselves, matching
// how src/ already treats calibration everywhere else — see .claude/CLAUDE.md's note on this being intentional.
bool SaveCalibProfile(const std::string& path, const std::vector<Calib>& calib, const ImVec2 roi[4], float noise, float blobSize, std::string& err) {
  JsonValue root = JsonValue::object();
  root.set("format", 1); root.set("app", "MikMap-calib");
  JsonValue cal = JsonValue::array(); for (auto& c : calib) cal.push(CalibJ(c));
  root.set("calib", cal);
  JsonValue roiJ = JsonValue::array(); for (int i = 0; i < 4; ++i) roiJ.push(V2(roi[i]));
  root.set("roi", roiJ);
  root.set("noise", noise); root.set("blobSize", blobSize);
  return WriteAtomic(path, root.dump(2), err);
}
bool LoadCalibProfile(const std::string& path, std::vector<Calib>& calib, ImVec2 roi[4], float& noise, float& blobSize, std::string& err) {
  bool ok; std::string text = ReadFile(path, ok);
  if (!ok) { err = "cannot open file"; return false; }
  JsonValue root; std::string perr;
  if (!JsonValue::parse(text, root, perr)) { err = "invalid JSON: " + perr; return false; }
  if (!root["calib"].isArray() || root["calib"].size() != 4) { err = "not a MikMap calibration profile"; return false; }
  calib.clear(); for (auto& co : root["calib"].arrayItems()) calib.push_back(ReadCalibPoint(co));
  if (root["roi"].isArray() && root["roi"].size() == 4) for (int i = 0; i < 4; ++i) roi[i] = ReadV2(root["roi"].at(i));
  noise = F(root, "noise", noise); blobSize = F(root, "blobSize", blobSize);
  return true;
}

void NewProject() {
  Prefs keepPrefs = A.prefs; int keepMon = A.outMonitor; int keepScreen = A.screen; auto keepMedia = A.mediaExtra;
  A = App(); A.init();
  A.prefs = keepPrefs; A.outMonitor = keepMon; A.screen = keepScreen; A.mediaExtra = keepMedia; A.mediaStale = true;
  A.projectPath.clear(); A.projectName = "MikMap Stage 01";
  MarkSaved();
}

void NewBlankProject() {
  Prefs keepPrefs = A.prefs; int keepMon = A.outMonitor; int keepScreen = A.screen; auto keepMedia = A.mediaExtra;
  A = App(); A.init();
  A.prefs = keepPrefs; A.outMonitor = keepMon; A.screen = keepScreen; A.mediaExtra = keepMedia; A.mediaStale = true;
  A.groups.clear(); A.layers.clear(); A.colNames.clear();
  for (int i = 0; i < 4; ++i) {
    Layer l; l.name = "Layer " + std::to_string(i + 1); l.blend = "Normal"; l.opacity = 100; l.clips.assign(8, Clip());
    A.layers.push_back(l);
  }
  EnsureLayerIds(A.layers);
  Screen s; s.id = "screen1"; s.name = "Screen 1"; s.outDev = "Display 1"; s.w = 1920; s.h = 1080; s.role = 0;
  Slice sl; sl.id = "slice1"; sl.name = "Slice 1"; sl.ix = 0; sl.iy = 0; sl.iw = 1920; sl.ih = 1080;
  sl.q[0] = ImVec2(0, 0); sl.q[1] = ImVec2(1920, 0); sl.q[2] = ImVec2(1920, 1080); sl.q[3] = ImVec2(0, 1080);
  s.slices.push_back(sl); A.screens.assign(1, s);
  A.selSc = "screen1"; A.selSl = "slice1"; A.selMk.clear(); A.selKind = -1;
  A.selectedCells.clear(); A.selLi = A.selLayer = A.selCi = 0; A.activeCol = 0; A.selMode = 2;
  A.projectPath.clear(); A.projectName = "MikMap Stage 01";
  MarkSaved();
}

bool CanUndo() { return !gUndo.empty(); }
bool CanRedo() { return !gRedo.empty(); }
bool UndoCommit() {
  std::string cur = Snapshot();
  if (cur == gLast) return false;
  gUndo.push_back(gLast);
  if (gUndo.size() > 60) gUndo.erase(gUndo.begin());
  gRedo.clear(); gLast = cur;
  return true;
}
// Called every frame. While a button is held or text is being typed, edits are still "in progress" (a slider drag or a
// corner drag is ONE undo step), so only snapshot once input is idle, at most 5 times a second.
void UndoTick(bool inputActive, double now) {
  if (!gPending || inputActive || now - gLastCheck < 0.2) return;
  gLastCheck = now; gPending = false;
  UndoCommit();
  A.projectDirty = gLast != A.savedSnapshot;
}

// Applies a snapshot to the *content* of the project only: selection, zoom, menus, prefs and the live/cued state of clips
// that still exist are kept, so undoing an edit during a show does not cut a playing clip.
static bool UndoApply(const std::string& snap) {
  JsonValue root; std::string perr;
  if (!JsonValue::parse(snap, root, perr)) return false;
  App tmp = A; std::string err;
  if (!Deserialize(root, tmp, err)) return false;
  for (size_t li = 0; li < tmp.layers.size() && li < A.layers.size(); ++li)
    for (size_t ci = 0; ci < tmp.layers[li].clips.size() && ci < A.layers[li].clips.size(); ++ci) {
      Clip& n = tmp.layers[li].clips[ci]; const Clip& o = A.layers[li].clips[ci];
      if (n.st != Clip::Empty && n.name == o.name) { n.st = o.st; n.progress = o.progress; }
    }
  for (auto& l : tmp.layers) { l.live = false; for (auto& c : l.clips) if (c.isLive()) l.live = true; }
  A.canvasW = tmp.canvasW; A.canvasH = tmp.canvasH; A.comp = tmp.comp; A.groups = tmp.groups; A.layers = tmp.layers; A.colNames = tmp.colNames;
  A.decks = tmp.decks; A.curDeckIdx = std::clamp(tmp.curDeckIdx, 0, (int)A.decks.size() - 1); A.deckMode = tmp.deckMode;   // undo also covers deck add/delete/switch
  A.screens = tmp.screens; A.calib = tmp.calib; for (int i = 0; i < 4; ++i) A.roi[i] = tmp.roi[i];
  A.noise = tmp.noise; A.blobSize = tmp.blobSize; A.routes = tmp.routes; A.devices = tmp.devices;
  int nl = (int)A.layers.size(), nc = A.colCount();
  A.selLi = std::clamp(A.selLi, 0, nl - 1); A.selLayer = std::clamp(A.selLayer, 0, nl - 1); A.selCi = std::clamp(A.selCi, 0, nc - 1);
  A.activeCol = std::clamp(A.activeCol, 0, nc - 1);
  std::vector<std::pair<int, int>> keep;
  for (auto& c : A.selectedCells) if (c.first >= 0 && c.first < nl && c.second >= 0 && c.second < nc) keep.push_back(c);
  A.selectedCells = keep;
  Screen* sc = nullptr; for (auto& s : A.screens) if (s.id == A.selSc) sc = &s;
  if (!sc) { sc = &A.screens[0]; A.selSc = sc->id; A.selKind = -1; }
  Slice* sl = nullptr; for (auto& s : sc->slices) if (s.id == A.selSl) sl = &s;
  if (!sl) { A.selSl = sc->slices.empty() ? "" : sc->slices[0].id; A.selMk.clear(); A.selKind = -1; }
  else if (!A.selMk.empty()) { bool found = false; for (auto& m : sl->masks) if (m.id == A.selMk) found = true; if (!found) { A.selMk.clear(); A.selKind = -1; } }
  A.pop.open = A.layerMenu.open = A.colMenu.open = A.deckMenu.open = A.ctx.open = A.blendDD.open = A.rename.open = false;
  return true;
}

void UndoStep(bool redo) {
  UndoCommit();                                   // fold any edit still pending into the history first
  auto& from = redo ? gRedo : gUndo; auto& to = redo ? gUndo : gRedo;
  if (from.empty()) { A.notify(redo ? "Nothing to redo" : "Nothing to undo", 1.5); return; }
  std::string target = from.back(); from.pop_back();
  std::string before = gLast;
  if (!UndoApply(target)) { from.push_back(target); A.notify("Undo failed"); return; }
  to.push_back(before);
  gLast = Snapshot();
  A.projectDirty = gLast != A.savedSnapshot;
  A.notify(redo ? "Redo" : "Undo", 1.2);
}

bool ProjectDirty() { return A.savedSnapshot != Snapshot(); }

// Convenience used by the menu/shortcuts. Returns a short message for the toast.
std::string DoSave(bool asCopy) {
  std::string err, path = A.projectPath;
  if (asCopy || path.empty()) {
    std::string base = A.projectName.empty() ? "MikMap Stage 01" : A.projectName;
    path = (fs::path(ProjectsDir()) / (base + (asCopy ? "-" + Stamp() : std::string()) + ".mikmap")).string();
  }
  if (asCopy) {   // a copy must not re-point the open project at the copy
    std::string oldPath = A.projectPath, oldName = A.projectName, oldSnap = A.savedSnapshot;
    bool ok = SaveProject(path, err);
    A.projectPath = oldPath; A.projectName = oldName; A.savedSnapshot = oldSnap;
    return ok ? "Saved copy: " + fs::path(path).filename().string() : "Save failed: " + err;
  }
  return SaveProject(path, err) ? "Saved: " + fs::path(path).filename().string() : "Save failed: " + err;
}

// F8: convenience for the "Save preset"/"Load preset" buttons on a Screen's properties panel — same
// toast-message convention as DoSave above. Always overwrites the file for this screen's current name (one
// preset per name, like re-saving a project), rather than stamping a new file on every save.
std::string DoSaveOutputPreset(const Screen& s) {
  std::string base; for (char ch : (s.name.empty() ? std::string("Preset") : s.name)) base += std::strchr("<>:\"/\\|?*", ch) ? '_' : ch;
  std::string err, path = (fs::path(PresetsDir()) / (base + ".mikmap-preset")).string();
  return SaveOutputPreset(path, s, err) ? "Saved preset: " + fs::path(path).filename().string() : "Save failed: " + err;
}

// G8: convenience for "Save profile" on the Sensor screen — calibration has no natural name to key a file on
// (unlike a Screen), so every save stamps a new file; old profiles stay around to load from.
std::string DoSaveCalibProfile() {
  std::string err, path = (fs::path(CalibDir()) / ("Calibration-" + Stamp() + ".mikmap-calib")).string();
  return SaveCalibProfile(path, A.calib, A.roi, A.noise, A.blobSize, err) ? "Saved profile: " + fs::path(path).filename().string() : "Save failed: " + err;
}

// ───────────── machine settings ─────────────
void SaveSettings() {
  JsonValue o = JsonValue::object();
  o.set("lang", A.prefs.lang); o.set("ui", A.prefs.ui); o.set("mono", A.prefs.mono); o.set("accent", A.prefs.accent);
  o.set("surface", A.prefs.surface); o.set("scale", A.prefs.scale); o.set("outMonitor", A.outMonitor);
  o.set("browserW", A.prefs.browserW); o.set("inspectorW", A.prefs.inspectorW);
  o.set("bandPct", A.prefs.bandPct); o.set("timelineH", A.prefs.timelineH);
  { JsonValue me = JsonValue::array(); for (auto& p : A.mediaExtra) me.push(p); o.set("mediaExtra", me); }
  std::string err; WriteAtomic(ConfigDir() / "settings.json", o.dump(2), err);
}

void LoadSettings() {
  bool ok; std::string text = ReadFile(ConfigDir() / "settings.json", ok);
  if (!ok) return;
  JsonValue o; std::string err;
  if (!JsonValue::parse(text, o, err) || !o.isObject()) return;   // a broken settings file just means defaults
  Prefs p;
  // Ranges match the option lists in settings.cpp; an out-of-range index from a hand-edited file would index past the tables.
  p.lang = std::clamp(o["lang"].asInt(p.lang), 0, 4); p.ui = std::clamp(o["ui"].asInt(p.ui), 0, 3); p.mono = std::clamp(o["mono"].asInt(p.mono), 0, 2);
  p.accent = std::clamp(o["accent"].asInt(p.accent), 0, 3); p.surface = std::clamp(o["surface"].asInt(p.surface), 0, 2);
  int sc = o["scale"].asInt(p.scale); p.scale = (sc == 90 || sc == 100 || sc == 110 || sc == 125 || sc == 150) ? sc : p.scale;
  p.browserW = std::clamp(o["browserW"].asInt(p.browserW), 140, 320);
  p.inspectorW = std::clamp(o["inspectorW"].asInt(p.inspectorW), 180, 360);
  p.bandPct = std::clamp(o["bandPct"].asInt(p.bandPct), 25, 70);
  p.timelineH = std::clamp(o["timelineH"].asInt(p.timelineH), 0, 96);
  A.prefs = p; A.outMonitor = std::max(0, o["outMonitor"].asInt(0));
  A.mediaExtra.clear();
  if (o["mediaExtra"].isArray()) for (auto& v : o["mediaExtra"].arrayItems()) { std::string s = v.asString(); if (!s.empty()) A.mediaExtra.push_back(s); }
}

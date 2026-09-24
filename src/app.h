// Application state (a direct port of the prototype's React state) and screen entry points.
#pragma once
#include "ui.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>

// ───────────── deck model ─────────────
struct Fx {
  int kind = 0;  // index into FX_LIB
  bool on = true, beat = false, react = false;
  float mix = 100;
  float p[2] = {0, 0};
  int en = 0;  // enum option
};
enum PlayMode { PM_LOOP, PM_BOUN, PM_HOLD, PM_ONCE };
struct Clip {
  enum St { Empty, Loaded, Selected, Live, LiveSel, Armed };
  St st = Empty;
  std::string name, dur;
  int color = 0;       // index into CLIP_COLORS
  std::string media;   // B2: path of an image file; empty = procedural generator
  int style = -1;      // generator look; -1 = derive from the name. Pinned on rename so a new name never changes the picture
  float progress = 0;  // 0..100, advances only while the clip is selected
  // transport (C2/C4/C5)
  int playMode = PM_LOOP;
  float speed = 100;   // percent
  int dir = 1;         // +1 forward, -1 reverse (also flipped by PM_BOUN)
  // transform (D1/D2/D3/D5/D6), in canvas units / percent / degrees
  float posX = 0, posY = 0, scale = 1, rotation = 0, opacity = 100;
  bool flipH = false, flipV = false;
  std::vector<Fx> fx;  // effects belong to the clip
  bool isLive() const { return st == Live || st == LiveSel; }
  // A4: cached deck-cell thumbnail (clipart.cpp:RenderClipThumbnail) — runtime GL state only, never
  // serialized (no ClipJ/ReadClip field) and never compared for undo/dirty-checking. A copy of a Clip (undo
  // snapshots, App copies) gets the same texture id by value; that's a harmless alias, not a double-free —
  // nothing here ever calls glDeleteTextures. thumbTex == 0 means "not rendered yet", not "empty texture".
  unsigned thumbTex = 0;
  double thumbAt = -1;
};
struct Layer {
  std::string id;   // F22: stable handle a slice's input source points at — names repeat and get renamed
  std::string name, group, blend;
  float blendTime = 0, opacity = 100, audio = 0;
  bool live = false, solo = false, muted = false, bypassed = false, collapsed = false;
  std::vector<Clip> clips;
  Clip fadeFrom; float fadeT = 1.f;   // A10 dissolve: the clip being replaced fades out while fadeT runs 0 -> 1 over blendTime (runtime only, not saved)
};
struct Group {
  std::string id, name;
  int role = 2;  // 0 live/coral, 1 audio/mint, 2 preview/cyan
  bool open = true;
  int count = 0, activeCol = 0;
  float opacity = 100;   // A13: group master fader, multiplies every member layer's opacity
};
// A performance deck: its own layers/groups/columns, switchable via tabs (design ref: deckTabs/deckStore).
// App::layers/groups/colNames/activeCol always mirror decks[curDeckIdx] — see App::switchDeck().
struct Deck {
  std::string name = "Deck A";
  std::vector<Group> groups;
  std::vector<Layer> layers;
  std::vector<std::string> colNames;
  int activeCol = 0;
};

// ───────────── mapping model ─────────────
struct Mask {
  std::string id, name;
  bool inverted = true;
  int feather = 4;
  ImVec2 pts[4];
};
struct Slice {
  std::string id, name;
  bool visible = true;
  bool solo = false;   // F16: while any slice of a screen is solo, only solo slices reach the output
  int warp = 0;  // 0 cornerPin, 1 mesh
  int meshCols = 4, meshRows = 3;
  std::vector<float> meshU, meshV;                 // custom column/row split positions (0..1)
  // Mesh vertices (rows x cols) in the keystone's own unit space, not output pixels: output = keystone(local).
  // That is what makes the warp ride along when a corner pin moves. Empty (or wrong size) = undeformed grid.
  std::vector<std::vector<ImVec2>> meshLocal;
  // F22 input source: what this slice takes its picture from. The input rect (ix..ih) crops that source.
  enum Src { SrcComp = 0, SrcLayer = 1, SrcGroup = 2 };
  int srcKind = SrcComp;
  std::string srcRef;   // Layer::id or Group::id; ignored for SrcComp
  int ix = 0, iy = 0, iw = 1920, ih = 1080;
  ImVec2 q[4];   // tl, tr, br, bl — keystone corners (perspective, like Resolume / engine WarpCornerPin)
  std::vector<Mask> masks;
};
struct Screen {
  std::string id, name, outDev;
  int w = 1920, h = 1080, fps = 60;
  bool edgeBlend = false, visible = true;
  int role = 0;  // colour role, see RoleHex
  std::vector<Slice> slices;
};

// Maps a canvas pixel through a slice (input rect → warped output quad) into projector pixels.
// Used by the output window so the generated art is drawn already warped, with no intermediate texture.
struct WarpMap {
  const Slice* slice = nullptr;
  float ox = 0, oy = 0, sx = 1, sy = 1;  // output space (1920x1080) → window pixels
  ImVec2 Map(float canvasX, float canvasY) const;
};
ImVec2 SliceMapUV(const Slice& s, float u, float v);  // unit square → output-space point
void SliceOutputBounds(const Slice& s, ImVec2& mn, ImVec2& mx);   // output-space bbox of the whole warped slice
// Files written before meshLocal existed stored mesh vertices in absolute output pixels (and ignored q in mesh mode).
// Converts such a grid into keystone-local space so the slice still looks exactly as it did.
void MigrateAbsoluteMesh(Slice& s, const std::vector<std::vector<ImVec2>>& abs);

// ───────────── sensor model ─────────────
struct Device { std::string id, name, endpoint; bool connected; int fps, latency; std::string packets; };
struct Route { std::string id, source, target; bool active; };
struct Touch { int id; float x, y; };
struct Calib { float tx, ty, mx, my; };

inline uint32_t RoleHex(int r) { return r == 0 ? pal::coral : r == 1 ? pal::mint : pal::cyan; }
extern const uint32_t CLIP_COLORS[6];
extern const char* CLIP_COLOR_NAMES[6];

struct Dropdown { bool open = false; int layer = -1; ImRect anchor; };
struct LayerMenu { bool open = false; int li = 0; ImVec2 pos; };
struct FxDef { const char* name; const char* icon; int tone; int nparams; const char* pl[2]; float def[2]; const char* enumLabel; const char* opts[3]; };
extern const FxDef FX_LIB[8];
constexpr int FX_COUNT = 8;
struct ColMenu { bool open = false; int ci = 0; ImVec2 pos; };
struct DeckMenu { bool open = false; int idx = 0; ImVec2 pos; };
struct DragSrc { bool active = false; std::string name, dur, media; int fxKind = -1; };
// Files dropped from the OS (Explorer/Finder) onto the window. The GLFW callback only records them; the UI code
// that owns the drop targets (Browser panel, deck clip cells, timeline lanes) consumes them the same frame, by
// testing `pos` against its own rects, and sets `handled` so main can tell the user when a drop hit nothing.
struct OsDrop { bool pending = false, handled = false; ImVec2 pos; std::vector<std::string> paths; };
struct Prefs { int lang = 0, ui = 0, mono = 0, accent = 0, surface = 0, scale = 100;
  int browserW = 200, inspectorW = 236, bandPct = 42, timelineH = 48; };
struct ProjectFile { std::string path, name; long long mtime = 0; };
void UndoStep(bool redo);   // defined in project.cpp
struct Popup { bool open = false; ImVec2 pos; int li = 0, ci = 0; };
struct RenameBox { bool open = false, fresh = false; int kind = 0, idx = 0; ImVec2 pos; char buf[64] = {}; };
struct CtxMenu { bool open = false; ImVec2 pos; std::vector<ui::MenuItem> items; };

struct App {
  int screen = 0;  // 0 deck, 1 mapping, 2 sensor
  int canvasW = 1920, canvasH = 1080;  // A1: virtual composition canvas, independent of any projector
  int outMonitor = 0;                  // F2: which physical display the projector window goes to
  bool quantize = false;                // "Sync": triggers wait for the next beat instead of firing immediately
  int autoStartCol = -1;                // Setting: fire this column automatically when the project is opened. -1 = off (default).
  struct PendingTrig { int li, ci; bool column; };
  std::vector<PendingTrig> pending;    // triggers waiting for the next beat
  bool flushing = false;
  void flushPending();
  float bpm = 128.f;                   // tempo the beat indicator / beat-synced FX follow (tap in the status bar)
  bool beat = false, playing = true, blackout = false, testCard = false, frozen = true;
  float progress = 0;
  double lastBeat = 0;

  // deck
  int tab = 0;  // 0 comp, 1 layer, 2 clip
  float topBandPx = 0;
  bool resizingDeck = false;
  int activeCol = 1, selLi = 0, selCi = 2, selLayer = 0;
  std::vector<std::pair<int, int>> selectedCells;
  std::vector<Group> groups;
  std::vector<Layer> layers;
  std::string browserSel = "Particle Vortex";
  std::string clipMode;
  float speed = 50;
  Popup pop;
  LayerMenu layerMenu;
  ColMenu colMenu;
  std::vector<std::string> colNames;
  // Multi-deck (design ref: deckTabs) — layers/groups/colNames/activeCol above always mirror decks[curDeckIdx];
  // switchDeck() syncs the live fields into decks[] before loading the target, so the array is the single
  // source of truth for content that isn't the currently-open deck. Not reset by New Project the same way the
  // rest of the deck is: switching decks keeps whatever run mode / timeline playhead the app was in.
  std::vector<Deck> decks;
  int curDeckIdx = 0;
  DeckMenu deckMenu;
  void switchDeck(int idx);
  void addDeck();
  void duplicateDeck(int idx);
  void deleteDeck(int idx);
  void moveDeckTo(int from, int to);
  // Timeline run mode (design ref: deckMode/tlLayout/syncTimeline) — an alternate READ of the same layers/clips:
  // each layer's non-empty clips play back-to-back in column order, sized by their own duration, looping over
  // one shared 0..100 playhead. No separate clip-block storage; advancing just flips the same Clip::st the grid
  // uses, so switching back to Grid mode shows exactly what the timeline had playing.
  int deckMode = 0;  // 0 grid, 1 timeline
  float tlProgress = 0;
  bool tlLoopOn = false;
  float tlIn = 0, tlOut = 100;
  struct TlLayoutBlock { int ci; float start, end; };  // percent along the shared 0..100 playhead
  std::vector<std::vector<TlLayoutBlock>> tlLayout() const;   // one vector per layer
  void tlSync(float pct);   // apply tlLayout()+pct to Clip::st/Layer::live, same transition trigger() makes
  int dragCol = -1, dropCol = -1, fxSel = 0;
  std::map<std::string, bool> browserOpen;   // absent = open
  DragSrc dragSrc, dragSrcCand;
  bool browserPress = false; std::string browserPressName; ImVec2 browserPressPos;
  // Topmost selected clip drives Timeline / Playhead
  const Clip* topClip() const {   // topmost selected non-empty clip (drives Timeline)
    int best = -1, bc = 0;
    for (auto& c : selectedCells) if (c.first >= 0 && c.first < (int)layers.size() && c.second >= 0 && c.second < (int)layers[c.first].clips.size()
        && layers[c.first].clips[c.second].st != Clip::Empty && (best < 0 || c.first < best)) { best = c.first; bc = c.second; }
    if (best < 0 && selLi < (int)layers.size() && selCi < (int)layers[selLi].clips.size()) { best = selLi; bc = selCi; }
    return best >= 0 ? &layers[best].clips[bc] : nullptr;
  }
  void setTopProgress(float pct);   // scrub: move the playhead of the topmost selected clip (0..100)
  float topProgress() {
    int best = -1; float p = 0;
    for (auto& c : selectedCells) if (c.first >= 0 && c.first < (int)layers.size() && c.second >= 0 && c.second < (int)layers[c.first].clips.size()
        && layers[c.first].clips[c.second].st != Clip::Empty && (best < 0 || c.first < best)) { best = c.first; p = layers[c.first].clips[c.second].progress; }
    if (best < 0 && selLi < (int)layers.size() && selCi < (int)layers[selLi].clips.size()) p = layers[selLi].clips[selCi].progress;
    return p;
  }
  int colCount() const { return layers.empty() ? 8 : (int)layers[0].clips.size(); }
  std::string colName(int i) { return i < (int)colNames.size() && !colNames[i].empty() ? colNames[i] : "Column " + std::to_string(i + 1); }
  void insertCol(int at); void deleteCol(int ci); void moveColTo(int from, int to);
  void addFx(int kind); void removeFx(int i); void dupFx(int i); void moveFx(int i, int d); void resetFx(int i);
  std::vector<Fx>& fxChain() { int li = std::clamp(selLi, 0, (int)layers.size() - 1); return layers[li].clips[std::clamp(selCi, 0, (int)layers[li].clips.size() - 1)].fx; }
  void loadClip(int li, int ci, const std::string& name, const std::string& dur, const std::string& media = std::string());
  std::vector<std::string> mediaList; bool mediaStale = true;   // Browser "Media" list cache (rescanned on demand, never per frame)
  std::vector<std::string> mediaExtra;                          // files imported by drag & drop, referenced in place (machine setting, not per project)
  OsDrop osDrop;
  void dropFilesOnCell(int li, int ci, const std::vector<std::string>& paths);   // first file -> this cell, the rest -> following empty cells of the layer
  // deck selection / drag & drop
  int selMode = 2;  // 0 layer, 1 clip, 2 column
  int dragLi = -1, dragCi = -1, dropLi = -1, dropCi = -1;
  bool dragging = false;
  int pressLi = -1, pressCi = -1;
  ImVec2 dragStart;
  // settings window
  bool settingsOpen = false;
  int setTab = 0;
  Prefs prefs;
  Dropdown blendDD;
  CtxMenu ctx;
  RenameBox rename;   // inline rename box for layers (kind 0) and columns (kind 1)
  void beginRename(int kind, int idx, ImVec2 pos, const std::string& cur);
  void commitRename(const char* text);
  bool projectMenu = false, logoHover = false;
  // project file (X1): path empty = never saved; savedSnapshot = compact JSON at last save/load, used to detect unsaved edits
  std::string projectPath, projectName = "MikMap Stage 01", savedSnapshot;
  bool projectDirty = false;
  bool openDialog = false; std::vector<ProjectFile> openList; int openSel = -1;
  std::string toast; double toastUntil = 0, discardUntil = 0;   // discardUntil: a 2nd press before this time confirms dropping unsaved edits
  bool helpOpen = false, showMode = false;   // showMode: hide all editing UI, show only the live composite
  void notify(const std::string& s, double secs = 3.0);

  // mapping
  std::vector<Screen> screens;
  std::string selSc = "screen1", selSl = "slice1", selMk;
  int selKind = -1;  // -1 auto, 0 screen, 1 slice, 2 mask
  float mapZ = 1, mapScrollX = 0, mapScrollY = 0, mapCx = 0.5f, mapCy = 0.5f;
  struct MapReq { bool valid = false; float z = 1, cx = 0.5f, cy = 0.5f; } mapReq;
  // cx/cy: view centre in canvas-normalised units — may be <0 or >1, since points can live outside the output box
  void setZoom(float z) { setZoom(z, mapCx, mapCy); }
  void setZoom(float z, float cx, float cy) { mapReq.valid = true; mapReq.z = z; mapReq.cx = cx; mapReq.cy = cy; }
  bool mapFocus = false;
  char meshArm = 0; bool meshPickOn = false; float meshPickU = 0, meshPickV = 0;
  // Undo/redo is global (project.cpp): snapshots are taken automatically when input goes idle, so pushHist() is a
  // no-op kept only so the many call sites in mapping.cpp stay valid.
  void pushHist() {}
  void undoMap() { UndoStep(false); }
  void redoMap() { UndoStep(true); }
  int MapKind() { return selKind >= 0 ? selKind : (selMk.empty() ? 1 : 2); }
  int mpage = 1;  // 0 input, 1 output
  bool treeCollapsed = false;
  std::map<std::string, bool> collapsedScreens;
  std::string railScreen;

  // sensor
  std::vector<Device> devices;
  std::vector<Route> routes;
  std::vector<Touch> touchPts;
  std::vector<Calib> calib;
  ImVec2 roi[4];
  bool editRoi = false;
  bool sensorOverlay = false;   // G13: draw live sensor touches on the projector output window (debug aid, off by default)
  int wizardStep = 0;
  float noise = 1.2f, blobSize = 15;
  float sweep = 0;

  // helpers
  Group* group(const std::string& id) { for (auto& g : groups) if (g.id == id) return &g; return nullptr; }
  Screen* curScreen();
  Slice* curSlice();
  Mask* curMask();
  void init();
  // deck actions
  void cue(int li, int ci);
  void trigger(int li, int ci);
  void selectColumn(int ci);
  void fireColumn(int ci);
  void selectGroupCue(const std::string& gid, int ci);
  void stepSel(int dir);
  void stepFireColumn(int dir);   // Timeline ⏮/⏭: navigate AND play, unlike stepSel()
  void moveClip(int fl, int fc, int tl, int tc);
  // mapping actions
  void addScreen(); void addSlice(); void addMask(); void deleteMask(); void deleteSlice();
  void resetWarp();         // output corner pins -> fullscreen default (0,0,1920,1080), mesh follows (keystone-relative)
  void resetMeshWarp();     // flatten mesh deformation only, keep grid density/splits
  void resetAllWarping();   // fullscreen + default 4x3 uniform grid, no deformation
  void matchOutputToInput();  // Resolume-style: output quad = current input rect (explicit match, NOT a reset)
  void resetInputRect();   // F13-ish, Resolume-style "Whole area": input rect back to the full canvas
  void dupScreen(const std::string& id); void dupSlice(const std::string& sc, const std::string& sl);
  void removeSlice(const std::string& sc, const std::string& sl);
  void deleteScreen(const std::string& id);
  void moveScreen(const std::string& id, int dir); void moveSlice(const std::string& sc, const std::string& sl, int dir);
  std::vector<ui::MenuItem> screenMenu(const std::string& scId);
  std::vector<ui::MenuItem> sliceMenu(const std::string& scId, const std::string& slId);
  std::vector<ui::MenuItem> maskMenu(const std::string& scId, const std::string& slId, const std::string& mkId);
  void openCtx(ImVec2 pos, std::vector<ui::MenuItem> items) { ctx.open = true; ctx.pos = pos; ctx.items = std::move(items); }
  int idCounter = 100;
  std::string uid(const char* p) { return std::string(p) + "-" + std::to_string(++idCounter); }
};
extern App A;

struct ScrollArea {
  ImVec2 origin;
  ImDrawList* prev = nullptr;
  bool Begin(const char* id, ImRect r, bool horizontal = false);
  void End(float contentW, float contentH);
};

void DrawDeck(ImRect body);
void DrawMapping(ImRect body);
void DrawSensor(ImRect body);
void DrawOverlays(ImVec2 display);
void DrawSettings(ImVec2 display);
void ApplyPrefs();
// project / settings persistence (project.cpp)
std::string ProjectsDir();
std::string MediaDir();                       // ~/Documents/MikMap/media
std::vector<std::string> ListMedia();        // image / video / audio files there, sorted by name
enum MediaKind { MEDIA_NONE, MEDIA_IMAGE, MEDIA_VIDEO, MEDIA_AUDIO };
MediaKind MediaKindOf(const std::string& path);   // by extension only
void SetSettingsPersistence(bool on);         // off in headless --shot/--roundtrip runs: they never load settings.json, so writing it would clobber the user's real one
void RebuildMediaList();                    // A.mediaList = ListMedia() + drag & drop imports that still exist
int ImportMedia(const std::vector<std::string>& paths);   // remember dropped files in the Browser; returns how many were new
void PreloadMedia(const std::string& path);   // upload the texture now instead of on first draw
std::vector<ProjectFile> ListProjects();
bool SaveProject(const std::string& path, std::string& err);
bool LoadProject(const std::string& path, std::string& err);
void NewProject();       // demo show
void NewBlankProject();  // empty deck + one screen/slice
// F8: output preset — one Screen (device/slices/masks), portable across projects. Own folder + extension so it
// never shows up in ListProjects()/the Open-project dialog.
std::string PresetsDir();                     // ~/Documents/MikMap/Presets
std::vector<ProjectFile> ListPresets();
bool SaveOutputPreset(const std::string& path, const Screen& s, std::string& err);
bool LoadOutputPreset(const std::string& path, Screen& out, std::string& err);
std::string DoSaveOutputPreset(const Screen& s);   // toast-message convenience, mirrors DoSave()
// G8: calibration profile — the sensor correspondence points + ROI/noise, portable across projects. `src/`
// recomputes H_s from these points at point of use (src/calib.cpp), so the profile is the points, not a matrix.
std::string CalibDir();                       // ~/Documents/MikMap/Calibration
std::vector<ProjectFile> ListCalibProfiles();
bool SaveCalibProfile(const std::string& path, const std::vector<Calib>& calib, const ImVec2 roi[4], float noise, float blobSize, std::string& err);
bool LoadCalibProfile(const std::string& path, std::vector<Calib>& calib, ImVec2 roi[4], float& noise, float& blobSize, std::string& err);
std::string DoSaveCalibProfile();                  // toast-message convenience, mirrors DoSave()
void MarkSaved();
bool ProjectDirty();
std::string DoSave(bool asCopy);
bool CanUndo();
bool CanRedo();
void UndoTick(bool inputActive, double now);   // call once per frame
void UndoNote();                                // mark "the user just did something" so the next idle tick snapshots
bool UndoCommit();                              // force a snapshot now (used by tests)
void SaveSettings();
void LoadSettings();
int ClipStyleOf(const std::string& name);
void DrawClipContent(ImRect area, const Clip& c, float t, float baseWidth, float alpha, float lod = 1.f);
// A4: deck-cell thumbnail cache (clipart.cpp). ResetThumbBudget(n) is called once per frame (DrawDeck); each
// RenderClipThumbnail() call spends 1 of it, so at most n clips redraw their thumbnail per frame regardless of
// deck size — see the comment above RenderClipThumbnail's definition for why that budget exists.
void ResetThumbBudget(int n);
bool ThumbBudgetLeft();
void RenderClipThumbnail(Clip& c);
void SetAdditive(bool on);
// blend modes (D4) — index order matches BLEND_NAMES
constexpr int BLEND_COUNT = 8;
extern const char* BLEND_NAMES[BLEND_COUNT];
int BlendIndex(const std::string& name);
void SetBlendMode(int mode);
void InitBlendModes(void* (*getProc)(const char*));
void AdvanceClip(Clip& c, float dt);
const char* PlayModeName(int m);
float ClipSeconds(const Clip& c);   // real length from Clip::dur ("16s"); generators (∞) loop over 10 s
// composition rendering (A1) — shared by the Live Output monitor and the projector window
ImRect CanvasRect(ImRect fit);                             // letterbox `fit` to the canvas aspect
void DrawComposite(ImRect canvas, float t, float alpha);   // all live clips, bottom layer first
// F22: a slice's picture — the whole composition, one layer, or one group's layers (same blend/opacity/dissolve rules).
// A source whose layer/group no longer exists falls back to the composition (SliceSourceValid tells the UI to warn).
void DrawSliceSource(const Slice& s, ImRect canvas, float t, float alpha);
bool SliceSourceValid(const Slice& s);
std::string SliceSourceName(const Slice& s);
void EnsureLayerIds(std::vector<Layer>& layers);            // give every layer a unique id (new, loaded or duplicated)
// projector output window (F2/I1)
bool OutputOpen();
void OpenOutput(struct GLFWwindow* share, int monitorIdx);
void CloseOutput();
void ToggleOutput(struct GLFWwindow* share, int monitorIdx);
void RenderOutput();
struct GLFWwindow* glfwWin();   // the control window, for opening the output on a shared context
int MonitorCount();
std::string MonitorName(int i);
void SetOutputCapture(const char* path);
void LoadAllFonts(ImGuiIO& io, const std::string& assets);
void FitAffine(float out[9], float* rms);
// G5 homography + G9 perf
bool FitHomography(const std::vector<Calib>& pts, float H[9], float* rms);
void ApplyH(const float H[9], float x, float y, float& ox, float& oy);
void PerfPush(float ms); float PerfFps(); float PerfP99(); int PerfDrops(float budgetMs);















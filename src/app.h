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
  // Properties > Clip (Resolume-style). LIVE: paused, inPt/outPt (playback range, % of the clip), blend (0 = layer's own),
  // chan (R/G/B render mask), anchor. Stored only: tMode, autoAction/autoLoops, width/height, volume/pan.
  bool paused = false;        // runtime only (never saved): the clip's own pause button
  float inPt = 0, outPt = 100;
  int blend = 0;              // 0 = "Layer Determined", else BLEND_NAMES index + 1
  int chan = 7;               // bit0 R, bit1 G, bit2 B
  float anchorX = 0, anchorY = 0;   // canvas px from the centre: scale/rotation pivot
  int tMode = 0, autoAction = 0, autoLoops = 1, width = 0, height = 0;
  float volume = 0, pan = 0;  // dB, -100..100 (no audio engine yet)
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
  // Properties > Layer. LIVE: master, opacity, blend, blendTime (transition duration), audio (strip A bar), transform.
  // Stored only: pan, size/autoSize (0 = canvas size), transBlend — no audio engine / layer canvas / transition blends yet.
  float master = 100, pan = 0;
  int color = 0;   // index into CLIP_COLORS: the accent of this layer's strip (selection bar, glow, live chevron, V slider)
  int width = 0, height = 0, autoSize = 0, transBlend = 0;
  float posX = 0, posY = 0, scale = 100, rotation = 0, anchorX = 0, anchorY = 0;   // canvas px, %, degrees
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
struct MaskHandle { int i = 0; ImVec2 t; };   // Bezier mask: point i's tangent, in the mask's unit square (follows its frame)
struct Mask {
  std::string id, name;
  bool inverted = true;   // Mask properties > Invert: on = cut a hole, off = keep only the inside
  bool visible = true;    // the eye in the Mapping tree: off = the mask is kept but does not cut the output
  int pointMode = 0;      // 0 Linear (straight between points), 1 Bezier (a smooth closed curve through them)
  std::vector<MaskHandle> handles;   // Bezier tangents the user dragged; the others are automatic (Catmull-Rom)
  int feather = 4;        // stored only: the output does not feather mask edges yet
  // Every mask is an outline (a preset shape, or a free one) placed by a rotated rectangle in COMPOSITION CANVAS px — it is edited
  // exactly like the input rect (move / resize / rotate frame, X Y Left Top Width Height Rotation).
  int shape = -1;                                       // App::MaskShape preset, or -1 = the free outline in `u` (pen, old files)
  float x = 560, y = 320, w = 800, h = 440, rot = 0;    // Left/Top of the unrotated rect, its size, degrees
  std::vector<ImVec2> u;                                // free outline in the unit square (used when shape < 0)
  std::vector<ImVec2> pts = std::vector<ImVec2>(4);     // derived polygon in canvas px (MaskRebuild) — what drawing / masking use
};
// Point Mode Bezier: a warp point's tangents (keystone-local units per unit of mesh parameter). The handle towards the next column
// sits at point + tu * (that patch's width) / 3, the one towards the previous column mirrors it; tv likewise for rows.
struct MeshHandle { int r = 0, c = 0; ImVec2 tu, tv; };
struct Slice {
  std::string id, name;
  bool visible = true;
  bool solo = false;   // F16: while any slice of a screen is solo, only solo slices reach the output
  // Resolume's model: every slice has 4 big perspective corners (q) AND a grid of warp points inside them (meshLocal, keystone-local).
  // Subdivisions 0 x 0 = just the grid's own 4 corners. `warp` only survives for old files: 0 there meant "corner pin only" and is
  // turned into a plain 1 x 1 grid on load, so it is always 1 now.
  int warp = 1;
  int meshCols = 1, meshRows = 1;   // patches across / down (Subdivisions X / Y + 1)
  float orot = 0;   // Output Transformation > Transform: the turn of the box drawn around the slice (degrees)
  int pointMode = 0;   // Warping > Point Mode: 0 Linear (straight lines between warp points), 1 Bezier (smooth bicubic surface)
  std::vector<MeshHandle> meshHandles;   // Bezier handles the user has set; every other point gets smooth automatic tangents
  std::vector<float> meshU, meshV;                 // custom column/row split positions (0..1)
  // Mesh vertices (rows x cols) in the keystone's own unit space, not output pixels: output = keystone(local).
  // That is what makes the warp ride along when a corner pin moves. Empty (or wrong size) = undeformed grid.
  std::vector<std::vector<ImVec2>> meshLocal;
  // F22 input source: what this slice takes its picture from. The input rect (ix..ih) crops that source.
  enum Src { SrcComp = 0, SrcLayer = 1, SrcGroup = 2 };
  int srcKind = SrcComp;
  std::string srcRef;   // Layer::id or Group::id; ignored for SrcComp
  int ix = 0, iy = 0, iw = 1920, ih = 1080;
  // The input rect can be rotated about its own centre (degrees, -180..180) and mirrored — Resolume's input-selection
  // transform. ix..ih stay the UNROTATED rect, so rotating never moves the centre or changes the numeric fields.
  float irot = 0;
  bool iflipX = false, iflipY = false;
  bool maskLegacy = false;   // runtime: masks came from a file that stored them in output px; converted to canvas px right after load
  bool softEdge = false;   // Slice properties > Soft Edge. A saved switch only: the projector output does not feather edges yet
  // Output-side slice properties (Output routing page). oflip / blackBg / the colour block are drawn by DrawSliceOutput; isKey, the
  // soft-edge curve and the black-level compensation are saved and edited but not rendered yet (they matter with edge blending).
  int oflip = 0;                       // 0 none, 1 mirror X, 2 mirror Y, 3 both — combined with the input rect's own mirror (WarpMap::Map)
  bool isKey = false, blackBg = false;
  int brightness = 0, contrast = 0, red = 0, green = 0, blue = 0;   // per-slice colour correction, -100..100, on top of the Screen's
  float seGammaR = 2.f, seGammaG = 2.f, seGammaB = 2.f, seGamma = 1.f, seLum = 0.5f, sePower = 2.f;
  int blR = 0, blG = 0, blB = 0;       // black level compensation, 0..100
  bool colorActive() const { return brightness || contrast || red || green || blue; }
  ImVec2 q[4];   // tl, tr, br, bl — keystone corners (perspective, like Resolume / engine WarpCornerPin)
  std::vector<Mask> masks;
};
struct Screen {
  std::string id, name, outDev;
  int w = 1920, h = 1080, fps = 60;
  bool edgeBlend = false, visible = true;
  int role = 0;  // colour role, see RoleHex
  // Screen properties > colour. Applied to what this screen sends to its projector window (output.cpp): opacity scales every
  // slice, the rest are multiply/gain/add passes over each slice's outline (DrawColorAdjust). Percent / -100..100, 0 = untouched.
  int opacity = 100, brightness = 0, contrast = 0, red = 0, green = 0, blue = 0;
  bool colorActive() const { return opacity != 100 || brightness || contrast || red || green || blue; }
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
// Composition-level properties (Properties > Comp), saved with the project. What is REAL today: master (fader over the
// whole composite), speed (multiplies every clip's playback rate), video opacity and the transform (applied to the
// whole composite). Audio volume/pan and the crossfader are stored but drive nothing yet: there is no audio engine
// and no A/B crossfader (A14) — the panel says so next to those sections.
struct CompProps {
  float master = 100, speed = 100;                       // %, playback-rate % (100 = 1x)
  float volume = 0, pan = 0;                              // dB, -100..100  (no audio engine yet)
  float opacity = 100;                                    // % (video)
  int xfBlend = 0, xfBehaviour = 0, xfCurve = 0;          // crossfader: blend / behaviour / curve indices (no crossfader yet)
  float posX = 0, posY = 0, scale = 100, rotation = 0;    // canvas px, %, degrees
  float anchorX = 0, anchorY = 0;                         // canvas px from the canvas centre
};
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
  CompProps comp;                      // Properties > Comp (master/speed/opacity/transform are live; audio + crossfader are stored only)
  void setCanvasSize(int w, int h);    // change the canvas resolution; every slice's input rect keeps covering the same part of it
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
  int resizingCol = 0;   // Composition column splitter being dragged: 1 = Browser|monitors, 2 = monitors|Properties
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
  // Preview Cue view: zoom relative to "fit" (1 = the canvas letterboxed in the monitor), pan in screen px, hand tool on/off
  float pvZoom = 1.f, pvPanX = 0.f, pvPanY = 0.f; bool pvHand = false;
  void addLayer(); void groupSelectedLayer(); void toggleSync();
  void setLayerColor(int li, int color);   // recolours the layer AND the clips that still have the layer's old colour   // Deck tools menu
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
  char meshArm = 0;   // 'u' / 'v': + Add col / + Add row is armed — a preview line follows the pointer and the next click places it
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
  void addScreen(); void addSlice(); void deleteMask(); void deleteSlice();
  // Input Mask shapes (Slice Properties): 0 heart, 1 square, 2 circle, 3 triangle, 4 hexagon. Created around the slice's output quad.
  enum MaskShape { MS_HEART, MS_SQUARE, MS_CIRCLE, MS_TRIANGLE, MS_HEXAGON };
  void addMask(int shape = MS_SQUARE);
  // Pen: click points on the Output stage, click the first point (or Enter / double-click) to close, Esc cancels. Runtime only.
  // Stage tools (Input and Output selection): hand = left-drag pans the view instead of editing; magnet = drags snap to points and edges.
  bool mapHand = false, mapSnap = false;
  // Advanced Output tools (Resolume): 0 = Edit Points, 1 = Transform. outTool = Output Transformation; inTool = Input Selection masks.
  int outTool = 1, inTool = 1;
  // Output points picked together (marquee drag, Ctrl/Cmd/Shift-click): idx 0..3 = a slice's corner pin, 1000 + row*100 + col = a mesh point.
  // Dragging any of them, or the arrow keys, moves the whole group. Runtime only.
  struct PtRef { std::string sl; int idx; };
  std::vector<PtRef> mapPts;
  bool maskPen = false; std::vector<ImVec2> penPts;
  bool penReplace = false;   // the pen redraws the SELECTED mask instead of adding a new one
  void startMaskPen(bool replaceSelected = false); void finishMaskPen(); void cancelMaskPen();
  void setMaskShape(int shape);   // Mask properties: change the selected mask's shape (rect and rotation stay)
  // Slice clipboard + stacking order (Advanced Mapping > right-click the input rect). The clipboard is runtime-only.
  // ── Advanced Mapping multi-selection ──
  // Several screens, OR several slices, OR several masks can be selected together (Ctrl/Cmd/Shift-click); the kinds never mix.
  // `selSc/selSl/selMk/selKind` stay the primary selection (what the properties and the stage frame edit); `mapMulti` lists EVERY
  // selected item of that kind (primary included) when there are two or more, and is dropped as soon as the primary moves elsewhere.
  struct MapRef { std::string sc, sl, mk; };
  std::vector<MapRef> mapMulti; int mapMultiKind = -1;
  void mapValidate();
  bool mapIsSel(int kind, const std::string& sc, const std::string& sl, const std::string& mk);
  std::vector<MapRef> mapSelection();                 // the selection of kind MapKind(): the multi list, or just the primary
  int mapSelCount() { return (int)mapSelection().size(); }
  void mapToggle(int kind, const MapRef& r);          // Ctrl/Cmd/Shift-click: add/remove r; a different kind is ignored
  void mapSelectRefs(int kind, const std::vector<MapRef>& refs);   // select exactly these (last one becomes the primary)
  // Clipboard: copying a screen carries its slices and their masks, a slice carries its masks. Pasting: screens go after the selected
  // screen, slices into the current screen, masks into the current slice; every copy gets fresh ids.
  struct MapClip { int kind = -1; std::vector<Screen> screens; std::vector<Slice> slices; std::vector<Mask> masks; } mapClip;
  bool hasClip() const { return mapClip.kind >= 0; }
  void copyKind(int kind); void deleteKind(int kind); void duplicateKind(int kind); void pasteClip();
  void copySelection() { copyKind(MapKind()); }
  void cutSelection() { copyKind(MapKind()); deleteKind(MapKind()); }
  void deleteSelection() { deleteKind(MapKind()); }
  void duplicateSelection() { duplicateKind(MapKind()); }
  void pasteSelection() { pasteClip(); }
  void nudgeSelection(float dx, float dy);            // arrow keys: input rects / masks on the Input page, slice quads on the Output page
  void duplicateSlice() { duplicateKind(1); } void copySlice() { copyKind(1); } void cutSlice() { copyKind(1); deleteKind(1); } void pasteSlice() { pasteClip(); }
  void moveSliceZ(int delta);   // +1 = bring forward (later in the list draws on top), -1 = send backwards
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
extern float gTestInspScroll;   // test aid (--inspscroll): scrolls the Properties panel so a headless screenshot can show its lower half (global: NewProject() rebuilds App)

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
void PreviewTransformMenu(int layer, int column, ImVec2 at);   // Preview Cue right-click presets (opens A.ctx)
void ClipEffectivePos(const Clip& c, float base, float& px, float& py);   // clip position with its anchor folded in (art units at `base` width)
bool MediaImageSize(const std::string& path, int& w, int& h);   // pixel size of a loaded image clip source
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
void UpdateTestPattern();   // repaint the Show TestCard texture at the comp's resolution (once per frame, main context)
unsigned LogoTexture();
unsigned SliceSourceTexture(const Slice& s, float t);   // the slice's source drawn once this frame into a canvas-sized texture (0 = unavailable)
void SliceSourcesRenderable(bool ok);                  // false while drawing in the projector window's context (no FBO there)
void DrawSliceTextured(const Slice& s, unsigned tex, float ox, float oy, float sx, float sy, float alpha);   // that texture through the warp   // GL texture of the MikMap mark (0 until the assets are loaded)
void DrawSliceOutput(const Screen& sc, const Slice& sl, float ox, float oy, float sx, float sy, float t);   // the slice as the projector shows it (output.cpp), also used by the Output stage's thumbnails
std::vector<ImVec2> SliceOutline(const Slice& s);   // output-space outline of a slice as the audience sees it (quad, or the mesh border)
// Everything drawn between MaskBegin and MaskEnd is limited to the slice's `outline`, then to the union of the `keep` mask polygons (when
// there are any), minus the `holes`. Polygons are in window px. Done with the stencil buffer through draw-list callbacks; SetBlendMode(0)
// re-arms the stencil test after the backend's ResetRenderState (which disables it). No feathering yet.
void MaskBegin(const std::vector<ImVec2>& outline, const std::vector<std::vector<ImVec2>>& keep, const std::vector<std::vector<ImVec2>>& holes);
void MaskEnd();
void MaskRebuild(Mask& m);
void MaskSetPoints(Mask& m, const std::vector<ImVec2>& pts);
std::vector<ImVec2> MaskOutline(const Mask& m);   // the outline actually cut (canvas px): the points, or the Bezier curve through them   // a free outline through these canvas points; keeps the mask's turn
void NormalizeWarp(Slice& s);
bool MappingSelfTest(std::string& why);   // --roundtrip: Transform box, subdivision resampling, old-file normalising, mask point edits   // old corner-pin-only slices (warp 0) become a plain 1 x 1 grid                                        // regenerate pts from shape/u + the rect (call after changing any of them)
void MaskFromPolygon(Mask& m, const std::vector<ImVec2>& poly);   // make m a free outline that fits a canvas-px polygon
// Brightness/contrast/RGB (each -1..1) on what is already drawn inside `poly` (window px): contrast pivots on mid-grey, RGB scale
// that channel, brightness shifts all. Done with GL multiply / gain (dst*(1+c)) / add / subtract passes, so it needs no shader.
void DrawColorAdjust(const ImVec2* poly, int n, float contrast, float brightness, float r, float gch, float b);
bool SliceSourceValid(const Slice& s);
std::string SliceSourceName(const Slice& s);
void EnsureLayerIds(std::vector<Layer>& layers);            // give every layer a unique id (new, loaded or duplicated)
// projector output window (F2/I1)
bool OutputOpen();                          // outputs switched on (F11)
int OutputWindowCount();                    // projector windows actually open (one per screen on a physical display)
int OutputMonitorOf(const std::string& screenId);   // the display a screen's window is on, or -1
void OpenOutput(struct GLFWwindow* share, int monitorIdx);
void CloseOutput();
void ToggleOutput(struct GLFWwindow* share, int monitorIdx);
// Keys that close the projector window while IT has focus (the main window's F11 only works when the main window does):
// Esc, F11, or Ctrl/Cmd+W. GLFW key/action/mods values are passed through so this stays testable without a window.
bool OutputKeyCloses(int key, int action, int mods);
bool OutputKeyWiringOk();   // test aid (--outkeytest): fires Esc through the output window's real key callback and reports whether it got flagged to close
void RenderOutput();
struct GLFWwindow* glfwWin();   // the control window, for opening the output on a shared context
int MonitorCount();
std::string MonitorName(int i);
extern const char* const kVirtualDevices[3];   // NDI / Spout / Virtual output: no display, so the resolution is typed in
bool IsVirtualDevice(const std::string& dev);
int DeviceMonitor(const std::string& dev);      // the physical display a screen's device name refers to, or -1
bool DeviceResolution(const std::string& dev, int& w, int& h);   // that display's real resolution
void SyncScreenResolutions();                   // screens on a physical display take its real resolution (once per frame)
void SetOutputCapture(const char* path);
void LoadAllFonts(ImGuiIO& io, const std::string& assets);
void FitAffine(float out[9], float* rms);
// G5 homography + G9 perf
bool FitHomography(const std::vector<Calib>& pts, float H[9], float* rms);
void ApplyH(const float H[9], float x, float y, float& ox, float& oy);
void PerfPush(float ms); float PerfFps(); float PerfP99(); int PerfDrops(float budgetMs);















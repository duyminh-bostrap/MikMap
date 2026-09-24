// Settings window (language · fonts · theme colour · text size) and the font / theme machinery behind it.
#include "app.h"
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <map>

using namespace ui;
namespace fs = std::filesystem;

// ───────────────────────── fonts ─────────────────────────
static ImFont* famUI[4][4];    // family × (regular, semibold, bold, extrabold)
static ImFont* famMono[3][3];  // family × (regular, medium, bold)

static std::map<std::string, std::vector<char>>& Blobs() { static std::map<std::string, std::vector<char>> m; return m; }
static const std::vector<char>* Blob(const std::string& path) {
  auto& m = Blobs();
  auto it = m.find(path);
  if (it != m.end()) return &it->second;
  std::ifstream f(path, std::ios::binary);
  if (!f) return nullptr;
  std::vector<char> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
  return &(m[path] = std::move(b));
}

static ImFont* AddFont(ImGuiIO& io, const std::string& path, const std::vector<std::string>& fallbacks) {
  const std::vector<char>* b = Blob(path);
  if (!b) return nullptr;
  ImFontConfig c; c.FontDataOwnedByAtlas = false;
  ImFont* f = io.Fonts->AddFontFromMemoryTTF((void*)b->data(), (int)b->size(), 0.f, &c);
  for (auto& fb : fallbacks) {
    const std::vector<char>* d = Blob(fb);
    if (!d) continue;
    ImFontConfig m; m.MergeMode = true; m.FontDataOwnedByAtlas = false;
    io.Fonts->AddFontFromMemoryTTF((void*)d->data(), (int)d->size(), 0.f, &m);
  }
  return f;
}

void LoadAllFonts(ImGuiIO& io, const std::string& assets) {
  // CJK/system-font fallback paths for merging glyphs the bundled UI fonts
  // don't cover (user-entered clip/layer/device names are never translated —
  // see the Settings text below — so a Chinese/Japanese/Korean name is
  // plausible on any OS). Missing files fail silently in Blob() and just
  // fall back to ImGui's built-in default font, same as before — this only
  // changes WHICH paths get tried per platform, not the failure behavior.
const std::string fd = assets + "/fonts/";
#ifdef _WIN32
  // %WINDIR% thay vì cứng C:\Windows — Windows không nhất thiết cài ở ổ C:.
  const char* winDir = std::getenv("WINDIR");
  // Dấu "/" chạy được trên Windows (fopen/CreateFile đều nhận), khỏi phải escape "\\".
  const std::string sysFontDir = std::string(winDir ? winDir : "C:/Windows") + "/Fonts/";
  const std::vector<std::string> cjkFallback = {sysFontDir + "msyh.ttc", sysFontDir + "YuGothM.ttc", sysFontDir + "malgun.ttf"};
  const char* segoe[4] = {"segoeui.ttf", "seguisb.ttf", "segoeuib.ttf", "seguibl.ttf"};
#elif defined(__APPLE__)
  const std::string sysFontDir = "/System/Library/Fonts/";
  const std::vector<std::string> cjkFallback = {sysFontDir + "PingFang.ttc"};
#else
  const std::string sysFontDir = "/usr/share/fonts/";
  const std::vector<std::string> cjkFallback = {
      sysFontDir + "opentype/noto/NotoSansCJK-Regular.ttc",
      sysFontDir + "truetype/noto/NotoSansCJK-Regular.ttc"};
#endif
  ImFont* dflt = io.Fonts->AddFontDefault();
  const char* uiFiles[4][4] = {
      {"Archivo-Regular.ttf", "Archivo-SemiBold.ttf", "Archivo-Bold.ttf", "Archivo-ExtraBold.ttf"},
      {"BarlowCondensed-Regular.ttf", "BarlowCondensed-SemiBold.ttf", "BarlowCondensed-Bold.ttf", "BarlowCondensed-ExtraBold.ttf"},
      {"IBMPlexSans-Var.ttf", "IBMPlexSans-Var.ttf", "IBMPlexSans-Var.ttf", "IBMPlexSans-Var.ttf"},
      {"SpaceGrotesk-Var.ttf", "SpaceGrotesk-Var.ttf", "SpaceGrotesk-Var.ttf", "SpaceGrotesk-Var.ttf"}};
  const char* monoFiles[3][3] = {{"JetBrainsMono-Regular.ttf", "JetBrainsMono-Medium.ttf", "JetBrainsMono-Bold.ttf"},
                                 {"IBMPlexMono-Regular.ttf", "IBMPlexMono-Medium.ttf", "IBMPlexMono-Bold.ttf"},
                                 {"RobotoMono-Var.ttf", "RobotoMono-Var.ttf", "RobotoMono-Var.ttf"}};
  for (int f = 0; f < 4; ++f) for (int w = 0; w < 4; ++w) {
#ifdef _WIN32
    std::vector<std::string> fb = {sysFontDir + segoe[w]};
    fb.insert(fb.end(), cjkFallback.begin(), cjkFallback.end());
#else
    std::vector<std::string> fb = cjkFallback;
#endif
    ImFont* x = AddFont(io, fd + uiFiles[f][w], fb);
#ifdef _WIN32
    if (!x) x = AddFont(io, sysFontDir + segoe[w], cjkFallback);
#endif
    famUI[f][w] = x ? x : dflt;
  }
  for (int f = 0; f < 3; ++f) for (int w = 0; w < 3; ++w) {
    ImFont* x = AddFont(io, fd + monoFiles[f][w], {});
    famMono[f][w] = x ? x : dflt;
  }
}

// ───────────────────────── prefs ─────────────────────────
struct Accent { const char* id; const char* name; uint32_t c1, c2, c3; };
static const Accent ACC[4] = {{"coral", "Coral \xC2\xB7 Cyan \xC2\xB7 Mint", 0xff7f50, 0x118ab2, 0x06d6a0},
                              {"uv", "Ultraviolet", 0xb388ff, 0x6c7bff, 0x06d6a0},
                              {"ember", "Ember", 0xef4444, 0xff7f50, 0xffd166},
                              {"arctic", "Arctic", 0x118ab2, 0x06d6a0, 0xffd166}};
struct Surf { const char* name; uint32_t ramp[8]; };
static const Surf SURF[3] = {{"Abyss", {0x050505, 0x0f0f0f, 0x101010, 0x121212, 0x141414, 0x161616, 0x181818, 0x1c1c1c}},
                             {"Graphite", {0x0c0c0c, 0x161616, 0x181818, 0x1b1b1b, 0x1d1d1d, 0x202020, 0x242424, 0x292929}},
                             {"True black", {0x000000, 0x040404, 0x060606, 0x090909, 0x0b0b0b, 0x0e0e0e, 0x111111, 0x151515}}};
static const char* UI_NAMES[4] = {"Archivo", "Barlow Condensed", "IBM Plex Sans", "Space Grotesk"};
static const char* MONO_NAMES[3] = {"JetBrains Mono", "IBM Plex Mono", "Roboto Mono"};
static const int SIZES[5] = {90, 100, 110, 125, 150};

void ApplyPrefs() {
  const Prefs& p = A.prefs;
  for (int i = 0; i < 4; ++i) g.fonts[UI_R + i] = famUI[p.ui][i];
  for (int i = 0; i < 3; ++i) g.fonts[MONO_R + i] = famMono[p.mono][i];
  pal::coral = ACC[p.accent].c1; pal::cyan = ACC[p.accent].c2; pal::mint = ACC[p.accent].c3;
  const uint32_t* r = SURF[p.surface].ramp;
  pal::g050 = r[0]; pal::g0f = r[1]; pal::g10 = r[2]; pal::g12 = r[3]; pal::g14 = r[4]; pal::g16 = r[5]; pal::g18 = r[6]; pal::g1c = r[7];
}

// ───────────────────────── strings ─────────────────────────
struct Tx {
  const char *title, *tabs[5], *langHead, *langNote, *fontUi, *fontMono, *accent, *surface, *sizeHead, *preview, *sizeNote, *applyNote, *reset, *done, *sizes[5];
  const char *layHead, *layBrowser, *layInspector, *layBand, *layTimeline, *layNote;
};
static const Tx TX[5] = {
    {"C\xC3\xA0i \xC4\x91\xE1\xBA\xB7t", {"Ng\xC3\xB4n ng\xE1\xBB\xAF", "Ph\xC3\xB4ng ch\xE1\xBB\xAF", "M\xC3\xA0u theme", "C\xE1\xBB\xA1 ch\xE1\xBB\xAF", "B\xE1\xBB\x91 c\xE1\xBB\xA5" "c"},
     "Ng\xC3\xB4n ng\xE1\xBB\xAF giao di\xE1\xBB\x87n", "T\xC3\xAAn l\xE1\xBB\x9Bp, clip v\xC3\xA0 thi\xE1\xBA\xBFt b\xE1\xBB\x8B do ng\xC6\xB0\xE1\xBB\x9Di d\xC3\xB9ng \xC4\x91\xE1\xBA\xB7t kh\xC3\xB4ng \xC4\x91\xC6\xB0\xE1\xBB\xA3" "c d\xE1\xBB\x8B" "ch.",
     "Ph\xC3\xB4ng giao di\xE1\xBB\x87n", "Ph\xC3\xB4ng k\xE1\xBB\xB9 thu\xE1\xBA\xADt (s\xE1\xBB\x91 li\xE1\xBB\x87u)", "M\xC3\xA0u t\xC3\xADn hi\xE1\xBB\x87u", "N\xE1\xBB\x81n workspace",
     "C\xE1\xBB\xA1 ch\xE1\xBB\xAF giao di\xE1\xBB\x87n", "Xem tr\xC6\xB0\xE1\xBB\x9B" "c", "\xC3\x81p d\xE1\xBB\xA5ng cho to\xC3\xA0n b\xE1\xBB\x99 workspace, kh\xC3\xB4ng \xC4\x91\xE1\xBB\x95i b\xE1\xBB\x91 c\xE1\xBB\xA5" "c.",
     "Thay \xC4\x91\xE1\xBB\x95i \xC3\xA1p d\xE1\xBB\xA5ng t\xE1\xBB\xA9" "c th\xC3\xAC", "M\xE1\xBA\xB7" "c \xC4\x91\xE1\xBB\x8Bnh", "Xong",
     {"G\xE1\xBB\x8Dn", "M\xE1\xBA\xB7" "c \xC4\x91\xE1\xBB\x8Bnh", "Tho\xC3\xA1ng", "L\xE1\xBB\x9Bn", "R\xE1\xBA\xA5t l\xE1\xBB\x9Bn"}, "\x42\xE1\xBB\x91\x20\x63\xE1\xBB\xA5\x63\x20\x77\x6F\x72\x6B\x73\x70\x61\x63\x65", "\x52\xE1\xBB\x99\x6E\x67\x20\x42\x72\x6F\x77\x73\x65\x72", "\x52\xE1\xBB\x99\x6E\x67\x20\x50\x72\x6F\x70\x65\x72\x74\x69\x65\x73", "\x43\x61\x6F\x20\x64\xE1\xBA\xA3\x69\x20\x74\x72\xC3\xAA\x6E", "Cao thanh timeline", "\x4B\xC3\xA9\x6F\x20\xC4\x91\xE1\xBB\x83\x20\xC4\x91\xE1\xBB\x95\x69\x20\x6E\x67\x61\x79\x3B\x20\x6C\xC6\xB0\x75\x20\x74\x72\xC3\xAA\x6E\x20\x6D\xC3\xA1\x79\x20\x6E\xC3\xA0\x79\x2E\x20\x54\x69\x6D\x65\x6C\x69\x6E\x65\x20\x3D\x20\x30\x20\xC4\x91\xE1\xBB\x83\x20\xE1\xBA\xA9\x6E\x20\x74\x68\x61\x6E\x68\x2E"},
    {"Settings", {"Language", "Fonts", "Theme color", "Text size", "Layout"}, "Interface language", "User-named layers, clips and devices are never translated.",
     "Interface face", "Technical face (numbers)", "Signal colors", "Workspace base", "Interface text size", "Preview",
     "Applies to the whole workspace; layout is unchanged.", "Changes apply instantly", "Defaults", "Done", {"Compact", "Default", "Roomy", "Large", "X-Large"}, "Workspace layout", "Browser width", "Properties width", "Top band height", "Timeline height", "Drag to apply instantly; saved on this machine. Timeline = 0 hides the bar."},
    {"\xE8\xA8\xAD\xE5\xAE\x9A", {"\xE8\xA8\x80\xE8\xAA\x9E", "\xE3\x83\x95\xE3\x82\xA9\xE3\x83\xB3\xE3\x83\x88", "\xE3\x83\x86\xE3\x83\xBC\xE3\x83\x9E\xE8\x89\xB2", "\xE6\x96\x87\xE5\xAD\x97\xE3\x82\xB5\xE3\x82\xA4\xE3\x82\xBA", "\xE3\x83\xAC\xE3\x82\xA4\xE3\x82\xA2\xE3\x82\xA6\xE3\x83\x88"},
     "\xE3\x82\xA4\xE3\x83\xB3\xE3\x82\xBF\xE3\x83\xBC\xE3\x83\x95\xE3\x82\xA7\xE3\x83\xBC\xE3\x82\xB9\xE8\xA8\x80\xE8\xAA\x9E", "\xE3\x83\xA6\xE3\x83\xBC\xE3\x82\xB6\xE3\x83\xBC\xE3\x81\x8C\xE4\xBB\x98\xE3\x81\x91\xE3\x81\x9F\xE3\x83\xAC\xE3\x82\xA4\xE3\x83\xA4\xE3\x83\xBC\xE3\x83\xBB\xE3\x82\xAF\xE3\x83\xAA\xE3\x83\x83\xE3\x83\x97\xE5\x90\x8D\xE3\x81\xAF\xE7\xBF\xBB\xE8\xA8\xB3\xE3\x81\x95\xE3\x82\x8C\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93\xE3\x80\x82",
     "UI \xE3\x83\x95\xE3\x82\xA9\xE3\x83\xB3\xE3\x83\x88", "\xE6\x95\xB0\xE5\x80\xA4\xE3\x83\x95\xE3\x82\xA9\xE3\x83\xB3\xE3\x83\x88", "\xE3\x82\xB7\xE3\x82\xB0\xE3\x83\x8A\xE3\x83\xAB\xE3\x82\xAB\xE3\x83\xA9\xE3\x83\xBC", "\xE3\x83\xAF\xE3\x83\xBC\xE3\x82\xAF\xE3\x82\xB9\xE3\x83\x9A\xE3\x83\xBC\xE3\x82\xB9\xE8\x83\x8C\xE6\x99\xAF",
     "UI \xE6\x96\x87\xE5\xAD\x97\xE3\x82\xB5\xE3\x82\xA4\xE3\x82\xBA", "\xE3\x83\x97\xE3\x83\xAC\xE3\x83\x93\xE3\x83\xA5\xE3\x83\xBC", "\xE3\x83\xAC\xE3\x82\xA4\xE3\x82\xA2\xE3\x82\xA6\xE3\x83\x88\xE3\x81\xAF\xE5\xA4\x89\xE3\x82\x8F\xE3\x82\x8A\xE3\x81\xBE\xE3\x81\x9B\xE3\x82\x93\xE3\x80\x82",
     "\xE5\xA4\x89\xE6\x9B\xB4\xE3\x81\xAF\xE5\x8D\xB3\xE6\x99\x82\xE5\x8F\x8D\xE6\x98\xA0", "\xE6\x97\xA2\xE5\xAE\x9A\xE5\x80\xA4", "\xE5\xAE\x8C\xE4\xBA\x86",
     {"\xE3\x82\xB3\xE3\x83\xB3\xE3\x83\x91\xE3\x82\xAF\xE3\x83\x88", "\xE6\xA8\x99\xE6\xBA\x96", "\xE3\x82\x86\xE3\x81\xA3\xE3\x81\x9F\xE3\x82\x8A", "\xE5\xA4\xA7", "\xE7\x89\xB9\xE5\xA4\xA7"}, "\xE3\x83\xAF\xE3\x83\xBC\xE3\x82\xAF\xE3\x82\xB9\xE3\x83\x9A\xE3\x83\xBC\xE3\x82\xB9\xE3\x81\xAE\xE3\x83\xAC\xE3\x82\xA4\xE3\x82\xA2\xE3\x82\xA6\xE3\x83\x88", "\xE3\x83\x96\xE3\x83\xA9\xE3\x82\xA6\xE3\x82\xB6\xE5\xB9\x85", "\xE3\x83\x97\xE3\x83\xAD\xE3\x83\x91\xE3\x83\x86\xE3\x82\xA3\xE5\xB9\x85", "\xE4\xB8\x8A\xE9\x83\xA8\xE3\x83\x90\xE3\x83\xB3\xE3\x83\x89\xE3\x81\xAE\xE9\xAB\x98\xE3\x81\x95", "\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\xA0\xE3\x83\xA9\xE3\x82\xA4\xE3\x83\xB3\xE3\x81\xAE\xE9\xAB\x98\xE3\x81\x95", "\xE3\x83\x89\xE3\x83\xA9\xE3\x83\x83\xE3\x82\xB0\xE3\x81\xA7\xE5\x8D\xB3\xE6\x99\x82\xE5\x8F\x8D\xE6\x98\xA0\xE3\x80\x81\xE3\x81\x93\xE3\x81\xAE\x50\x43\xE3\x81\xAB\xE4\xBF\x9D\xE5\xAD\x98\xE3\x80\x82\xE3\x82\xBF\xE3\x82\xA4\xE3\x83\xA0\xE3\x83\xA9\xE3\x82\xA4\xE3\x83\xB3\x3D\x30\xE3\x81\xA7\xE9\x9D\x9E\xE8\xA1\xA8\xE7\xA4\xBA\xE3\x80\x82"},
    {"\xEC\x84\xA4\xEC\xA0\x95", {"\xEC\x96\xB8\xEC\x96\xB4", "\xEA\xB8\x80\xEA\xBC\xB4", "\xED\x85\x8C\xEB\xA7\x88 \xEC\x83\x89", "\xEA\xB8\x80\xEC\x9E\x90 \xED\x81\xAC\xEA\xB8\xB0", "\xEB\xA0\x88\xEC\x9D\xB4\xEC\x95\x84\xEC\x9B\x83"},
     "\xEC\x9D\xB8\xED\x84\xB0\xED\x8E\x98\xEC\x9D\xB4\xEC\x8A\xA4 \xEC\x96\xB8\xEC\x96\xB4", "\xEC\x82\xAC\xEC\x9A\xA9\xEC\x9E\x90\xEA\xB0\x80 \xEC\xA7\x80\xEC\xA0\x95\xED\x95\x9C \xEB\xA0\x88\xEC\x9D\xB4\xEC\x96\xB4\xC2\xB7\xED\x81\xB4\xEB\xA6\xBD \xEC\x9D\xB4\xEB\xA6\x84\xEC\x9D\x80 \xEB\xB2\x88\xEC\x97\xAD\xEB\x90\x98\xEC\xA7\x80 \xEC\x95\x8A\xEC\x8A\xB5\xEB\x8B\x88\xEB\x8B\xA4.",
     "UI \xEA\xB8\x80\xEA\xBC\xB4", "\xEC\x88\xAB\xEC\x9E\x90 \xEA\xB8\x80\xEA\xBC\xB4", "\xEC\x8B\x9C\xEA\xB7\xB8\xEB\x84\x90 \xEC\x83\x89\xEC\x83\x81", "\xEC\x9E\x91\xEC\x97\x85 \xEA\xB3\xB5\xEA\xB0\x84 \xEB\xB0\xB0\xEA\xB2\xBD",
     "UI \xEA\xB8\x80\xEC\x9E\x90 \xED\x81\xAC\xEA\xB8\xB0", "\xEB\xAF\xB8\xEB\xA6\xAC\xEB\xB3\xB4\xEA\xB8\xB0", "\xEB\xA0\x88\xEC\x9D\xB4\xEC\x95\x84\xEC\x9B\x83\xEC\x9D\x80 \xEB\xB3\x80\xED\x95\x98\xEC\xA7\x80 \xEC\x95\x8A\xEC\x8A\xB5\xEB\x8B\x88\xEB\x8B\xA4.",
     "\xEC\xA6\x89\xEC\x8B\x9C \xEC\xA0\x81\xEC\x9A\xA9\xEB\x90\xA8", "\xEA\xB8\xB0\xEB\xB3\xB8\xEA\xB0\x92", "\xEC\x99\x84\xEB\xA3\x8C",
     {"\xEC\xA2\x81\xEA\xB2\x8C", "\xEA\xB8\xB0\xEB\xB3\xB8", "\xEB\x84\x93\xEA\xB2\x8C", "\xED\x81\xAC\xEA\xB2\x8C", "\xED\x8A\xB9\xEB\x8C\x80"}, "\xEC\x9E\x91\xEC\x97\x85\x20\xEA\xB3\xB5\xEA\xB0\x84\x20\xEB\xA0\x88\xEC\x9D\xB4\xEC\x95\x84\xEC\x9B\x83", "\xEB\xB8\x8C\xEB\x9D\xBC\xEC\x9A\xB0\xEC\xA0\x80\x20\xEB\x84\x88\xEB\xB9\x84", "\xEC\x86\x8D\xEC\x84\xB1\x20\xEB\x84\x88\xEB\xB9\x84", "\xEC\x83\x81\xEB\x8B\xA8\x20\xEB\xB0\xB4\xEB\x93\x9C\x20\xEB\x86\x92\xEC\x9D\xB4", "\xED\x83\x80\xEC\x9E\x84\xEB\x9D\xBC\xEC\x9D\xB8\x20\xEB\x86\x92\xEC\x9D\xB4", "\xEB\x93\x9C\xEB\x9E\x98\xEA\xB7\xB8\xED\x95\x98\xEB\xA9\xB4\x20\xEC\xA6\x89\xEC\x8B\x9C\x20\xEC\xA0\x81\xEC\x9A\xA9\xEB\x90\x98\xEB\xA9\xB0\x20\xEC\x9D\xB4\x20\x50\x43\xEC\x97\x90\x20\xEC\xA0\x80\xEC\x9E\xA5\xEB\x90\xA9\xEB\x8B\x88\xEB\x8B\xA4\x2E\x20\xED\x83\x80\xEC\x9E\x84\xEB\x9D\xBC\xEC\x9D\xB8\x3D\x30\xEC\x9D\xB4\xEB\xA9\xB4\x20\xEC\x88\xA8\xEA\xB9\x80\x2E"},
    {"\xE8\xAE\xBE\xE7\xBD\xAE", {"\xE8\xAF\xAD\xE8\xA8\x80", "\xE5\xAD\x97\xE4\xBD\x93", "\xE4\xB8\xBB\xE9\xA2\x98\xE8\x89\xB2", "\xE6\x96\x87\xE5\xAD\x97\xE5\xA4\xA7\xE5\xB0\x8F", "\xE5\xB8\x83\xE5\xB1\x80"},
     "\xE7\x95\x8C\xE9\x9D\xA2\xE8\xAF\xAD\xE8\xA8\x80", "\xE7\x94\xA8\xE6\x88\xB7\xE5\x91\xBD\xE5\x90\x8D\xE7\x9A\x84\xE5\x9B\xBE\xE5\xB1\x82\xE3\x80\x81\xE7\x89\x87\xE6\xAE\xB5\xE4\xB8\x8D\xE4\xBC\x9A\xE8\xA2\xAB\xE7\xBF\xBB\xE8\xAF\x91\xE3\x80\x82",
     "\xE7\x95\x8C\xE9\x9D\xA2\xE5\xAD\x97\xE4\xBD\x93", "\xE6\x95\xB0\xE6\x8D\xAE\xE5\xAD\x97\xE4\xBD\x93", "\xE4\xBF\xA1\xE5\x8F\xB7\xE9\x85\x8D\xE8\x89\xB2", "\xE5\xB7\xA5\xE4\xBD\x9C\xE5\x8C\xBA\xE5\xBA\x95\xE8\x89\xB2",
     "\xE7\x95\x8C\xE9\x9D\xA2\xE6\x96\x87\xE5\xAD\x97\xE5\xA4\xA7\xE5\xB0\x8F", "\xE9\xA2\x84\xE8\xA7\x88", "\xE4\xBB\x85\xE7\xBC\xA9\xE6\x94\xBE\xE6\x96\x87\xE5\xAD\x97\xEF\xBC\x8C\xE5\xB8\x83\xE5\xB1\x80\xE4\xB8\x8D\xE5\x8F\x98\xE3\x80\x82",
     "\xE6\x9B\xB4\xE6\x94\xB9\xE5\x8D\xB3\xE6\x97\xB6\xE7\x94\x9F\xE6\x95\x88", "\xE9\xBB\x98\xE8\xAE\xA4", "\xE5\xAE\x8C\xE6\x88\x90",
     {"\xE7\xB4\xA7\xE5\x87\x91", "\xE9\xBB\x98\xE8\xAE\xA4", "\xE5\xAE\xBD\xE6\x9D\xBE", "\xE5\xA4\xA7", "\xE7\x89\xB9\xE5\xA4\xA7"}, "\xE5\xB7\xA5\xE4\xBD\x9C\xE5\x8C\xBA\xE5\xB8\x83\xE5\xB1\x80", "\xE6\xB5\x8F\xE8\xA7\x88\xE5\x99\xA8\xE5\xAE\xBD\xE5\xBA\xA6", "\xE5\xB1\x9E\xE6\x80\xA7\xE5\xAE\xBD\xE5\xBA\xA6", "\xE9\xA1\xB6\xE9\x83\xA8\xE6\x9D\xA1\xE5\xB8\xA6\xE9\xAB\x98\xE5\xBA\xA6", "\xE6\x97\xB6\xE9\x97\xB4\xE8\xBD\xB4\xE9\xAB\x98\xE5\xBA\xA6", "\xE6\x8B\x96\xE5\x8A\xA8\xE5\x8D\xB3\xE6\x97\xB6\xE5\xBA\x94\xE7\x94\xA8\xEF\xBC\x8C\xE4\xBF\x9D\xE5\xAD\x98\xE5\x9C\xA8\xE6\x9C\xAC\xE6\x9C\xBA\xE3\x80\x82\xE6\x97\xB6\xE9\x97\xB4\xE8\xBD\xB4\x3D\x30\x20\xE5\x8F\xAF\xE9\x9A\x90\xE8\x97\x8F\xE3\x80\x82"}};;

struct Lang { const char* code; const char* native; const char* region; };
static const Lang LANGS[5] = {{"VI", "Ti\xE1\xBA\xBF" "ng Vi\xE1\xBB\x87t", "VN"}, {"EN", "English", "US"}, {"JA", "\xE6\x97\xA5\xE6\x9C\xAC\xE8\xAA\x9E", "JP"},
                              {"KO", "\xED\x95\x9C\xEA\xB5\xAD\xEC\x96\xB4", "KR"}, {"ZH", "\xE4\xB8\xAD\xE6\x96\x87 (\xE7\xAE\x80\xE4\xBD\x93)", "CN"}};

static std::vector<std::string> Wrap(FontId f, float sz, const char* text, float width) {
  std::vector<std::string> lines; std::string cur, word;
  auto flush = [&]() {
    if (word.empty()) return;
    std::string t = cur.empty() ? word : cur + " " + word;
    if (!cur.empty() && TextW(f, sz, t.c_str()) > width) { lines.push_back(cur); cur = word; } else cur = t;
    word.clear();
  };
  for (const char* p = text; *p; ++p) { if (*p == ' ') flush(); else word += *p; }
  flush();
  if (!cur.empty()) lines.push_back(cur);
  return lines;
}

static bool RawHover(ImRect r) { return r.Contains(ImGui::GetIO().MousePos); }

// skin(on, tone): tinted when selected, neutral control otherwise
static void Skin(ImRect r, bool on, uint32_t tone, float rd = 3) {
  if (on) { Glow(r, tone, 0.30f, 12, rd); Fill(r, K(pal::g12), rd); }
  Box(r, on ? K(tone, 0.15f) : K(pal::g1c), on ? K(tone) : K(pal::g22), rd);
}

static void Label(float x, float cy, const char* t, uint32_t hex = pal::t88) {
  Text(x, cy, MONO_R, 9, K(hex), Upper(t).c_str(), 0.09f);
}

void DrawSettings(ImVec2 disp) {
  static bool prevOpen = false;
  bool fresh = A.settingsOpen && !prevOpen;
  prevOpen = A.settingsOpen;
  if (!A.settingsOpen) return;
  ImGuiIO& io = ImGui::GetIO();
  bool click = io.MouseClicked[0] && !fresh;
  Prefs& p = A.prefs;
  const Tx& t = TX[p.lang];

  Fill(ImRect(0, 0, disp.x, disp.y), K(0x050505, 0.72f));
  float W = std::min(748.f, disp.x - 40), H = std::min(496.f, disp.y - 40);
  ImRect win((disp.x - W) * 0.5f, (disp.y - H) * 0.5f, (disp.x + W) * 0.5f, (disp.y + H) * 0.5f);
  for (int k = 6; k >= 1; --k) g.dl->AddRectFilled(ImVec2(win.Min.x - 8 * k, win.Min.y - 8 * k + 12), ImVec2(win.Max.x + 8 * k, win.Max.y + 8 * k + 12), Ca(K(0, 0.09f)), 4 + 8 * k);
  Box(win, K(pal::g12), K(pal::g2a), 4);
  if (click && !win.Contains(io.MousePos)) A.settingsOpen = false;
  g.dl->PushClipRect(win.Min, win.Max, true);

  // title bar
  ImRect tb(win.Min.x + 1, win.Min.y + 1, win.Max.x - 1, win.Min.y + 29);
  Fill(tb, K(pal::g1c)); HLine(tb.Min.x, tb.Max.x, tb.Max.y - 1, K(pal::g2a));
  float cy = (tb.Min.y + tb.Max.y - 1) * 0.5f;
  Icon("settings", ImVec2(tb.Min.x + 8 + 5.5f, cy), 11, K(pal::coral));
  Text(tb.Min.x + 8 + 11 + 6, cy, UI_B, 9, K(pal::tf3), Upper(t.title).c_str(), 0.14f);
  {
    ImRect xb(tb.Max.x - 6 - 20, cy - 10, tb.Max.x - 6, cy + 10);
    bool hv = RawHover(xb);
    if (hv) Fill(xb, K(pal::g1c), 3);
    Icon("x", ImVec2((xb.Min.x + xb.Max.x) * 0.5f, cy), 11, K(hv ? pal::red : pal::t88));
    if (hv && click) A.settingsOpen = false;
    char sum[32]; snprintf(sum, sizeof sum, "%s \xC2\xB7 %d%%", LANGS[p.lang].code, p.scale);
    TextR(xb.Min.x - 6, cy, MONO_R, 9, K(pal::t66), sum);
  }

  // footer
  ImRect ft(win.Min.x + 1, win.Max.y - 37, win.Max.x - 1, win.Max.y - 1);
  Fill(ft, K(pal::g18)); HLine(ft.Min.x, ft.Max.x, ft.Min.y, K(pal::g2a));
  float fcy = (ft.Min.y + ft.Max.y) * 0.5f;
  Text(ft.Min.x + 8, fcy, UI_S, 10, K(pal::t66), t.applyNote);
  {
    float dw = ButtonW(t.done, 1), rw = ButtonW(t.reset, 1);
    // buttons use HitR (window-hover based); overlays run on the foreground list, so gate manually
    auto btn = [&](ImRect r, const char* lab, Tone tone, bool active) {
      bool hv = RawHover(r);
      uint32_t hex = ToneHex(tone);
      if (active) { Glow(r, hex, 0.3f, 12, 3); Fill(r, K(pal::g12), 3); }
      Box(r, active ? K(hex, 0.15f) : K(pal::g1c), active ? K(hex) : hv ? K(pal::g33) : K(pal::g22), 3);
      TextC((r.Min.x + r.Max.x) * 0.5f, (r.Min.y + r.Max.y) * 0.5f, UI_B, 9, K(active ? hex : hv ? pal::white : pal::t77), Upper(lab).c_str(), 0.09f);
      if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      return hv && click;
    };
    if (btn(ImRect(ft.Max.x - 8 - dw, fcy - 10, ft.Max.x - 8, fcy + 10), t.done, T_LIVE, true)) A.settingsOpen = false;
    if (btn(ImRect(ft.Max.x - 8 - dw - 6 - rw, fcy - 10, ft.Max.x - 8 - dw - 6, fcy + 10), t.reset, T_NEUTRAL, false)) { A.prefs = Prefs(); A.topBandPx = 0; ApplyPrefs(); }
  }

  // sidebar
  ImRect sb(win.Min.x + 1, tb.Max.y, win.Min.x + 1 + 178, ft.Min.y);
  Fill(sb, K(pal::g14)); VLine(sb.Max.x - 1, sb.Min.y, sb.Max.y, K(pal::g2a));
  const char* tabIcons[5] = {"languages", "type", "palette", "a-large-small", "frame"};
  for (int i = 0; i < 5; ++i) {
    ImRect tr(sb.Min.x, sb.Min.y + 6 + i * 26, sb.Max.x - 1, sb.Min.y + 6 + i * 26 + 26);
    bool on = A.setTab == i, hv = RawHover(tr);
    if (on) Fill(tr, K(pal::g18));
    if (on) Fill(Rc(tr.Min.x, tr.Min.y, 2, 26), K(pal::coral));
    ImU32 fg = K(on ? pal::coral : pal::t88);
    float ty = (tr.Min.y + tr.Max.y) * 0.5f;
    Icon(tabIcons[i], ImVec2(tr.Min.x + 8 + 6, ty), 12, fg);
    TextEll(tr.Min.x + 8 + 12 + 6, ty, tr.GetWidth() - 40, UI_B, 10, fg, Upper(t.tabs[i]).c_str(), 0.09f);
    if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    if (hv && click) A.setTab = i;
  }
  Text(sb.Min.x + 8, sb.Max.y - 22, MONO_R, 9, K(pal::t66), "MIKMAP ENGINE", 0.09f);
  Text(sb.Min.x + 8, sb.Max.y - 10, MONO_R, 9, K(pal::t66), "v1.0.0 \xC2\xB7 prefs local");

  // content
  ImRect ct(sb.Max.x, tb.Max.y, win.Max.x - 1, ft.Min.y);
  g.dl->PushClipRect(ct.Min, ct.Max, true);
  float x = ct.Min.x + 8, w = ct.GetWidth() - 16, y = ct.Min.y + 8;
  auto wrapNote = [&](const char* s) {
    for (auto& ln : Wrap(UI_S, 10, s, w)) { Text(x, y + 8, UI_S, 10, K(pal::t66), ln.c_str(), 0.01f); y += 16; }
  };
  if (A.setTab == 0) {
    Label(x, y + 5, t.langHead); y += 9 + 6;
    for (int i = 0; i < 5; ++i) {
      ImRect r(x, y, x + w, y + 34);
      bool on = p.lang == i, hv = RawHover(r);
      Skin(r, on, pal::coral);
      float ly = y + 17;
      uint32_t fgHex = on ? pal::coral : pal::tcc;
      Text(x + 6, ly, MONO_B, 10, K(fgHex), LANGS[i].code);
      Text(x + 6 + 26 + 8, ly, UI_B, 12, K(fgHex), LANGS[i].native);
      float xr = r.Max.x - 6;
      if (on) { Check(ImVec2(xr - 6, ly), 12, K(pal::coral)); }
      xr -= 12 + 8;
      TextR(xr, ly, MONO_R, 9, K(pal::t66), LANGS[i].region);
      if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      if (hv && click) { p.lang = i; }
      y += 34 + 6;
    }
    wrapNote(t.langNote);
  } else if (A.setTab == 1) {
    Label(x, y + 5, t.fontUi); y += 9 + 6;
    float cw = (w - 6) / 2.f;
    for (int i = 0; i < 4; ++i) {
      ImRect r(x + (i % 2) * (cw + 6), y + (i / 2) * (40 + 6), x + (i % 2) * (cw + 6) + cw, y + (i / 2) * (40 + 6) + 40);
      bool on = p.ui == i, hv = RawHover(r);
      Skin(r, on, pal::cyan);
      ImFont* save = g.fonts[UI_B]; g.fonts[UI_B] = famUI[i][2];
      Text(r.Min.x + 6, r.Min.y + 6 + 8, UI_B, 13, K(on ? pal::cyan : pal::tcc), "LIVE OUTPUT", 0.09f);
      g.fonts[UI_B] = save;
      Text(r.Min.x + 6, r.Min.y + 6 + 16 + 3 + 5, MONO_R, 9, K(pal::t66), UI_NAMES[i]);
      if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      if (hv && click) { p.ui = i; ApplyPrefs(); }
    }
    y += 2 * 40 + 6 + 16;
    Label(x, y + 5, t.fontMono); y += 9 + 6;
    float mw = (w - 12) / 3.f;
    for (int i = 0; i < 3; ++i) {
      ImRect r(x + i * (mw + 6), y, x + i * (mw + 6) + mw, y + 38);
      bool on = p.mono == i, hv = RawHover(r);
      Skin(r, on, pal::cyan);
      ImFont* save = g.fonts[MONO_M]; g.fonts[MONO_M] = famMono[i][1];
      Text(r.Min.x + 6, r.Min.y + 6 + 7, MONO_M, 11, K(on ? pal::cyan : pal::tcc), "1920\xC3\x97" "1080 59.94");
      g.fonts[MONO_M] = save;
      Text(r.Min.x + 6, r.Min.y + 6 + 14 + 3 + 5, MONO_R, 9, K(pal::t66), MONO_NAMES[i]);
      if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      if (hv && click) { p.mono = i; ApplyPrefs(); }
    }
  } else if (A.setTab == 2) {
    Label(x, y + 5, t.accent); y += 9 + 6;
    for (int i = 0; i < 4; ++i) {
      ImRect r(x, y, x + w, y + 32);
      bool on = p.accent == i, hv = RawHover(r);
      Skin(r, on, ACC[i].c1);
      float ly = y + 16;
      uint32_t cs[3] = {ACC[i].c1, ACC[i].c2, ACC[i].c3};
      for (int k = 0; k < 3; ++k) g.dl->AddCircleFilled(ImVec2(x + 6 + 5 + k * 14, ly), 5, Ca(K(cs[k])), 16);
      float tx = x + 6 + 3 * 10 + 2 * 4 + 8;
      Text(tx, ly, UI_B, 12, K(on ? ACC[i].c1 : pal::tcc), ACC[i].name);
      float xr = r.Max.x - 6;
      if (on) Check(ImVec2(xr - 6, ly), 12, K(ACC[i].c1));
      xr -= 12 + 8;
      char hx[16]; snprintf(hx, sizeof hx, "#%06X", ACC[i].c1);
      TextR(xr, ly, MONO_R, 9, K(pal::t66), hx);
      if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      if (hv && click) { p.accent = i; ApplyPrefs(); }
      y += 32 + 6;
    }
    y += 6;
    Label(x, y + 5, t.surface); y += 9 + 6;
    float sw = (w - 12) / 3.f;
    for (int i = 0; i < 3; ++i) {
      ImRect r(x + i * (sw + 6), y, x + i * (sw + 6) + sw, y + 6 + 16 + 4 + 9 + 6);
      bool on = p.surface == i, hv = RawHover(r);
      Skin(r, on, pal::coral);
      ImRect strip(r.Min.x + 6, r.Min.y + 6, r.Max.x - 6, r.Min.y + 22);
      float th = strip.GetWidth() / 3.f;
      const uint32_t* rp = SURF[i].ramp;
      Fill(ImRect(strip.Min.x, strip.Min.y, strip.Min.x + th, strip.Max.y), K(rp[1]));
      Fill(ImRect(strip.Min.x + th, strip.Min.y, strip.Min.x + 2 * th, strip.Max.y), K(rp[3]));
      Fill(ImRect(strip.Min.x + 2 * th, strip.Min.y, strip.Max.x, strip.Max.y), K(rp[7]));
      Border(strip, K(pal::g2a), 2);
      Text(r.Min.x + 6, r.Min.y + 6 + 16 + 4 + 4.5f, MONO_R, 9, K(on ? pal::coral : pal::tcc), Upper(SURF[i].name).c_str(), 0.09f);
      if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      if (hv && click) { p.surface = i; ApplyPrefs(); }
    }
  } else if (A.setTab == 3) {
    Label(x, y + 5, t.sizeHead); y += 9 + 8;
    float zw = (w - 24) / 5.f;
    for (int i = 0; i < 5; ++i) {
      ImRect r(x + i * (zw + 6), y, x + i * (zw + 6) + zw, y + 6 + 13 + 2 + 12 + 6);
      bool on = p.scale == SIZES[i], hv = RawHover(r);
      Skin(r, on, pal::coral);
      char pc[8]; snprintf(pc, sizeof pc, "%d%%", SIZES[i]);
      TextC((r.Min.x + r.Max.x) * 0.5f, r.Min.y + 6 + 7, MONO_B, 13, K(on ? pal::coral : pal::tcc), pc);
      TextC((r.Min.x + r.Max.x) * 0.5f, r.Min.y + 6 + 13 + 2 + 6, UI_S, 10, K(pal::t66), Upper(t.sizes[i]).c_str(), 0.09f);
      if (hv) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
      if (hv && click) p.scale = SIZES[i];
    }
    y += 6 + 13 + 2 + 12 + 6 + 8;
    ImRect pv(x, y, x + w, y + 8 + 9 + 6 + 14 + 8);
    Box(pv, K(pal::g050), K(pal::g2a), 3);
    Label(x + 8, pv.Min.y + 8 + 4.5f, t.preview, pal::t66);
    Text(x + 8, pv.Min.y + 8 + 9 + 6 + 7, UI_B, 11, K(pal::coral), "LIVE OUTPUT", 0.09f);
    float lw = TextW(UI_B, 11, "LIVE OUTPUT", 0.09f);
    Text(x + 8 + lw + 8, pv.Min.y + 8 + 9 + 6 + 7, MONO_R, 10, K(pal::tcc), "1920\xC3\x97" "1080 \xC2\xB7 59.94 fps \xC2\xB7 128.0 BPM");
    y = pv.Max.y + 8;
    wrapNote(t.sizeNote);
  } else {
    Label(x, y + 5, t.layHead); y += 9 + 8;
    auto layRow = [&](const char* lab, const char* val, float& v, float mn, float mx, uint32_t id) {
      Text(x, y + 5, UI_B, 9, K(pal::coral), Upper(lab).c_str(), 0.09f);
      TextR(x + w, y + 5, MONO_B, 10, K(pal::tf3), val);
      y += 10 + 6;
      Slider(id, Rc(x, y + 4, w, 6), v, pal::coral, mn, mx);
      y += 14 + 8;
    };
    char vb[16];
    float v = (float)p.browserW;
    snprintf(vb, sizeof vb, "%d px", p.browserW);
    layRow(t.layBrowser, vb, v, 140, 320, 0x3001);
    p.browserW = std::clamp((int)std::round(v), 140, 320);
    v = (float)p.inspectorW;
    snprintf(vb, sizeof vb, "%d px", p.inspectorW);
    layRow(t.layInspector, vb, v, 180, 360, 0x3002);
    p.inspectorW = std::clamp((int)std::round(v), 180, 360);
    v = (float)p.bandPct;
    snprintf(vb, sizeof vb, "%d %%", p.bandPct);
    layRow(t.layBand, vb, v, 25, 70, 0x3003);
    if ((int)std::round(v) != p.bandPct) { p.bandPct = std::clamp((int)std::round(v), 25, 70); A.topBandPx = p.bandPct / 100.f * disp.y; }
    v = (float)p.timelineH;
    snprintf(vb, sizeof vb, "%d px", p.timelineH);
    layRow(t.layTimeline, vb, v, 0, 96, 0x3004);
    p.timelineH = std::clamp((int)std::round(v), 0, 96);
    y += 2;
    wrapNote(t.layNote);
  }
  g.dl->PopClipRect();
  g.dl->PopClipRect();
  g.blocked = true;
  if (ImGui::IsKeyPressed(ImGuiKey_Escape)) A.settingsOpen = false;
}

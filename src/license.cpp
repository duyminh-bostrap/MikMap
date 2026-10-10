// License gate + watermark. The decision itself is engine/core/license (pure, tested); this file only holds the
// current answer where the render thread can read it for free, and draws the watermark when there is no licence.
//
// For now nothing real feeds it: the answer is "no licence" unless the --license test flag forces a state. The web
// check (activate / release a key online) will call LicenseApply() from its own thread; the render thread only ever
// reads the three bits below, so it never locks, allocates or touches the network (principle #1).
#include "app.h"

#include "core/license/License.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <ctime>

#ifndef BUILD_DATE
#define BUILD_DATE ""   // not built through src/CMakeLists.txt: unknown, so a time-limited licence is not trusted
#endif

using namespace ui;

namespace {
enum : unsigned { kLicensed = 1u, kSensorOut = 2u, kNdiOut = 4u };
std::atomic<unsigned> gBits{0};   // 0 = no licence: the safe default
}  // namespace

void LicenseApply(const mikmap::license::Claims& c, int64_t serverNow) {
  int ymd = 0;
  mikmap::license::parseBuildDate(BUILD_DATE, ymd);   // stays 0 on failure = "unknown"
  const mikmap::license::Entitlements e = mikmap::license::evaluate(c, ymd, serverNow);
  unsigned bits = 0;
  if (e.licensed()) bits |= kLicensed;
  if (e.sensorOut) bits |= kSensorOut;
  if (e.ndiOut) bits |= kNdiOut;
  gBits.store(bits, std::memory_order_relaxed);
}

void LicenseForce(const std::string& stateName) {
  mikmap::license::Claims c;
  c.state = mikmap::license::stateFromName(stateName.c_str());
  LicenseApply(c, (int64_t)std::time(nullptr));
}

bool LicenseWatermark() { return (gBits.load(std::memory_order_relaxed) & kLicensed) == 0; }
bool LicenseSensorOut() { return (gBits.load(std::memory_order_relaxed) & kSensorOut) != 0; }
bool LicenseNdiOut() { return (gBits.load(std::memory_order_relaxed) & kNdiOut) != 0; }

// Sparse, staggered tiles of the app mark + name over the whole `area`. Drawn on top of everything the area shows, never inside a
// slice: a watermark inside a slice would be warped, cut by that slice's masks and disappear with it. Sizes follow the area so it
// reads the same at 720p and 4K; the floor keeps it legible in the small Live Output monitor.
void DrawWatermark(ImRect a) {
  const float W = a.GetWidth(), H = a.GetHeight();
  if (W < 8.f || H < 8.f || !g.dl) return;
  const float u = std::max(0.25f, std::min(W / 1920.f, H / 1080.f));
  const float tileW = 560.f * u, tileH = 300.f * u, logoPx = 44.f * u, gap = 12.f * u;
  const float nameSz = 30.f * u / kTextScale, tagSz = 13.f * u / kTextScale;   // pixels wanted / kTextScale: Text() multiplies it back
  const float alpha = 0.22f;
  const unsigned logo = LogoTexture();
  const float nameW = TextW(UI_X, nameSz, "MIKMAP", 0.09f);
  const float blockW = (logo ? logoPx + gap : 0.f) + nameW;

  g.dl->PushClipRect(a.Min, a.Max, true);
  int row = 0;
  for (float cy = a.Min.y + tileH * 0.5f; cy < a.Max.y + tileH * 0.5f; cy += tileH, ++row) {
    const float shift = (row & 1) ? tileW * 0.5f : 0.f;
    for (float cx = a.Min.x + tileW * 0.5f - shift; cx < a.Max.x + tileW * 0.5f; cx += tileW) {
      float x = cx - blockW * 0.5f;
      if (logo) {
        g.dl->AddImage((ImTextureID)(intptr_t)logo, ImVec2(x, cy - logoPx * 0.5f - 6.f * u), ImVec2(x + logoPx, cy + logoPx * 0.5f - 6.f * u),
                       ImVec2(0, 0), ImVec2(1, 1), IM_COL32(255, 255, 255, (int)(alpha * 255.f)));
        x += logoPx + gap;
      }
      Text(x, cy - 6.f * u, UI_X, nameSz, K(0xffffff, alpha), "MIKMAP", 0.09f);
      Text(cx - TextW(MONO_B, tagSz, "UNLICENSED", 0.18f) * 0.5f, cy + logoPx * 0.5f + 4.f * u, MONO_B, tagSz, K(0xffffff, alpha), "UNLICENSED", 0.18f);
    }
  }
  g.dl->PopClipRect();
}

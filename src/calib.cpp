// G5: homography solver (DLT with Hartley normalisation + RANSAC outlier rejection) + G9: frame-time statistics.
// Pure math, no GL — the sensor→projector mapping the calibration wizard produces.
#include "app.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// ── Gaussian elimination with partial pivoting, n<=8 ──
static bool Solve(double* m, int n, double* x) {
  for (int c = 0; c < n; ++c) {
    int piv = c;
    for (int r = c + 1; r < n; ++r) if (std::fabs(m[r * (n + 1) + c]) > std::fabs(m[piv * (n + 1) + c])) piv = r;
    if (std::fabs(m[piv * (n + 1) + c]) < 1e-12) return false;
    if (piv != c) for (int k = c; k <= n; ++k) std::swap(m[c * (n + 1) + k], m[piv * (n + 1) + k]);
    double d = m[c * (n + 1) + c];
    for (int k = c; k <= n; ++k) m[c * (n + 1) + k] /= d;
    for (int r = 0; r < n; ++r) {
      if (r == c) continue;
      double f = m[r * (n + 1) + c];
      if (f == 0) continue;
      for (int k = c; k <= n; ++k) m[r * (n + 1) + k] -= f * m[c * (n + 1) + k];
    }
  }
  for (int i = 0; i < n; ++i) x[i] = m[i * (n + 1) + n];
  return true;
}

// Similarity transform that centres the points and scales the mean distance to sqrt(2) (Hartley).
struct Norm { double cx = 0, cy = 0, s = 1; };
static Norm Normalise(const std::vector<std::pair<double, double>>& p) {
  Norm n;
  for (auto& q : p) { n.cx += q.first; n.cy += q.second; }
  n.cx /= p.size(); n.cy /= p.size();
  double d = 0;
  for (auto& q : p) d += std::hypot(q.first - n.cx, q.second - n.cy);
  d /= p.size();
  n.s = d > 1e-9 ? std::sqrt(2.0) / d : 1.0;
  return n;
}

// Plain DLT least-squares fit over exactly the given points (no outlier rejection).
static bool FitDLT(const std::vector<Calib>& pts, float H[9]) {
  for (int i = 0; i < 9; ++i) H[i] = (i % 4 == 0) ? 1.f : 0.f;
  if (pts.size() < 4) return false;

  std::vector<std::pair<double, double>> src, dst;
  for (auto& p : pts) { src.push_back({p.mx, p.my}); dst.push_back({p.tx, p.ty}); }
  Norm ns = Normalise(src), nd = Normalise(dst);

  const int n = (int)pts.size();
  // 2n equations, 8 unknowns — least squares via normal equations when n > 4.
  std::vector<double> ata(8 * 9, 0.0);
  for (int i = 0; i < n; ++i) {
    double x = (src[i].first - ns.cx) * ns.s, y = (src[i].second - ns.cy) * ns.s;
    double u = (dst[i].first - nd.cx) * nd.s, v = (dst[i].second - nd.cy) * nd.s;
    // h0x + h1y + h2 - u(h6x + h7y + 1) = 0, so the constant column carries -u (and -v).
    double r1[9] = {x, y, 1, 0, 0, 0, -u * x, -u * y, -u};
    double r2[9] = {0, 0, 0, x, y, 1, -v * x, -v * y, -v};
    for (int a = 0; a < 8; ++a)
      for (int b = 0; b < 9; ++b)
        ata[a * 9 + b] += r1[a] * r1[b] + r2[a] * r2[b];
  }
  // move the constant column to the right-hand side: A'A h = A'b, with b the 9th column
  for (int a = 0; a < 8; ++a) ata[a * 9 + 8] = -ata[a * 9 + 8];
  double h[8];
  if (!Solve(ata.data(), 8, h)) return false;

  // undo normalisation: H = Td^-1 * Hn * Ts
  double Hn[9] = {h[0], h[1], h[2], h[3], h[4], h[5], h[6], h[7], 1.0};
  double Ts[9] = {ns.s, 0, -ns.s * ns.cx, 0, ns.s, -ns.s * ns.cy, 0, 0, 1};
  double Di[9] = {1 / nd.s, 0, nd.cx, 0, 1 / nd.s, nd.cy, 0, 0, 1};
  double tmp[9], out[9];
  for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) {
    double s = 0; for (int k = 0; k < 3; ++k) s += Hn[r * 3 + k] * Ts[k * 3 + c];
    tmp[r * 3 + c] = s;
  }
  for (int r = 0; r < 3; ++r) for (int c = 0; c < 3; ++c) {
    double s = 0; for (int k = 0; k < 3; ++k) s += Di[r * 3 + k] * tmp[k * 3 + c];
    out[r * 3 + c] = s;
  }
  if (std::fabs(out[8]) > 1e-12) for (int i = 0; i < 9; ++i) out[i] /= out[8];
  for (int i = 0; i < 9; ++i) H[i] = (float)out[i];
  return true;
}

static float PointErr(const float H[9], const Calib& p) {
  float ox, oy;
  ApplyH(H, p.mx, p.my, ox, oy);
  return std::hypot(ox - p.tx, oy - p.ty);
}

// Fits H so that H * measured ≈ target (homogeneous, H[8] normalised to 1).
// Needs 4+ correspondences; returns false and leaves H as identity when the points are degenerate.
// With 6+ points a RANSAC pass (fixed seed → deterministic) discards gross outliers such as a
// mis-clicked calibration point, then refits on the inliers; `rms` is the mean error over the points used.
bool FitHomography(const std::vector<Calib>& pts, float H[9], float* rms) {
  for (int i = 0; i < 9; ++i) H[i] = (i % 4 == 0) ? 1.f : 0.f;
  if (rms) *rms = 0;
  const int n = (int)pts.size();
  if (n < 4) return false;

  std::vector<Calib> use = pts;
  if (n >= 6) {
    float minx = 1e30f, maxx = -1e30f, miny = 1e30f, maxy = -1e30f;
    for (auto& p : pts) {
      minx = std::min(minx, p.tx); maxx = std::max(maxx, p.tx);
      miny = std::min(miny, p.ty); maxy = std::max(maxy, p.ty);
    }
    const float thr = std::max(1.f, 0.005f * std::hypot(maxx - minx, maxy - miny));
    uint32_t rng = 0x9E3779B9u;
    auto next = [&] { rng = rng * 1664525u + 1013904223u; return (int)((rng >> 8) % (uint32_t)n); };
    std::vector<int> best;
    for (int it = 0; it < 200; ++it) {
      int idx[4];
      for (int k = 0; k < 4;) {
        int c = next(); bool dup = false;
        for (int j = 0; j < k; ++j) dup |= idx[j] == c;
        if (!dup) idx[k++] = c;
      }
      std::vector<Calib> sample;
      for (int k : idx) sample.push_back(pts[k]);
      float h[9];
      if (!FitDLT(sample, h)) continue;
      std::vector<int> in;
      for (int i = 0; i < n; ++i) if (PointErr(h, pts[i]) < thr) in.push_back(i);
      if (in.size() > best.size()) best = in;
      if ((int)best.size() == n) break;
    }
    if (best.size() >= 4 && (int)best.size() < n) {
      use.clear();
      for (int i : best) use.push_back(pts[i]);
    }
  }

  if (!FitDLT(use, H)) return false;
  if (rms) {
    double e = 0;
    for (auto& p : use) e += PointErr(H, p);
    *rms = (float)(e / use.size());
  }
  return true;
}

void ApplyH(const float H[9], float x, float y, float& ox, float& oy) {
  float w = H[6] * x + H[7] * y + H[8];
  if (std::fabs(w) < 1e-9f) w = 1e-9f;
  ox = (H[0] * x + H[1] * y + H[2]) / w;
  oy = (H[3] * x + H[4] * y + H[5]) / w;
}

// H_s of one device: homography of its 4 points, affine when they are degenerate. Raw sensor coordinates only ever
// meet this matrix where they are used (principle #5), never on the sensor thread.
bool DeviceH(const Device& d, float H[9]) {
  if (!d.calibrated || d.calib.size() != 4) return false;
  float rms; if (!FitHomography(d.calib, H, &rms)) FitAffine(d.calib, H, &rms);
  return true;
}

Screen* DeviceScreen(const Device& d, bool* fellBack) {
  if (fellBack) *fellBack = false;
  if (A.screens.empty()) return nullptr;
  for (auto& s : A.screens) if (s.id == d.screenId) return &s;
  if (fellBack) *fellBack = !d.screenId.empty();
  return &A.screens[0];
}

// The four screen corners in mm -> where the lidar sees them; paired with the four output-pixel corners this is an ordinary H_s,
// so everything downstream (DeviceH, touch mapping) is unchanged.
bool PlacementCalib(Device& d, const Screen& sc, const mikmap::LidarPose& pose) {
  if (sc.realW <= 0 || sc.realH <= 0) return false;
  const float W = 1920, H = 1080;   // calib tx,ty are always stage space (1920x1080), whatever sc.w/sc.h the display reports
  const float tx[4] = {0, W, W, 0}, ty[4] = {0, 0, H, H};
  const mikmap::Vec2 corner[4] = {{0, 0}, {sc.realW, 0}, {sc.realW, sc.realH}, {0, sc.realH}};
  d.calib.clear();
  for (int i = 0; i < 4; ++i) {
    const mikmap::Vec2 m = mikmap::surfaceToLidar(pose, corner[i]);
    d.calib.push_back({tx[i], ty[i], (float)m.x, (float)m.y});
  }
  d.calibrated = true;
  return true;
}

bool ApplyPlacement(Device& d) {
  if (!d.placed) return false;   // "Custom (4-point)": the hand-tuned quad wins until the user presses Re-apply placement
  bool fb = false; const Screen* sc = DeviceScreen(d, &fb);
  if (fb) return false;          // its screen was deleted: keep the calibration it has rather than re-derive it on the fallback screen
  return sc && PlacementCalib(d, *sc, d.pose);
}

bool QuadEdit(Device& d, const ImVec2 q[4]) {
  constexpr float eps = 0.01f;   // a Transform click that moves nothing still round-trips the points by ~1e-4 px
  if (d.calib.size() != 4) return false;
  bool moved = false;
  for (int i = 0; i < 4; ++i) moved = moved || std::fabs(q[i].x - d.calib[i].tx) > eps || std::fabs(q[i].y - d.calib[i].ty) > eps;
  if (!moved) return false;
  for (int i = 0; i < 4; ++i) { d.calib[i].tx = q[i].x; d.calib[i].ty = q[i].y; }
  d.placed = false;   // a hand edit is Custom (4-point) from the first real move, so the placement never pulls the point back mid-drag
  return true;
}

bool RoiToScreen(Device& d) {
  const Screen* sc = DeviceScreen(d);
  if (!sc || sc->realW <= 0 || sc->realH <= 0) return false;
  const mikmap::Vec2 corner[4] = {{0, 0}, {sc->realW, 0}, {sc->realW, sc->realH}, {0, sc->realH}};
  for (int i = 0; i < 4; ++i) { const mikmap::Vec2 m = mikmap::surfaceToLidar(d.pose, corner[i]); d.roi[i] = ImVec2((float)m.x, (float)m.y); }
  return true;
}

Device MakeDevice(DevKind k, const std::string& id) {
  Device d; d.id = id; d.kind = k;
  const float tx[4] = {100, 1820, 1820, 100}, ty[4] = {100, 100, 980, 980};   // the same output-pixel targets for every kind
  for (int i = 0; i < 4; ++i) d.calib.push_back({tx[i], ty[i], 0, 0});
  if (k == DevKind::Sim) {
    d.name = "Simulated radar"; d.calibrated = true;
    const float mx[4] = {-0.65f, 0.65f, 0.75f, -0.75f}, my[4] = {-0.55f, -0.55f, 0.65f, 0.65f};
    for (int i = 0; i < 4; ++i) { d.calib[i].mx = mx[i]; d.calib[i].my = my[i]; }
    d.roi[0] = {-0.7f, -0.6f}; d.roi[1] = {0.7f, -0.6f}; d.roi[2] = {0.85f, 0.7f}; d.roi[3] = {-0.85f, 0.7f};
  } else if (k == DevKind::Skeleton) {
    d.name = "Camera skeleton"; d.port = 9000;
  } else if (k == DevKind::Hokuyo) {
    d.name = "Hokuyo 2D lidar"; d.host = "192.168.0.10"; d.port = 10940;
    d.roi[0] = {3000, 1500}; d.roi[1] = {3000, -1500}; d.roi[2] = {500, -1500}; d.roi[3] = {500, 1500};   // mm
  } else {   // Plugin: a lidar until the DLL says otherwise; the menu fills plugin/config from the scanned ABI
    d.name = "Plugin";
    d.roi[0] = {3000, 1500}; d.roi[1] = {3000, -1500}; d.roi[2] = {500, -1500}; d.roi[3] = {500, 1500};   // mm
  }
  return d;
}

// ── G9: frame-time statistics ──
static float gFrames[240];
static int gCount = 0, gHead = 0;

void PerfPush(float ms) {
  gFrames[gHead] = ms;
  gHead = (gHead + 1) % 240;
  if (gCount < 240) ++gCount;
}
float PerfFps() {
  if (!gCount) return 0;
  float s = 0;
  for (int i = 0; i < gCount; ++i) s += gFrames[i];
  return s > 0 ? gCount * 1000.f / s : 0;
}
float PerfP99() {
  if (!gCount) return 0;
  std::vector<float> v(gFrames, gFrames + gCount);
  std::sort(v.begin(), v.end());
  return v[std::min(gCount - 1, (int)(gCount * 0.99f))];
}
int PerfDrops(float budgetMs) {
  int d = 0;
  for (int i = 0; i < gCount; ++i) if (gFrames[i] > budgetMs) ++d;
  return d;
}

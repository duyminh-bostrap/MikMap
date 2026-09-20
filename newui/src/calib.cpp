// G5: homography solver (DLT with Hartley normalisation) + G9: frame-time statistics.
// Pure math, no GL — the sensor→projector mapping the calibration wizard produces.
#include "app.h"
#include <algorithm>
#include <cmath>
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

// Fits H so that H * measured ≈ target (homogeneous, H[8] normalised to 1).
// Needs 4+ correspondences; returns false and leaves H as identity when the points are degenerate.
bool FitHomography(const std::vector<Calib>& pts, float H[9], float* rms) {
  for (int i = 0; i < 9; ++i) H[i] = (i % 4 == 0) ? 1.f : 0.f;
  if (rms) *rms = 0;
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

  if (rms) {
    double e = 0;
    for (auto& p : pts) {
      float ox, oy;
      ApplyH(H, p.mx, p.my, ox, oy);
      e += std::hypot(ox - p.tx, oy - p.ty);
    }
    *rms = (float)(e / pts.size());
  }
  return true;
}

void ApplyH(const float H[9], float x, float y, float& ox, float& oy) {
  float w = H[6] * x + H[7] * y + H[8];
  if (std::fabs(w) < 1e-9f) w = 1e-9f;
  ox = (H[0] * x + H[1] * y + H[2]) / w;
  oy = (H[3] * x + H[4] * y + H[5]) / w;
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

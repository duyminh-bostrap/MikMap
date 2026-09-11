#include "core/math/Homography.h"

#include "core/math/LinearSolver.h"

#include <algorithm>
#include <cmath>
#include <random>

namespace hexmap {
namespace homography {
namespace {

// ── Chuẩn hoá Hartley ──────────────────────────────────────────────────
// Đưa tập điểm về: centroid tại gốc toạ độ, khoảng cách trung bình tới
// gốc bằng √2. Trả về ma trận T thực hiện phép biến đổi đó.
//
// Không có bước này, việc trộn toạ độ sensor (vd 0..2000 mm) với toạ độ
// máy chiếu (0..3840 px) trong cùng một hệ 8×8 làm số điều kiện tăng vọt.
Mat3 computeNormalization(const std::vector<Vec2>& pts) {
    if (pts.empty()) return Mat3::identity();

    const double n = static_cast<double>(pts.size());

    Vec2 centroid{0.0, 0.0};
    for (const Vec2& p : pts) centroid += p;
    centroid = centroid / n;

    double meanDist = 0.0;
    for (const Vec2& p : pts) meanDist += (p - centroid).length();
    meanDist /= n;

    // Tất cả điểm trùng nhau → không chuẩn hoá được, và cũng không giải được.
    if (meanDist < 1e-12) return Mat3::identity();

    const double s = std::sqrt(2.0) / meanDist;

    return Mat3{s, 0.0, -s * centroid.x,
                0.0, s, -s * centroid.y,
                0.0, 0.0, 1.0};
}

/// Giải DLT có chuẩn hoá cho n ≥ 4 cặp điểm.
///   · n == 4 → giải trực tiếp hệ 8×8 (chính xác)
///   · n  > 4 → phương trình chuẩn tắc AᵀA·h = Aᵀb (bình phương tối thiểu)
///
/// Đặt h22 = 1 (dạng không thuần nhất). Hợp lệ với mọi homography thực tế
/// trong dựng hình chiếu — h22 = 0 chỉ xảy ra khi gốc toạ độ đích bị ánh
/// xạ ra vô cực, điều không xảy ra với slice hay sensor.
bool dltSolve(const std::vector<Vec2>& src, const std::vector<Vec2>& dst, Mat3& out) {
    const size_t n = src.size();
    if (n < 4 || dst.size() != n) return false;

    const Mat3 Tsrc = computeNormalization(src);
    const Mat3 Tdst = computeNormalization(dst);

    std::vector<Vec2> ns(n), nd(n);
    for (size_t i = 0; i < n; ++i) {
        ns[i] = Tsrc.transformPoint(src[i]);
        nd[i] = Tdst.transformPoint(dst[i]);
    }

    // Xây ma trận thiết kế A (2n × 8) và vế phải b (2n).
    //   [ x  y  1  0  0  0  -x·u  -y·u ] · h = u
    //   [ 0  0  0  x  y  1  -x·v  -y·v ] · h = v
    const size_t rows = 2 * n;
    std::vector<double> A(rows * 8, 0.0);
    std::vector<double> b(rows, 0.0);

    for (size_t i = 0; i < n; ++i) {
        const double x = ns[i].x, y = ns[i].y;
        const double u = nd[i].x, v = nd[i].y;

        double* r0 = &A[(2 * i) * 8];
        r0[0] = x;  r0[1] = y;  r0[2] = 1.0;
        r0[3] = 0.0; r0[4] = 0.0; r0[5] = 0.0;
        r0[6] = -x * u; r0[7] = -y * u;
        b[2 * i] = u;

        double* r1 = &A[(2 * i + 1) * 8];
        r1[0] = 0.0; r1[1] = 0.0; r1[2] = 0.0;
        r1[3] = x;  r1[4] = y;  r1[5] = 1.0;
        r1[6] = -x * v; r1[7] = -y * v;
        b[2 * i + 1] = v;
    }

    std::vector<double> h;

    if (n == 4) {
        // Hệ vuông 8×8 — giải trực tiếp, tránh bình phương số điều kiện.
        if (!solveLinearSystem(A, b, 8, h)) return false;
    } else {
        // Phương trình chuẩn tắc: AᵀA (8×8), Aᵀb (8).
        std::vector<double> AtA(64, 0.0);
        std::vector<double> Atb(8, 0.0);

        for (size_t r = 0; r < rows; ++r) {
            const double* row = &A[r * 8];
            for (int i = 0; i < 8; ++i) {
                Atb[static_cast<size_t>(i)] += row[i] * b[r];
                for (int j = 0; j < 8; ++j) {
                    AtA[static_cast<size_t>(i) * 8 + static_cast<size_t>(j)] += row[i] * row[j];
                }
            }
        }

        if (!solveLinearSystem(AtA, Atb, 8, h)) return false;
    }

    const Mat3 Hnorm{h[0], h[1], h[2],
                     h[3], h[4], h[5],
                     h[6], h[7], 1.0};

    // Khử chuẩn hoá:  H = T_dst⁻¹ · H_norm · T_src
    Mat3 TdstInv;
    if (!Tdst.invert(TdstInv)) return false;

    out = (TdstInv * Hnorm * Tsrc).normalized();
    return out.isFinite();
}

/// Diện tích có dấu của tam giác — dùng để kiểm tra thẳng hàng.
double triArea2(const Vec2& a, const Vec2& b, const Vec2& c) {
    return (b - a).cross(c - a);
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  API công khai
// ═══════════════════════════════════════════════════════════════════════

bool isValidQuad(const Vec2 quad[4], double minArea) {
    for (int i = 0; i < 4; ++i) {
        if (!quad[i].isFinite()) return false;
    }

    // Diện tích shoelace — bắt trường hợp suy biến hoàn toàn.
    double area2 = 0.0;
    for (int i = 0; i < 4; ++i) {
        const Vec2& p = quad[i];
        const Vec2& q = quad[(i + 1) % 4];
        area2 += p.cross(q);
    }
    if (std::abs(area2) * 0.5 < minArea) return false;

    // Kiểm tra lồi: tích có hướng của các cạnh liên tiếp phải cùng dấu.
    // Tứ giác "hình nơ" (bow-tie) do kéo góc chéo nhau sẽ bị loại ở đây —
    // nó không nghịch đảo được nên phải chặn ngay trong UI.
    int positive = 0;
    int negative = 0;
    for (int i = 0; i < 4; ++i) {
        const Vec2 e0 = quad[(i + 1) % 4] - quad[i];
        const Vec2 e1 = quad[(i + 2) % 4] - quad[(i + 1) % 4];
        const double cr = e0.cross(e1);
        if (cr > 1e-12)       ++positive;
        else if (cr < -1e-12) ++negative;
        else                  return false;   // ba điểm thẳng hàng
    }
    return (positive == 4) || (negative == 4);
}

HomographyResult solve4Point(const Vec2 src[4], const Vec2 dst[4]) {
    HomographyResult r;
    r.totalCount = 4;

    if (!isValidQuad(src)) { r.message = "Tu giac nguon suy bien hoac tu cat"; return r; }
    if (!isValidQuad(dst)) { r.message = "Tu giac dich suy bien hoac tu cat"; return r; }

    const std::vector<Vec2> s(src, src + 4);
    const std::vector<Vec2> d(dst, dst + 4);

    if (!dltSolve(s, d, r.H)) {
        r.message = "He phuong trinh suy bien";
        return r;
    }

    r.ok = true;
    r.inlierCount = 4;

    double worst = 0.0;
    double sumSq = 0.0;
    for (int i = 0; i < 4; ++i) {
        const double e = reprojectionError(r.H, src[i], dst[i]);
        sumSq += e * e;
        worst = std::max(worst, e);
    }
    r.rmsError = std::sqrt(sumSq / 4.0);
    r.maxError = worst;
    return r;
}

HomographyResult unitSquareToQuad(const Vec2 quad[4]) {
    const Vec2 unit[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
    return solve4Point(unit, quad);
}

HomographyResult solveLeastSquares(const std::vector<CorrespondencePair>& pairs) {
    HomographyResult r;

    std::vector<Vec2> src, dst;
    src.reserve(pairs.size());
    dst.reserve(pairs.size());
    for (const CorrespondencePair& p : pairs) {
        if (!p.enabled) continue;
        src.push_back(p.src);
        dst.push_back(p.dst);
    }

    r.totalCount = static_cast<int>(src.size());

    if (src.size() < 4) {
        r.message = "Can it nhat 4 diem tuong ung";
        return r;
    }

    if (!dltSolve(src, dst, r.H)) {
        r.message = "He phuong trinh suy bien (diem thang hang hoac trung nhau?)";
        return r;
    }

    r.ok = true;
    r.inlierCount = r.totalCount;
    evaluate(r.H, pairs, r.rmsError, r.maxError);
    return r;
}

HomographyResult solveRANSAC(const std::vector<CorrespondencePair>& pairs,
                             const RansacParams& params) {
    HomographyResult r;
    r.inliers.assign(pairs.size(), false);

    // Lập chỉ mục các cặp đang bật.
    std::vector<size_t> idx;
    idx.reserve(pairs.size());
    for (size_t i = 0; i < pairs.size(); ++i) {
        if (pairs[i].enabled) idx.push_back(i);
    }

    const size_t n = idx.size();
    r.totalCount = static_cast<int>(n);

    if (n < 4) {
        r.message = "Can it nhat 4 diem tuong ung";
        return r;
    }

    // Đúng 4 điểm thì không có gì để loại — giải trực tiếp.
    if (n == 4) {
        const Vec2 s[4] = {pairs[idx[0]].src, pairs[idx[1]].src,
                           pairs[idx[2]].src, pairs[idx[3]].src};
        const Vec2 d[4] = {pairs[idx[0]].dst, pairs[idx[1]].dst,
                           pairs[idx[2]].dst, pairs[idx[3]].dst};
        r = solve4Point(s, d);
        r.inliers.assign(pairs.size(), false);
        if (r.ok) for (size_t i : idx) r.inliers[i] = true;
        return r;
    }

    std::mt19937 rng(params.seed);
    std::uniform_int_distribution<size_t> pick(0, n - 1);

    Mat3   bestH;
    int    bestInliers = 0;
    bool   found       = false;
    int    iterations  = params.maxIterations;
    const double thr2  = params.inlierThreshold * params.inlierThreshold;

    for (int iter = 0; iter < iterations; ++iter) {
        // ── Lấy mẫu 4 chỉ số phân biệt ────────────────────────────────
        size_t a = pick(rng), b = pick(rng), c = pick(rng), d = pick(rng);
        if (a == b || a == c || a == d || b == c || b == d || c == d) continue;

        const Vec2 s[4] = {pairs[idx[a]].src, pairs[idx[b]].src,
                           pairs[idx[c]].src, pairs[idx[d]].src};
        const Vec2 t[4] = {pairs[idx[a]].dst, pairs[idx[b]].dst,
                           pairs[idx[c]].dst, pairs[idx[d]].dst};

        // Bỏ qua mẫu suy biến sớm — rẻ hơn nhiều so với giải rồi mới biết hỏng.
        if (std::abs(triArea2(s[0], s[1], s[2])) < 1e-9) continue;
        if (std::abs(triArea2(s[0], s[2], s[3])) < 1e-9) continue;

        std::vector<Vec2> sv(s, s + 4), tv(t, t + 4);
        Mat3 H;
        if (!dltSolve(sv, tv, H)) continue;

        // ── Đếm inlier ────────────────────────────────────────────────
        int count = 0;
        for (size_t i : idx) {
            const Vec2 proj = H.transformPoint(pairs[i].src);
            if (proj.distanceSqTo(pairs[i].dst) <= thr2) ++count;
        }

        if (count > bestInliers) {
            bestInliers = count;
            bestH       = H;
            found       = true;

            // ── Dừng sớm thích nghi ───────────────────────────────────
            // Khi tỉ lệ inlier w đã cao, số vòng lặp cần thiết để đạt
            // độ tin cậy mong muốn giảm rất nhanh:  N = log(1-p)/log(1-w⁴)
            const double w = static_cast<double>(count) / static_cast<double>(n);
            if (w > 0.0 && w < 1.0) {
                const double denom = std::log(1.0 - std::pow(w, 4.0));
                if (denom < -1e-12) {
                    const double needed = std::log(1.0 - params.confidence) / denom;
                    if (needed >= 0.0 && needed < static_cast<double>(iterations)) {
                        iterations = std::max(iter + 1, static_cast<int>(needed) + 1);
                    }
                }
            } else if (w >= 1.0) {
                iterations = iter + 1;   // toàn bộ là inlier, dừng ngay
            }
        }
    }

    if (!found || bestInliers < params.minInliers) {
        r.message = "RANSAC khong tim duoc mo hinh du inlier";
        return r;
    }

    // ── Tinh chỉnh: giải lại bằng least-squares trên TOÀN BỘ inlier ──
    // Mô hình từ 4 điểm chỉ dùng để phân loại inlier/outlier; lời giải
    // cuối cùng phải tận dụng mọi điểm tốt để giảm sai số.
    std::vector<CorrespondencePair> inlierPairs;
    inlierPairs.reserve(static_cast<size_t>(bestInliers));
    for (size_t i : idx) {
        const Vec2 proj = bestH.transformPoint(pairs[i].src);
        if (proj.distanceSqTo(pairs[i].dst) <= thr2) {
            r.inliers[i] = true;
            inlierPairs.push_back(pairs[i]);
        }
    }

    HomographyResult refined = solveLeastSquares(inlierPairs);
    if (!refined.ok) {
        // Hiếm gặp; giữ lại mô hình 4 điểm còn hơn là thất bại hoàn toàn.
        r.H  = bestH;
        r.ok = true;
    } else {
        r.H  = refined.H;
        r.ok = true;
    }

    r.inlierCount = bestInliers;
    evaluate(r.H, inlierPairs, r.rmsError, r.maxError);
    return r;
}

double reprojectionError(const Mat3& H, const Vec2& src, const Vec2& dst) {
    return H.transformPoint(src).distanceTo(dst);
}

void evaluate(const Mat3& H,
              const std::vector<CorrespondencePair>& pairs,
              double& outRms,
              double& outMax) {
    double sumSq = 0.0;
    double worst = 0.0;
    int    count = 0;

    for (const CorrespondencePair& p : pairs) {
        if (!p.enabled) continue;
        const double e = reprojectionError(H, p.src, p.dst);
        sumSq += e * e;
        worst = std::max(worst, e);
        ++count;
    }

    outRms = (count > 0) ? std::sqrt(sumSq / static_cast<double>(count)) : 0.0;
    outMax = worst;
}

} // namespace homography
} // namespace hexmap

#include "core/model/BezierMask.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace mikmap {
namespace {

/// Bezier bậc 3 tại tham số t.
Vec2 cubic(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, double t) {
    const double u = 1.0 - t;
    const double a = u * u * u;
    const double b = 3.0 * u * u * t;
    const double c = 3.0 * u * t * t;
    const double d = t * t * t;
    return {a * p0.x + b * p1.x + c * p2.x + d * p3.x,
            a * p0.y + b * p1.y + c * p2.y + d * p3.y};
}

double dist2(const Vec2& a, const Vec2& b) {
    const double dx = a.x - b.x;
    const double dy = a.y - b.y;
    return dx * dx + dy * dy;
}

/// Điểm gần nhất trên đoạn thẳng ab, trả về tham số t đã kẹp về [0,1].
double closestTOnSegment(const Vec2& a, const Vec2& b, const Vec2& p) {
    const double dx = b.x - a.x;
    const double dy = b.y - a.y;
    const double len2 = dx * dx + dy * dy;
    if (len2 <= 0.0) return 0.0;
    const double t = ((p.x - a.x) * dx + (p.y - a.y) * dy) / len2;
    return std::clamp(t, 0.0, 1.0);
}

/// Bộ đệm dùng lại giữa các lần gọi containsUV / boundsUV.
///
/// Hai hàm này bị gọi trong đường sensor (mỗi điểm chạm × mỗi slice, mỗi
/// frame). Cấp phát vector ở đó nghĩa là cấp phát trong hot path — điều
/// architecture.md §6 cấm ở render thread. thread_local để mỗi thread có
/// bộ đệm riêng, không phải khoá.
std::vector<Vec2>& scratch() {
    static thread_local std::vector<Vec2> buf;
    return buf;
}

/// Tia ngang, quy tắc CHẴN-LẺ. Chọn chẵn-lẻ chứ không phải nonzero để
/// hình tự cắt vẫn cho kết quả nhìn thấy được thay vì tô đặc bất ngờ.
bool pointInPolygon(const std::vector<Vec2>& poly, const Vec2& p) {
    if (poly.size() < 3) return false;
    bool inside = false;
    for (size_t i = 0, j = poly.size() - 1; i < poly.size(); j = i++) {
        const Vec2& a = poly[i];
        const Vec2& b = poly[j];
        if ((a.y > p.y) != (b.y > p.y)) {
            const double t = (p.y - a.y) / (b.y - a.y);
            if (p.x < a.x + t * (b.x - a.x)) inside = !inside;
        }
    }
    return inside;
}

void hashBytes(uint64_t& h, const void* data, size_t n) {
    // FNV-1a. Ở đây chỉ cần phát hiện THAY ĐỔI, không cần chống va chạm
    // có chủ đích, nên hàm băm đơn giản là đủ.
    const unsigned char* p = static_cast<const unsigned char*>(data);
    for (size_t i = 0; i < n; ++i) {
        h ^= static_cast<uint64_t>(p[i]);
        h *= 1099511628211ull;
    }
}

void hashDouble(uint64_t& h, double v) {
    // +0.0 và -0.0 có bit khác nhau nhưng bằng nhau về giá trị; chuẩn hoá
    // để kéo một điểm về 0 rồi thả không gây dựng lại texture vô ích.
    if (v == 0.0) v = 0.0;
    hashBytes(h, &v, sizeof(v));
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  Hình dựng sẵn
// ═══════════════════════════════════════════════════════════════════════

BezierMask BezierMask::rectangle(double inset) {
    inset = std::clamp(inset, 0.0, 0.45);
    const double a = inset;
    const double b = 1.0 - inset;

    BezierMask m;
    m.enabled = true;
    m.nodes.resize(4);
    m.nodes[0].point = Vec2{a, a};
    m.nodes[1].point = Vec2{b, a};
    m.nodes[2].point = Vec2{b, b};
    m.nodes[3].point = Vec2{a, b};
    return m;
}

BezierMask BezierMask::ellipse(int nodeCount) {
    nodeCount = std::clamp(nodeCount, 3, kMaxNodes);

    BezierMask m;
    m.enabled = true;
    m.nodes.resize(static_cast<size_t>(nodeCount));

    constexpr double kPi = 3.14159265358979323846;
    const double step = 2.0 * kPi / static_cast<double>(nodeCount);

    // Độ dài tay nắm để cung tròn `step` radian khớp gần đúng nhất với
    // bezier bậc 3. Với 4 nút cho ra hằng số kappa quen thuộc 0.5523.
    const double k = (4.0 / 3.0) * std::tan(step * 0.25);

    for (int i = 0; i < nodeCount; ++i) {
        const double th = step * static_cast<double>(i) - kPi * 0.5;
        const double cx = std::cos(th);
        const double sy = std::sin(th);

        // Bán kính 0.5 quanh tâm (0.5, 0.5) → elip nội tiếp ô UV.
        m.nodes[static_cast<size_t>(i)].point = Vec2{0.5 + 0.5 * cx, 0.5 + 0.5 * sy};

        // Tiếp tuyến của đường tròn tại góc th là (-sin, cos).
        const Vec2 tangent{-sy * 0.5 * k, cx * 0.5 * k};
        m.nodes[static_cast<size_t>(i)].outHandle = tangent;
        m.nodes[static_cast<size_t>(i)].inHandle  = Vec2{-tangent.x, -tangent.y};
    }
    return m;
}

// ═══════════════════════════════════════════════════════════════════════
//  Hình học
// ═══════════════════════════════════════════════════════════════════════

void BezierMask::flatten(std::vector<Vec2>& out, int segmentsPerCurve) const {
    out.clear();
    const size_t n = nodes.size();
    if (n < 2) {
        if (n == 1) out.push_back(nodes[0].point);
        return;
    }

    segmentsPerCurve = std::clamp(segmentsPerCurve, 1, 256);
    out.reserve(n * static_cast<size_t>(segmentsPerCurve));

    for (size_t i = 0; i < n; ++i) {
        const MaskNode& a = nodes[i];
        const MaskNode& b = nodes[(i + 1) % n];

        const Vec2 p0 = a.point;
        const Vec2 p1 = a.outPoint();
        const Vec2 p2 = b.inPoint();
        const Vec2 p3 = b.point;

        // Hai tay nắm bằng 0 nghĩa là đoạn THẲNG. Chia nhỏ nó ra chỉ tốn
        // đỉnh mà không chính xác thêm chút nào — và mặt nạ đa giác
        // (kiểu cắt theo mép tường) thì mọi đoạn đều thẳng.
        if (a.outHandle.x == 0.0 && a.outHandle.y == 0.0
            && b.inHandle.x == 0.0 && b.inHandle.y == 0.0) {
            out.push_back(p0);
            continue;
        }

        for (int k = 0; k < segmentsPerCurve; ++k) {
            const double t = static_cast<double>(k) / static_cast<double>(segmentsPerCurve);
            out.push_back(cubic(p0, p1, p2, p3, t));
        }
    }
}

Vec2 BezierMask::pointOnSegment(int segmentIndex, double t) const {
    const int n = static_cast<int>(nodes.size());
    if (n < 2 || segmentIndex < 0 || segmentIndex >= n) return Vec2{0.0, 0.0};

    const MaskNode& a = nodes[static_cast<size_t>(segmentIndex)];
    const MaskNode& b = nodes[static_cast<size_t>((segmentIndex + 1) % n)];
    return cubic(a.point, a.outPoint(), b.inPoint(), b.point, std::clamp(t, 0.0, 1.0));
}

bool BezierMask::containsUV(const Vec2& uv) const {
    if (!isActive()) return true;   // không mặt nạ = mọi điểm đều chiếu

    std::vector<Vec2>& poly = scratch();
    flatten(poly);

    const bool in = pointInPolygon(poly, uv);
    return invert ? !in : in;
}

bool BezierMask::boundsUV(Vec2& lo, Vec2& hi) const {
    if (nodes.size() < 2) return false;

    std::vector<Vec2>& poly = scratch();
    flatten(poly);
    if (poly.empty()) return false;

    lo = hi = poly[0];
    for (const Vec2& p : poly) {
        lo.x = std::min(lo.x, p.x);
        lo.y = std::min(lo.y, p.y);
        hi.x = std::max(hi.x, p.x);
        hi.y = std::max(hi.y, p.y);
    }
    return true;
}

uint64_t BezierMask::geometryHash() const {
    uint64_t h = 1469598103934665603ull;   // FNV offset basis

    const uint64_t flags = (enabled ? 1ull : 0ull) | (invert ? 2ull : 0ull);
    hashBytes(h, &flags, sizeof(flags));

    const uint64_t count = static_cast<uint64_t>(nodes.size());
    hashBytes(h, &count, sizeof(count));

    for (const MaskNode& nd : nodes) {
        hashDouble(h, nd.point.x);
        hashDouble(h, nd.point.y);
        hashDouble(h, nd.inHandle.x);
        hashDouble(h, nd.inHandle.y);
        hashDouble(h, nd.outHandle.x);
        hashDouble(h, nd.outHandle.y);
    }
    return h;
}

// ═══════════════════════════════════════════════════════════════════════
//  Sửa đổi
// ═══════════════════════════════════════════════════════════════════════

int BezierMask::insertNodeOnSegment(int segmentIndex, double t) {
    const int n = static_cast<int>(nodes.size());
    if (n < 2 || segmentIndex < 0 || segmentIndex >= n) return -1;
    if (n >= kMaxNodes) return -1;

    t = std::clamp(t, 0.0, 1.0);

    MaskNode& a = nodes[static_cast<size_t>(segmentIndex)];
    MaskNode& b = nodes[static_cast<size_t>((segmentIndex + 1) % n)];

    const Vec2 p0 = a.point;
    const Vec2 p1 = a.outPoint();
    const Vec2 p2 = b.inPoint();
    const Vec2 p3 = b.point;

    // Chia đôi de Casteljau: cho ra HAI đoạn bezier có hình y hệt đoạn
    // gốc. Chèn nút bằng cách nội suy thẳng điểm giữa sẽ làm đường cong
    // nhảy — người dùng thêm một nút mà hình đổi là lỗi khó chấp nhận.
    const Vec2 q0{p0.x + (p1.x - p0.x) * t, p0.y + (p1.y - p0.y) * t};
    const Vec2 q1{p1.x + (p2.x - p1.x) * t, p1.y + (p2.y - p1.y) * t};
    const Vec2 q2{p2.x + (p3.x - p2.x) * t, p2.y + (p3.y - p2.y) * t};

    const Vec2 r0{q0.x + (q1.x - q0.x) * t, q0.y + (q1.y - q0.y) * t};
    const Vec2 r1{q1.x + (q2.x - q1.x) * t, q1.y + (q2.y - q1.y) * t};

    const Vec2 mid{r0.x + (r1.x - r0.x) * t, r0.y + (r1.y - r0.y) * t};

    // Tay nắm mới, đổi về dạng TƯƠNG ĐỐI.
    a.outHandle = Vec2{q0.x - p0.x, q0.y - p0.y};
    b.inHandle  = Vec2{q2.x - p3.x, q2.y - p3.y};

    MaskNode inserted;
    inserted.point     = mid;
    inserted.inHandle  = Vec2{r0.x - mid.x, r0.y - mid.y};
    inserted.outHandle = Vec2{r1.x - mid.x, r1.y - mid.y};

    const int at = segmentIndex + 1;
    nodes.insert(nodes.begin() + at, inserted);
    return at;
}

bool BezierMask::removeNode(int index) {
    if (index < 0 || index >= static_cast<int>(nodes.size())) return false;

    // Còn 3 nút là hình kín nhỏ nhất. Cho xoá tiếp thì mặt nạ lặng lẽ
    // ngừng cắt (isActive() thành false) và người dùng tưởng phần mềm hỏng.
    if (nodes.size() <= 3) return false;

    nodes.erase(nodes.begin() + index);
    return true;
}

int BezierMask::closestSegment(const Vec2& uv, double& outT, double& outDist) const {
    outT = 0.0;
    outDist = 0.0;

    const int n = static_cast<int>(nodes.size());
    if (n < 2) return -1;

    int    bestSeg = -1;
    double bestD2  = 0.0;

    for (int i = 0; i < n; ++i) {
        const MaskNode& a = nodes[static_cast<size_t>(i)];
        const MaskNode& b = nodes[static_cast<size_t>((i + 1) % n)];

        const Vec2 p0 = a.point;
        const Vec2 p1 = a.outPoint();
        const Vec2 p2 = b.inPoint();
        const Vec2 p3 = b.point;

        // Quét thô trên đoạn đã làm phẳng rồi tinh lại trong khoảng nhỏ.
        // Giải đúng nghiệm gần nhất của bezier bậc 3 là phương trình bậc
        // 5 — không đáng cho một thao tác chuột.
        constexpr int kCoarse = kFlattenSegments;
        for (int k = 0; k < kCoarse; ++k) {
            const double t0 = static_cast<double>(k)       / kCoarse;
            const double t1 = static_cast<double>(k + 1)   / kCoarse;
            const Vec2 c0 = cubic(p0, p1, p2, p3, t0);
            const Vec2 c1 = cubic(p0, p1, p2, p3, t1);

            const double lt = closestTOnSegment(c0, c1, uv);
            const Vec2 q{c0.x + (c1.x - c0.x) * lt, c0.y + (c1.y - c0.y) * lt};
            const double d2 = dist2(q, uv);

            if (bestSeg < 0 || d2 < bestD2) {
                bestD2  = d2;
                bestSeg = i;
                outT    = t0 + (t1 - t0) * lt;
            }
        }
    }

    outDist = std::sqrt(bestD2);
    return bestSeg;
}

} // namespace mikmap

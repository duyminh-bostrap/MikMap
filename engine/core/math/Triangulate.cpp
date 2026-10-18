#include "core/math/Triangulate.h"

#include <algorithm>
#include <cmath>

namespace mikmap::tri {
namespace {

double cross(P o, P a, P b) { return (a.x - o.x) * (b.y - o.y) - (a.y - o.y) * (b.x - o.x); }

// Ngưỡng "bằng 0" cho tích chéo, theo tỉ lệ kích thước đa giác (toạ độ pixel hay UV đều đúng).
double epsFor(const std::vector<P>& p) {
    double x0 = p[0].x, x1 = x0, y0 = p[0].y, y1 = y0;
    for (const P& q : p) {
        x0 = std::min(x0, q.x); x1 = std::max(x1, q.x);
        y0 = std::min(y0, q.y); y1 = std::max(y1, q.y);
    }
    const double d2 = (x1 - x0) * (x1 - x0) + (y1 - y0) * (y1 - y0);
    return 1e-12 * d2;
}

double signedArea2(const std::vector<P>& p) {
    double s = 0;
    for (size_t i = 0, n = p.size(); i < n; ++i) {
        const P a = p[i], b = p[(i + 1) % n];
        s += a.x * b.y - b.x * a.y;
    }
    return s;
}

// q nằm trên đoạn ab (giả sử đã thẳng hàng).
bool within(P a, P b, P q) {
    return std::min(a.x, b.x) <= q.x && q.x <= std::max(a.x, b.x) &&
           std::min(a.y, b.y) <= q.y && q.y <= std::max(a.y, b.y);
}

int side(P a, P b, P q, double eps) {
    const double c = cross(a, b, q);
    return c > eps ? 1 : (c < -eps ? -1 : 0);
}

// Hai đoạn cắt hoặc chạm nhau (kể cả chồng thẳng hàng).
bool segmentsTouch(P a, P b, P c, P d, double eps) {
    const int s1 = side(a, b, c, eps), s2 = side(a, b, d, eps);
    const int s3 = side(c, d, a, eps), s4 = side(c, d, b, eps);
    if (s1 * s2 < 0 && s3 * s4 < 0) return true;
    return (s1 == 0 && within(a, b, c)) || (s2 == 0 && within(a, b, d)) ||
           (s3 == 0 && within(c, d, a)) || (s4 == 0 && within(c, d, b));
}

// q trong hoặc trên biên tam giác abc (abc đã quay theo dấu `o`).
bool inTriangle(P a, P b, P c, P q, double o, double eps) {
    return cross(a, b, q) * o >= -eps && cross(b, c, q) * o >= -eps && cross(c, a, q) * o >= -eps;
}

}  // namespace

bool simple(const std::vector<P>& poly) {
    const size_t n = poly.size();
    if (n < 3) return false;
    const double eps = epsFor(poly);
    if (!(std::fabs(signedArea2(poly)) > eps)) return false;  // cũng loại NaN
    for (size_t i = 0; i < n; ++i) {
        const P a = poly[i], b = poly[(i + 1) % n];
        if (a.x == b.x && a.y == b.y) return false;
        for (size_t j = i + 1; j < n; ++j) {
            const P c = poly[j], d = poly[(j + 1) % n];
            if (j == i + 1) {  // kề: b == c; cạnh sau không được quay ngược chồng lên cạnh trước
                if (side(a, b, d, eps) == 0 && within(a, b, d)) return false;
                continue;
            }
            if (i == 0 && j == n - 1) {  // kề vòng: d == a
                if (side(a, b, c, eps) == 0 && within(a, b, c)) return false;
                continue;
            }
            if (segmentsTouch(a, b, c, d, eps)) return false;
        }
    }
    return true;
}

bool triangulate(const std::vector<P>& poly, std::vector<std::array<int, 3>>& out) {
    out.clear();
    if (!simple(poly)) return false;
    const double eps = epsFor(poly);
    const double o = signedArea2(poly) > 0 ? 1.0 : -1.0;
    std::vector<int> idx(poly.size());
    for (size_t i = 0; i < idx.size(); ++i) idx[i] = static_cast<int>(i);

    auto isEar = [&](size_t k) {
        const size_t m = idx.size();
        const int ip = idx[(k + m - 1) % m], ic = idx[k], in = idx[(k + 1) % m];
        const P a = poly[ip], b = poly[ic], c = poly[in];
        if (cross(a, b, c) * o <= eps) return false;  // lõm hoặc thẳng hàng
        for (const int j : idx)
            if (j != ip && j != ic && j != in && inTriangle(a, b, c, poly[j], o, eps)) return false;
        return true;
    };

    while (idx.size() > 3) {
        const size_t m = idx.size();
        size_t pick = m;
        for (size_t k = 0; k < m && pick == m; ++k)
            if (isEar(k)) pick = k;
        if (pick == m)  // không còn tai dương: cắt một đỉnh thẳng hàng (tam giác dẹt)
            for (size_t k = 0; k < m && pick == m; ++k)
                if (std::fabs(cross(poly[idx[(k + m - 1) % m]], poly[idx[k]], poly[idx[(k + 1) % m]])) <= eps)
                    pick = k;
        if (pick == m) { out.clear(); return false; }  // không thể xảy ra với đa giác đơn
        out.push_back({idx[(pick + m - 1) % m], idx[pick], idx[(pick + 1) % m]});
        idx.erase(idx.begin() + static_cast<std::ptrdiff_t>(pick));
    }
    out.push_back({idx[0], idx[1], idx[2]});
    return true;
}

bool barycentric(P a, P b, P c, P p, double& u, double& v, double& w) {
    const double d = cross(a, b, c);
    const double l2 = std::max({(b.x - a.x) * (b.x - a.x) + (b.y - a.y) * (b.y - a.y),
                                (c.x - a.x) * (c.x - a.x) + (c.y - a.y) * (c.y - a.y),
                                (c.x - b.x) * (c.x - b.x) + (c.y - b.y) * (c.y - b.y)});
    if (!(std::fabs(d) > 1e-12 * l2)) return false;
    v = cross(a, p, c) / d;
    w = cross(a, b, p) / d;
    u = 1.0 - v - w;
    return true;
}

bool locate(const std::vector<P>& poly, const std::vector<std::array<int, 3>>& tris, P p,
            int& tri, double& u, double& v, double& w) {
    const int n = static_cast<int>(poly.size());
    for (size_t k = 0; k < tris.size(); ++k) {
        const auto& t = tris[k];
        if (t[0] < 0 || t[1] < 0 || t[2] < 0 || t[0] >= n || t[1] >= n || t[2] >= n) continue;
        double a, b, c;
        if (!barycentric(poly[t[0]], poly[t[1]], poly[t[2]], p, a, b, c)) continue;
        constexpr double tol = 1e-9;  // barycentric đã chuẩn hoá theo diện tích
        if (a >= -tol && b >= -tol && c >= -tol) {
            tri = static_cast<int>(k); u = a; v = b; w = c;
            return true;
        }
    }
    return false;
}

}  // namespace mikmap::tri

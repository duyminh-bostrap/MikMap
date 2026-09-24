#include "core/model/WarpMesh.h"

#include "core/math/BilinearInverse.h"

#include <algorithm>
#include <cmath>

namespace mikmap {

WarpMesh::WarpMesh() {
    resetToRect({0.0, 0.0}, {1920.0, 1080.0});
}

WarpMesh::WarpMesh(int cols, int rows, const Vec2& topLeft, const Vec2& size) {
    m_cols = std::max(1, cols);
    m_rows = std::max(1, rows);
    resetToRect(topLeft, size);
}

void WarpMesh::resetToRect(const Vec2& topLeft, const Vec2& size) {
    m_points.assign(static_cast<size_t>(m_cols + 1) * static_cast<size_t>(m_rows + 1),
                    Vec2{});

    for (int cy = 0; cy <= m_rows; ++cy) {
        for (int cx = 0; cx <= m_cols; ++cx) {
            const double u = static_cast<double>(cx) / m_cols;
            const double v = static_cast<double>(cy) / m_rows;
            m_points[index(cx, cy)] = Vec2{topLeft.x + u * size.x,
                                           topLeft.y + v * size.y};
        }
    }
}

const Vec2& WarpMesh::controlPoint(int cx, int cy) const {
    cx = std::clamp(cx, 0, m_cols);
    cy = std::clamp(cy, 0, m_rows);
    return m_points[index(cx, cy)];
}

void WarpMesh::setControlPoint(int cx, int cy, const Vec2& p) {
    if (cx < 0 || cx > m_cols || cy < 0 || cy > m_rows) return;
    m_points[index(cx, cy)] = p;
}

void WarpMesh::resize(int newCols, int newRows) {
    newCols = std::max(1, newCols);
    newRows = std::max(1, newRows);
    if (newCols == m_cols && newRows == m_rows) return;

    // Lấy mẫu lại bề mặt HIỆN TẠI ở mật độ mới — công căn chỉnh của
    // người dùng được giữ nguyên thay vì bị xoá về hình chữ nhật.
    std::vector<Vec2> resampled(
        static_cast<size_t>(newCols + 1) * static_cast<size_t>(newRows + 1));

    for (int cy = 0; cy <= newRows; ++cy) {
        for (int cx = 0; cx <= newCols; ++cx) {
            const Vec2 uv{static_cast<double>(cx) / newCols,
                          static_cast<double>(cy) / newRows};
            resampled[static_cast<size_t>(cy) * static_cast<size_t>(newCols + 1)
                      + static_cast<size_t>(cx)] = forward(uv);
        }
    }

    m_cols   = newCols;
    m_rows   = newRows;
    m_points = std::move(resampled);
}

Vec2 WarpMesh::forward(const Vec2& contentUV) const {
    const double u = std::clamp(contentUV.x, 0.0, 1.0);
    const double v = std::clamp(contentUV.y, 0.0, 1.0);

    // Xác định ô và toạ độ cục bộ trong ô.
    const double fx = u * m_cols;
    const double fy = v * m_rows;

    int cx = static_cast<int>(std::floor(fx));
    int cy = static_cast<int>(std::floor(fy));

    // Điểm nằm đúng biên phải/dưới thuộc về ô cuối cùng, không phải ô kế tiếp.
    cx = std::clamp(cx, 0, m_cols - 1);
    cy = std::clamp(cy, 0, m_rows - 1);

    const double lu = fx - cx;
    const double lv = fy - cy;

    return bilerp(m_points[index(cx,     cy)],
                  m_points[index(cx + 1, cy)],
                  m_points[index(cx + 1, cy + 1)],
                  m_points[index(cx,     cy + 1)],
                  lu, lv);
}

bool WarpMesh::findCell(const Vec2& p, int& outCx, int& outCy) const {
    // Duyệt tuyến tính có lọc bằng hộp bao.
    //
    // Với lưới ≤ 16×16 (256 ô) việc này tốn vài µs — chấp nhận được vì
    // chỉ chạy cho mỗi điểm chạm (thường < 10 điểm/frame), không phải
    // cho mỗi pixel.
    //
    // Nếu sau này cần lưới dày hơn nhiều: dựng spatial hash trên hộp bao
    // của các ô, xây lại khi lưới dirty. Chưa làm vì chưa cần — và một
    // cấu trúc tăng tốc chưa cần thiết chỉ tạo thêm chỗ để sai.
    for (int cy = 0; cy < m_rows; ++cy) {
        for (int cx = 0; cx < m_cols; ++cx) {
            const Vec2& a = m_points[index(cx,     cy)];
            const Vec2& b = m_points[index(cx + 1, cy)];
            const Vec2& c = m_points[index(cx + 1, cy + 1)];
            const Vec2& d = m_points[index(cx,     cy + 1)];

            const double minX = std::min({a.x, b.x, c.x, d.x});
            const double maxX = std::max({a.x, b.x, c.x, d.x});
            const double minY = std::min({a.y, b.y, c.y, d.y});
            const double maxY = std::max({a.y, b.y, c.y, d.y});

            if (p.x < minX || p.x > maxX || p.y < minY || p.y > maxY) continue;

            if (pointInQuad(a, b, c, d, p)) {
                outCx = cx;
                outCy = cy;
                return true;
            }
        }
    }
    return false;
}

bool WarpMesh::inverse(const Vec2& outputPx, Vec2& outContentUV) const {
    int cx = 0, cy = 0;
    if (!findCell(outputPx, cx, cy)) return false;

    Vec2 localUV;
    const bool ok = invertBilinear(m_points[index(cx,     cy)],
                                   m_points[index(cx + 1, cy)],
                                   m_points[index(cx + 1, cy + 1)],
                                   m_points[index(cx,     cy + 1)],
                                   outputPx, localUV);
    if (!ok) return false;

    // Ghép (u,v) cục bộ của ô về (u,v) toàn cục của lưới.
    outContentUV = {(cx + localUV.x) / m_cols,
                    (cy + localUV.y) / m_rows};
    return true;
}

void WarpMesh::tessellate(int cols, int rows, WarpGeometry& out) const {
    out.clear();
    cols = std::max(1, cols);
    rows = std::max(1, rows);

    out.vertices.reserve(static_cast<size_t>(cols + 1) * static_cast<size_t>(rows + 1));

    for (int y = 0; y <= rows; ++y) {
        for (int x = 0; x <= cols; ++x) {
            const Vec2 uv{static_cast<double>(x) / cols,
                          static_cast<double>(y) / rows};
            out.vertices.push_back(WarpVertex{forward(uv), uv});
        }
    }

    const int stride = cols + 1;
    out.indices.reserve(static_cast<size_t>(cols) * static_cast<size_t>(rows) * 6);

    for (int y = 0; y < rows; ++y) {
        for (int x = 0; x < cols; ++x) {
            const unsigned int i0 = static_cast<unsigned int>(y * stride + x);
            const unsigned int i1 = i0 + 1;
            const unsigned int i2 = i0 + static_cast<unsigned int>(stride);
            const unsigned int i3 = i2 + 1;

            out.indices.insert(out.indices.end(), {i0, i1, i2});
            out.indices.insert(out.indices.end(), {i1, i3, i2});
        }
    }
}

Vec2 WarpMesh::controlPointAt(int index) const {
    if (index < 0 || index >= controlPointCount()) return Vec2{};
    return m_points[static_cast<size_t>(index)];
}

bool WarpMesh::setControlPointAt(int index, const Vec2& p) {
    if (index < 0 || index >= controlPointCount()) return false;
    // Mesh khong co rang buoc loi nhu corner pin: mot o bi xoan van
    // nghich dao duoc (nghiem bac hai). Nen luon chap nhan.
    m_points[static_cast<size_t>(index)] = p;
    return true;
}

std::unique_ptr<IWarp> WarpMesh::clone() const {
    return std::make_unique<WarpMesh>(*this);
}

void WarpMesh::boundingBox(Vec2& outMin, Vec2& outMax) const {
    if (m_points.empty()) {
        outMin = outMax = Vec2{};
        return;
    }
    outMin = outMax = m_points[0];
    for (const Vec2& p : m_points) {
        outMin.x = std::min(outMin.x, p.x);
        outMin.y = std::min(outMin.y, p.y);
        outMax.x = std::max(outMax.x, p.x);
        outMax.y = std::max(outMax.y, p.y);
    }
}

} // namespace mikmap

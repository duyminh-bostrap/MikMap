#include "core/model/WarpCornerPin.h"

#include "core/math/Homography.h"

#include <algorithm>

namespace mikmap {

WarpCornerPin::WarpCornerPin() {
    resetToRect({0.0, 0.0}, {1920.0, 1080.0});
}

WarpCornerPin::WarpCornerPin(const Vec2 corners[4]) {
    setCorners(corners);
}

void WarpCornerPin::resetToRect(const Vec2& topLeft, const Vec2& size) {
    const Vec2 c[4] = {
        topLeft,
        {topLeft.x + size.x, topLeft.y},
        {topLeft.x + size.x, topLeft.y + size.y},
        {topLeft.x,          topLeft.y + size.y},
    };
    setCorners(c);
}

void WarpCornerPin::setCorners(const Vec2 corners[4]) {
    for (int i = 0; i < 4; ++i) m_corners[i] = corners[i];
    rebuild();
}

bool WarpCornerPin::setCorner(int i, const Vec2& p) {
    if (i < 0 || i > 3) return false;

    // Thử trước rồi mới ghi đè: nếu vị trí mới làm tứ giác tự cắt hoặc
    // suy biến, ta từ chối và giữ nguyên trạng thái cũ. Nhờ vậy UI không
    // bao giờ đưa được slice vào trạng thái không nghịch đảo được —
    // vốn sẽ âm thầm phá hỏng calibration sensor.
    const Vec2 saved = m_corners[i];
    m_corners[i] = p;

    if (!homography::isValidQuad(m_corners)) {
        m_corners[i] = saved;
        return false;
    }

    rebuild();
    return m_valid;
}

void WarpCornerPin::rebuild() {
    const HomographyResult r = homography::unitSquareToQuad(m_corners);
    m_valid = r.ok;

    if (!m_valid) {
        m_toOutput  = Mat3::identity();
        m_toContent = Mat3::identity();
        return;
    }

    m_toOutput = r.H;

    // Tính trước nghịch đảo: sensor gọi inverse() cho MỖI ĐIỂM CHẠM,
    // MỖI FRAME. Nghịch đảo ma trận ở đó là lãng phí — góc chỉ đổi khi
    // người dùng kéo chuột, tức vài lần mỗi giây là cùng.
    if (!m_toOutput.invert(m_toContent)) {
        m_valid = false;
        m_toContent = Mat3::identity();
    }
}

Vec2 WarpCornerPin::forward(const Vec2& contentUV) const {
    return m_toOutput.transformPoint(contentUV);
}

bool WarpCornerPin::inverse(const Vec2& outputPx, Vec2& outContentUV) const {
    if (!m_valid) return false;

    const Vec2 uv = m_toContent.transformPoint(outputPx);
    if (!uv.isFinite()) return false;

    // Nới biên một chút để điểm chạm sát mép không bị loại vì sai số
    // dấu phẩy động — người dùng chạm đúng viền là chuyện bình thường.
    constexpr double kEdgeTolerance = 1e-6;
    if (uv.x < -kEdgeTolerance || uv.x > 1.0 + kEdgeTolerance) return false;
    if (uv.y < -kEdgeTolerance || uv.y > 1.0 + kEdgeTolerance) return false;

    outContentUV = {std::clamp(uv.x, 0.0, 1.0), std::clamp(uv.y, 0.0, 1.0)};
    return true;
}

void WarpCornerPin::tessellate(int cols, int rows, WarpGeometry& out) const {
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

Vec2 WarpCornerPin::controlPointAt(int index) const {
    if (index < 0 || index > 3) return Vec2{};
    return m_corners[index];
}

bool WarpCornerPin::setControlPointAt(int index, const Vec2& p) {
    // setCorner() da lo phan kiem tra hop le va khoi phuc trang thai cu.
    return setCorner(index, p);
}

std::unique_ptr<IWarp> WarpCornerPin::clone() const {
    return std::make_unique<WarpCornerPin>(*this);
}

void WarpCornerPin::boundingBox(Vec2& outMin, Vec2& outMax) const {
    outMin = outMax = m_corners[0];
    for (int i = 1; i < 4; ++i) {
        outMin.x = std::min(outMin.x, m_corners[i].x);
        outMin.y = std::min(outMin.y, m_corners[i].y);
        outMax.x = std::max(outMax.x, m_corners[i].x);
        outMax.y = std::max(outMax.y, m_corners[i].y);
    }
}

} // namespace mikmap

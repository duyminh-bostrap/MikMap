#include "core/model/WarpBezier.h"

#include "core/math/BilinearInverse.h"

#include <algorithm>
#include <cmath>

namespace mikmap {
namespace {

/// Bốn đa thức Bernstein bậc ba tại t.
inline void bernstein3(double t, double b[4]) {
    const double s = 1.0 - t;
    b[0] = s * s * s;
    b[1] = 3.0 * s * s * t;
    b[2] = 3.0 * s * t * t;
    b[3] = t * t * t;
}

/// Đạo hàm của bốn đa thức Bernstein bậc ba tại t.
inline void bernstein3d(double t, double b[4]) {
    const double s = 1.0 - t;
    b[0] = -3.0 * s * s;
    b[1] =  3.0 * s * s - 6.0 * s * t;
    b[2] =  6.0 * s * t - 3.0 * t * t;
    b[3] =  3.0 * t * t;
}

/// Lưới lấy mẫu cho bước ĐOÁN THÔ của nghịch đảo.
///
/// 8×8 ô là đủ: nó chỉ cần đưa Newton vào đúng lưu vực hội tụ, không cần
/// chính xác. Dày hơn thì tốn thời gian mỗi điểm chạm mà không tăng độ
/// chính xác cuối cùng chút nào — Newton lo phần đó.
constexpr int kSeedCells = 8;

/// Số vòng Newton tối đa. Hội tụ bậc hai nên 8 vòng là quá dư; giới hạn
/// tồn tại để trường hợp bệnh lý không treo vòng lặp giữa buổi diễn.
constexpr int kNewtonIters = 8;

} // namespace

WarpBezier::WarpBezier() {
    resetToRect(Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0});
}

WarpBezier::WarpBezier(const Vec2& topLeft, const Vec2& size) {
    resetToRect(topLeft, size);
}

const Vec2& WarpBezier::controlPoint(int cx, int cy) const {
    const int x = std::clamp(cx, 0, kDim - 1);
    const int y = std::clamp(cy, 0, kDim - 1);
    return m_points[static_cast<size_t>(idx(x, y))];
}

void WarpBezier::setControlPoint(int cx, int cy, const Vec2& p) {
    if (cx < 0 || cx >= kDim || cy < 0 || cy >= kDim) return;
    m_points[static_cast<size_t>(idx(cx, cy))] = p;
}

Vec2 WarpBezier::controlPointAt(int index) const {
    if (index < 0 || index >= kPointCount) return Vec2{0.0, 0.0};
    return m_points[static_cast<size_t>(index)];
}

bool WarpBezier::setControlPointAt(int index, const Vec2& p) {
    if (index < 0 || index >= kPointCount) return false;
    m_points[static_cast<size_t>(index)] = p;
    return true;
}

Vec2 WarpBezier::forward(const Vec2& contentUV) const {
    double bu[4], bv[4];
    bernstein3(contentUV.x, bu);
    bernstein3(contentUV.y, bv);

    Vec2 s{0.0, 0.0};
    for (int cy = 0; cy < kDim; ++cy) {
        for (int cx = 0; cx < kDim; ++cx) {
            const double w = bu[cx] * bv[cy];
            const Vec2& p = m_points[static_cast<size_t>(idx(cx, cy))];
            s.x += w * p.x;
            s.y += w * p.y;
        }
    }
    return s;
}

Vec2 WarpBezier::derivativeU(const Vec2& uv) const {
    double bu[4], bv[4];
    bernstein3d(uv.x, bu);
    bernstein3(uv.y, bv);

    Vec2 d{0.0, 0.0};
    for (int cy = 0; cy < kDim; ++cy) {
        for (int cx = 0; cx < kDim; ++cx) {
            const double w = bu[cx] * bv[cy];
            const Vec2& p = m_points[static_cast<size_t>(idx(cx, cy))];
            d.x += w * p.x;
            d.y += w * p.y;
        }
    }
    return d;
}

Vec2 WarpBezier::derivativeV(const Vec2& uv) const {
    double bu[4], bv[4];
    bernstein3(uv.x, bu);
    bernstein3d(uv.y, bv);

    Vec2 d{0.0, 0.0};
    for (int cy = 0; cy < kDim; ++cy) {
        for (int cx = 0; cx < kDim; ++cx) {
            const double w = bu[cx] * bv[cy];
            const Vec2& p = m_points[static_cast<size_t>(idx(cx, cy))];
            d.x += w * p.x;
            d.y += w * p.y;
        }
    }
    return d;
}

bool WarpBezier::inverse(const Vec2& outputPx, Vec2& outContentUV) const {
    // ── Bước 1: đoán thô trên lưới lấy mẫu ─────────────────────────────
    //
    // Lấy mẫu bề mặt rồi tìm ô chứa điểm bằng đúng bộ máy của mesh. Không
    // tìm được ô nào thì điểm nằm ngoài bề mặt — trả false, KHÔNG đưa một
    // giá trị bịa cho Newton nắn (nó sẽ hội tụ ra một (u,v) ngoài [0,1]
    // và ta lại phải loại, chỉ tốn thêm thời gian).
    Vec2 grid[(kSeedCells + 1) * (kSeedCells + 1)];
    for (int gy = 0; gy <= kSeedCells; ++gy) {
        for (int gx = 0; gx <= kSeedCells; ++gx) {
            const Vec2 uv{static_cast<double>(gx) / kSeedCells,
                          static_cast<double>(gy) / kSeedCells};
            grid[gy * (kSeedCells + 1) + gx] = forward(uv);
        }
    }

    bool seeded = false;
    Vec2 uv{0.5, 0.5};
    for (int gy = 0; gy < kSeedCells && !seeded; ++gy) {
        for (int gx = 0; gx < kSeedCells && !seeded; ++gx) {
            const Vec2& a = grid[gy       * (kSeedCells + 1) + gx];
            const Vec2& b = grid[gy       * (kSeedCells + 1) + gx + 1];
            const Vec2& c = grid[(gy + 1) * (kSeedCells + 1) + gx + 1];
            const Vec2& d = grid[(gy + 1) * (kSeedCells + 1) + gx];

            const double minX = std::min({a.x, b.x, c.x, d.x});
            const double maxX = std::max({a.x, b.x, c.x, d.x});
            const double minY = std::min({a.y, b.y, c.y, d.y});
            const double maxY = std::max({a.y, b.y, c.y, d.y});

            // ★ Hop bao phai co DUNG SAI, neu khong thi diem nam dung tren
            //   MEP be mat bi loai.
            //
            //   Ly do khong hien nhien: forward(1.0, v) khong tra ve dung
            //   1920.0 ma 1920.0000000000002 — tong bon so hang Bernstein
            //   khong tai lap chinh xac gia tri dau mut, va sai so lech
            //   vai ULP KHAC NHAU theo tung v. Nen diem hoi va goc cua o
            //   luoi lay mau khong con trung nhau tuyet doi, du ve mat
            //   toan hoc chung la mot.
            //
            //   Hau qua neu bo qua: cham vao mep phai / mep duoi cua vung
            //   chieu thi khong an gi — dung kieu loi bi do cho "sensor
            //   khong tinh o gan mep".
            const double padX = (maxX - minX) * 1e-9 + 1e-9;
            const double padY = (maxY - minY) * 1e-9 + 1e-9;
            if (outputPx.x < minX - padX || outputPx.x > maxX + padX
                || outputPx.y < minY - padY || outputPx.y > maxY + padY) continue;

            if (!pointInQuad(a, b, c, d, outputPx)) continue;

            Vec2 local;
            if (!invertBilinear(a, b, c, d, outputPx, local)) continue;

            uv = Vec2{(gx + local.x) / kSeedCells, (gy + local.y) / kSeedCells};
            seeded = true;
        }
    }
    if (!seeded) {
        // ── Lưới mẫu là DÂY CUNG, bề mặt thật là CUNG ──────────────────
        //
        // ★ Chỗ bề mặt phồng ra ngoài, đa giác nối các điểm mẫu nằm HẲN
        //   BÊN TRONG nó. Nên một điểm nằm đúng trên mép cong của bề mặt
        //   thật sự nằm ngoài mọi ô mẫu — không ô nào nhận, dù điểm đó
        //   hoàn toàn hợp lệ. Nới hộp bao không cứu được: khoảng hở là
        //   hình học (độ cong × cỡ ô²), không phải sai số làm tròn.
        //
        //   Nên bước đoán thô chỉ được quyền GỢI Ý. Trọng tài là Newton:
        //   lấy điểm mẫu gần nhất làm mốc, chạy Newton, rồi phán bằng sai
        //   số cuối cùng. Điểm nằm thật sự ngoài bề mặt vẫn bị loại — nó
        //   sẽ kẹt ở biên với sai số lớn.
        double best = -1.0;
        for (int gy = 0; gy <= kSeedCells; ++gy) {
            for (int gx = 0; gx <= kSeedCells; ++gx) {
                const Vec2& s = grid[gy * (kSeedCells + 1) + gx];
                const double dx = s.x - outputPx.x;
                const double dy = s.y - outputPx.y;
                const double d2 = dx * dx + dy * dy;
                if (best < 0.0 || d2 < best) {
                    best = d2;
                    uv = Vec2{static_cast<double>(gx) / kSeedCells,
                              static_cast<double>(gy) / kSeedCells};
                }
            }
        }
        if (best < 0.0) return false;
    }

    // ── Bước 2: nắn bằng Newton trên phương trình S(u,v) − P = 0 ───────
    //
    // Jacobian là [∂S/∂u  ∂S/∂v] — ma trận 2×2, nghịch đảo bằng tay.
    for (int it = 0; it < kNewtonIters; ++it) {
        const Vec2 s = forward(uv);
        const double rx = s.x - outputPx.x;
        const double ry = s.y - outputPx.y;

        // Đã đủ gần thì dừng. Ngưỡng theo pixel vì đây là không gian
        // output px: 1e-9 px nhỏ hơn mọi thứ có ý nghĩa vật lý.
        if (std::abs(rx) < 1e-9 && std::abs(ry) < 1e-9) break;

        const Vec2 du = derivativeU(uv);
        const Vec2 dv = derivativeV(uv);
        const double det = du.x * dv.y - du.y * dv.x;

        // Jacobian suy biến: bề mặt bị gấp hoặc bẹp ngay tại đây. Giữ
        // nguyên nghiệm đoán thô thay vì chia cho ~0 rồi văng đi đâu đó.
        if (std::abs(det) < 1e-12) break;

        const double stepU = ( dv.y * rx - dv.x * ry) / det;
        const double stepV = (-du.y * rx + du.x * ry) / det;

        uv.x -= stepU;
        uv.y -= stepV;

        // Kẹp về [0,1] SAU MỖI VÒNG, không phải chỉ ở cuối: để Newton
        // đi lạc ra xa rồi mới kéo về thì nó có thể rơi vào một nhánh
        // nghiệm khác của bề mặt (Bézier bậc ba có thể tự cắt).
        uv.x = std::clamp(uv.x, 0.0, 1.0);
        uv.y = std::clamp(uv.y, 0.0, 1.0);
    }

    // Kiểm lại lần cuối: nghiệm phải THỰC SỰ đưa về đúng điểm đã hỏi.
    // Newton bị kẹp ở biên có thể dừng ở một (u,v) không phải nghiệm.
    const Vec2 check = forward(uv);
    const double dx = check.x - outputPx.x;
    const double dy = check.y - outputPx.y;
    if (std::sqrt(dx * dx + dy * dy) > 0.5) return false;   // sai quá nửa pixel

    outContentUV = uv;
    return true;
}

bool WarpBezier::isInvertible() const {
    // Bề mặt nghịch đảo được khi ánh xạ (u,v) → px không GẤP: định thức
    // Jacobian không đổi dấu và không triệt tiêu trong toàn miền.
    //
    // Lấy mẫu thay vì chứng minh giải tích: định thức là đa thức bậc cao
    // theo (u,v), tìm nghiệm chính xác vừa đắt vừa không cần thiết. Lưới
    // mẫu bắt được mọi trường hợp gấp mà người dùng tạo ra bằng tay.
    int sign = 0;
    constexpr int kN = 8;
    for (int gy = 0; gy <= kN; ++gy) {
        for (int gx = 0; gx <= kN; ++gx) {
            const Vec2 uv{static_cast<double>(gx) / kN,
                          static_cast<double>(gy) / kN};
            const Vec2 du = derivativeU(uv);
            const Vec2 dv = derivativeV(uv);
            const double det = du.x * dv.y - du.y * dv.x;

            // Thang đo để so "gần bằng 0": tích độ dài hai vector tiếp
            // tuyến. Ngưỡng tuyệt đối sẽ sai với slice to hoặc nhỏ.
            const double scale = std::sqrt((du.x * du.x + du.y * du.y)
                                         * (dv.x * dv.x + dv.y * dv.y));
            if (scale <= 0.0 || std::abs(det) < scale * 1e-6) return false;

            const int s = (det > 0.0) ? 1 : -1;
            if (sign == 0) sign = s;
            else if (s != sign) return false;   // đổi dấu ⇒ bề mặt gấp
        }
    }
    return sign != 0;
}

void WarpBezier::tessellate(int cols, int rows, WarpGeometry& out) const {
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

std::unique_ptr<IWarp> WarpBezier::clone() const {
    return std::make_unique<WarpBezier>(*this);
}

void WarpBezier::resetToRect(const Vec2& topLeft, const Vec2& size) {
    // Điểm trong đặt đúng tại 1/3 và 2/3 thì mặt Bézier trùng khít với
    // hình chữ nhật phẳng, và forward() thành ánh xạ affine — nghĩa là
    // "reset" cho ra đúng thứ người dùng mong đợi, không cong nhẹ.
    for (int cy = 0; cy < kDim; ++cy) {
        for (int cx = 0; cx < kDim; ++cx) {
            const double u = static_cast<double>(cx) / (kDim - 1);
            const double v = static_cast<double>(cy) / (kDim - 1);
            m_points[static_cast<size_t>(idx(cx, cy))] =
                Vec2{topLeft.x + u * size.x, topLeft.y + v * size.y};
        }
    }
}

void WarpBezier::boundingBox(Vec2& outMin, Vec2& outMax) const {
    // Tính chất bao lồi của Bézier: bề mặt LUÔN nằm trong bao lồi của các
    // điểm điều khiển. Nên hộp bao của điểm điều khiển là hộp bao hợp lệ
    // (rộng hơn thực tế một chút) mà không phải lấy mẫu bề mặt.
    outMin = m_points[0];
    outMax = m_points[0];
    for (const Vec2& p : m_points) {
        outMin.x = std::min(outMin.x, p.x);
        outMin.y = std::min(outMin.y, p.y);
        outMax.x = std::max(outMax.x, p.x);
        outMax.y = std::max(outMax.y, p.y);
    }
}

WarpBezier WarpBezier::fromWarp(const IWarp& src) {
    WarpBezier out;

    // Lấy mẫu bề mặt nguồn tại lưới 4×4 rồi dùng làm điểm điều khiển.
    //
    // ★ Đây là XẤP XỈ, và chỗ sai lệch nằm ở giữa chứ không ở góc: điểm
    //   điều khiển trong của Bézier KHÔNG nằm trên bề mặt. Bốn góc thì
    //   khớp chính xác (Bernstein bậc ba nội suy hai đầu), mà bốn góc lại
    //   chính là phần người vận hành tốn công căn nhất.
    for (int cy = 0; cy < kDim; ++cy) {
        for (int cx = 0; cx < kDim; ++cx) {
            const double u = static_cast<double>(cx) / (kDim - 1);
            const double v = static_cast<double>(cy) / (kDim - 1);
            out.setControlPoint(cx, cy, src.forward(Vec2{u, v}));
        }
    }
    return out;
}

} // namespace mikmap

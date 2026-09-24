// ════════════════════════════════════════════════════════════════════════
//  tests/test_beziermask.cpp — mặt nạ bezier (F12)
//
//  Mặt nạ quyết định chỗ nào ĐƯỢC CHIẾU. Sai ở đây thì ánh sáng tràn ra
//  ngoài vật thể — đúng cái mà cả tính năng này sinh ra để ngăn — hoặc
//  cắt mất một phần hình mà không có thông báo gì.
// ════════════════════════════════════════════════════════════════════════
#include "TestHarness.h"

#include "core/model/BezierMask.h"
#include "core/model/Screen.h"
#include "core/model/Slice.h"

#include <cmath>
#include <vector>

using namespace mikmap;

namespace {

/// Bezier bậc 3, viết lại độc lập trong test.
///
/// Cố ý KHÔNG dùng lại hàm của BezierMask.cpp: nếu công thức ở đó sai,
/// một test dùng chính công thức đó sẽ vẫn xanh.
Vec2 cubicAt(const Vec2& p0, const Vec2& p1, const Vec2& p2, const Vec2& p3, double t) {
    const double u = 1.0 - t;
    return {u*u*u*p0.x + 3*u*u*t*p1.x + 3*u*t*t*p2.x + t*t*t*p3.x,
            u*u*u*p0.y + 3*u*u*t*p1.y + 3*u*t*t*p2.y + t*t*t*p3.y};
}

/// Bốn điểm điều khiển của đoạn `i` (nút i → nút i+1) trong mặt nạ.
void segmentOf(const BezierMask& m, int i, Vec2 out[4]) {
    const MaskNode& a = m.nodes[static_cast<size_t>(i)];
    const MaskNode& b = m.nodes[static_cast<size_t>((i + 1) % static_cast<int>(m.nodes.size()))];
    out[0] = a.point;
    out[1] = a.outPoint();
    out[2] = b.inPoint();
    out[3] = b.point;
}

/// Diện tích đa giác (công thức dây giày). Dùng để kiểm chứng hình dạng
/// mà không phụ thuộc vào thứ tự hay số lượng đỉnh cụ thể.
double polygonArea(const std::vector<Vec2>& p) {
    if (p.size() < 3) return 0.0;
    double a = 0.0;
    for (size_t i = 0, j = p.size() - 1; i < p.size(); j = i++) {
        a += (p[j].x + p[i].x) * (p[j].y - p[i].y);
    }
    return std::abs(a) * 0.5;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  Mặc định: không mặt nạ = không cắt gì
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("Mask: mac dinh khong cat gi") {
    BezierMask m;
    CHECK(!m.isActive());
    CHECK(m.isIdentity());

    // ★ Bất biến quan trọng nhất: slice chưa đặt mặt nạ phải chiếu TOÀN
    //   BỘ. Nếu containsUV mặc định trả về false thì mọi slice cũ trong
    //   mọi project cũ sẽ đen sì sau khi nâng cấp.
    CHECK(m.containsUV(Vec2{0.5, 0.5}));
    CHECK(m.containsUV(Vec2{0.0, 0.0}));
    CHECK(m.containsUV(Vec2{-5.0, 12.0}));
}

TEST_CASE("Mask: bat nhung duoi 3 nut thi chua co tac dung") {
    BezierMask m;
    m.enabled = true;
    m.nodes.resize(2);
    CHECK(!m.isActive());
    CHECK(m.containsUV(Vec2{0.5, 0.5}));
}

// ═══════════════════════════════════════════════════════════════════════
//  Hình dựng sẵn
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("Mask: hinh chu nhat bao dung vung ben trong") {
    const BezierMask m = BezierMask::rectangle(0.25);
    REQUIRE(m.isActive());
    CHECK(m.nodes.size() == 4);

    CHECK(m.containsUV(Vec2{0.5,  0.5}));      // giữa
    CHECK(m.containsUV(Vec2{0.30, 0.30}));     // vừa bên trong
    CHECK(!m.containsUV(Vec2{0.1,  0.5}));     // ngoài, bên trái
    CHECK(!m.containsUV(Vec2{0.9,  0.5}));     // ngoài, bên phải
    CHECK(!m.containsUV(Vec2{0.5,  0.1}));     // ngoài, phía trên
    CHECK(!m.containsUV(Vec2{0.5,  0.9}));     // ngoài, phía dưới
}

TEST_CASE("Mask: hinh chu nhat toan dinh goc thi lam phang thanh 4 diem") {
    const BezierMask m = BezierMask::rectangle(0.0);
    std::vector<Vec2> poly;
    m.flatten(poly);

    // Bốn cạnh đều là đoạn THẲNG (tay nắm = 0), nên không được chia nhỏ.
    // Chia nhỏ đoạn thẳng chỉ tốn đỉnh mà không chính xác thêm — và mặt
    // nạ đa giác kiểu cắt theo mép tường thì cạnh nào cũng thẳng.
    CHECK(poly.size() == 4);
    CHECK_NEAR(polygonArea(poly), 1.0, 1e-12);
}

TEST_CASE("Mask: elip 4 nut co dien tich dung cua hinh elip") {
    const BezierMask m = BezierMask::ellipse(4);
    REQUIRE(m.isActive());

    std::vector<Vec2> poly;
    m.flatten(poly, 64);

    // Elip nội tiếp ô đơn vị: bán trục 0.5 → diện tích = pi·a·b = pi/4.
    // Đây là phép kiểm chứng hằng số kappa của tay nắm: sai kappa thì
    // đường cong phồng hoặc lép và diện tích lệch đi thấy rõ.
    constexpr double kPi = 3.14159265358979323846;
    CHECK_NEAR(polygonArea(poly), kPi * 0.25, 2e-3);

    CHECK(m.containsUV(Vec2{0.5, 0.5}));

    // Bốn góc ô vuông nằm NGOÀI elip nội tiếp.
    CHECK(!m.containsUV(Vec2{0.02, 0.02}));
    CHECK(!m.containsUV(Vec2{0.98, 0.02}));
    CHECK(!m.containsUV(Vec2{0.98, 0.98}));
    CHECK(!m.containsUV(Vec2{0.02, 0.98}));
}

TEST_CASE("Mask: elip nhieu nut hoi tu ve cung mot hinh") {
    std::vector<Vec2> a, b;
    BezierMask::ellipse(4).flatten(a, 64);
    BezierMask::ellipse(12).flatten(b, 64);
    CHECK_NEAR(polygonArea(a), polygonArea(b), 2e-3);
}

// ═══════════════════════════════════════════════════════════════════════
//  Đảo
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("Mask: dao thi lat nguoc trong / ngoai") {
    BezierMask m = BezierMask::rectangle(0.25);
    CHECK(m.containsUV(Vec2{0.5, 0.5}));
    CHECK(!m.containsUV(Vec2{0.05, 0.05}));

    m.invert = true;
    CHECK(!m.containsUV(Vec2{0.5, 0.5}));
    CHECK(m.containsUV(Vec2{0.05, 0.05}));
}

// ═══════════════════════════════════════════════════════════════════════
//  Hình lõm — đây là lý do phải dùng quy tắc chẵn-lẻ
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ Mask: hinh LOM van cho ket qua dung") {
    // Hình chữ L. Một phép kiểm hộp-bao đơn giản sẽ nói điểm ở khuyết
    // nằm bên trong — vật thể thật (góc tường, bậc thang) thường lõm,
    // nên đây không phải trường hợp hiếm.
    //
    //   (0,0) ┌────┐ (1,0)
    //         │    │
    //         │    └──┐ (1,0.5)
    //         │       │
    //   (0,1) └───────┘ (1,1)   ← khuyết ở góc trên-phải
    BezierMask m;
    m.enabled = true;
    m.nodes.resize(6);
    m.nodes[0].point = Vec2{0.0, 0.0};
    m.nodes[1].point = Vec2{0.5, 0.0};
    m.nodes[2].point = Vec2{0.5, 0.5};
    m.nodes[3].point = Vec2{1.0, 0.5};
    m.nodes[4].point = Vec2{1.0, 1.0};
    m.nodes[5].point = Vec2{0.0, 1.0};

    CHECK(m.containsUV(Vec2{0.25, 0.25}));   // nhánh dọc
    CHECK(m.containsUV(Vec2{0.75, 0.75}));   // nhánh ngang
    CHECK(!m.containsUV(Vec2{0.75, 0.25}));  // ★ khuyết — phải NGOÀI
}

// ═══════════════════════════════════════════════════════════════════════
//  Chèn nút không được làm đổi hình
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ Mask: chen nut GIU NGUYEN hinh dang") {
    BezierMask m = BezierMask::ellipse(4);

    // Đoạn 0 TRƯỚC khi chèn.
    Vec2 orig[4];
    segmentOf(m, 0, orig);

    constexpr double kT = 0.37;
    const int at = m.insertNodeOnSegment(0, kT);
    REQUIRE(at == 1);
    REQUIRE(m.nodes.size() == 5);

    // ★ Chia đôi de Casteljau cho ra HAI đoạn vẽ lại CHÍNH XÁC đường cong
    //   gốc. Nếu chèn bằng cách nội suy thẳng điểm giữa, đường cong sẽ
    //   NHẢY — người dùng thêm một nút mà hình đổi là lỗi không chấp
    //   nhận được khi đang căn theo mép vật thể thật.
    //
    //   Đo bằng cách so TỪNG ĐIỂM trên đường cong, không so diện tích:
    //   sau khi chèn, flatten() sinh ra nhiều đỉnh hơn nên đa giác xấp xỉ
    //   sát đường cong hơn và diện tích tăng lên chút ít — đó là sai số
    //   LÀM PHẲNG, không phải hình đổi. So diện tích sẽ đo nhầm thứ.
    Vec2 half0[4], half1[4];
    segmentOf(m, 0, half0);
    segmentOf(m, 1, half1);

    for (int k = 0; k <= 20; ++k) {
        const double u = static_cast<double>(k) / 20.0;

        // Nửa đầu phủ t ∈ [0, kT] của đường cong gốc.
        const Vec2 a = cubicAt(half0[0], half0[1], half0[2], half0[3], u);
        const Vec2 e1 = cubicAt(orig[0], orig[1], orig[2], orig[3], kT * u);
        CHECK_NEAR(a.x, e1.x, 1e-12);
        CHECK_NEAR(a.y, e1.y, 1e-12);

        // Nửa sau phủ t ∈ [kT, 1].
        const Vec2 b = cubicAt(half1[0], half1[1], half1[2], half1[3], u);
        const Vec2 e2 = cubicAt(orig[0], orig[1], orig[2], orig[3], kT + (1.0 - kT) * u);
        CHECK_NEAR(b.x, e2.x, 1e-12);
        CHECK_NEAR(b.y, e2.y, 1e-12);
    }

    // Các đoạn còn lại không được đụng tới.
    CHECK_NEAR(m.nodes[2].point.x, BezierMask::ellipse(4).nodes[1].point.x, 1e-15);
    CHECK_NEAR(m.nodes[2].point.y, BezierMask::ellipse(4).nodes[1].point.y, 1e-15);
}

TEST_CASE("Mask: lam phang min hon thi dien tich HOI TU, khong nhay") {
    // Kiểm chứng cách đọc kết quả của test trên: đa giác nội tiếp luôn
    // NHỎ hơn hình cong thật, và tiến dần lên khi chia nhỏ hơn.
    const BezierMask m = BezierMask::ellipse(4);
    std::vector<Vec2> a, b, c;
    m.flatten(a, 8);
    m.flatten(b, 32);
    m.flatten(c, 128);

    constexpr double kPi = 3.14159265358979323846;
    CHECK(polygonArea(a) < polygonArea(b));
    CHECK(polygonArea(b) < polygonArea(c));

    // ★ Giá trị hội tụ KHÔNG phải đúng bằng pi/4 mà hơi LỚN hơn.
    //
    //   Bezier bậc 3 với hằng số kappa là XẤP XỈ của cung tròn, không
    //   phải cung tròn: nó phình ra ngoài khoảng 0.03% ở giữa mỗi cung.
    //   Ghi nhận đúng dấu ở đây, vì một test viết `< pi/4` sẽ đỏ và trông
    //   như lỗi trong khi toán học đang đúng — đúng cái bẫy đã mắc lúc
    //   viết test này lần đầu.
    const double truth = kPi * 0.25;
    CHECK(polygonArea(c) > truth);
    CHECK(polygonArea(c) < truth * 1.001);
}

TEST_CASE("Mask: chen nut voi chi so doan sai bi tu choi") {
    BezierMask m = BezierMask::rectangle();
    CHECK(m.insertNodeOnSegment(-1, 0.5) == -1);
    CHECK(m.insertNodeOnSegment(99, 0.5) == -1);
    CHECK(m.nodes.size() == 4);
}

TEST_CASE("Mask: chen nut vao doan THANG cho ra diem giua") {
    BezierMask m = BezierMask::rectangle(0.0);
    const int at = m.insertNodeOnSegment(0, 0.5);
    REQUIRE(at == 1);
    CHECK_NEAR(m.nodes[1].point.x, 0.5, 1e-12);
    CHECK_NEAR(m.nodes[1].point.y, 0.0, 1e-12);
}

// ═══════════════════════════════════════════════════════════════════════
//  Xoá nút
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ Mask: khong xoa duoc xuong duoi 3 nut") {
    BezierMask m = BezierMask::rectangle();
    REQUIRE(m.nodes.size() == 4);

    CHECK(m.removeNode(0));
    CHECK(m.nodes.size() == 3);

    // ★ Cho xoá tiếp thì isActive() lặng lẽ thành false: mặt nạ vẫn hiện
    //   "đang bật" trên giao diện nhưng không cắt gì nữa. Người dùng sẽ
    //   đi tìm lỗi ở chỗ khác.
    CHECK(!m.removeNode(0));
    CHECK(m.nodes.size() == 3);
    CHECK(m.isActive());
}

TEST_CASE("Mask: xoa nut voi chi so ngoai pham vi khong lam sap") {
    BezierMask m = BezierMask::ellipse(6);
    CHECK(!m.removeNode(-1));
    CHECK(!m.removeNode(999));
    CHECK(m.nodes.size() == 6);
}

// ═══════════════════════════════════════════════════════════════════════
//  Tìm đoạn gần nhất
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("Mask: closestSegment tim dung canh nguoi dung bam vao") {
    const BezierMask m = BezierMask::rectangle(0.0);

    double t = 0.0, d = 0.0;

    // Điểm ngay trên cạnh TRÊN (nút 0 → nút 1).
    CHECK(m.closestSegment(Vec2{0.5, 0.0}, t, d) == 0);
    CHECK_NEAR(d, 0.0, 1e-9);
    CHECK_NEAR(t, 0.5, 1e-6);

    // Cạnh PHẢI (nút 1 → nút 2).
    CHECK(m.closestSegment(Vec2{1.0, 0.5}, t, d) == 1);
    CHECK_NEAR(d, 0.0, 1e-9);

    // Cách xa đường thì vẫn trả về đoạn gần nhất, kèm khoảng cách thật.
    CHECK(m.closestSegment(Vec2{0.5, -0.3}, t, d) == 0);
    CHECK_NEAR(d, 0.3, 1e-6);
}

TEST_CASE("Mask: closestSegment tren hinh chua du nut tra ve -1") {
    BezierMask m;
    double t = 0.0, d = 0.0;
    CHECK(m.closestSegment(Vec2{0.5, 0.5}, t, d) == -1);
    m.nodes.resize(1);
    CHECK(m.closestSegment(Vec2{0.5, 0.5}, t, d) == -1);
}

// ═══════════════════════════════════════════════════════════════════════
//  Băm — cơ chế làm mới texture của RenderEngine
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ Mask: bam DOI khi hinh doi") {
    BezierMask m = BezierMask::rectangle();
    const uint64_t h0 = m.geometryHash();

    m.nodes[2].point.x += 0.001;
    CHECK(m.geometryHash() != h0);

    m.nodes[2].point.x -= 0.001;
    CHECK(m.geometryHash() == h0);   // quay lại đúng giá trị cũ
}

TEST_CASE("★ Mask: bam KHONG doi khi chi doi feather") {
    BezierMask m = BezierMask::rectangle();
    const uint64_t h0 = m.geometryHash();

    // ★ feather được xử lý lúc lấy mẫu trong shader, không nằm trong
    //   texture mặt nạ. Nếu nó vào băm thì mỗi frame kéo thanh trượt sẽ
    //   dựng lại texture — giật ngay giữa lúc đang căn chỉnh.
    m.feather = 0.2;
    CHECK(m.geometryHash() == h0);
}

TEST_CASE("Mask: bam doi khi bat/tat hoac dao") {
    BezierMask m = BezierMask::rectangle();
    const uint64_t h0 = m.geometryHash();

    m.enabled = !m.enabled;
    CHECK(m.geometryHash() != h0);
    m.enabled = !m.enabled;

    m.invert = true;
    CHECK(m.geometryHash() != h0);
}

TEST_CASE("Mask: bam khong phan biet +0 va -0") {
    BezierMask a = BezierMask::rectangle(0.0);
    BezierMask b = a;
    b.nodes[0].point.x = -0.0;
    a.nodes[0].point.x = 0.0;
    CHECK(a.geometryHash() == b.geometryHash());
}

// ═══════════════════════════════════════════════════════════════════════
//  Tích hợp với Slice và Screen
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ Slice: outputToContent BO QUA mat na, isLit thi khong") {
    Slice s(Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0},
            Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0});
    s.mask = BezierMask::rectangle(0.25);

    const Vec2 centre{960.0, 540.0};
    const Vec2 corner{40.0, 40.0};       // nằm ngoài mặt nạ

    Vec2 uv;
    // ★ Phép nghịch đảo hình học phải chạy được ở MỌI điểm trong slice.
    //   Wizard calibration dựa vào nó, và điểm ngắm hoàn toàn có thể rơi
    //   vào vùng bị mặt nạ cắt — lúc đó vẫn phải calibrate được.
    CHECK(s.outputToContent(centre, uv));
    CHECK(s.outputToContent(corner, uv));

    // isLit mới là câu hỏi "chỗ này có ánh sáng không".
    CHECK(s.isLit(centre, uv));
    CHECK(!s.isLit(corner, uv));
}

TEST_CASE("★ Screen: hitTest bo qua vung bi mat na cat") {
    Screen sc(0, "Test", Vec2{1920.0, 1080.0});
    sc.addFullScreenSlice(Vec2{1920.0, 1080.0});
    REQUIRE(sc.sliceCount() == 1);

    // Chưa có mặt nạ: mọi điểm trong khung đều trúng.
    CHECK(sc.hitTest(Vec2{40.0, 40.0}) == 0);
    CHECK(sc.hitTest(Vec2{960.0, 540.0}) == 0);

    sc.slices[0].mask = BezierMask::rectangle(0.25);

    CHECK(sc.hitTest(Vec2{960.0, 540.0}) == 0);    // trong mặt nạ
    CHECK(sc.hitTest(Vec2{40.0, 40.0})  == -1);    // ★ bị cắt → không trúng
}

TEST_CASE("Slice: sao chep mang theo ca mat na") {
    Slice a;
    a.mask = BezierMask::ellipse(5);
    a.mask.feather = 0.15;
    a.mask.invert = true;

    const Slice b = a;                 // copy ctor tự viết (vì có unique_ptr)
    CHECK(b.mask.nodes.size() == 5);
    CHECK(b.mask.feather == 0.15);
    CHECK(b.mask.invert);
    CHECK(b.mask.geometryHash() == a.mask.geometryHash());

    Slice c;
    c = a;                             // operator=
    CHECK(c.mask.nodes.size() == 5);
    CHECK(c.mask.geometryHash() == a.mask.geometryHash());
}

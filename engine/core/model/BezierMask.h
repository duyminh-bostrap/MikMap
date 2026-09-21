// ════════════════════════════════════════════════════════════════════════
//  core/model/BezierMask.h — mặt nạ bezier cho từng slice (F12)
//
//  Cắt ánh sáng theo đúng bóng của vật thể thật. Chiếu lên một cây cột,
//  một mái vòm, một chiếc xe — hình chữ nhật của slice bao giờ cũng tràn
//  ra tường phía sau. Mặt nạ khoanh lại phần được chiếu.
//
//  ── Vì sao điểm nút ở contentUV, không phải pixel máy chiếu ──────────
//  Keystone là thứ ánh xạ nội dung slice lên BỀ MẶT THẬT. Đặt mặt nạ ở
//  contentUV nghĩa là nó đi qua đúng phép warp đó: vẽ xong bóng vật thể
//  một lần, sau này máy chiếu bị xê dịch thì chỉ cần kéo lại 4 góc —
//  mặt nạ theo cùng, không phải vẽ lại.
//
//  Đặt ở pixel máy chiếu thì mỗi lần chỉnh keystone là mỗi lần mất công
//  vẽ lại toàn bộ. Cùng lý do khiến TriggerZone nằm ở không gian canvas.
//
//      contentUV (0..1)      warp.forward()        pixel máy chiếu
//      ┌───────────┐                              ╱‾‾‾‾‾‾‾╲
//      │   ╭───╮   │        ──────────►          ╱  ╭──╮   ╲
//      │  ╱ mặt ╲  │                            ╱  ╱ đã ╲   ╲
//      │ ╰──nạ──╯  │                            ╲ ╰─warp╯   ╱
//      └───────────┘                             ╲_________╱
//
//  ── Tay nắm là TƯƠNG ĐỐI so với điểm neo ────────────────────────────
//  Giống Illustrator / After Effects: kéo điểm neo thì tay nắm đi theo.
//  Lưu tuyệt đối thì kéo neo xong tay nắm nằm lại chỗ cũ và đường cong
//  méo đi — sai với mọi thói quen người dùng có sẵn.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"

#include <cstdint>
#include <vector>

namespace hexmap {

/// Một điểm neo trên đường mặt nạ, kèm hai tay nắm bezier.
struct MaskNode {
    Vec2 point{0.0, 0.0};       ///< điểm neo, contentUV

    /// Tay nắm, TƯƠNG ĐỐI so với `point`. (0,0) = đoạn thẳng.
    Vec2 inHandle{0.0, 0.0};    ///< điều khiển đoạn ĐI VÀO điểm này
    Vec2 outHandle{0.0, 0.0};   ///< điều khiển đoạn ĐI RA khỏi điểm này

    Vec2 inPoint()  const { return {point.x + inHandle.x,  point.y + inHandle.y}; }
    Vec2 outPoint() const { return {point.x + outHandle.x, point.y + outHandle.y}; }

    bool isCorner() const {
        return inHandle.x == 0.0 && inHandle.y == 0.0
            && outHandle.x == 0.0 && outHandle.y == 0.0;
    }
};

class BezierMask {
public:
    /// Số nút tối đa. Không phải giới hạn kỹ thuật mà là chặn file hỏng:
    /// một project sai định dạng không được làm treo lúc mở.
    static constexpr int kMaxNodes = 512;

    /// Số đoạn thẳng dùng để làm phẳng MỘT đoạn cong.
    ///
    /// Dùng CHUNG cho cả vẽ lẫn kiểm tra điểm nằm trong. Nếu hai bên làm
    /// phẳng khác nhau thì có những điểm nhìn thấy sáng mà hit-test bảo
    /// nằm ngoài — sai lệch chỉ vài pixel nhưng cực kỳ khó lần ra.
    static constexpr int kFlattenSegments = 16;

    bool enabled = false;

    /// Đảo: cắt phần BÊN TRONG thay vì bên ngoài. Dùng để khoét lỗ —
    /// ví dụ chừa ra một ô cửa sổ thật trên bức tường đang chiếu.
    bool invert = false;

    /// Làm mờ mép, theo tỉ lệ cạnh NGẮN của slice. 0 = mép sắc.
    ///
    /// Không phải để cho đẹp: mép sắc phơi bày mọi sai số căn chỉnh giữa
    /// hình chiếu và cạnh vật thể thật. Mép mờ vài phần trăm giấu được
    /// sai số đó — cùng nguyên lý với hoà viền (F20).
    double feather = 0.0;

    std::vector<MaskNode> nodes;

    /// Có thực sự cắt gì không. Dưới 3 nút thì không thành hình kín.
    bool isActive() const { return enabled && nodes.size() >= 3; }

    bool isIdentity() const {
        return !enabled && !invert && feather == 0.0 && nodes.empty();
    }

    void reset() { *this = BezierMask{}; }

    // ── Hình dựng sẵn ──────────────────────────────────────────────────
    /// Hình chữ nhật thụt vào `inset` (tỉ lệ). Điểm khởi đầu hợp lý nhất:
    /// người dùng kéo từng góc vào cho khớp vật thể.
    static BezierMask rectangle(double inset = 0.05);

    /// Hình elip nội tiếp, `nodeCount` nút. Bốn nút với tay nắm chuẩn
    /// cho ra elip gần đúng tới ~0.02% — đủ chính xác hơn mọi máy chiếu.
    static BezierMask ellipse(int nodeCount = 4);

    // ── Hình học ───────────────────────────────────────────────────────
    /// Đường biên đã làm phẳng, theo contentUV, KHÉP KÍN (điểm cuối nối
    /// về điểm đầu, không lặp lại điểm đầu ở cuối mảng).
    void flatten(std::vector<Vec2>& out,
                 int segmentsPerCurve = kFlattenSegments) const;

    /// Điểm trên đoạn `segmentIndex` tại tham số t. Trả về (0,0) nếu chỉ
    /// số không hợp lệ. Giao diện dùng để đo khoảng cách từ con trỏ tới
    /// đường biên bằng PIXEL, chứ không bằng đơn vị UV — cùng một sai
    /// lệch UV trên slice bẹt sẽ là vài pixel theo trục này và vài chục
    /// theo trục kia.
    Vec2 pointOnSegment(int segmentIndex, double t) const;

    /// Điểm có được CHIẾU RA không — đã tính cả `invert`.
    /// Mặt nạ tắt thì mọi điểm đều qua.
    bool containsUV(const Vec2& uv) const;

    /// Hộp bao của đường biên, theo contentUV. Trả về false nếu chưa có
    /// hình. Không tính feather.
    bool boundsUV(Vec2& lo, Vec2& hi) const;

    /// Băm nội dung hình học — KHÔNG gồm `feather`.
    ///
    /// RenderEngine dựng texture mặt nạ lại khi giá trị này đổi. Cố ý
    /// dùng băm thay vì cờ "đã sửa": giao diện sửa thẳng `nodes[i].point`
    /// (đúng quy ước của dự án — ui/ được sửa trực tiếp model), nên một
    /// cờ sẽ phải được bật ở mọi chỗ sửa và sớm muộn cũng sót một chỗ.
    /// Băm thì không thể lệch khỏi dữ liệu.
    ///
    /// `feather` nằm ngoài vì nó được xử lý lúc lấy mẫu trong shader —
    /// kéo thanh trượt feather không phải dựng lại texture.
    uint64_t geometryHash() const;

    // ── Sửa đổi ────────────────────────────────────────────────────────
    /// Chèn một nút vào giữa đoạn `segmentIndex` (nút này → nút kế tiếp),
    /// tại tham số t. Trả về chỉ số nút mới, hoặc -1 nếu không hợp lệ.
    int insertNodeOnSegment(int segmentIndex, double t);

    /// Xoá nút. Từ chối nếu sẽ còn dưới 3 nút — một hình 2 nút không
    /// khoanh được vùng nào, và người dùng sẽ thấy mặt nạ "tự tắt".
    bool removeNode(int index);

    /// Đoạn biên gần điểm `uv` nhất, kèm tham số t trên đoạn đó.
    /// Dùng để chèn nút đúng chỗ người dùng bấm lên đường.
    /// Trả về -1 nếu chưa có hình.
    int closestSegment(const Vec2& uv, double& outT, double& outDist) const;
};

} // namespace hexmap

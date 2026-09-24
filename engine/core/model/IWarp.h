// ════════════════════════════════════════════════════════════════════════
//  core/model/IWarp.h — giao diện biến dạng của slice
//
//  ★ QUY TẮC BẤT DI BẤT DỊCH (architecture.md §10.3)
//    Mọi IWarp PHẢI cài đặt được cả forward() lẫn inverse().
//    Không có inverse() thì slice đó vô dụng với calibration sensor —
//    ta sẽ không biết người dùng đang chạm vào pixel nào của nội dung.
//
//  Hai không gian toạ độ:
//    · CONTENT UV  ∈ [0,1]²  — vị trí bên trong inputRect của slice
//    · OUTPUT PX             — pixel trên màn hình máy chiếu
//
//    forward:  content UV  ──▶  output px      (dùng để vẽ)
//    inverse:  output px   ──▶  content UV     (dùng cho sensor)
//
//  ── Vì sao inverse() trả về bool, không trả về Vec2 ──────────────────
//  Tài liệu kiến trúc bản đầu ghi `Vec2 inverse(Vec2)`. Khi cài đặt thật
//  mới thấy chữ ký đó sai: phép nghịch đảo CÓ THỂ THẤT BẠI hợp lệ —
//  điểm nằm ngoài vùng warp, hoặc ô lưới suy biến. Trả về Vec2 buộc phải
//  bịa ra một giá trị (thường là {0,0}), mà {0,0} lại là một toạ độ HỢP LỆ
//  (góc trên-trái). Người gọi không thể phân biệt "chạm vào góc" với
//  "trượt ra ngoài" → sinh ra hiệu ứng ma ở góc màn hình.
//  Chữ ký bool + tham số ra khiến việc thất bại là điều BẮT BUỘC phải xử lý.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"

#include <memory>
#include <vector>

namespace mikmap {

enum class WarpType {
    CornerPin,   ///< keystone 4 điểm — homography, nghịch đảo dạng đóng
    Mesh,        ///< lưới N×M — nghịch đảo song tuyến tính theo từng ô
    Bezier,      ///< lưới điều khiển bezier (P1 — chưa cài đặt)
};

/// Một đỉnh đã tessellate, sẵn sàng đẩy vào VBO.
struct WarpVertex {
    Vec2 position;   ///< output px
    Vec2 uv;         ///< content UV ∈ [0,1]²
};

/// Lưới tam giác đã tessellate của một warp.
struct WarpGeometry {
    std::vector<WarpVertex>   vertices;
    std::vector<unsigned int> indices;   ///< tam giác, 3 chỉ số mỗi tam giác

    void clear() { vertices.clear(); indices.clear(); }
    size_t triangleCount() const { return indices.size() / 3; }
};

class IWarp {
public:
    virtual ~IWarp() = default;

    virtual WarpType type() const = 0;

    /// content UV [0,1]² → output px. Luôn thành công.
    virtual Vec2 forward(const Vec2& contentUV) const = 0;

    /// output px → content UV [0,1]².
    /// @return false nếu điểm không nằm trong vùng warp — NGƯỜI GỌI PHẢI XỬ LÝ.
    virtual bool inverse(const Vec2& outputPx, Vec2& outContentUV) const = 0;

    /// Warp có ở trạng thái hợp lệ để nghịch đảo không?
    /// Sai khi tứ giác suy biến / tự cắt — UI phải cảnh báo người dùng.
    virtual bool isInvertible() const = 0;

    /// Sinh lưới tam giác để render.
    /// @param cols,rows số ô chia — càng cao càng mượt với bề mặt cong,
    ///        nhưng tốn vertex. Corner pin chỉ cần 1×1 vì homography
    ///        đã chính xác tuyệt đối trên GPU qua nội suy phối cảnh.
    virtual void tessellate(int cols, int rows, WarpGeometry& out) const = 0;

    /// Số ô chia hợp lý mặc định cho loại warp này.
    virtual int defaultSubdivisions() const = 0;

    // ── Điểm điều khiển — cho UI kéo thả (F6) ──────────────────────────
    //
    // API tổng quát này khiến UI KHÔNG cần biết mình đang kéo corner pin
    // hay mesh. Nếu không có nó, mỗi loại warp mới lại phải sửa cả
    // AppController lẫn RenderEngine — đúng kiểu phụ thuộc mà kiến trúc
    // này cố tránh.
    //   · CornerPin → 4 điểm
    //   · Mesh      → (cols+1) × (rows+1) điểm

    virtual int  controlPointCount() const = 0;
    virtual Vec2 controlPointAt(int index) const = 0;

    /// Đặt lại vị trí một điểm điều khiển.
    /// @return false nếu bị TỪ CHỐI (vd corner pin sẽ thành hình lõm).
    ///         Khi đó trạng thái cũ được giữ nguyên — UI không bao giờ
    ///         đưa được slice vào trạng thái không nghịch đảo được.
    virtual bool setControlPointAt(int index, const Vec2& p) = 0;

    virtual std::unique_ptr<IWarp> clone() const = 0;

    /// Đặt lại về hình chữ nhật, dùng khi tạo slice mới hoặc bấm Reset.
    virtual void resetToRect(const Vec2& topLeft, const Vec2& size) = 0;

    /// Hộp bao theo trục toạ độ trong không gian output — dùng để lọc nhanh
    /// trước khi gọi inverse() (đắt hơn nhiều).
    virtual void boundingBox(Vec2& outMin, Vec2& outMax) const = 0;
};


/// Con trỏ sở hữu warp, SAO CHÉP ĐƯỢC (tự gọi clone()).
///
/// ── Vì sao cần một lớp riêng cho việc này ────────────────────────────
/// Trước đây Slice giữ `unique_ptr<IWarp>` nên phải TỰ VIẾT copy
/// constructor và operator= để clone warp. Mà copy constructor viết tay
/// thì liệt kê từng trường một — nên mỗi lần thêm một trường mới vào
/// Slice, ai đó phải nhớ thêm nó vào cả hai chỗ.
///
/// Chuyện đã xảy ra thật: trường `mask` (F12) được thêm vào Slice nhưng
/// sót ở cả hai hàm sao chép, và mặt nạ lặng lẽ biến mất mỗi lần slice
/// bị sao chép — mà slice bị sao chép ở khắp nơi (nạp project, thêm
/// slice, undo). Không có lỗi biên dịch, không có cảnh báo.
///
/// Bọc riêng phần "khó sao chép" vào đây thì Slice không còn cần hàm sao
/// chép viết tay nữa: `= default` lo hết, và trường mới TỰ ĐỘNG được
/// sao chép. Cả một lớp lỗi biến mất thay vì được vá từng lần.
class WarpPtr {
public:
    WarpPtr() = default;
    WarpPtr(std::unique_ptr<IWarp> p) : m_p(std::move(p)) {}

    WarpPtr(const WarpPtr& o) : m_p(o.m_p ? o.m_p->clone() : nullptr) {}
    WarpPtr& operator=(const WarpPtr& o) {
        if (this != &o) m_p = o.m_p ? o.m_p->clone() : nullptr;
        return *this;
    }
    WarpPtr(WarpPtr&&) noexcept = default;
    WarpPtr& operator=(WarpPtr&&) noexcept = default;
    ~WarpPtr() = default;

    WarpPtr& operator=(std::unique_ptr<IWarp> p) { m_p = std::move(p); return *this; }

    IWarp*       get()       { return m_p.get(); }
    const IWarp* get() const { return m_p.get(); }
    explicit operator bool() const { return m_p != nullptr; }
    IWarp*       operator->()       { return m_p.get(); }
    const IWarp* operator->() const { return m_p.get(); }

private:
    std::unique_ptr<IWarp> m_p;
};

} // namespace mikmap

// ════════════════════════════════════════════════════════════════════════
//  core/model/Slice.h — đối tượng mapping cơ bản (F3 F4 F16)
//
//  Một slice trả lời hai câu hỏi:
//    1. LẤY vùng nào của Composition Canvas?   → inputOrigin + inputSize
//    2. ĐẶT nó ở đâu trên máy chiếu?           → warp (corner pin / mesh)
//
//      ┌──────── COMPOSITION CANVAS ────────┐
//      │        ┌──────────┐                │
//      │        │ inputRect│ ← F4           │
//      │        └──────────┘                │
//      └────────────┬───────────────────────┘
//                   │ warp.forward()
//                   ▼
//         ╔═══ SCREEN (máy chiếu) ═══╗
//         ║      ╱‾‾‾‾‾‾‾╲           ║
//         ║     ╱ đã warp  ╲         ║
//         ║    ╲___________╱         ║
//         ╚══════════════════════════╝
//
//  ── Ghi chú về sở hữu ────────────────────────────────────────────────
//  Slice SỞ HỮU warp qua unique_ptr (đa hình). Vì vậy phải tự viết
//  copy constructor dùng clone() — nếu quên, sao chép slice sẽ mất warp
//  và mọi căn chỉnh biến mất.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"
#include "core/model/BezierMask.h"
#include "core/model/IWarp.h"

#include <memory>
#include <string>

namespace hexmap {

/// F19 — hieu chinh mau cho tung slice.
///
/// ── Vi sao CAN o muc slice, khong phai muc composition ──────────────
/// Ghep nhieu may chieu thi moi may co do sang va sac do khac nhau —
/// khac nhau ca theo tuoi bong den. Hieu chinh o muc composition khong
/// giai quyet duoc: phai chinh RIENG tung vung chieu.
///
/// Gia tri mac dinh la "khong lam gi", nen slice moi tao khong bi doi mau.
struct ColorAdjust {
    double brightness = 0.0;    ///< cong them, [-1, 1]
    double contrast   = 1.0;    ///< nhan quanh diem giua, [0, 3]
    double gamma      = 1.0;    ///< [0.1, 4]
    double gainR      = 1.0;
    double gainG      = 1.0;
    double gainB      = 1.0;
    double opacity    = 1.0;    ///< de lam mo mep khi ghep tay

    bool isIdentity() const {
        return brightness == 0.0 && contrast == 1.0 && gamma == 1.0
            && gainR == 1.0 && gainG == 1.0 && gainB == 1.0 && opacity == 1.0;
    }

    void reset() { *this = ColorAdjust{}; }
};

/// F20 — hoa vien de ghep nhieu may chieu lien mach.
///
/// ── Bai toan ────────────────────────────────────────────────────────
/// Hai may chieu ghep canh nhau phai chong len nhau ~10-20%, neu khong
/// se lo ra mot vet den o giua do sai so co hoc. Nhung vung chong len
/// se SANG GAP DOI vi hai may cung chieu.
///
/// Cach giai: moi may lam mo dan ve phia mep trong vung chong. Tong
/// hai duong cong phai bang 1 o moi diem — do la ly do co tham so gamma
/// va luminance: duong cong tuyen tinh KHONG cong lai thanh 1 vi may
/// chieu co dap ung gamma phi tuyen.
///
/// Do rong tinh theo TI LE cua slice (0..0.5), khong theo pixel — nen
/// doi do phan giai may chieu khong lam hong can chinh.
struct SoftEdge {
    double left = 0.0, right = 0.0, top = 0.0, bottom = 0.0;

    /// Do cong. 1.0 = tuyen tinh. May chieu thuc te thuong can 1.8-2.4.
    double gamma = 1.0;

    /// Diem giua duong cong. 0.5 = can bang.
    double luminance = 0.5;

    bool isIdentity() const {
        return left == 0.0 && right == 0.0 && top == 0.0 && bottom == 0.0;
    }

    void reset() { *this = SoftEdge{}; }
};

class Slice {
public:
    Slice();
    Slice(const Vec2& inputOrigin, const Vec2& inputSize,
          const Vec2& outputTopLeft, const Vec2& outputSize);

    // ★ Sao chép mặc định là ĐÚNG và cố ý như vậy.
    //
    //   Phần duy nhất khó sao chép — con trỏ đa hình tới warp — đã được
    //   `WarpPtr` lo (nó tự gọi clone()). Nhờ đó Slice không cần hàm sao
    //   chép viết tay, và trường mới thêm vào lớp này TỰ ĐỘNG được sao
    //   chép theo.
    //
    //   Trước đây hai hàm này viết tay, liệt kê từng trường. Khi thêm
    //   `mask` (F12) thì cả hai đều sót, và mặt nạ biến mất mỗi lần slice
    //   bị sao chép — nạp project, thêm slice, mọi chỗ. Không lỗi biên
    //   dịch, không cảnh báo. Xem ghi chú ở `WarpPtr` trong IWarp.h.
    Slice(const Slice&) = default;
    Slice& operator=(const Slice&) = default;
    Slice(Slice&&) noexcept = default;
    Slice& operator=(Slice&&) noexcept = default;
    ~Slice() = default;

    std::string name = "Slice";

    /// F16 — tắt slice mà không xoá.
    bool enabled = true;

    /// F16 — chỉ hiện slice này. Screen quyết định, không phải slice.
    bool solo = false;

    /// F19 — hiệu chỉnh màu riêng cho slice này.
    ColorAdjust color;

    /// F20 — hoà viền để ghép nhiều máy chiếu.
    SoftEdge softEdge;

    /// F12 — mặt nạ bezier, cắt ánh sáng theo bóng vật thể thật.
    /// Điểm nút ở contentUV nên đi cùng warp: chỉnh lại keystone thì
    /// mặt nạ theo cùng, không phải vẽ lại. Xem `BezierMask.h`.
    BezierMask mask;

    // ── F4: vùng lấy trên Composition Canvas ───────────────────────────
    Vec2 inputOrigin{0.0, 0.0};
    Vec2 inputSize{1920.0, 1080.0};

    // ── Warp ───────────────────────────────────────────────────────────
    void setWarp(std::unique_ptr<IWarp> w);
    IWarp*       warp()       { return m_warp.get(); }
    const IWarp* warp() const { return m_warp.get(); }

    /// Thay bằng loại warp khác, giữ nguyên vùng output hiện tại.
    /// Dùng khi người dùng đổi từ corner pin sang mesh giữa chừng.
    void convertWarp(WarpType type, int meshCols = 4, int meshRows = 4);

    // ── Chuyển đổi không gian ──────────────────────────────────────────
    /// contentUV [0,1]² → toạ độ Composition Canvas.
    Vec2 contentToCanvas(const Vec2& contentUV) const {
        return {inputOrigin.x + contentUV.x * inputSize.x,
                inputOrigin.y + contentUV.y * inputSize.y};
    }

    /// Canvas → contentUV. Không kẹp — giá trị ngoài [0,1] nghĩa là điểm
    /// nằm ngoài vùng lấy của slice này.
    Vec2 canvasToContent(const Vec2& canvasPx) const;

    /// ★ output px → contentUV. Đây là bước sensor cần.
    /// @return false nếu điểm nằm ngoài slice.
    ///
    /// ★ CỐ Ý bỏ qua mặt nạ (F12) — đây là phép nghịch đảo HÌNH HỌC
    ///   thuần tuý. Wizard calibration dùng nó và phải chạy được kể cả
    ///   khi điểm ngắm rơi vào vùng bị mặt nạ cắt; nếu không, đúng lúc
    ///   người vận hành cần calibrate lại thì công cụ lại từ chối.
    ///
    ///   Nơi CẦN xét mặt nạ là `isLit()` và `Screen::hitTest`.
    bool outputToContent(const Vec2& outputPx, Vec2& contentUV) const;

    /// Điểm output này có thực sự ĐƯỢC CHIẾU SÁNG không: nằm trong slice
    /// VÀ không bị mặt nạ cắt.
    ///
    /// Đây mới là câu hỏi của sensor. Chạm vào một chỗ bị mặt nạ cắt là
    /// chạm vào chỗ tối — không có gì ở đó để chạm, nên không nên kích
    /// hoạt gì.
    bool isLit(const Vec2& outputPx, Vec2& contentUV) const;

    /// ★ output px → toạ độ Composition Canvas.
    bool outputToCanvas(const Vec2& outputPx, Vec2& canvasPx) const;

    /// contentUV → output px.
    Vec2 contentToOutput(const Vec2& contentUV) const;

    /// Có sẵn sàng dùng không: đang bật, có warp, và warp nghịch đảo được.
    bool isUsable() const {
        return enabled && m_warp && m_warp->isInvertible();
    }

private:
    /// WarpPtr chứ không phải unique_ptr — xem ghi chú ở hàm sao chép.
    WarpPtr m_warp;
};

} // namespace hexmap

// ════════════════════════════════════════════════════════════════════════
//  core/math/Snapping.h — hút điểm về đường gióng khi kéo (F21)
//
//  Khi người vận hành kéo một góc slice, thứ họ thường muốn là cho nó
//  THẲNG HÀNG với một mốc nào đó: mép máy chiếu, góc của slice bên cạnh,
//  đường giữa màn. Căn bằng mắt tới từng pixel là việc vừa lâu vừa không
//  bao giờ chính xác — và sai một pixel ở khe ghép hai máy chiếu là một
//  vệt sáng hoặc vệt tối chạy dọc suốt buổi diễn.
//
//  ── Ba quyết định làm nên hay dở của tính năng này ───────────────────
//
//  1. HAI TRỤC HÚT ĐỘC LẬP NHAU. Chỉ hút khi cả x lẫn y cùng khớp thì
//     gần như không bao giờ kích hoạt. Người dùng hay muốn "thẳng cột với
//     góc kia" mà chiều còn lại thì tuỳ ý.
//
//  2. CHỌN ĐƯỜNG GẦN NHẤT, không phải đường đầu tiên trong ngưỡng. Khi
//     nhiều mốc nằm sát nhau (mép slice và mép màn chỉ cách vài pixel),
//     lấy đường đầu tiên nghĩa là kết quả phụ thuộc thứ tự trong mảng —
//     tức là người dùng không đoán được nó sẽ hút vào đâu.
//
//  3. NGƯỠNG TÍNH THEO PIXEL MÀN HÌNH, KHÔNG PHẢI ĐƠN VỊ THẾ GIỚI.
//     Độ chính xác của bàn tay là hằng số theo pixel màn hình. Nếu ngưỡng
//     tính theo đơn vị output thì khi thu nhỏ khung nhìn, một ngưỡng
//     "8 đơn vị" chỉ còn 2 pixel trên màn — hút gần như không bao giờ ăn.
//     Còn khi phóng to thì nó thành 40 pixel và hút loạn xạ.
//     ⇒ Người gọi phải chia ngưỡng cho hệ số phóng: xem `snapThresholdFor`.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"

#include <vector>

namespace hexmap {

struct SnapResult {
    Vec2 position;           ///< vị trí sau khi hút

    bool   snappedX = false;
    bool   snappedY = false;

    /// Toạ độ đường gióng đã hút — chỉ có nghĩa khi cờ tương ứng bật.
    /// UI vẽ đường này ra để người dùng thấy mình đang thẳng hàng với gì.
    double guideX = 0.0;
    double guideY = 0.0;
};

/// Hút `p` về đường gióng gần nhất trong `threshold`.
///
/// @param guidesX  các đường DỌC (giá trị x). Không cần sắp xếp.
/// @param guidesY  các đường NGANG (giá trị y).
/// @param threshold  khoảng cách tối đa còn hút, cùng đơn vị với `p`.
///                   ≤ 0 nghĩa là tắt hút — trả về nguyên `p`.
SnapResult snapPoint(const Vec2& p,
                     const std::vector<double>& guidesX,
                     const std::vector<double>& guidesY,
                     double threshold);

/// Đổi ngưỡng từ PIXEL MÀN HÌNH sang đơn vị thế giới.
///
/// @param pixels     ngưỡng mong muốn tính theo pixel trên màn (vd 8)
/// @param zoom       số pixel màn hình cho mỗi đơn vị thế giới
/// @return ngưỡng theo đơn vị thế giới; 0 khi zoom không hợp lệ (tắt hút)
double snapThresholdFor(double pixels, double zoom);

} // namespace hexmap

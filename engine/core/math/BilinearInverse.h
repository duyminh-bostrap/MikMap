// ════════════════════════════════════════════════════════════════════════
//  core/math/BilinearInverse.h — nghịch đảo phép nội suy song tuyến tính
//
//  BÀI TOÁN
//  Cho một tứ giác cong (4 điểm điều khiển) và một điểm P nằm trong nó,
//  tìm (u,v) ∈ [0,1]² sao cho bilerp(quad, u, v) == P.
//
//  VÌ SAO CẦN
//  Corner pin là homography 3×3 → có nghịch đảo dạng đóng, dễ.
//  Mesh warp thì KHÔNG: mỗi ô lưới là một phép nội suy song tuyến tính,
//  và nó không phải phép biến đổi tuyến tính. Nhưng calibration sensor
//  BẮT BUỘC phải đi ngược (output px → content UV), nên phải giải được.
//
//  CÁCH GIẢI — có nghiệm đóng, không cần lặp
//  Đặt  A = p00, E = p10−A, F = p01−A, G = A−p10+p11−p01, H = P−A.
//  Khi đó   H = u·E + v·F + u·v·G.
//  Khử u bằng thành phần x rồi thế vào thành phần y, ta được phương
//  trình BẬC HAI theo v:
//
//      k₂·v² + k₁·v + k₀ = 0
//      k₂ = G × F
//      k₁ = E × F + H × G
//      k₀ = H × E                       (× là tích có hướng 2D)
//
//  Giải v bằng công thức nghiệm, rồi suy ra u. Toàn bộ là O(1) — quan
//  trọng vì hàm này chạy cho mỗi điểm chạm, mỗi frame.
//
//  Khi G = 0 (ô lưới là hình bình hành, không xoắn) thì k₂ = 0 và
//  phương trình suy biến thành bậc nhất — phải xử lý riêng, nếu không
//  sẽ chia cho 0.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"

namespace mikmap {

/// Nghịch đảo nội suy song tuyến tính trên một tứ giác.
///
/// Thứ tự đỉnh khớp với Vec2::bilerp và quy ước corner pin:
///   p00 = (u=0,v=0)   p10 = (1,0)   p11 = (1,1)   p01 = (0,1)
///
/// @param outUV  chỉ ghi khi trả về true
/// @param tolerance  nới biên [0,1] một chút, để điểm nằm sát mép không
///                   bị loại do sai số dấu phẩy động
/// @return false nếu P không có ảnh ngược trong ô (nằm ngoài, hoặc ô suy biến)
bool invertBilinear(const Vec2& p00, const Vec2& p10,
                    const Vec2& p11, const Vec2& p01,
                    const Vec2& P,
                    Vec2& outUV,
                    double tolerance = 1e-6);

/// Kiểm tra điểm có nằm trong tứ giác không (chia thành 2 tam giác).
/// Hoạt động với cả tứ giác lõm, khác với phép kiểm tra bằng nửa mặt phẳng.
bool pointInQuad(const Vec2& p00, const Vec2& p10,
                 const Vec2& p11, const Vec2& p01,
                 const Vec2& P);

} // namespace mikmap

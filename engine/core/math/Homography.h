// ════════════════════════════════════════════════════════════════════════
//  core/math/Homography.h — giải ma trận biến đổi đồng phôi 3×3
//
//  ĐÂY LÀ TRÁI TIM CỦA CẢ HỆ THỐNG. Dùng ở hai nơi:
//
//    1. CORNER PIN (H_w) — người dùng kéo 4 góc của slice.
//       src = hình chữ nhật đơn vị của content, dst = tứ giác trên máy chiếu.
//
//    2. CALIBRATION SENSOR (H_s) — người dùng chạm vào 4+ điểm đã biết.
//       src = toạ độ thô của sensor, dst = toạ độ pixel máy chiếu.
//
//  Công thức lõi:   p_content = H_w⁻¹ · H_s · p_sensor
//
//  ── Vì sao cần chuẩn hoá Hartley ─────────────────────────────────────
//  DLT thô rất nhạy với thang đo. Toạ độ sensor có thể là mm (0..2000),
//  toạ độ máy chiếu là pixel (0..3840). Trộn hai thang đo lệch nhau vào
//  cùng một hệ 8×8 làm số điều kiện (condition number) tăng vọt và mất
//  độ chính xác. Chuẩn hoá Hartley đưa cả hai tập điểm về centroid tại
//  gốc với khoảng cách trung bình √2 trước khi giải, rồi khử chuẩn hoá
//  sau. Đây là khác biệt giữa sai số ~0.01px và sai số ~5px.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Mat3.h"
#include "core/math/Vec2.h"

#include <cstdint>
#include <vector>

namespace mikmap {

/// Một cặp điểm tương ứng dùng cho calibration.
struct CorrespondencePair {
    Vec2   src;              ///< toạ độ nguồn (vd: sensor thô)
    Vec2   dst;              ///< toạ độ đích  (vd: pixel máy chiếu)
    double weight = 1.0;     ///< dành cho least-squares có trọng số (chưa dùng)
    bool   enabled = true;   ///< người vận hành có thể tắt điểm xấu trong UI
};

/// Kết quả giải, kèm chỉ số chất lượng để HIỂN THỊ CHO NGƯỜI VẬN HÀNH.
/// Con số sai số quan trọng ngang bản thân ma trận: nếu rmsError = 40px
/// thì calibration đã hỏng và người vận hành cần biết ngay, chứ không
/// phải đoán mò khi thấy hiệu ứng lệch chỗ.
struct HomographyResult {
    Mat3   H;                     ///< ma trận tìm được (chuẩn hoá h22 = 1)
    bool   ok = false;
    double rmsError = 0.0;        ///< sai số tái chiếu RMS (đơn vị của dst)
    double maxError = 0.0;        ///< sai số điểm tệ nhất
    int    inlierCount = 0;
    int    totalCount = 0;
    std::vector<bool> inliers;    ///< chỉ RANSAC mới điền; song song với input
    const char* message = "";     ///< lý do thất bại, để hiện lên UI
};

/// Tham số cho RANSAC.
struct RansacParams {
    double   inlierThreshold = 3.0;   ///< ngưỡng sai số coi là inlier (pixel)
    int      maxIterations   = 2000;
    double   confidence      = 0.995; ///< dừng sớm khi đạt độ tin cậy này
    uint32_t seed            = 12345; ///< cố định để test tái lập được
    int      minInliers      = 4;
};

namespace homography {

// ── Giải chính xác từ đúng 4 điểm ──────────────────────────────────────
/// Dùng cho CORNER PIN: người dùng kéo đúng 4 góc, không có nhiễu,
/// cần khớp tuyệt đối.
/// Thất bại nếu 3 trong 4 điểm thẳng hàng (tứ giác suy biến).
HomographyResult solve4Point(const Vec2 src[4], const Vec2 dst[4]);

/// Tiện ích: ánh xạ hình chữ nhật đơn vị [0,1]² sang một tứ giác.
/// Đây chính là phép biến đổi của corner pin.
/// Thứ tự góc: (0,0) → (1,0) → (1,1) → (0,1), tức trên-trái theo chiều
/// kim đồng hồ, khớp với Vec2::bilerp và với quy ước của Resolume.
HomographyResult unitSquareToQuad(const Vec2 quad[4]);

// ── Bình phương tối thiểu từ N ≥ 4 điểm ───────────────────────────────
/// Dùng cho CALIBRATION SENSOR: người dùng chạm nhiều điểm, dữ liệu có
/// nhiễu, cần lời giải tối ưu trên toàn bộ.
/// Bỏ qua các cặp có enabled == false.
HomographyResult solveLeastSquares(const std::vector<CorrespondencePair>& pairs);

// ── RANSAC — chống điểm ngoại lai ─────────────────────────────────────
/// Dùng khi dữ liệu sensor có điểm rác: chạm nhầm, nhiễu hồng ngoại,
/// blob depth bắt sai. Lấy mẫu ngẫu nhiên 4 điểm nhiều lần, giữ mô hình
/// có nhiều inlier nhất, rồi tinh chỉnh lại bằng least-squares trên
/// TOÀN BỘ inlier.
HomographyResult solveRANSAC(const std::vector<CorrespondencePair>& pairs,
                             const RansacParams& params = {});

// ── Đánh giá ───────────────────────────────────────────────────────────
/// Sai số tái chiếu: khoảng cách giữa H·src và dst.
double reprojectionError(const Mat3& H, const Vec2& src, const Vec2& dst);

/// Tính RMS và max trên một tập cặp điểm.
void evaluate(const Mat3& H,
              const std::vector<CorrespondencePair>& pairs,
              double& outRms,
              double& outMax);

/// Kiểm tra 4 điểm có tạo thành tứ giác hợp lệ (lồi, không suy biến).
/// Corner pin bị kéo thành hình "nơ" (bow-tie) sẽ không nghịch đảo được
/// → phải chặn ngay trong UI thay vì để calibration hỏng âm thầm.
bool isValidQuad(const Vec2 quad[4], double minArea = 1e-6);

} // namespace homography
} // namespace mikmap

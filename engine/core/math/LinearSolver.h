// ════════════════════════════════════════════════════════════════════════
//  core/math/LinearSolver.h — giải hệ phương trình tuyến tính đặc
//
//  Khử Gauss có chọn phần tử trụ theo cột (partial pivoting).
//  Kích thước hệ ở đây rất nhỏ (8×8 cho homography, 2×2 cho inverse
//  bilinear) nên không cần thư viện đại số tuyến tính ngoài — giữ cho
//  core/ chỉ phụ thuộc STL đúng như ràng buộc kiến trúc.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <vector>

namespace hexmap {

/// Giải A·x = b với A là ma trận đặc n×n (lưu row-major, n*n phần tử).
///
/// A và b bị SỬA ĐỔI tại chỗ (biến thành dạng bậc thang) — truyền bản sao
/// nếu còn cần dữ liệu gốc.
///
/// @return false nếu ma trận suy biến (trụ ≈ 0) → hệ vô nghiệm hoặc vô số nghiệm.
bool solveLinearSystem(std::vector<double>& A,
                       std::vector<double>& b,
                       int n,
                       std::vector<double>& x);

} // namespace hexmap

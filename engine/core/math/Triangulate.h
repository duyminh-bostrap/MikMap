// ════════════════════════════════════════════════════════════════════════
//  core/math/Triangulate.h — chia đa giác đơn thành tam giác (ear clipping)
//
//  Nền hình học cho slice đa giác: ánh xạ điểm input → output từng tam giác
//  bằng toạ độ barycentric. Đa giác nhỏ (3..64 điểm) nên O(n²) là đủ.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <array>
#include <vector>

namespace mikmap::tri {

struct P { double x, y; };

// ≥ 3 điểm, không có hai cạnh cắt/chạm nhau (trừ hai cạnh kề chung đỉnh),
// không có cạnh dài 0 (điểm trùng liên tiếp), diện tích ≠ 0.
bool simple(const std::vector<P>& poly);

// Ear clipping, chiều quay nào cũng được. Ra n − 2 tam giác theo chỉ số gốc;
// điểm thẳng hàng được phép (tai diện tích 0 chỉ cắt khi không còn tai nào khác).
// false (out rỗng) khi !simple(poly).
bool triangulate(const std::vector<P>& poly, std::vector<std::array<int, 3>>& out);

// p = u·a + v·b + w·c, u + v + w = 1. false khi tam giác suy biến.
bool barycentric(P a, P b, P c, P p, double& u, double& v, double& w);

// Tam giác chứa p (dung sai cạnh 1e-9 theo tỉ lệ diện tích tam giác); false nếu không có.
bool locate(const std::vector<P>& poly, const std::vector<std::array<int, 3>>& tris, P p,
            int& tri, double& u, double& v, double& w);

}  // namespace mikmap::tri

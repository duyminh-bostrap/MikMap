#include "core/math/LinearSolver.h"

#include <cmath>
#include <cstddef>

namespace mikmap {

bool solveLinearSystem(std::vector<double>& A,
                       std::vector<double>& b,
                       int n,
                       std::vector<double>& x) {
    if (n <= 0) return false;
    const size_t N = static_cast<size_t>(n);
    if (A.size() < N * N || b.size() < N) return false;

    x.assign(N, 0.0);

    for (size_t col = 0; col < N; ++col) {
        // ── Chọn trụ: hàng có |giá trị| lớn nhất trong cột hiện tại ──
        // Không có bước này, hệ 8×8 của homography mất tới ~6 chữ số
        // có nghĩa khi 4 điểm gần suy biến.
        size_t pivotRow = col;
        double pivotMag = std::abs(A[col * N + col]);
        for (size_t r = col + 1; r < N; ++r) {
            const double mag = std::abs(A[r * N + col]);
            if (mag > pivotMag) {
                pivotMag = mag;
                pivotRow = r;
            }
        }

        if (pivotMag < 1e-14) return false;   // suy biến

        if (pivotRow != col) {
            for (size_t c = 0; c < N; ++c) {
                std::swap(A[col * N + c], A[pivotRow * N + c]);
            }
            std::swap(b[col], b[pivotRow]);
        }

        // ── Khử các hàng bên dưới ────────────────────────────────────
        const double pivot = A[col * N + col];
        for (size_t r = col + 1; r < N; ++r) {
            const double factor = A[r * N + col] / pivot;
            if (factor == 0.0) continue;
            for (size_t c = col; c < N; ++c) {
                A[r * N + c] -= factor * A[col * N + c];
            }
            b[r] -= factor * b[col];
        }
    }

    // ── Thế ngược ────────────────────────────────────────────────────
    for (size_t i = N; i-- > 0;) {
        double sum = b[i];
        for (size_t c = i + 1; c < N; ++c) {
            sum -= A[i * N + c] * x[c];
        }
        x[i] = sum / A[i * N + i];
    }

    return true;
}

} // namespace mikmap

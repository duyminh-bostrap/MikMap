#include "Homography.h"
#include <vector>
#include <cmath>

namespace HexMap::Core::Math {

// Giải hệ phương trình 8x8 bằng khử Gauss (Gaussian elimination)
static bool solve8x8(double A[8][8], double b[8], double x[8]) {
    for (int i = 0; i < 8; ++i) {
        // Tìm pivot
        int maxRow = i;
        double maxVal = std::abs(A[i][i]);
        for (int k = i + 1; k < 8; ++k) {
            if (std::abs(A[k][i]) > maxVal) {
                maxVal = std::abs(A[k][i]);
                maxRow = k;
            }
        }
        if (maxVal < 1e-12) return false;

        // Hoán vị hàng
        if (maxRow != i) {
            for (int k = i; k < 8; ++k) std::swap(A[i][k], A[maxRow][k]);
            std::swap(b[i], b[maxRow]);
        }

        // Khử
        for (int k = i + 1; k < 8; ++k) {
            double factor = A[k][i] / A[i][i];
            b[k] -= factor * b[i];
            for (int j = i; j < 8; ++j) {
                A[k][j] -= factor * A[i][j];
            }
        }
    }

    // Thế ngược
    for (int i = 7; i >= 0; --i) {
        double sum = b[i];
        for (int j = i + 1; j < 8; ++j) {
            sum -= A[i][j] * x[j];
        }
        x[i] = sum / A[i][i];
    }
    return true;
}

std::optional<Mat3> Homography::find4Point(const std::array<Vec2, 4>& src,
                                           const std::array<Vec2, 4>& dst) {
    double A[8][8] = { 0 };
    double b[8] = { 0 };

    for (int i = 0; i < 4; ++i) {
        double sx = src[i].x, sy = src[i].y;
        double dx = dst[i].x, dy = dst[i].y;

        // Hàng 2*i: x' = (h0*x + h1*y + h2) / (h6*x + h7*y + 1)
        A[2 * i][0] = sx;
        A[2 * i][1] = sy;
        A[2 * i][2] = 1.0;
        A[2 * i][3] = 0.0;
        A[2 * i][4] = 0.0;
        A[2 * i][5] = 0.0;
        A[2 * i][6] = -sx * dx;
        A[2 * i][7] = -sy * dx;
        b[2 * i]    = dx;

        // Hàng 2*i + 1: y' = (h3*x + h4*y + h5) / (h6*x + h7*y + 1)
        A[2 * i + 1][0] = 0.0;
        A[2 * i + 1][1] = 0.0;
        A[2 * i + 1][2] = 0.0;
        A[2 * i + 1][3] = sx;
        A[2 * i + 1][4] = sy;
        A[2 * i + 1][5] = 1.0;
        A[2 * i + 1][6] = -sx * dy;
        A[2 * i + 1][7] = -sy * dy;
        b[2 * i + 1]    = dy;
    }

    double h[8];
    if (!solve8x8(A, b, h)) {
        return std::nullopt;
    }

    Mat3 res;
    res.at(0, 0) = h[0]; res.at(0, 1) = h[1]; res.at(0, 2) = h[2];
    res.at(1, 0) = h[3]; res.at(1, 1) = h[4]; res.at(1, 2) = h[5];
    res.at(2, 0) = h[6]; res.at(2, 1) = h[7]; res.at(2, 2) = 1.0;

    return res;
}

} // namespace HexMap::Core::Math

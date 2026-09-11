// ════════════════════════════════════════════════════════════════════════
//  tests/TestHarness.h — khung unit test tối giản
//
//  Tự viết thay vì dùng Catch2/GoogleTest để giữ đúng ràng buộc kiến
//  trúc: core/ và bộ test của nó KHÔNG phụ thuộc bất kỳ thư viện ngoài
//  nào. Nhờ vậy `cmake -S . -B build && ctest` chạy được trên máy trắng,
//  không cần vcpkg, không cần tải gì.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <cmath>
#include <functional>
#include <string>
#include <vector>

namespace hextest {

struct TestCase {
    std::string           name;
    std::function<void()> fn;
};

std::vector<TestCase>& registry();

struct Registrar {
    Registrar(const char* name, std::function<void()> fn);
};

extern int g_checks;
extern int g_failuresInCurrentTest;

void reportFailure(const char* file, int line, const std::string& expr);
int  runAll();

inline bool nearlyEqual(double a, double b, double eps) {
    return std::abs(a - b) <= eps;
}

} // namespace hextest

// ── Macro ──────────────────────────────────────────────────────────────

#define HEX_CONCAT_(a, b) a##b
#define HEX_CONCAT(a, b)  HEX_CONCAT_(a, b)

/// Khai báo một test case. Tự đăng ký lúc khởi động chương trình.
#define TEST_CASE(displayName)                                                 \
    static void HEX_CONCAT(hexTestFn_, __LINE__)();                            \
    static ::hextest::Registrar HEX_CONCAT(hexTestReg_, __LINE__)(             \
        displayName, HEX_CONCAT(hexTestFn_, __LINE__));                        \
    static void HEX_CONCAT(hexTestFn_, __LINE__)()

/// Thất bại thì DỪNG test hiện tại (tránh nổ dây chuyền).
#define REQUIRE(cond)                                                          \
    do {                                                                       \
        ++::hextest::g_checks;                                                 \
        if (!(cond)) {                                                         \
            ::hextest::reportFailure(__FILE__, __LINE__, "REQUIRE(" #cond ")"); \
            return;                                                            \
        }                                                                      \
    } while (0)

/// Thất bại thì GHI NHẬN nhưng chạy tiếp — hữu ích khi kiểm nhiều giá trị.
#define CHECK(cond)                                                            \
    do {                                                                       \
        ++::hextest::g_checks;                                                 \
        if (!(cond)) {                                                         \
            ::hextest::reportFailure(__FILE__, __LINE__, "CHECK(" #cond ")");   \
        }                                                                      \
    } while (0)

/// Như CHECK nhưng kèm mô tả tính lúc chạy.
///
/// Cần khi thứ đang kiểm là một PHẦN TỬ trong tập hợp: "CHECK(a == b)"
/// chỉ cho biết có sai, không cho biết KHOÁ NÀO sai trong 190 khoá.
#define CHECK_MSG(cond, msg)                                                       do {                                                                               ++::hextest::g_checks;                                                         if (!(cond)) {                                                                     ::hextest::reportFailure(__FILE__, __LINE__,                                                            std::string("CHECK(" #cond ") - ")                                             + (msg));                                         }                                                                          } while (0)

#define REQUIRE_NEAR(a, b, eps)                                                \
    do {                                                                       \
        ++::hextest::g_checks;                                                 \
        if (!::hextest::nearlyEqual((a), (b), (eps))) {                        \
            ::hextest::reportFailure(                                          \
                __FILE__, __LINE__,                                            \
                std::string("REQUIRE_NEAR(" #a ", " #b ")  got ")              \
                    + std::to_string(static_cast<double>(a)) + " vs "          \
                    + std::to_string(static_cast<double>(b))                   \
                    + "  (eps=" + std::to_string(static_cast<double>(eps)) + ")"); \
            return;                                                            \
        }                                                                      \
    } while (0)

#define CHECK_NEAR(a, b, eps)                                                  \
    do {                                                                       \
        ++::hextest::g_checks;                                                 \
        if (!::hextest::nearlyEqual((a), (b), (eps))) {                        \
            ::hextest::reportFailure(                                          \
                __FILE__, __LINE__,                                            \
                std::string("CHECK_NEAR(" #a ", " #b ")  got ")                \
                    + std::to_string(static_cast<double>(a)) + " vs "          \
                    + std::to_string(static_cast<double>(b)));                 \
        }                                                                      \
    } while (0)

/// So sánh hai Vec2 theo từng thành phần.
#define REQUIRE_VEC_NEAR(v, ex, ey, eps)                                       \
    do {                                                                       \
        REQUIRE_NEAR((v).x, (ex), (eps));                                      \
        REQUIRE_NEAR((v).y, (ey), (eps));                                      \
    } while (0)

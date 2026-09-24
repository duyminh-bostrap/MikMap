#include "TestHarness.h"

#include <chrono>
#include <cstdio>

namespace hextest {

int g_checks = 0;
int g_failuresInCurrentTest = 0;

std::vector<TestCase>& registry() {
    // Khởi tạo lười (Meyers singleton) — tránh phụ thuộc thứ tự khởi tạo
    // biến tĩnh giữa các đơn vị biên dịch, vì các Registrar chạy trước main().
    static std::vector<TestCase> cases;
    return cases;
}

Registrar::Registrar(const char* name, std::function<void()> fn) {
    registry().push_back(TestCase{name, std::move(fn)});
}

void reportFailure(const char* file, int line, const std::string& expr) {
    ++g_failuresInCurrentTest;
    std::printf("      [FAIL] %s:%d\n             %s\n", file, line, expr.c_str());
    std::fflush(stdout);
}

int runAll() {
    int passed = 0;
    int failed = 0;

    std::printf("\n");
    std::printf("========================================================\n");
    std::printf("  MikMap — core unit tests\n");
    std::printf("========================================================\n\n");
    std::fflush(stdout);

    for (const TestCase& tc : registry()) {
        g_failuresInCurrentTest = 0;
        const int checksBefore = g_checks;

        // In tên test TRƯỚC khi chạy rồi flush ngay.
        // Nếu một test treo, dòng cuối trên màn hình chỉ đích danh nó —
        // không có bước này thì chỉ biết "treo ở đâu đó".
        std::printf("  ....    %s\n", tc.name.c_str());
        std::fflush(stdout);

        const auto t0 = std::chrono::steady_clock::now();
        tc.fn();
        const double ms = std::chrono::duration<double, std::milli>(
                              std::chrono::steady_clock::now() - t0).count();

        const int checksRun = g_checks - checksBefore;

        if (g_failuresInCurrentTest == 0) {
            std::printf("  [ OK ]  %-54s %5d checks  %7.1f ms\n",
                        tc.name.c_str(), checksRun, ms);
            ++passed;
        } else {
            std::printf("  [FAIL]  %-54s %5d failed  %7.1f ms\n",
                        tc.name.c_str(), g_failuresInCurrentTest, ms);
            ++failed;
        }
        std::fflush(stdout);
    }

    std::printf("\n--------------------------------------------------------\n");
    std::printf("  %d passed, %d failed   |   %d assertions total\n",
                passed, failed, g_checks);
    std::printf("========================================================\n\n");
    std::fflush(stdout);

    return (failed == 0) ? 0 : 1;
}

} // namespace hextest

int main() {
    return hextest::runAll();
}

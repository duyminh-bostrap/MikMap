#include "core/license/License.h"

#include <cstring>

namespace mikmap {
namespace license {

namespace {

constexpr int64_t kDay = 86400;

/// Ngày dân sự từ số ngày kể từ 1970-01-01 (thuật toán civil_from_days của
/// H. Hinnant, đúng cho cả ngày âm).
void civilFromDays(int64_t z, int& y, int& m, int& d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t yy  = yoe + era * 400;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp  = (5 * doy + 2) / 153;
    d = static_cast<int>(doy - (153 * mp + 2) / 5 + 1);
    m = static_cast<int>(mp < 10 ? mp + 3 : mp - 9);
    y = static_cast<int>(yy + (m <= 2 ? 1 : 0));
}

bool grants(State s) { return s == State::Licensed || s == State::Trial; }

} // namespace

const char* stateName(State s) {
    switch (s) {
        case State::None:         return "none";
        case State::Licensed:     return "licensed";
        case State::Trial:        return "trial";
        case State::Expired:      return "expired";
        case State::OtherMachine: return "other_machine";
        case State::Revoked:      return "revoked";
        case State::Invalid:      return "invalid";
    }
    return "none";
}

State stateFromName(const char* name) {
    if (!name) return State::None;
    static const State all[] = {State::None, State::Licensed, State::Trial, State::Expired,
                                State::OtherMachine, State::Revoked, State::Invalid};
    for (const State s : all) {
        if (std::strcmp(name, stateName(s)) == 0) return s;
    }
    return State::None;
}

int ymdFromUnix(int64_t unixSec) {
    // chia làm tròn xuống để giây Unix âm vẫn ra đúng ngày
    int64_t days = unixSec / kDay;
    if (unixSec % kDay < 0) --days;
    int y = 0, m = 0, d = 0;
    civilFromDays(days, y, m, d);
    return y * 10000 + m * 100 + d;
}

bool parseBuildDate(const char* text, int& ymd) {
    if (!text) return false;
    // đúng "YYYY-MM-DD", mười ký tự
    if (std::strlen(text) != 10 || text[4] != '-' || text[7] != '-') return false;
    int digits[8];
    const int at[8] = {0, 1, 2, 3, 5, 6, 8, 9};
    for (int i = 0; i < 8; ++i) {
        const char c = text[at[i]];
        if (c < '0' || c > '9') return false;
        digits[i] = c - '0';
    }
    const int y = digits[0] * 1000 + digits[1] * 100 + digits[2] * 10 + digits[3];
    const int m = digits[4] * 10 + digits[5];
    const int d = digits[6] * 10 + digits[7];
    if (y < 1970 || m < 1 || m > 12 || d < 1 || d > 31) return false;
    ymd = y * 10000 + m * 100 + d;
    return true;
}

Entitlements evaluate(const Claims& c, int buildYmd, int64_t serverNow) {
    Entitlements none;                       // mặc định: không license
    if (!grants(c.state)) return none;
    if (c.expiresAt != 0 && c.expiresAt <= serverNow) return none;

    // Trial không bị ràng buộc bởi quyền cập nhật.
    if (c.state == State::Licensed && c.updatesUntil != 0) {
        if (buildYmd <= 0) return none;      // không biết ngày build thì không dám tin
        if (buildYmd > ymdFromUnix(c.updatesUntil)) return none;
    }

    Entitlements ok;
    ok.watermark = false;
    ok.sensorOut = true;
    ok.ndiOut    = true;
    return ok;
}

Entitlements evaluateCached(const Claims& c, int64_t lastCheckUnix, int64_t nowUnix,
                            int buildYmd, int graceDays) {
    if (graceDays < 0) graceDays = 0;
    if (nowUnix < lastCheckUnix - kDay) return Entitlements{};          // đồng hồ bị lùi
    if (nowUnix - lastCheckUnix > static_cast<int64_t>(graceDays) * kDay) return Entitlements{};
    return evaluate(c, buildYmd, nowUnix);
}

} // namespace license
} // namespace mikmap

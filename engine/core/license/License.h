// ════════════════════════════════════════════════════════════════════════
//  core/license/License.h — "bản này được làm gì?"
//
//  Hàm THUẦN, chỉ dùng STL: không mạng, không đĩa, không đồng hồ. Máy chủ
//  (web) quyết định ai có license, loại gì, bao lâu; ứng dụng chỉ nhận kết
//  quả (Claims) và hỏi hàm này ba câu: có watermark không, có gửi sensor ra
//  ngoài không, có gửi NDI không.
//
//  Mặc định khi không rõ là KHÔNG có license. Mọi giờ giấc là giây Unix của
//  MÁY CHỦ (server_now), không phải đồng hồ máy, trừ evaluateCached() là
//  nơi duy nhất dùng giờ máy, vì lúc đó không có mạng.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstdint>

namespace mikmap {
namespace license {

/// Trạng thái máy chủ trả về cho một (key, mã máy).
enum class State {
    None,          // chưa có key / key chưa kích hoạt
    Licensed,      // key trả phí hợp lệ cho máy này
    Trial,         // dùng thử hợp lệ cho máy này
    Expired,       // hết hạn dùng
    OtherMachine,  // key đang gắn với máy khác
    Revoked,       // key bị thu hồi
    Invalid,       // không phải key do web cấp
};

const char* stateName(State s);
/// Tên lạ hoặc null -> State::None (không bao giờ bật quyền vì chuỗi lạ).
State stateFromName(const char* name);

/// Nội dung kết quả kiểm tra. 0 nghĩa là "không có giới hạn đó".
struct Claims {
    State   state        = State::None;
    int64_t expiresAt    = 0;   // hết hạn DÙNG (giây Unix, giờ máy chủ)
    int64_t updatesUntil = 0;   // bản build phát hành SAU ngày này không được phủ
};

/// Ba quyền cần thiết. Mặc định = không có license.
struct Entitlements {
    bool watermark = true;
    bool sensorOut = false;
    bool ndiOut    = false;

    bool licensed() const { return !watermark; }
};

/// Ngày UTC của một mốc giây Unix, dạng số YYYYMMDD (ví dụ 20261010).
int ymdFromUnix(int64_t unixSec);

/// "YYYY-MM-DD" -> YYYYMMDD. Sai định dạng/null -> false, ymd giữ nguyên.
bool parseBuildDate(const char* text, int& ymd);

/// Quyết định chính. buildYmd là ngày build của CHÍNH bản đang chạy
/// (BUILD_DATE); <= 0 nghĩa là không biết.
///  * Licensed/Trial thì mới có quyền; mọi trạng thái khác thì không.
///  * expiresAt đã qua (so với serverNow) thì không.
///  * Licensed có updatesUntil: bản build phát hành sau ngày đó thì chạy như
///    không license. Trial không xét updatesUntil.
///  * buildYmd không biết mà có updatesUntil thì coi là không được phủ.
Entitlements evaluate(const Claims& c, int buildYmd, int64_t serverNow);

/// Như evaluate() nhưng cho lúc MẤT MẠNG: tin kết quả lần kiểm tra thành
/// công cuối (lúc lastCheckUnix) tối đa graceDays ngày, và không bao giờ quá
/// expiresAt. nowUnix là giờ máy (chỉ ở đây mới dùng). Đồng hồ bị lùi quá
/// một ngày so với lần kiểm tra cuối thì không tin.
Entitlements evaluateCached(const Claims& c, int64_t lastCheckUnix, int64_t nowUnix,
                            int buildYmd, int graceDays = 30);

} // namespace license
} // namespace mikmap

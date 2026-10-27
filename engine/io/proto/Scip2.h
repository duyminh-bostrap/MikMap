// ════════════════════════════════════════════════════════════════════════
//  io/proto/Scip2.h — giải mã SCIP 2.0 (Hokuyo URG/UST), KHÔNG socket
//
//  Tách khỏi TCP vì hai lý do:
//    · hàm thuần → test được đầy đủ, không cần lidar thật;
//    · TCP cắt dòng/khối ở bất kỳ byte nào, kể cả giữa một nhóm 3 ký tự.
//      MdStream giữ buffer và chỉ trả kết quả khi đã đủ một block.
//
//  ── Định dạng liên quan ─────────────────────────────────────────────
//    Lệnh MD : "MD" + start(4) + end(4) + "01000" + "\n"
//    MD trả lời, mỗi block:
//      [echo "MD…"]            (chỉ block đầu, không checksum)
//      99b                     status 2 ký tự + checksum
//      <timestamp 4 ký tự><sum>
//      <dữ liệu ≤64 ký tự><sum>   (nhiều dòng; nhóm 3 ký tự có thể vắt dòng)
//      <dòng rỗng>
//    Status "00" = ack, không có dữ liệu. Status khác "99" = lỗi.
//
//  Checksum một dòng = (tổng các byte TRƯỚC ký tự cuối & 0x3F) + 0x30.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <string_view>

namespace mikmap::scip2 {

/// Giải mã nChars ký tự SCIP: mỗi ký tự (c - 0x30) & 0x3F, ghép 6 bit big-endian.
uint32_t decode(const char* p, int nChars);

/// Ký tự cuối dòng có đúng là checksum của phần trước nó không.
bool lineChecksumOk(std::string_view line);

/// Lệnh MD lấy các bước startStep..endStep, quét liên tục.
std::string mdCommand(int startStep, int endStep);

/// Tham số PP của thiết bị (một lần lúc kết nối).
struct DeviceParams {
    int dmin = 0, dmax = 0, ares = 0, amin = 0, amax = 0, afrt = 0;
};

/// Đọc các dòng "KEY:VALUE;<sum>" (PP). KHÔNG kiểm checksum: block chỉ đọc
/// một lần lúc kết nối, giá trị hỏng sẽ bị bắt ở kiểm tra ares>0 / amax>amin.
/// @return true nếu có ít nhất một khoá đã đọc được.
bool parsePP(std::string_view block, DeviceParams& out);

enum class MdResult { None, Scan, Error };

/// Bộ ghép block MD từ luồng byte TCP. feed() được gọi với đoạn bất kỳ;
/// next() trả tối đa một Scan (hoặc Error) mỗi lần gọi.
class MdStream {
public:
    MdStream();

    void feed(const char* data, size_t n);

    /// Lấy một block hoàn chỉnh.
    /// @param ranges    đệm đích, tối đa maxOut phần tử
    /// @param count     số khoảng cách đọc được (0 khi None/Error)
    /// @param timestamp timestamp 4 ký tự của block (chỉ có nghĩa khi Scan)
    MdResult next(uint32_t* ranges, int maxOut, int& count, uint32_t& timestamp);

    /// Khi next() trả Error: "checksum", "MD status XX" hoặc "data length".
    const std::string& error() const { return m_err; }

    void reset();

private:
    enum class State { Status, Timestamp, Data, Skip };

    State       m_state = State::Status;
    std::string m_buf;       // byte chưa thành dòng
    std::string m_data;      // dữ liệu đã bỏ checksum, chưa cắt nhóm 3
    std::string m_err;
    uint32_t    m_ts = 0;
    bool        m_failed = false;   // block hiện tại đã lỗi; trả Error ở dòng rỗng
};

} // namespace mikmap::scip2

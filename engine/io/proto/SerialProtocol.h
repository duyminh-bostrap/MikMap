// ════════════════════════════════════════════════════════════════════════
//  io/proto/SerialProtocol.h — phương ngữ dòng lệnh cho Arduino (G3)
//
//  ── Vì sao là ASCII từng dòng, không phải nhị phân ───────────────────
//  Nhị phân gọn hơn và nhanh hơn. Nhưng thứ quyết định một dự án Arduino
//  chạy được hay không là khả năng MỞ SERIAL MONITOR RA NHÌN. Với ASCII,
//  người làm phần cứng cắm dây vào là thấy ngay thiết bị đang gửi gì, và
//  gõ tay một dòng để thử phần mềm mà không cần nạp firmware.
//
//  Ở 115200 baud, một dòng "T 3 812 455" là 12 byte — 40 điểm ở 60 Hz vẫn
//  chỉ chiếm khoảng 3% băng thông. Chỗ này không phải nút thắt.
//
//  ── Phương ngữ ───────────────────────────────────────────────────────
//      T <id> <x> <y>     điểm chạm (xuống hoặc di chuyển)
//      U <id>             nhấc lên
//      C                  xoá tất cả
//
//  Toạ độ là SỐ NGUYÊN theo đơn vị sensor của thiết bị — không phải
//  [0,1]. Arduino làm số nguyên nhanh và không có rủi ro định dạng float
//  khác nhau giữa các nền tảng.
//
//  ── ★ Cái bẫy thật của serial: DÒNG BỊ CẮT NGANG ─────────────────────
//  Serial KHÔNG giao hàng theo dòng. Một lần read() trả về đúng những byte
//  vừa tới — có thể là nửa dòng, có thể là hai dòng rưỡi. Ai giả định
//  "mỗi lần đọc là một dòng" sẽ có phần mềm chạy hoàn hảo trên bàn (dữ
//  liệu thưa, mỗi dòng tới trọn vẹn) rồi hỏng ngay khi cắm thiết bị thật
//  gửi nhanh — và hỏng theo kiểu mất rải rác vài điểm, rất khó lần ra.
//
//  Bộ này giữ phần dòng dở lại giữa các lần nạp, nên nạp theo lô nào cũng
//  cho cùng một kết quả.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace mikmap {

struct SerialCommand {
    enum class Kind { Touch, Up, Clear };

    Kind    kind = Kind::Touch;
    int32_t id = 0;
    double  x = 0.0;
    double  y = 0.0;
};

class SerialProtocol {
public:
    /// Nạp một lô byte vừa đọc từ cổng. Lô có thể cắt ngang dòng ở bất kỳ
    /// đâu; phần dở được giữ lại cho lần sau.
    ///
    /// @param out  lệnh đọc được, ĐƯỢC THÊM vào (không xoá nội dung cũ)
    void feed(const uint8_t* data, size_t size, std::vector<SerialCommand>& out);

    /// Quên phần dòng dở. Gọi khi mở lại cổng — byte còn sót từ phiên
    /// trước gần như chắc chắn là rác.
    void reset();

    /// Số dòng bị bỏ vì không đọc được. Hiện trên PerfPanel để người làm
    /// phần cứng biết firmware của mình có đang gửi rác không.
    uint64_t badLines() const { return m_badLines; }

    /// Trần độ dài một dòng.
    ///
    /// ★ Không có trần này, một thiết bị gửi rác không có ký tự xuống dòng
    ///   sẽ làm bộ đệm phình vô hạn cho tới khi hết RAM. Thiết bị hỏng thì
    ///   phải làm mất dữ liệu, không được làm sập phần mềm điều khiển.
    static constexpr size_t kMaxLine = 128;

private:
    void parseLine(const std::string& line, std::vector<SerialCommand>& out);

    std::string m_partial;
    bool        m_overflow = false;   ///< đang bỏ phần còn lại của dòng quá dài
    uint64_t    m_badLines = 0;
};

} // namespace mikmap

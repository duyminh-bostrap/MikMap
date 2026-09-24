#include "io/proto/SerialProtocol.h"

#include <cstdlib>

namespace mikmap {
namespace {

/// Tách một dòng thành các từ ngăn bởi khoảng trắng.
///
/// Tự viết thay vì dùng istringstream: chỗ này chạy cho MỌI dòng của mọi
/// frame, và istringstream cấp phát ở mỗi lần dựng. Ở 60 Hz × 40 điểm thì
/// đó là 2400 lần cấp phát mỗi giây cho một việc không cần cấp phát nào.
void splitWords(const std::string& s, std::vector<std::string>& out) {
    out.clear();
    size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && (s[i] == ' ' || s[i] == '\t')) ++i;
        const size_t start = i;
        while (i < s.size() && s[i] != ' ' && s[i] != '\t') ++i;
        if (i > start) out.push_back(s.substr(start, i - start));
    }
}

/// Đọc số nguyên có dấu. Trả về false nếu chuỗi KHÔNG phải số hợp lệ.
///
/// ★ Không dùng std::atoi: nó trả 0 cho chuỗi rác, nên "T abc def" sẽ
///   thành một điểm chạm ở (0,0) — góc trên-trái là toạ độ HỢP LỆ, nên
///   rác từ firmware sẽ hiện ra như những cú chạm thật ở góc màn hình.
bool parseInt(const std::string& s, long& out) {
    if (s.empty()) return false;

    size_t i = 0;
    if (s[0] == '+' || s[0] == '-') i = 1;
    if (i >= s.size()) return false;

    for (size_t k = i; k < s.size(); ++k) {
        if (s[k] < '0' || s[k] > '9') return false;
    }

    out = std::strtol(s.c_str(), nullptr, 10);
    return true;
}

} // namespace

void SerialProtocol::reset() {
    m_partial.clear();
    m_overflow = false;
}

void SerialProtocol::feed(const uint8_t* data, size_t size,
                          std::vector<SerialCommand>& out) {
    if (data == nullptr) return;

    for (size_t i = 0; i < size; ++i) {
        const char c = static_cast<char>(data[i]);

        // Nhận cả "\n" lẫn "\r\n": Arduino IDE mặc định gửi CRLF, còn
        // firmware viết tay thường chỉ gửi LF. Bắt người dùng chọn đúng
        // là một cái bẫy không cần thiết.
        if (c == '\n' || c == '\r') {
            if (m_overflow) {
                // Dòng quá dài đã bị bỏ; đây là chỗ nó kết thúc.
                m_overflow = false;
                m_partial.clear();
                continue;
            }
            if (!m_partial.empty()) {
                parseLine(m_partial, out);
                m_partial.clear();
            }
            continue;
        }

        if (m_overflow) continue;

        if (m_partial.size() >= kMaxLine) {
            // ★ Thiết bị hỏng thì phải làm MẤT DỮ LIỆU, không được làm
            //   sập phần mềm điều khiển. Bỏ cả dòng và đợi ký tự xuống
            //   dòng kế tiếp để đồng bộ lại.
            m_overflow = true;
            m_partial.clear();
            ++m_badLines;
            continue;
        }

        m_partial.push_back(c);
    }
}

void SerialProtocol::parseLine(const std::string& line,
                               std::vector<SerialCommand>& out) {
    std::vector<std::string> w;
    splitWords(line, w);
    if (w.empty()) return;

    const std::string& cmd = w[0];

    if (cmd == "C" || cmd == "c") {
        SerialCommand c;
        c.kind = SerialCommand::Kind::Clear;
        out.push_back(c);
        return;
    }

    if (cmd == "U" || cmd == "u") {
        long id = 0;
        if (w.size() < 2 || !parseInt(w[1], id)) { ++m_badLines; return; }

        SerialCommand c;
        c.kind = SerialCommand::Kind::Up;
        c.id = static_cast<int32_t>(id);
        out.push_back(c);
        return;
    }

    if (cmd == "T" || cmd == "t") {
        long id = 0, x = 0, y = 0;
        if (w.size() < 4
            || !parseInt(w[1], id) || !parseInt(w[2], x) || !parseInt(w[3], y)) {
            // ★ Thiếu hoặc sai một trường thì BỎ CẢ DÒNG, không lấy phần
            //   đọc được. Một điểm chạm với y bịa bằng 0 trông y hệt một
            //   cú chạm thật ở mép trên màn hình.
            ++m_badLines;
            return;
        }

        SerialCommand c;
        c.kind = SerialCommand::Kind::Touch;
        c.id = static_cast<int32_t>(id);
        c.x  = static_cast<double>(x);
        c.y  = static_cast<double>(y);
        out.push_back(c);
        return;
    }

    // Dòng lạ: rất có thể là log gỡ lỗi của firmware ("Serial.println("boot
    // ok")"). Đếm vào badLines để người làm phần cứng thấy con số tăng và
    // biết mình đang gửi lẫn thứ khác, nhưng không làm gì thêm.
    ++m_badLines;
}

} // namespace mikmap

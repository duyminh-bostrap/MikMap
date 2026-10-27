#include "io/proto/Scip2.h"

#include <charconv>
#include <cstdio>
#include <system_error>

namespace mikmap::scip2 {
namespace {

bool parseInt(std::string_view s, int& out) {
    if (s.empty()) return false;
    int v = 0;
    const auto res = std::from_chars(s.data(), s.data() + s.size(), v);
    if (res.ec != std::errc() || res.ptr != s.data() + s.size()) return false;
    out = v;
    return true;
}

} // namespace

uint32_t decode(const char* p, int nChars) {
    uint32_t v = 0;
    for (int i = 0; i < nChars; ++i) {
        const uint32_t c = static_cast<unsigned char>(p[i]);
        v = (v << 6) | ((c - 0x30u) & 0x3Fu);
    }
    return v;
}

bool lineChecksumOk(std::string_view line) {
    if (line.size() < 2) return false;

    unsigned sum = 0;
    for (size_t i = 0; i + 1 < line.size(); ++i)
        sum += static_cast<unsigned char>(line[i]);

    return line.back() == static_cast<char>((sum & 0x3Fu) + 0x30u);
}

std::string mdCommand(int startStep, int endStep) {
    char buf[32];
    std::snprintf(buf, sizeof buf, "MD%04d%04d01000\n", startStep, endStep);
    return buf;
}

bool parsePP(std::string_view block, DeviceParams& out) {
    bool any = false;

    while (!block.empty()) {
        const size_t nl = block.find('\n');
        std::string_view line = block.substr(0, nl);
        block.remove_prefix(nl == std::string_view::npos ? block.size() : nl + 1);

        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);

        const size_t colon = line.find(':');
        if (colon == std::string_view::npos) continue;

        const size_t semi = line.find(';', colon + 1);
        if (semi == std::string_view::npos) continue;

        int v = 0;
        if (!parseInt(line.substr(colon + 1, semi - colon - 1), v)) continue;

        const std::string_view key = line.substr(0, colon);
        if (key == "DMIN") { out.dmin = v; any = true; }
        else if (key == "DMAX") { out.dmax = v; any = true; }
        else if (key == "ARES") { out.ares = v; any = true; }
        else if (key == "AMIN") { out.amin = v; any = true; }
        else if (key == "AMAX") { out.amax = v; any = true; }
        else if (key == "AFRT") { out.afrt = v; any = true; }
        // MODL/SCAN/... không dùng cho LaserScan 2D.
    }

    return any;
}

MdStream::MdStream() {
    m_buf.reserve(64 * 1024);
    m_data.reserve(4 * 1024);
}

void MdStream::feed(const char* data, size_t n) {
    if (!data || n == 0) return;
    m_buf.append(data, n);
}

void MdStream::reset() {
    m_state = State::Status;
    m_buf.clear();
    m_data.clear();
    m_err.clear();
    m_ts = 0;
    m_failed = false;
}

MdResult MdStream::next(uint32_t* ranges, int maxOut, int& count, uint32_t& timestamp) {
    count = 0;
    if (maxOut < 0) maxOut = 0;

    for (;;) {
        const size_t nl = m_buf.find('\n');
        if (nl == std::string::npos) return MdResult::None;

        std::string line = m_buf.substr(0, nl);
        m_buf.erase(0, nl + 1);

        switch (m_state) {
        case State::Status: {
            // Dòng echo "MD…" của lệnh không có checksum và chỉ đến một lần.
            if (line.rfind("MD", 0) == 0) continue;

            if (line.size() < 3) continue;   // dòng rác giữa các block

            if (!lineChecksumOk(line)) {
                m_err = "checksum";
                m_failed = true;
                m_state = State::Skip;
                continue;
            }

            const std::string status = line.substr(0, 2);
            if (status == "00") {           // ack: không có dữ liệu
                m_failed = false;
                m_state = State::Skip;
                continue;
            }
            if (status != "99") {
                m_err = "MD status " + status;
                m_failed = true;
                m_state = State::Skip;
                continue;
            }

            m_err.clear();
            m_failed = false;
            m_state = State::Timestamp;
            break;
        }

        case State::Timestamp:
            if (line.size() < 5 || !lineChecksumOk(line)) {
                m_err = "checksum";
                m_failed = true;
                m_state = State::Skip;
                continue;
            }
            m_ts = decode(line.data(), 4);
            m_data.clear();
            m_state = State::Data;
            break;

        case State::Data:
            if (line.empty()) {             // hết block
                m_state = State::Status;
                if (m_failed) {
                    m_failed = false;
                    return MdResult::Error;
                }
                if (m_data.size() % 3 != 0) {
                    m_err = "data length";
                    return MdResult::Error;
                }

                int n = static_cast<int>(m_data.size() / 3);
                if (n > maxOut) n = maxOut;
                for (int i = 0; i < n; ++i)
                    ranges[i] = decode(m_data.data() + i * 3, 3);

                count = n;
                timestamp = m_ts;
                return MdResult::Scan;
            }

            if (!lineChecksumOk(line)) {
                m_err = "checksum";
                m_failed = true;
                m_state = State::Skip;
                continue;
            }
            // Bỏ ký tự checksum cuối, nối phần còn lại: nhóm 3 ký tự có thể
            // bắt đầu ở dòng trước và kết thúc ở dòng này.
            m_data.append(line.data(), line.size() - 1);
            break;

        case State::Skip:
            if (line.empty()) {
                m_state = State::Status;
                if (m_failed) {
                    m_failed = false;
                    return MdResult::Error;
                }
                // ack: bỏ xong block, thử block kế tiếp đã có trong buffer.
            }
            break;
        }
    }
}

} // namespace mikmap::scip2

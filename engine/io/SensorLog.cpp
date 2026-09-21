#include "io/SensorLog.h"

#include <cstdio>
#include <cstring>

namespace hexmap {
namespace {

constexpr char     kMagic[8]   = {'H','E','X','S','L','O','G','1'};
constexpr uint32_t kVersion    = 1;

/// ★ Trần cho một gói: 64 KB, đúng bằng giới hạn payload của UDP.
///
///   Không có trần này, một file hỏng khai độ dài 4 tỉ byte sẽ làm
///   `resize()` cố cấp phát 4 GB — chương trình chết vì hết bộ nhớ khi
///   người dùng chỉ định mở một file log. Đọc file là chỗ dữ liệu KHÔNG
///   đáng tin, kể cả khi chính ta đã ghi nó ra.
constexpr uint32_t kMaxPacket = 65536;

void putU32(std::FILE* f, uint32_t v) {
    uint8_t b[4] = {static_cast<uint8_t>( v        & 0xFF),
                    static_cast<uint8_t>((v >>  8) & 0xFF),
                    static_cast<uint8_t>((v >> 16) & 0xFF),
                    static_cast<uint8_t>((v >> 24) & 0xFF)};
    std::fwrite(b, 1, 4, f);
}

void putI64(std::FILE* f, int64_t v) {
    const auto u = static_cast<uint64_t>(v);
    uint8_t b[8];
    for (int i = 0; i < 8; ++i) b[i] = static_cast<uint8_t>((u >> (i * 8)) & 0xFF);
    std::fwrite(b, 1, 8, f);
}

bool getU32(std::FILE* f, uint32_t& out) {
    uint8_t b[4];
    if (std::fread(b, 1, 4, f) != 4) return false;
    out = static_cast<uint32_t>(b[0])
        | (static_cast<uint32_t>(b[1]) <<  8)
        | (static_cast<uint32_t>(b[2]) << 16)
        | (static_cast<uint32_t>(b[3]) << 24);
    return true;
}

bool getI64(std::FILE* f, int64_t& out) {
    uint8_t b[8];
    if (std::fread(b, 1, 8, f) != 8) return false;
    uint64_t u = 0;
    for (int i = 0; i < 8; ++i) u |= static_cast<uint64_t>(b[i]) << (i * 8);
    out = static_cast<int64_t>(u);
    return true;
}

} // namespace

// ── Ghi ────────────────────────────────────────────────────────────────

SensorLogWriter::~SensorLogWriter() { close(); }

bool SensorLogWriter::open(const std::string& path) {
    close();

    std::FILE* f = std::fopen(path.c_str(), "wb");
    if (f == nullptr) return false;

    std::fwrite(kMagic, 1, sizeof(kMagic), f);
    putU32(f, kVersion);

    m_file      = f;
    m_haveFirst = false;
    m_tFirst    = 0;
    m_count     = 0;
    return true;
}

void SensorLogWriter::close() {
    if (m_file == nullptr) return;
    std::fclose(static_cast<std::FILE*>(m_file));
    m_file = nullptr;
}

void SensorLogWriter::write(int64_t tNs, const uint8_t* data, size_t size) {
    if (m_file == nullptr || data == nullptr) return;
    if (size == 0 || size > kMaxPacket) return;

    if (!m_haveFirst) {
        m_tFirst    = tNs;
        m_haveFirst = true;
    }

    std::FILE* f = static_cast<std::FILE*>(m_file);
    putI64(f, tNs - m_tFirst);
    putU32(f, static_cast<uint32_t>(size));
    std::fwrite(data, 1, size, f);
    ++m_count;
}

// ── Đọc ────────────────────────────────────────────────────────────────

bool readSensorLog(const std::string& path,
                   std::vector<LoggedPacket>& out,
                   std::string& outWarning) {
    outWarning.clear();

    std::FILE* f = std::fopen(path.c_str(), "rb");
    if (f == nullptr) return false;

    char magic[8] = {};
    uint32_t version = 0;
    if (std::fread(magic, 1, sizeof(magic), f) != sizeof(magic)
        || std::memcmp(magic, kMagic, sizeof(kMagic)) != 0
        || !getU32(f, version)) {
        std::fclose(f);
        return false;
    }

    if (version > kVersion) {
        // Vẫn thử đọc: định dạng chỉ THÊM trường ở cuối bản ghi, nên bản
        // mới hơn vẫn đọc được phần đầu. Nhưng phải nói rõ.
        outWarning = "File log ban " + std::to_string(version)
                   + " moi hon ban doc duoc (" + std::to_string(kVersion) + ")";
    }

    while (true) {
        int64_t  t = 0;
        uint32_t n = 0;
        if (!getI64(f, t)) break;         // hết file bình thường
        if (!getU32(f, n)) {
            outWarning = "File log cut giua ban ghi — van dung duoc phan doc duoc";
            break;
        }
        if (n == 0 || n > kMaxPacket) {
            outWarning = "File log co do dai goi vo ly (" + std::to_string(n)
                       + " byte) — dung doc tai day";
            break;
        }

        LoggedPacket p;
        p.tOffsetNs = t;
        p.data.resize(n);
        if (std::fread(p.data.data(), 1, n, f) != n) {
            outWarning = "File log cut giua goi — van dung duoc phan doc duoc";
            break;
        }
        out.push_back(std::move(p));
    }

    std::fclose(f);
    return true;
}

// ── Phát lại ───────────────────────────────────────────────────────────

size_t collectDuePackets(const std::vector<LoggedPacket>& log,
                         int64_t elapsedNs,
                         size_t& cursor,
                         std::vector<const LoggedPacket*>& out) {
    size_t added = 0;

    // ★ Vòng WHILE chứ không phải IF: một lần gọi có thể phải bơm NHIỀU
    //   gói. Sensor 40 Hz mà render chạy 60 fps thì thường là một gói mỗi
    //   frame — nhưng chỉ cần một lần khựng (nạp media, đổi cửa sổ) là đã
    //   dồn hàng chục gói. Bơm mỗi lần một gói thì bản phát lại sẽ TỤT
    //   HẬU dần và không bao giờ đuổi kịp, tức là mất đúng tính chất
    //   quan trọng nhất của replay: đúng thời điểm.
    while (cursor < log.size() && log[cursor].tOffsetNs <= elapsedNs) {
        out.push_back(&log[cursor]);
        ++cursor;
        ++added;
    }
    return added;
}

} // namespace hexmap

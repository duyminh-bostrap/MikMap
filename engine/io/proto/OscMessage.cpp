#include "io/proto/OscMessage.h"

#include <cstring>

namespace mikmap {
namespace {

/// OSC đệm mọi thứ tới bội số của 4.
size_t padTo4(size_t n) { return (n + 3u) & ~size_t(3); }

/// Đọc int32 big-endian.
bool readInt32(const uint8_t* d, size_t size, size_t& pos, int32_t& out) {
    if (pos + 4 > size) return false;
    out = static_cast<int32_t>((static_cast<uint32_t>(d[pos])     << 24) |
                               (static_cast<uint32_t>(d[pos + 1]) << 16) |
                               (static_cast<uint32_t>(d[pos + 2]) << 8)  |
                               (static_cast<uint32_t>(d[pos + 3])));
    pos += 4;
    return true;
}

bool readFloat32(const uint8_t* d, size_t size, size_t& pos, float& out) {
    int32_t bits = 0;
    if (!readInt32(d, size, pos, bits)) return false;
    // Không dùng reinterpret_cast — memcpy là cách duy nhất đúng chuẩn
    // để diễn giải lại bit mà không vi phạm strict aliasing.
    std::memcpy(&out, &bits, sizeof(out));
    return true;
}

/// Đọc chuỗi kết thúc null, đã đệm.
bool readString(const uint8_t* d, size_t size, size_t& pos, std::string& out) {
    if (pos >= size) return false;

    size_t end = pos;
    while (end < size && d[end] != '\0') ++end;
    if (end >= size) return false;              // không có null kết thúc

    out.assign(reinterpret_cast<const char*>(d + pos), end - pos);

    const size_t rawLen = (end - pos) + 1;      // kể cả null
    pos += padTo4(rawLen);
    return pos <= size;
}

bool parseMessage(const uint8_t* d, size_t size, std::vector<OscMessage>& out) {
    size_t pos = 0;

    OscMessage msg;
    if (!readString(d, size, pos, msg.address)) return false;
    if (msg.address.empty() || msg.address[0] != '/') return false;

    // Type tag là tuỳ chọn trong OSC 1.0 — thiếu nghĩa là không có tham số.
    if (pos >= size) {
        out.push_back(std::move(msg));
        return true;
    }

    std::string tags;
    if (!readString(d, size, pos, tags)) return false;
    if (tags.empty() || tags[0] != ',') return false;

    for (size_t i = 1; i < tags.size(); ++i) {
        OscArg a;
        switch (tags[i]) {
        case 'i':
            a.type = OscArg::Type::Int32;
            if (!readInt32(d, size, pos, a.i)) return false;
            break;
        case 'f':
            a.type = OscArg::Type::Float32;
            if (!readFloat32(d, size, pos, a.f)) return false;
            break;
        case 's':
            a.type = OscArg::Type::String;
            if (!readString(d, size, pos, a.s)) return false;
            break;
        case 'T': a.type = OscArg::Type::Int32; a.i = 1; break;   // true
        case 'F': a.type = OscArg::Type::Int32; a.i = 0; break;   // false
        case 'N': case 'I': a.type = OscArg::Type::Int32; a.i = 0; break;
        case 'b': {
            // Blob: bỏ qua nội dung nhưng phải nhảy đúng số byte, nếu
            // không mọi tham số phía sau sẽ lệch.
            int32_t blobLen = 0;
            if (!readInt32(d, size, pos, blobLen)) return false;
            if (blobLen < 0) return false;
            const size_t skip = padTo4(static_cast<size_t>(blobLen));
            if (pos + skip > size) return false;
            pos += skip;
            continue;   // không thêm tham số
        }
        default:
            // Kiểu chưa hỗ trợ: không biết nó dài bao nhiêu byte nên
            // không thể nhảy qua an toàn. Dừng ở đây và giữ những tham
            // số đã đọc được.
            out.push_back(std::move(msg));
            return true;
        }
        msg.args.push_back(std::move(a));
    }

    out.push_back(std::move(msg));
    return true;
}

// Khai báo trước: bundle và packet gọi lẫn nhau, và ĐỘ SÂU phải được
// truyền qua lại. Nếu để bundle gọi thẳng parseOscPacket() (vốn luôn đặt
// depth = 0), giới hạn độ sâu sẽ vô tác dụng và một gói tin lồng nhau
// đủ sâu vẫn làm tràn ngăn xếp.
bool parsePacketImpl(const uint8_t* d, size_t size, std::vector<OscMessage>& out, int depth);

bool parseBundle(const uint8_t* d, size_t size, std::vector<OscMessage>& out, int depth) {
    if (depth > 8) return false;

    size_t pos = 0;
    std::string tag;
    if (!readString(d, size, pos, tag)) return false;
    if (tag != "#bundle") return false;

    pos += 8;                                   // bỏ qua timetag
    if (pos > size) return false;

    bool allOk = true;
    while (pos < size) {
        int32_t elemSize = 0;
        if (!readInt32(d, size, pos, elemSize)) return false;
        if (elemSize <= 0) return false;
        if (pos + static_cast<size_t>(elemSize) > size) return false;

        if (!parsePacketImpl(d + pos, static_cast<size_t>(elemSize), out, depth + 1)) {
            allOk = false;   // phần tử hỏng — bỏ qua, xử lý tiếp phần còn lại
        }
        pos += static_cast<size_t>(elemSize);
    }
    return allOk;
}

bool parsePacketImpl(const uint8_t* d, size_t size, std::vector<OscMessage>& out, int depth) {
    if (d == nullptr || size < 4) return false;

    if (size >= 8 && std::memcmp(d, "#bundle", 7) == 0 && d[7] == '\0') {
        return parseBundle(d, size, out, depth);
    }
    return parseMessage(d, size, out);
}

} // namespace

bool parseOscPacket(const uint8_t* data, size_t size, std::vector<OscMessage>& out) {
    return parsePacketImpl(data, size, out, 0);
}

} // namespace mikmap

// ════════════════════════════════════════════════════════════════════════
//  io/proto/OscMessage.h — bộ phân tích OSC 1.0 (G4)
//
//  Tách RIÊNG khỏi socket. Lý do:
//    · parse là hàm thuần → test được đầy đủ mà không cần mở cổng mạng
//    · TUIO (G14) chồng lên OSC, sẽ dùng lại đúng bộ này
//    · gói tin hỏng / độc hại được xử lý ở một chỗ duy nhất
//
//  ── Định dạng OSC 1.0 ────────────────────────────────────────────────
//    [address]  chuỗi kết thúc null, đệm tới bội số 4
//    [typetag]  bắt đầu bằng ',', vd ",iff", cũng đệm tới bội số 4
//    [args]     dữ liệu BIG-ENDIAN theo thứ tự typetag
//
//  Bundle bắt đầu bằng "#bundle", theo sau là timetag 8 byte, rồi các
//  phần tử dạng [int32 size][nội dung].
//
//  ── An toàn ──────────────────────────────────────────────────────────
//  Dữ liệu đến từ MẠNG. Mọi phép đọc đều kiểm tra biên; gói tin dị dạng
//  bị bỏ qua chứ không bao giờ đọc tràn bộ đệm.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace hexmap {

struct OscArg {
    enum class Type { Int32, Float32, String };

    Type        type = Type::Int32;
    int32_t     i = 0;
    float       f = 0.0f;
    std::string s;

    /// Đọc dưới dạng số bất kể kiểu gốc — nhiều thiết bị gửi toạ độ dưới
    /// dạng int, số khác gửi float. Ép người gọi phân biệt là vô ích.
    double asDouble() const {
        switch (type) {
        case Type::Int32:   return static_cast<double>(i);
        case Type::Float32: return static_cast<double>(f);
        default:            return 0.0;
        }
    }

    int32_t asInt() const {
        switch (type) {
        case Type::Int32:   return i;
        case Type::Float32: return static_cast<int32_t>(f);
        default:            return 0;
        }
    }
};

struct OscMessage {
    std::string         address;
    std::vector<OscArg> args;

    double argDouble(size_t idx, double def = 0.0) const {
        return (idx < args.size()) ? args[idx].asDouble() : def;
    }
    int32_t argInt(size_t idx, int32_t def = 0) const {
        return (idx < args.size()) ? args[idx].asInt() : def;
    }
};

/// Phân tích một gói UDP. Xử lý được cả message đơn lẫn bundle (lồng nhau).
/// @param out  các message được THÊM vào, không xoá nội dung cũ
/// @return false nếu gói dị dạng (out có thể đã chứa message hợp lệ đọc được)
bool parseOscPacket(const uint8_t* data, size_t size, std::vector<OscMessage>& out);

} // namespace hexmap

// ════════════════════════════════════════════════════════════════════════
//  core/model/Clip.h — một ô trong lưới (A4)
//
//  Clip là DỮ LIỆU THUẦN. Nó không sở hữu decoder, không sở hữu texture.
//  render/ đọc clip rồi tự quản lý tài nguyên GPU tương ứng.
//
//  Nhờ vậy: lưới 32 clip chỉ tốn vài KB RAM, dù mỗi clip trỏ tới một
//  file 4K. Việc nạp/giải phóng thực sự do render/ quyết định (C11).
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"
#include "core/model/BlendMode.h"
#include "core/model/Transform2D.h"
#include "core/model/Transport.h"

#include <cstdint>
#include <string>

namespace mikmap {

enum class MediaType {
    Empty = 0,   ///< ô trống
    Video,       ///< file HAP / HAP Q
    Image,       ///< PNG / JPG
    Generator,   ///< shader sinh hình
};

struct MediaRef {
    MediaType   type = MediaType::Empty;
    std::string path;              ///< đường dẫn file, hoặc tên generator
    Vec2        size{0.0, 0.0};    ///< px — biết được sau khi nạp
    double      durationSec = 0.0; ///< 0 với ảnh và generator

    bool isEmpty() const { return type == MediaType::Empty; }
};

struct Clip {
    std::string  name;
    MediaRef     media;
    Transport    transport;
    Transform2D  transform;
    BlendMode    blend = BlendMode::Normal;
    double       opacity = 1.0;
    TriggerStyle triggerStyle = TriggerStyle::Toggle;

    /// Màu gán cho ô trên UI (A12). 0 = không gán.
    uint32_t colorTag = 0;

    bool isEmpty() const { return media.isEmpty(); }

    /// Chuẩn bị clip để bắt đầu phát từ đầu.
    void rewindAndPlay() {
        transport.durationSec = media.durationSec;
        transport.stop();      // đưa đầu phát về đúng đầu đoạn cắt
        transport.play();
    }
};

} // namespace mikmap

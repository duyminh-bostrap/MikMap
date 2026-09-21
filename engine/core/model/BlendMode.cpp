#include "core/model/BlendMode.h"

#include <cstring>

namespace hexmap {

BlendMode blendModeFromName(const char* name) {
    if (name == nullptr) return BlendMode::Normal;

    for (int i = 0; i < static_cast<int>(BlendMode::Count); ++i) {
        const auto m = static_cast<BlendMode>(i);
        if (std::strcmp(blendModeName(m), name) == 0) return m;
    }

    // Tên lạ (project của bản cũ, hoặc mode chưa cài đặt) → Normal.
    // Không ném lỗi: một mode không nhận ra không đáng để làm hỏng cả
    // project của người dùng.
    return BlendMode::Normal;
}

} // namespace hexmap

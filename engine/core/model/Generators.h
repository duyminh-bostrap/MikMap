// ════════════════════════════════════════════════════════════════════════
//  core/model/Generators.h — DANH MỤC nguồn hình sinh bằng shader
//
//  Chỉ là dữ liệu: mã định danh (đi vào file project) và nhãn hiển thị.
//  Việc biên dịch shader và vẽ thật nằm ở render/GeneratorBank.
//
//  ── Vì sao danh mục nằm ở core/ ──────────────────────────────────────
//  ui/ cần liệt kê generator cho người dùng chọn, nhưng ui/ KHÔNG được
//  phụ thuộc render/ (xem architecture.md). Tách phần dữ liệu xuống core/
//  thì cả hai tầng cùng đọc được một nguồn sự thật duy nhất.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <string>
#include <vector>

namespace hexmap {

struct GeneratorInfo {
    const char* id;      ///< khoá bền, ghi vào file project
    const char* label;   ///< tên hiện trên giao diện
};

/// Ba generator đầu tiên chuyển thẳng từ bản thiết kế tham khảo
/// (MikMap_Web, LiveCanvas.tsx) — cùng công thức, cùng bảng màu.
inline const std::vector<GeneratorInfo>& generatorCatalog() {
    static const std::vector<GeneratorInfo> kList = {
        {"plasma",    "Plasma Waves"},
        {"wireframe", "Geometric Wireframe"},
        {"vortex",    "Particle Vortex"},
    };
    return kList;
}

inline bool isKnownGenerator(const std::string& id) {
    for (const GeneratorInfo& i : generatorCatalog()) {
        if (id == i.id) return true;
    }
    return false;
}

inline const char* generatorLabel(const std::string& id) {
    for (const GeneratorInfo& i : generatorCatalog()) {
        if (id == i.id) return i.label;
    }
    return "";
}

} // namespace hexmap

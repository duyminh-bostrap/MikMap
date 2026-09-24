// ════════════════════════════════════════════════════════════════════════
//  core/model/BlendMode.h — chế độ hoà trộn (D4, A7)
//
//  P0 chỉ làm ~10 mode thiết yếu. Resolume có ~30, nhưng 10 mode này phủ
//  hầu hết nhu cầu thực tế; phần còn lại thuộc D9 (P2).
//
//  core/ chỉ ĐỊNH NGHĨA enum. Công thức hoà trộn thật nằm ở render/ dưới
//  dạng shader — core không được biết gì về GL.
// ════════════════════════════════════════════════════════════════════════
#pragma once

namespace mikmap {

enum class BlendMode {
    Normal = 0,   ///< alpha blend thông thường
    Add,          ///< cộng — sáng lên, hay dùng cho hiệu ứng phát sáng
    Multiply,     ///< nhân — tối đi
    Screen,       ///< nghịch đảo của multiply
    Overlay,
    SoftLight,
    HardLight,
    Difference,
    Subtract,
    Lighten,      ///< max từng kênh
    Darken,       ///< min từng kênh

    Count
};

/// Tên hiển thị trên UI và dùng khi lưu project.
/// Trả về chuỗi hằng — an toàn để lưu vào JSON.
inline const char* blendModeName(BlendMode m) {
    switch (m) {
    case BlendMode::Normal:     return "Normal";
    case BlendMode::Add:        return "Add";
    case BlendMode::Multiply:   return "Multiply";
    case BlendMode::Screen:     return "Screen";
    case BlendMode::Overlay:    return "Overlay";
    case BlendMode::SoftLight:  return "SoftLight";
    case BlendMode::HardLight:  return "HardLight";
    case BlendMode::Difference: return "Difference";
    case BlendMode::Subtract:   return "Subtract";
    case BlendMode::Lighten:    return "Lighten";
    case BlendMode::Darken:     return "Darken";
    default:                    return "Normal";
    }
}

/// Phân giải ngược từ tên. Dùng khi nạp project.
/// Tên lạ (file cũ, hoặc mode chưa cài đặt) → Normal, không làm hỏng project.
BlendMode blendModeFromName(const char* name);

} // namespace mikmap

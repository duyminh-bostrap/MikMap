// ════════════════════════════════════════════════════════════════════════
//  ui/Theme.h — bảng màu và các mảnh giao diện dùng lại (MikMap)
//
//  Bảng màu lấy từ bản thiết kế `mikmap_ui.tsx`. Bốn màu nhấn, mỗi màu
//  MỘT nghĩa cố định — dùng đúng nghĩa thì người vận hành đọc được màn
//  hình bằng ngoại vi mắt, không phải dừng lại đọc chữ:
//
//    · Cam   — đang PHÁT / đang chọn / đường ra máy chiếu
//    · Vàng  — đang chờ / rê chuột lên / đang kết nối
//    · Lục   — dữ liệu sống, đã kết nối, đã nạp, chỉ số tốt
//    · Lam   — lưới, đường dẫn hướng, thứ yếu
//
//  ── Vì sao nền tối đến vậy ───────────────────────────────────────────
//  Phần mềm này chạy trong phòng tối, cạnh máy chiếu, suốt buổi diễn.
//  Một bảng điều khiển nền sáng hắt lên mặt người vận hành và lọt vào
//  khung hình. #080808 gần đen nhưng KHÔNG đen tuyệt đối, để viền và
//  bóng đổ còn phân biệt được lớp.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "ofxImGui.h"

namespace hexmap {
namespace theme {

// ── Bảng màu ───────────────────────────────────────────────────────────
constexpr ImU32 Primary   = IM_COL32(0xFF, 0x7F, 0x50, 0xFF);   // #FF7F50
constexpr ImU32 Warning   = IM_COL32(0xFF, 0xD1, 0x66, 0xFF);   // #FFD166
constexpr ImU32 Success   = IM_COL32(0x06, 0xD6, 0xA0, 0xFF);   // #06D6A0
constexpr ImU32 Info      = IM_COL32(0x11, 0x8A, 0xB2, 0xFF);   // #118AB2
constexpr ImU32 Danger    = IM_COL32(0xE8, 0x3A, 0x3A, 0xFF);

constexpr ImU32 BgApp     = IM_COL32(0x08, 0x08, 0x08, 0xFF);
constexpr ImU32 BgPanel   = IM_COL32(0x12, 0x12, 0x12, 0xFF);
constexpr ImU32 BgCard    = IM_COL32(0x1A, 0x1A, 0x1A, 0xFF);
constexpr ImU32 BgSunken  = IM_COL32(0x05, 0x05, 0x05, 0xFF);
constexpr ImU32 Border    = IM_COL32(0x2A, 0x2A, 0x2A, 0xFF);
constexpr ImU32 BorderLit = IM_COL32(0x44, 0x44, 0x44, 0xFF);

constexpr ImU32 Text      = IM_COL32(0xE0, 0xE0, 0xE0, 0xFF);
constexpr ImU32 TextDim   = IM_COL32(0x88, 0x88, 0x88, 0xFF);
constexpr ImU32 TextFaint = IM_COL32(0x55, 0x55, 0x55, 0xFF);

// ── Kích thước cố định (theo bản thiết kế) ─────────────────────────────
constexpr float TopBarH     = 46.0f;   ///< thanh điều hướng trên cùng
constexpr float ToolBarH    = 42.0f;   ///< thanh công cụ của trang Mapping
constexpr float BrowserW    = 240.0f;
constexpr float InspectorW  = 280.0f;
constexpr float TreeW       = 260.0f;   ///< cây SCREEN SETUP
constexpr float LayerCtrlW  = 220.0f;  ///< cột điều khiển bên trái mỗi layer
constexpr float ClipW       = 120.0f;
constexpr float LayerRowH   = 90.0f;

/// Tỉ lệ chiều cao dành cho nửa trên trang Composition.
constexpr float CompTopRatio = 0.55f;

// ── Tiện ích ───────────────────────────────────────────────────────────
ImVec4 v4(ImU32 c);
ImU32  alpha(ImU32 c, float a);

/// Áp bảng màu + bo góc + khoảng đệm cho toàn bộ ImGuiStyle.
/// Gọi MỘT LẦN sau khi context ImGui đã tồn tại.
void apply();

// ── Mảnh giao diện dùng lại ────────────────────────────────────────────

/// Nhãn mục nhỏ, chữ hoa, màu mờ — "TRANSFORM", "ACTIVE SENSORS".
void sectionLabel(const char* text);

/// Nút tab (thanh trên cùng và các bộ chuyển chế độ).
/// @param accent màu chữ khi đang chọn
bool tabButton(const char* label, bool active,
               const ImVec2& size = ImVec2(0, 0), ImU32 accent = Primary);

/// Nút viền màu, nền trong suốt — kiểu nút phụ trong bản thiết kế.
bool outlineButton(const char* label, ImU32 accent,
                   const ImVec2& size = ImVec2(0, 0));

/// Chấm tròn trạng thái có quầng sáng. Vẽ tại vị trí con trỏ hiện tại.
void statusDot(ImU32 c, bool glow = true);

/// Thanh tiêu đề của một bảng: dải nền #1a1a1a chạy hết chiều ngang.
///
/// Bản thiết kế cho mỗi bảng một dải tiêu đề riêng thay vì để chữ trôi
/// trên nền bảng. Nó phân tách các bảng rõ hơn hẳn một dòng chữ suông —
/// nhất là khi ba bảng nằm cạnh nhau và đều cùng màu nền.
void panelHeader(const char* icon, const char* title, ImU32 accent = Text);

/// Một hàng trong cây SCREEN SETUP.
///
/// @param depth  0 = screen, 1 = slice, 2 = mask
/// @param accent màu khi được chọn — mỗi cấp một màu, xem `Theme.cpp`
/// @return true nếu vừa được bấm
bool treeRow(const char* icon, const char* label, int depth,
             bool selected, ImU32 accent);

/// Nút chỉ có icon, vuông.
bool iconButton(const char* icon, bool active, ImU32 accent = Text);

/// Nút NỀN ĐẶC — dành cho hành động chính duy nhất của một thanh công cụ.
/// Bản thiết kế chỉ dùng cho "Apply"; dùng nhiều thì mất hết trọng số.
bool filledButton(const char* label, ImU32 bg,
                  const ImVec2& size = ImVec2(0, 0));

/// Khung nền + viền cho một "thẻ" bao quanh vùng vừa vẽ.
/// Dùng cặp: beginCard() … endCard().
void beginCard(const char* id, const ImVec2& size, ImU32 border = Border);
void endCard();

/// Viền quanh phần tử vừa vẽ xong (dùng sau một widget).
void outlineLastItem(ImU32 c, float thickness = 1.0f);

} // namespace theme
} // namespace hexmap

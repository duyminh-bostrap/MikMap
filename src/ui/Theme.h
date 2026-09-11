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

// Các sắc nền/viền phụ — đo thẳng từ bản thiết kế tham khảo đang chạy
// (MikMap_Web), không phỏng đoán. Chúng khác BgCard/Border đủ để mắt
// nhận ra thứ bậc lớp, nên không gộp chung được.
constexpr ImU32 BgHeader    = IM_COL32(0x18, 0x18, 0x18, 0xFF);  // dải tiêu đề bảng, thẻ con
constexpr ImU32 BgDock      = IM_COL32(0x14, 0x14, 0x14, 0xFF);  // khay nút đáy cột
constexpr ImU32 BgInput     = IM_COL32(0x0A, 0x0A, 0x0A, 0xFF);  // ô nhập liệu
constexpr ImU32 BgButton    = IM_COL32(0x1C, 0x1C, 0x1C, 0xFF);  // nút phụ
constexpr ImU32 BorderSoft  = IM_COL32(0x22, 0x22, 0x22, 0xFF);  // vạch ngăn mục
constexpr ImU32 BorderField = IM_COL32(0x33, 0x33, 0x33, 0xFF);  // viền ô nhập / nút phụ

constexpr ImU32 Text      = IM_COL32(0xE0, 0xE0, 0xE0, 0xFF);
constexpr ImU32 TextDim   = IM_COL32(0x88, 0x88, 0x88, 0xFF);
constexpr ImU32 TextMuted = IM_COL32(0x77, 0x77, 0x77, 0xFF);
constexpr ImU32 TextFaint = IM_COL32(0x55, 0x55, 0x55, 0xFF);

// ── Kích thước cố định (theo bản thiết kế) ─────────────────────────────
constexpr float TopBarH     = 56.0f;   ///< thanh điều hướng trên cùng (h-14)
constexpr float ToolBarH    = 42.0f;   ///< thanh công cụ của trang Mapping
constexpr float PanelHeadH  = 40.0f;   ///< dải tiêu đề của mỗi bảng
constexpr float BrowserW    = 220.0f;
constexpr float InspectorW  = 280.0f;  ///< cột THIẾT LẬP SLICE (w-[280px])
constexpr float TreeW       = 240.0f;  ///< cây SCREEN SETUP (w-[240px])
constexpr float LayerCtrlW  = 220.0f;  ///< cột điều khiển bên trái mỗi layer
constexpr float ClipW       = 120.0f;
constexpr float LayerRowH   = 90.0f;

/// Tỉ lệ chiều cao dành cho nửa trên trang Composition.
constexpr float CompTopRatio = 0.50f;

// ── Thang cỡ chữ ───────────────────────────────────────────────────────
//
// Lấy đúng từ các lớp Tailwind trong bản thiết kế:
//   text-[9px] · text-[10px] · text-[11px] · text-xs(12) · text-sm(14) · text-lg(18)
//
// ★ Một cỡ chữ duy nhất cho cả giao diện là sai — và sai theo kiểu khó
//   thấy. Bản thiết kế dùng SÁU cỡ, và chính sự chênh lệch đó tạo ra thứ
//   bậc: liếc mắt là biết đâu là tiêu đề bảng, đâu là nhãn mục, đâu là
//   giá trị. Cào bằng hết thì mọi thứ đọc như nhau và phải dừng lại đọc.
namespace fs {
constexpr float Micro = 9.0f;    ///< tên clip, số opacity, nhãn cột
constexpr float Tiny  = 10.0f;   ///< nhãn mục (SECTION LABEL)
constexpr float Small = 11.0f;   ///< tab, hàng slice/mask trong cây
constexpr float Body  = 12.0f;   ///< chữ thân, tiêu đề bảng, nút
constexpr float Head  = 14.0f;   ///< tiêu đề thẻ
constexpr float Brand = 14.0f;   ///< chữ MIKMAP (text-sm trong bản thiết kế)
}

// ── Font ───────────────────────────────────────────────────────────────
//
// Ba độ đậm cắt từ Inter (font variable, trục wght 100..900) + một font
// mono cho số liệu. ImGui 1.92 đổi được cỡ lúc chạy nên mỗi độ đậm chỉ
// cần nạp MỘT lần.
struct Fonts {
    ImFont* regular  = nullptr;   ///< wght 400
    ImFont* semibold = nullptr;   ///< wght 600
    ImFont* bold     = nullptr;   ///< wght 700
    ImFont* mono     = nullptr;
};
void setFonts(const Fonts& f);
const Fonts& fonts();

/// Đổi font + cỡ. Luôn đi cặp với popFont().
/// An toàn khi font chưa nạp được — khi đó chỉ đổi cỡ.
void pushRegular(float size);
void pushSemiBold(float size);
void pushBold(float size);
void pushMono(float size);
void popFont();

/// Chữ một dòng với font + cỡ + màu chỉ định. Gọn hơn push/pop 3 dòng.
void text(ImFont* f, float size, ImU32 col, const char* txt);

// ── Tiện ích ───────────────────────────────────────────────────────────
ImVec4 v4(ImU32 c);
ImU32  alpha(ImU32 c, float a);

/// Áp bảng màu + bo góc + khoảng đệm cho toàn bộ ImGuiStyle.
/// Gọi MỘT LẦN sau khi context ImGui đã tồn tại.
void apply();

// ── Mảnh giao diện dùng lại ────────────────────────────────────────────

/// Nhãn mục nhỏ, chữ hoa, màu mờ — "TRANSFORM", "ACTIVE SENSORS".
void sectionLabel(const char* txt);

/// Nhãn của MỘT ô nhập: 10px đậm, màu #888 — đúng bản thiết kế.
void fieldLabel(const char* txt);

/// Vẽ chữ có GIÃN CHỮ (letter-spacing) vào một draw list.
///
/// ★ ImGui không có letter-spacing. Các nhãn chữ hoa cỡ 10px trong bản
///   thiết kế đều giãn 1px — không có nó thì chữ hoa nhỏ dính vào nhau và
///   đọc như một vệt. Vẽ từng ký tự rồi tự cộng thêm bước nhảy là cách
///   duy nhất tái tạo được.
void textTracked(ImDrawList* dl, ImFont* f, float size, ImVec2 pos,
                 ImU32 col, const char* txt, float spacing);

/// Bề rộng của chuỗi khi vẽ bằng textTracked().
float trackedWidth(ImFont* f, float size, const char* txt, float spacing);

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

/// Nút chỉ có ICON, kèm tooltip giải thích.
///
/// ★ Tooltip là BẮT BUỘC, không phải tuỳ chọn. Một icon không nhãn chỉ
///   đọc được nếu người dùng đã biết nó là gì; tooltip là chỗ duy nhất
///   họ tra được. Nút icon không tooltip là nút đố người dùng.
bool toolButton(const char* icon, const char* tooltip, ImU32 accent = Text,
                bool active = false);

/// Nút chỉ có icon, vuông.
bool iconButton(const char* icon, bool active, ImU32 accent = Text);

/// Nút NỀN ĐẶC — dành cho hành động chính duy nhất của một thanh công cụ.
/// Bản thiết kế chỉ dùng cho "Apply"; dùng nhiều thì mất hết trọng số.
bool filledButton(const char* label, ImU32 bg,
                  const ImVec2& size = ImVec2(0, 0));

/// Thanh phần trăm dạng "viên thuốc" — nhãn (VD "OPACITY  100%") ở trên,
/// rãnh bo tròn hoàn toàn ở dưới, núm tròn trắng nổi tại đầu mút.
///
/// ★ Đây KHÔNG phải ImGui::SliderFloat viết lại cho vui — hai thứ khác
///   hẳn nhau về hình dạng. SliderFloat vẽ nhãn CHỒNG LÊN rãnh; bản thiết
///   kế đặt nhãn thành một DÒNG RIÊNG phía trên, rãnh không chữ, và núm
///   là một chấm tròn trắng nổi hẳn ra ngoài rãnh — chỉ vẽ lại từ đầu
///   bằng ImDrawList mới ra đúng hình đó.
///
/// @param value01  0..1, được SỬA TRỰC TIẾP khi người dùng kéo/bấm
/// @param fillColor màu phần đã lấp đầy
/// @param glow    có quầng sáng quanh núm không — dành cho giá trị đang
///                THỰC SỰ tác động (layer đang phát); dùng tràn lan thì
///                mất hết ý nghĩa "cái này đang nổi bật"
/// @return true nếu value01 vừa đổi
bool opacityBar(const char* id, const char* label, double* value01,
                ImU32 fillColor, bool glow, float width = -1.0f);

/// Khung nền + viền cho một "thẻ" bao quanh vùng vừa vẽ.
/// Dùng cặp: beginCard() … endCard().
void beginCard(const char* id, const ImVec2& size, ImU32 border = Border);
void endCard();

/// Viền quanh phần tử vừa vẽ xong (dùng sau một widget).
void outlineLastItem(ImU32 c, float thickness = 1.0f);

} // namespace theme
} // namespace hexmap

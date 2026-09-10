#include "ui/Theme.h"

#include <cstring>

namespace hexmap {
namespace theme {

namespace { Fonts g_fonts; }

void setFonts(const Fonts& f) { g_fonts = f; }
const Fonts& fonts() { return g_fonts; }

void pushRegular(float size)  { ImGui::PushFont(g_fonts.regular,  size); }
void pushSemiBold(float size) { ImGui::PushFont(g_fonts.semibold, size); }
void pushBold(float size)     { ImGui::PushFont(g_fonts.bold,     size); }
void pushMono(float size)     { ImGui::PushFont(g_fonts.mono,     size); }
void popFont()                { ImGui::PopFont(); }

void text(ImFont* f, float size, ImU32 col, const char* txt) {
    ImGui::PushFont(f, size);
    ImGui::PushStyleColor(ImGuiCol_Text, v4(col));
    ImGui::TextUnformatted(txt);
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

ImVec4 v4(ImU32 c) {
    return ImVec4(static_cast<float>((c >> IM_COL32_R_SHIFT) & 0xFF) / 255.0f,
                  static_cast<float>((c >> IM_COL32_G_SHIFT) & 0xFF) / 255.0f,
                  static_cast<float>((c >> IM_COL32_B_SHIFT) & 0xFF) / 255.0f,
                  static_cast<float>((c >> IM_COL32_A_SHIFT) & 0xFF) / 255.0f);
}

ImU32 alpha(ImU32 c, float a) {
    const ImU32 rgb = c & ~IM_COL32_A_MASK;
    const ImU32 av  = static_cast<ImU32>(a * 255.0f) & 0xFF;
    return rgb | (av << IM_COL32_A_SHIFT);
}

void apply() {
    ImGuiStyle& s = ImGui::GetStyle();

    // ── Hình khối ──────────────────────────────────────────────────────
    // Bo góc nhỏ và ĐỀU. Bo nhiều làm các bảng trông như thẻ rời rạc;
    // đây là một bàn điều khiển liền mạch, không phải một trang web.
    s.WindowRounding    = 0.0f;
    s.ChildRounding     = 4.0f;
    s.FrameRounding     = 3.0f;
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding      = 3.0f;
    s.TabRounding       = 4.0f;

    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize  = 1.0f;
    s.FrameBorderSize  = 1.0f;
    s.PopupBorderSize  = 1.0f;

    s.WindowPadding    = ImVec2(10, 10);
    s.FramePadding     = ImVec2(8, 5);
    s.ItemSpacing      = ImVec2(8, 7);
    s.ItemInnerSpacing = ImVec2(6, 5);
    s.IndentSpacing    = 18.0f;
    s.ScrollbarSize    = 10.0f;
    s.GrabMinSize      = 9.0f;

    s.WindowTitleAlign = ImVec2(0.0f, 0.5f);
    s.SeparatorTextBorderSize = 1.0f;
    s.SeparatorTextAlign      = ImVec2(0.0f, 0.5f);
    s.SeparatorTextPadding    = ImVec2(14, 4);

    // ── Màu ────────────────────────────────────────────────────────────
    ImVec4* c = s.Colors;

    c[ImGuiCol_Text]                 = v4(Text);
    c[ImGuiCol_TextDisabled]         = v4(TextDim);
    c[ImGuiCol_WindowBg]             = v4(BgApp);
    c[ImGuiCol_ChildBg]              = v4(BgPanel);
    c[ImGuiCol_PopupBg]              = v4(BgCard);
    c[ImGuiCol_Border]               = v4(Border);
    c[ImGuiCol_BorderShadow]         = ImVec4(0, 0, 0, 0);

    c[ImGuiCol_FrameBg]              = v4(BgSunken);
    c[ImGuiCol_FrameBgHovered]       = v4(alpha(Info, 0.16f));
    c[ImGuiCol_FrameBgActive]        = v4(alpha(Info, 0.28f));

    c[ImGuiCol_TitleBg]              = v4(BgCard);
    c[ImGuiCol_TitleBgActive]        = v4(BgCard);
    c[ImGuiCol_TitleBgCollapsed]     = v4(BgPanel);
    c[ImGuiCol_MenuBarBg]            = v4(BgPanel);

    c[ImGuiCol_ScrollbarBg]          = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ScrollbarGrab]        = v4(Border);
    c[ImGuiCol_ScrollbarGrabHovered] = v4(BorderLit);
    c[ImGuiCol_ScrollbarGrabActive]  = v4(alpha(Primary, 0.7f));

    // ★ Tay nắm thanh trượt màu CAM, giống mọi thứ "đang tác động".
    //   Người vận hành nhìn thấy cam là biết chỗ đó đang ảnh hưởng tới
    //   hình đang chiếu.
    c[ImGuiCol_CheckMark]            = v4(Primary);
    c[ImGuiCol_SliderGrab]           = v4(Primary);
    c[ImGuiCol_SliderGrabActive]     = v4(Warning);

    c[ImGuiCol_Button]               = v4(BgCard);
    c[ImGuiCol_ButtonHovered]        = v4(alpha(Primary, 0.18f));
    c[ImGuiCol_ButtonActive]         = v4(alpha(Primary, 0.34f));

    c[ImGuiCol_Header]               = v4(alpha(Primary, 0.16f));
    c[ImGuiCol_HeaderHovered]        = v4(alpha(Primary, 0.26f));
    c[ImGuiCol_HeaderActive]         = v4(alpha(Primary, 0.36f));

    c[ImGuiCol_Separator]            = v4(Border);
    c[ImGuiCol_SeparatorHovered]     = v4(alpha(Info, 0.7f));
    c[ImGuiCol_SeparatorActive]      = v4(Info);

    c[ImGuiCol_ResizeGrip]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_ResizeGripHovered]    = v4(alpha(Primary, 0.4f));
    c[ImGuiCol_ResizeGripActive]     = v4(alpha(Primary, 0.7f));

    c[ImGuiCol_Tab]                  = v4(BgCard);
    c[ImGuiCol_TabHovered]           = v4(alpha(Primary, 0.24f));
    c[ImGuiCol_TabSelected]          = v4(alpha(Primary, 0.34f));

    c[ImGuiCol_PlotLines]            = v4(Info);
    c[ImGuiCol_PlotLinesHovered]     = v4(Warning);
    c[ImGuiCol_PlotHistogram]        = v4(Success);
    c[ImGuiCol_PlotHistogramHovered] = v4(Warning);

    c[ImGuiCol_TableHeaderBg]        = v4(BgCard);
    c[ImGuiCol_TableBorderStrong]    = v4(Border);
    c[ImGuiCol_TableBorderLight]     = v4(alpha(Border, 0.6f));
    c[ImGuiCol_TableRowBg]           = ImVec4(0, 0, 0, 0);
    c[ImGuiCol_TableRowBgAlt]        = v4(alpha(BgCard, 0.4f));

    c[ImGuiCol_TextSelectedBg]       = v4(alpha(Primary, 0.35f));
    c[ImGuiCol_DragDropTarget]       = v4(Warning);
    c[ImGuiCol_NavCursor]            = v4(Primary);
    c[ImGuiCol_ModalWindowDimBg]     = ImVec4(0.0f, 0.0f, 0.0f, 0.65f);
}

// ═══════════════════════════════════════════════════════════════════════
//  Mảnh giao diện
// ═══════════════════════════════════════════════════════════════════════

void sectionLabel(const char* txt) {
    pushBold(fs::Tiny);
    ImGui::PushStyleColor(ImGuiCol_Text, v4(TextFaint));
    ImGui::TextUnformatted(txt);
    ImGui::PopStyleColor();
    popFont();

    // Gạch chân mảnh chạy hết chiều rộng: tách mục mà không tốn một dòng
    // Separator đầy đủ, vốn quá nặng cho các mục nhỏ liên tiếp.
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float w = ImGui::GetContentRegionAvail().x;
    ImGui::GetWindowDrawList()->AddLine(ImVec2(p.x, p.y - 3.0f),
                                        ImVec2(p.x + w, p.y - 3.0f),
                                        alpha(Border, 0.8f));
    ImGui::Dummy(ImVec2(0.0f, 1.0f));
}

bool tabButton(const char* label, bool active, const ImVec2& size, ImU32 accent) {
    if (active) {
        ImGui::PushStyleColor(ImGuiCol_Button,        v4(BgCard));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, v4(BgCard));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  v4(BgCard));
        ImGui::PushStyleColor(ImGuiCol_Text,          v4(accent));
        ImGui::PushStyleColor(ImGuiCol_Border,        v4(BorderLit));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button,        ImVec4(0, 0, 0, 0));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, v4(alpha(BgCard, 0.8f)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  v4(BgCard));
        ImGui::PushStyleColor(ImGuiCol_Text,          v4(TextDim));
        ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(0, 0, 0, 0));
    }
    const bool hit = ImGui::Button(label, size);
    ImGui::PopStyleColor(5);
    return hit;
}

bool outlineButton(const char* label, ImU32 accent, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        v4(alpha(accent, 0.10f)));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, v4(alpha(accent, 0.22f)));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  v4(alpha(accent, 0.34f)));
    ImGui::PushStyleColor(ImGuiCol_Text,          v4(accent));
    ImGui::PushStyleColor(ImGuiCol_Border,        v4(alpha(accent, 0.45f)));
    const bool hit = ImGui::Button(label, size);
    ImGui::PopStyleColor(5);
    return hit;
}

void statusDot(ImU32 c, bool glow) {
    const float r = 4.0f;
    const ImVec2 p = ImGui::GetCursorScreenPos();
    const float cy = p.y + ImGui::GetTextLineHeight() * 0.5f;
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Quầng sáng: chấm 4px trên nền gần đen rất dễ lọt khỏi tầm nhìn
    // ngoại vi. Quầng làm nó "nảy" lên mà không phải phóng to chấm.
    if (glow) dl->AddCircleFilled(ImVec2(p.x + r, cy), r * 2.4f, alpha(c, 0.22f));
    dl->AddCircleFilled(ImVec2(p.x + r, cy), r, c);

    ImGui::Dummy(ImVec2(r * 2.0f, ImGui::GetTextLineHeight()));
}

void panelHeader(const char* icon, const char* title, ImU32 accent) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float h  = ImGui::GetFrameHeight() + 6.0f;
    const ImVec2 p = ImGui::GetCursorScreenPos();

    // Dai nen chay het chieu ngang cua BANG, khong phai chi rong bang chu.
    const float w = ImGui::GetContentRegionAvail().x
                  + ImGui::GetStyle().WindowPadding.x * 2.0f;
    const ImVec2 a(p.x - ImGui::GetStyle().WindowPadding.x, p.y - ImGui::GetStyle().WindowPadding.y);
    const ImVec2 b(a.x + w, a.y + h);

    dl->AddRectFilled(a, b, BgCard);
    dl->AddLine(ImVec2(a.x, b.y - 1.0f), ImVec2(b.x, b.y - 1.0f), Border);

    pushBold(fs::Body);
    ImGui::SetCursorScreenPos(ImVec2(p.x, a.y + (h - ImGui::GetTextLineHeight()) * 0.5f));
    if (icon != nullptr && icon[0] != 0) {
        ImGui::PushStyleColor(ImGuiCol_Text, v4(accent));
        ImGui::TextUnformatted(icon);
        ImGui::PopStyleColor();
        ImGui::SameLine(0.0f, 7.0f);
    }
    ImGui::TextUnformatted(title);
    popFont();

    ImGui::SetCursorScreenPos(ImVec2(p.x, b.y + 8.0f));
}

bool treeRow(const char* icon, const char* label, int depth,
             bool selected, ImU32 accent) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    const float rowH   = ImGui::GetTextLineHeight() + 8.0f;
    const float indent = 10.0f + static_cast<float>(depth) * 18.0f;
    const float w      = ImGui::GetContentRegionAvail().x;

    const ImVec2 p0 = ImGui::GetCursorScreenPos();
    const ImVec2 p1(p0.x + w, p0.y + rowH);

    ImGui::InvisibleButton(label, ImVec2(w, rowH));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();

    if (selected) {
        dl->AddRectFilled(p0, p1, alpha(accent, 0.20f));
        // Vach mau ben trai: cho biet CAP nao dang duoc chon ngay ca khi
        // hang bi cuon che mat mot phan.
        dl->AddRectFilled(p0, ImVec2(p0.x + 2.0f, p1.y), accent);
    } else if (hovered) {
        dl->AddRectFilled(p0, p1, BgCard);
    }

    const ImU32 fg = selected ? accent : (hovered ? Text : TextDim);
    float x = p0.x + indent;
    const float ty = p0.y + 4.0f;

    // Cap 0 (screen) dam va to hon; cap duoi thuong va nho hon. Thu bac
    // nay lam cau truc cay doc duoc ma khong can them duong ke.
    ImFont* f  = (depth == 0) ? g_fonts.bold : g_fonts.regular;
    const float sz = (depth == 0) ? fs::Body : fs::Small;

    if (icon != nullptr && icon[0] != 0) {
        dl->AddText(g_fonts.regular, fs::Body, ImVec2(x, ty), fg, icon);
        x += 20.0f;
    }
    dl->AddText(f, sz, ImVec2(x, ty + (depth == 0 ? 0.0f : 1.0f)), fg, label);

    return clicked;
}

bool toolButton(const char* icon, const char* tooltip, ImU32 accent, bool active) {
    const bool hit = iconButton(icon, active, accent);
    if (tooltip != nullptr && ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", tooltip);
    }
    return hit;
}

bool iconButton(const char* icon, bool active, ImU32 accent) {
    const float sz = ImGui::GetFrameHeight();
    return tabButton(icon, active, ImVec2(sz + 6.0f, sz), accent);
}

bool filledButton(const char* label, ImU32 bg, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button,        v4(bg));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, v4(Warning));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive,  v4(alpha(bg, 0.8f)));
    ImGui::PushStyleColor(ImGuiCol_Text,          v4(IM_COL32(10, 10, 10, 255)));
    ImGui::PushStyleColor(ImGuiCol_Border,        ImVec4(0, 0, 0, 0));
    const bool hit = ImGui::Button(label, size);
    ImGui::PopStyleColor(5);
    return hit;
}

void beginCard(const char* id, const ImVec2& size, ImU32 border) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, v4(BgCard));
    ImGui::PushStyleColor(ImGuiCol_Border,  v4(border));
    ImGui::BeginChild(id, size, ImGuiChildFlags_Borders);
}

void endCard() {
    ImGui::EndChild();
    ImGui::PopStyleColor(2);
}

void outlineLastItem(ImU32 c, float thickness) {
    ImGui::GetWindowDrawList()->AddRect(ImGui::GetItemRectMin(),
                                        ImGui::GetItemRectMax(),
                                        c, ImGui::GetStyle().FrameRounding,
                                        0, thickness);
}

} // namespace theme
} // namespace hexmap

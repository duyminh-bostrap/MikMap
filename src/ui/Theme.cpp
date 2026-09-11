#include "ui/Theme.h"

#include <algorithm>
#include <cfloat>
#include <cstdio>
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
    s.FrameRounding     = 4.0f;   // rounded (0.25rem) trong bản thiết kế
    s.PopupRounding     = 4.0f;
    s.ScrollbarRounding = 6.0f;
    s.GrabRounding      = 3.0f;
    s.TabRounding       = 4.0f;

    s.WindowBorderSize = 0.0f;
    s.ChildBorderSize  = 1.0f;
    s.FrameBorderSize  = 1.0f;
    s.PopupBorderSize  = 1.0f;

    // Ô nhập trong bản thiết kế cao 30px: chữ 12px + đệm dọc 9px hai bên.
    s.WindowPadding    = ImVec2(10, 10);
    s.FramePadding     = ImVec2(10, 9);
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

    // Ô nhập: nền #0a0a0a, viền #333 — đo từ bản thiết kế. Viền sáng hơn
    // Border thường vì ô nhập phải mời người ta bấm vào, khác với vạch
    // ngăn vốn chỉ nên tách lớp.
    c[ImGuiCol_FrameBg]              = v4(BgInput);
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
        // ★ Nut DANG BAT duoc NHUOM theo chinh mau nhan cua no — nen
        //   accent/15, vien accent/40, chu accent — dung ngu phap cua ban
        //   thiet ke tham khao (MikMap_Web). Nen xam trung tinh nhu truoc
        //   thi trang thai bat/tat chi khac nhau o mau CHU, va trong phong
        //   toi liec qua rat de doc nham la dang tat.
        ImGui::PushStyleColor(ImGuiCol_Button,        v4(alpha(accent, 0.15f)));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, v4(alpha(accent, 0.22f)));
        ImGui::PushStyleColor(ImGuiCol_ButtonActive,  v4(alpha(accent, 0.30f)));
        ImGui::PushStyleColor(ImGuiCol_Text,          v4(accent));
        ImGui::PushStyleColor(ImGuiCol_Border,        v4(alpha(accent, 0.40f)));
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

void textTracked(ImDrawList* dl, ImFont* f, float size, ImVec2 pos,
                 ImU32 col, const char* txt, float spacing) {
    for (const char* p = txt; *p != 0; ) {
        // UTF-8: gom tron mot ky tu truoc khi ve, neu khong chu co dau
        // (tieng Viet) se bi cat thanh cac byte vo nghia.
        const char* next = p + 1;
        while ((*next & 0xC0) == 0x80) ++next;

        char ch[8] = {0};
        std::memcpy(ch, p, static_cast<size_t>(next - p));
        dl->AddText(f, size, pos, col, ch);
        pos.x += f->CalcTextSizeA(size, FLT_MAX, 0.0f, ch).x + spacing;
        p = next;
    }
}

float trackedWidth(ImFont* f, float size, const char* txt, float spacing) {
    float w = 0.0f;
    int   n = 0;
    for (const char* p = txt; *p != 0; ) {
        const char* next = p + 1;
        while ((*next & 0xC0) == 0x80) ++next;
        char ch[8] = {0};
        std::memcpy(ch, p, static_cast<size_t>(next - p));
        w += f->CalcTextSizeA(size, FLT_MAX, 0.0f, ch).x;
        ++n;
        p = next;
    }
    return w + spacing * static_cast<float>(n > 0 ? n - 1 : 0);
}

void fieldLabel(const char* txt) {
    pushBold(fs::Tiny);
    ImGui::PushStyleColor(ImGuiCol_Text, v4(TextDim));
    ImGui::TextUnformatted(txt);
    ImGui::PopStyleColor();
    popFont();
}

void panelHeader(const char* icon, const char* title, ImU32 accent) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // So do lay thang tu ban thiet ke dang chay: dai cao 40px, nen
    // #181818, chu 10px dam gian 1px.
    const float h  = PanelHeadH;
    const ImVec2 p = ImGui::GetCursorScreenPos();

    // Dai nen chay het chieu ngang cua BANG, khong phai chi rong bang chu.
    const float w = ImGui::GetContentRegionAvail().x
                  + ImGui::GetStyle().WindowPadding.x * 2.0f;
    const ImVec2 a(p.x - ImGui::GetStyle().WindowPadding.x, p.y - ImGui::GetStyle().WindowPadding.y);
    const ImVec2 b(a.x + w, a.y + h);

    dl->AddRectFilled(a, b, BgHeader);
    dl->AddLine(ImVec2(a.x, b.y - 1.0f), ImVec2(b.x, b.y - 1.0f), Border);

    float x = p.x;
    const float ty = a.y + (h - fs::Tiny) * 0.5f - 1.0f;
    if (icon != nullptr && icon[0] != 0) {
        dl->AddText(g_fonts.regular, fs::Body, ImVec2(x, ty - 1.0f), accent, icon);
        x += 20.0f;
    }
    textTracked(dl, g_fonts.bold, fs::Tiny, ImVec2(x, ty), Text, title, 1.0f);

    ImGui::SetCursorScreenPos(ImVec2(p.x, b.y + 10.0f));
}

bool treeRow(const char* icon, const char* label, int depth,
             bool selected, ImU32 accent, const char* badge) {
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // ★ Ba cấp — ba cỡ hàng, đo thẳng từ bản thiết kế tham khảo đang
    //   chạy: screen 34px/12px đậm, slice 29px/11px, mặt nạ 23px/10px.
    //   Chính chênh lệch này làm cấu trúc cây đọc được mà không cần thêm
    //   một đường kẻ nào.
    struct Spec { float h, indent, iconSz, textSz; };
    constexpr Spec kSpec[3] = {
        {34.0f,  0.0f, fs::Body,  fs::Body },
        {29.0f, 16.0f, fs::Small, fs::Small},
        {23.0f, 26.0f, fs::Tiny,  fs::Tiny },
    };
    const Spec& sp = kSpec[std::clamp(depth, 0, 2)];

    const float w = ImGui::GetContentRegionAvail().x;

    const ImVec2 p0(ImGui::GetCursorScreenPos().x + sp.indent,
                    ImGui::GetCursorScreenPos().y);
    const ImVec2 p1(ImGui::GetCursorScreenPos().x + w, p0.y + sp.h);

    // ★ Cho phep vat gi do CHONG LEN sau no (vd nut mat bat/tat o canh
    //   phai hang) van bam duoc — mac dinh ImGui khoa hover cho item DAU
    //   TIEN nam duoi con tro, item ve SAU bi chan hoan toan du ve TREN.
    ImGui::SetNextItemAllowOverlap();
    ImGui::InvisibleButton(label, ImVec2(w, sp.h));
    const bool hovered = ImGui::IsItemHovered();
    const bool clicked = ImGui::IsItemClicked();

    // Hàng đang chọn là một THẺ bo góc có viền, không phải một vệt màu
    // chạy hết chiều ngang — đúng bản thiết kế, và nhờ có viền nên nó
    // vẫn tách khỏi nền ngay cả khi màu nhấn bị nhạt đi.
    if (selected) {
        if (depth == 0) {
            dl->AddRectFilled(p0, p1, IM_COL32(0x22, 0x22, 0x22, 0xFF), 4.0f);
            dl->AddRect(p0, p1, IM_COL32(0x38, 0x38, 0x38, 0xFF), 4.0f);
        } else {
            dl->AddRectFilled(p0, p1, alpha(accent, 0.15f), 4.0f);
            dl->AddRect(p0, p1, alpha(accent, 0.30f), 4.0f);
        }
    } else if (hovered) {
        dl->AddRectFilled(p0, p1, BgHeader, 4.0f);
    }

    const ImU32 fg = selected ? (depth == 0 ? Primary : accent)
                              : (hovered ? Text : (depth == 2 ? TextMuted : TextDim));

    float x = p0.x + (depth == 0 ? 9.0f : 8.0f);
    const float ty = p0.y + (sp.h - sp.textSz) * 0.5f - 1.0f;

    if (icon != nullptr && icon[0] != 0) {
        // Icon giữ MÀU RIÊNG theo cấp kể cả khi hàng không được chọn: nó
        // là thứ cho biết đây là screen / slice / mặt nạ.
        const ImU32 ic = selected ? fg : (depth == 0 ? fg : accent);
        dl->AddText(g_fonts.regular, sp.iconSz, ImVec2(x, ty), ic, icon);
        x += sp.iconSz + 6.0f;
    }
    dl->AddText(selected ? g_fonts.bold : (depth == 0 ? g_fonts.bold : g_fonts.regular),
                sp.textSz, ImVec2(x, ty), fg, label);

    // Nhãn phụ căn phải (vd độ phân giải của screen) — chữ nhỏ, mờ, để
    // đọc được thông số mà không phải mở bảng thuộc tính. Chừa sẵn 22px
    // sát mép phải cho nút con mắt bật/tắt mà bên gọi vẽ chồng lên sau.
    if (badge != nullptr && badge[0] != 0) {
        ImFont* bf = (g_fonts.bold != nullptr) ? g_fonts.bold : ImGui::GetFont();
        const float bw = bf->CalcTextSizeA(fs::Micro, FLT_MAX, 0.0f, badge).x;
        dl->AddText(bf, fs::Micro,
                    ImVec2(p1.x - 31.0f - bw, p0.y + (sp.h - fs::Micro) * 0.5f),
                    IM_COL32(0x66, 0x66, 0x66, 0xFF), badge);
    }

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

bool opacityBar(const char* id, const char* label, double* value01,
                ImU32 fillColor, bool glow, float width) {
    const float w = (width > 0.0f) ? width : ImGui::GetContentRegionAvail().x;

    // ── Hang nhan: ten (trai) + phan tram (phai), mono nho ─────────────
    pushMono(fs::Micro);
    char pct[16];
    std::snprintf(pct, sizeof(pct), "%.0f%%", std::clamp(*value01, 0.0, 1.0) * 100.0);
    const ImVec2 pctSize = ImGui::CalcTextSize(pct);

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddText(p0, TextDim, label);
    dl->AddText(ImVec2(p0.x + w - pctSize.x, p0.y), TextDim, pct);
    ImGui::Dummy(ImVec2(w, ImGui::GetTextLineHeight()));
    popFont();

    ImGui::Dummy(ImVec2(w, 3.0f));   // khe he giua nhan va rang, nhu "mb-1"

    // ── Rang dang vien thuoc ────────────────────────────────────────────
    constexpr float kTrackH = 8.0f;
    constexpr float kHitH   = 16.0f;   // vung bam RONG hon rang de de keo

    p0 = ImGui::GetCursorScreenPos();
    ImGui::InvisibleButton(id, ImVec2(w, kHitH));

    bool changed = false;
    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float mx = ImGui::GetIO().MousePos.x;
        double t = static_cast<double>((mx - p0.x) / w);
        t = std::clamp(t, 0.0, 1.0);
        if (t != *value01) { *value01 = t; changed = true; }
    }

    const ImVec2 trackP0(p0.x, p0.y + (kHitH - kTrackH) * 0.5f);
    const ImVec2 trackP1(trackP0.x + w, trackP0.y + kTrackH);

    dl->AddRectFilled(trackP0, trackP1, BgSunken, kTrackH * 0.5f);
    dl->AddRect(trackP0, trackP1, Border, kTrackH * 0.5f);

    // ★ Fill LUON bo tron toan bo bon goc, khong phai chi hai goc trai.
    //   Dung y cua ban thiet ke: mot vien thuoc NHO nam trong vien thuoc
    //   LON — o gia tri 100% hai vien trung khop, o gia tri thap hon no
    //   la mot khoi tron doc lap troi giua rang, khong phai mot thanh bi
    //   cat cut o dau phai.
    const float fillW = static_cast<float>(*value01) * w;
    if (fillW > 0.5f) {
        dl->AddRectFilled(trackP0, ImVec2(trackP0.x + fillW, trackP1.y),
                          fillColor, kTrackH * 0.5f);
    }

    // ── Num tron trang, noi ra ngoai rang ───────────────────────────────
    constexpr float kThumbR = 6.0f;
    ImVec2 thumb(trackP0.x + fillW, (trackP0.y + trackP1.y) * 0.5f);
    thumb.x = std::clamp(thumb.x, trackP0.x, trackP1.x);

    if (glow) dl->AddCircleFilled(thumb, kThumbR * 2.0f, alpha(fillColor, 0.30f));
    dl->AddCircleFilled(thumb, kThumbR, IM_COL32(255, 255, 255, 255));

    return changed;
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

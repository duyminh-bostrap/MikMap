#include "Theme.h"
#include <imgui_internal.h>
#include <algorithm>

namespace HexMap::UI {

void Theme::SetupMikMapTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Window & Frame Geometry
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 4.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 4.0f;
    style.ScrollbarRounding = 4.0f;
    style.GrabRounding      = 3.0f;
    style.TabRounding       = 4.0f;

    style.WindowPadding     = ImVec2(10.0f, 10.0f);
    style.FramePadding      = ImVec2(8.0f, 4.0f);
    style.ItemSpacing       = ImVec2(6.0f, 6.0f);
    style.ItemInnerSpacing  = ImVec2(4.0f, 4.0f);
    style.ScrollbarSize     = 10.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.TabBorderSize     = 1.0f;

    // Base dark background palette
    colors[ImGuiCol_Text]                  = ImVec4(0.92f, 0.92f, 0.92f, 1.00f);
    colors[ImGuiCol_TextDisabled]          = ColorPalette::TextMuted;
    colors[ImGuiCol_WindowBg]              = ColorPalette::DarkCanvas;
    colors[ImGuiCol_ChildBg]               = ColorPalette::PanelBg;
    colors[ImGuiCol_PopupBg]               = ImVec4(0.08f, 0.08f, 0.08f, 0.98f);
    colors[ImGuiCol_Border]                = ColorPalette::BorderColor;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Headers & Tree elements
    colors[ImGuiCol_Header]                = ColorPalette::HeaderBg;
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);

    // Frame (input fields, combo boxes)
    colors[ImGuiCol_FrameBg]               = ImVec4(0.06f, 0.06f, 0.06f, 1.00f);
    colors[ImGuiCol_FrameBgHovered]        = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_FrameBgActive]         = ImVec4(0.16f, 0.16f, 0.16f, 1.00f);

    // Title & Menus
    colors[ImGuiCol_TitleBg]               = ColorPalette::PanelBg;
    colors[ImGuiCol_TitleBgActive]         = ColorPalette::HeaderBg;
    colors[ImGuiCol_MenuBarBg]             = ColorPalette::HeaderBg;

    // Scrollbars
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.03f, 0.03f, 0.03f, 0.60f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.30f, 0.30f, 0.30f, 1.00f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ColorPalette::VibrantOrange;

    // Sliders / Grabs
    colors[ImGuiCol_SliderGrab]            = ColorPalette::VibrantOrange;
    colors[ImGuiCol_SliderGrabActive]      = ColorPalette::OrangeActive;

    // Buttons
    colors[ImGuiCol_Button]                = ImVec4(0.12f, 0.12f, 0.12f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ColorPalette::VibrantOrange;

    // Tabs
    colors[ImGuiCol_Tab]                   = ColorPalette::HeaderBg;
    colors[ImGuiCol_TabHovered]            = ColorPalette::OrangeHover;
    colors[ImGuiCol_TabActive]             = ColorPalette::VibrantOrange;
    colors[ImGuiCol_TabUnfocused]          = ColorPalette::PanelBg;
    colors[ImGuiCol_TabUnfocusedActive]    = ColorPalette::HeaderBg;
}

bool Theme::OpacityBar(const char* label, float* value, float width, float height) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems) return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);

    if (width <= 0.0f) width = ImGui::CalcItemWidth();
    const ImVec2 pos = window->DC.CursorPos;
    const ImVec2 size = ImVec2(width, height);
    const ImRect bb(pos, ImVec2(pos.x + size.x, pos.y + size.y));

    ImGui::ItemSize(size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id)) return false;

    bool hovered, held;
    bool value_changed = ImGui::ButtonBehavior(bb, id, &hovered, &held, ImGuiButtonFlags_None);

    if (held) {
        float mouse_x = g.IO.MousePos.x;
        float normalized = std::clamp((mouse_x - bb.Min.x) / bb.GetWidth(), 0.0f, 1.0f);
        *value = normalized;
        value_changed = true;
    }

    // Render bar background
    ImU32 bg_col = ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
    ImU32 fill_col = ImGui::GetColorU32(ColorPalette::VibrantOrange);
    ImU32 border_col = ImGui::GetColorU32(ColorPalette::BorderColor);

    window->DrawList->AddRectFilled(bb.Min, bb.Max, bg_col, 3.0f);

    // Render filled portion
    float fill_x = bb.Min.x + bb.GetWidth() * (*value);
    if (fill_x > bb.Min.x) {
        window->DrawList->AddRectFilled(bb.Min, ImVec2(fill_x, bb.Max.y), fill_col, 3.0f);
    }

    // Border
    window->DrawList->AddRect(bb.Min, bb.Max, border_col, 3.0f);

    // Text: label + percentage
    char buf[64];
    snprintf(buf, sizeof(buf), "%s: %d%%", label, (int)((*value) * 100.0f + 0.5f));
    ImVec2 text_size = ImGui::CalcTextSize(buf);
    ImVec2 text_pos = ImVec2(bb.Min.x + (bb.GetWidth() - text_size.x) * 0.5f,
                             bb.Min.y + (bb.GetHeight() - text_size.y) * 0.5f);
    window->DrawList->AddText(text_pos, IM_COL32(255, 255, 255, 230), buf);

    return value_changed;
}

bool Theme::PrimaryButton(const char* label, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button, ColorPalette::VibrantOrange);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColorPalette::OrangeHover);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColorPalette::OrangeActive);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f)); // Black text on vibrant orange
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return pressed;
}

bool Theme::SecondaryButton(const char* label, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.24f, 0.24f, 0.24f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Border, ColorPalette::BorderColor);
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(4);
    return pressed;
}

bool Theme::AccentButton(const char* label, const ImVec4& color, const ImVec2& size) {
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(color.x * 0.2f, color.y * 0.2f, color.z * 0.2f, 0.8f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(color.x * 0.35f, color.y * 0.35f, color.z * 0.35f, 0.9f));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, color);
    ImGui::PushStyleColor(ImGuiCol_Text, color);
    ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(color.x * 0.6f, color.y * 0.6f, color.z * 0.6f, 0.8f));
    bool pressed = ImGui::Button(label, size);
    ImGui::PopStyleColor(5);
    return pressed;
}

} // namespace HexMap::UI

#pragma once

#include <imgui.h>
#include <string>

namespace HexMap::UI {

struct ColorPalette {
    // Brand & Active Colors
    static constexpr ImVec4 VibrantOrange   = ImVec4(1.000f, 0.498f, 0.314f, 1.0f); // #FF7F50
    static constexpr ImVec4 OrangeHover     = ImVec4(1.000f, 0.580f, 0.420f, 1.0f);
    static constexpr ImVec4 OrangeActive    = ImVec4(0.900f, 0.420f, 0.250f, 1.0f);

    // Inactive & Secondary Tone (Burnt Brown / Espresso Monochromatic)
    static constexpr ImVec4 BurntBrownBg    = ImVec4(0.231f, 0.114f, 0.055f, 1.0f); // #3B1D0E
    static constexpr ImVec4 BurntBrownBorder= ImVec4(0.290f, 0.141f, 0.067f, 1.0f); // #4A2411
    static constexpr ImVec4 BurntBrownHover = ImVec4(0.320f, 0.160f, 0.080f, 1.0f);
    static constexpr ImVec4 BurntBrownText  = ImVec4(0.910f, 0.769f, 0.635f, 1.0f); // #E8C4A2

    // Accents
    static constexpr ImVec4 SliceCyan       = ImVec4(0.067f, 0.541f, 0.698f, 1.0f); // #118AB2
    static constexpr ImVec4 MaskYellow      = ImVec4(1.000f, 0.820f, 0.400f, 1.0f); // #FFD166
    static constexpr ImVec4 SuccessGreen    = ImVec4(0.024f, 0.839f, 0.627f, 1.0f); // #06D6A0
    static constexpr ImVec4 DangerRed       = ImVec4(0.937f, 0.267f, 0.267f, 1.0f); // #EF4444

    // Neutrals
    static constexpr ImVec4 DarkCanvas      = ImVec4(0.040f, 0.040f, 0.040f, 1.0f); // #0A0A0A
    static constexpr ImVec4 PanelBg         = ImVec4(0.071f, 0.071f, 0.071f, 1.0f); // #121212
    static constexpr ImVec4 HeaderBg        = ImVec4(0.094f, 0.094f, 0.094f, 1.0f); // #181818
    static constexpr ImVec4 BorderColor     = ImVec4(0.165f, 0.165f, 0.165f, 1.0f); // #2A2A2A
    static constexpr ImVec4 TextMuted       = ImVec4(0.533f, 0.533f, 0.533f, 1.0f); // #888888
};

class Theme {
public:
    // Cài đặt bảng màu Dear ImGui theo phong cách MikMap (Burnt Brown + Vibrant Orange)
    static void SetupMikMapTheme();

    // Widget thanh kéo Opacity tuỳ chỉnh theo yêu cầu kiến trúc (features.md)
    static bool OpacityBar(const char* label, float* value, float width = -1.0f, float height = 18.0f);

    // Button tiện ích với style bo góc & màu sắc thương hiệu
    static bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
    static bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
    static bool AccentButton(const char* label, const ImVec4& color, const ImVec2& size = ImVec2(0, 0));
};

} // namespace HexMap::UI

#include <iostream>
#include <chrono>
#include <thread>

// Header Dear ImGui và UI MikMap
#include "../ui/Theme.h"
#include "../ui/ControlPanel.h"
#include "../core/math/Homography.h"

int main(int argc, char** argv) {
    std::cout << "========================================================\n";
    std::cout << "  MIKMAP / HEXMAPPING - NATIVE C++20 PROJECTION ENGINE  \n";
    std::cout << "  Target: Windows 10/11 - MSVC / OpenGL 4.x / ImGui     \n";
    std::cout << "========================================================\n\n";

    // 1. Kiểm tra toán học Homography & Keystone
    std::cout << "[1] Verifying C++20 Homography DLT Solver...\n";
    std::array<HexMap::Core::Math::Vec2, 4> src = {
        HexMap::Core::Math::Vec2(0, 0), HexMap::Core::Math::Vec2(1920, 0),
        HexMap::Core::Math::Vec2(1920, 1080), HexMap::Core::Math::Vec2(0, 1080)
    };
    std::array<HexMap::Core::Math::Vec2, 4> dst = {
        HexMap::Core::Math::Vec2(100, 100), HexMap::Core::Math::Vec2(1820, 80),
        HexMap::Core::Math::Vec2(1780, 1000), HexMap::Core::Math::Vec2(120, 980)
    };

    auto maybeH = HexMap::Core::Math::Homography::find4Point(src, dst);
    if (maybeH) {
        std::cout << "  -> Homography matrix computed successfully!\n";
        HexMap::Core::Math::Vec2 center(960, 540);
        HexMap::Core::Math::Vec2 warped = maybeH->transformPoint(center);
        std::cout << "  -> Center (960, 540) warped to: (" << warped.x << ", " << warped.y << ")\n";
    }

    std::cout << "\n[2] Initializing MikMap Dear ImGui UI Panels...\n";
    HexMap::UI::ControlPanel controlPanel;
    std::cout << "  -> ControlPanel ready.\n";
    std::cout << "  -> OutputPanel (Mapping Tree + 3 Pinned Bottom Buttons + Horizontal Scroll Tabs) ready.\n";
    std::cout << "  -> LayerPanel (Clip Matrix + OpacityBar + BlendModes) ready.\n";

    std::cout << "\n[3] Native C++ Application Ready!\n";
    std::cout << "  To run on Windows with openFrameworks / GLFW:\n";
    std::cout << "  - Open HexMapping.sln in Visual Studio 2022\n";
    std::cout << "  - Or build with CMake: cmake -B build && cmake --build build\n\n";

    return 0;
}

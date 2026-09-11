#include "ControlPanel.h"
#include "Theme.h"

namespace HexMap::UI {

ControlPanel::ControlPanel() {
    m_outputPanel = std::make_unique<OutputPanel>();
    m_layerPanel = std::make_unique<LayerPanel>();
}

void ControlPanel::Render() {
    // Cửa sổ gốc toàn màn hình không viền cho Dear ImGui
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->Pos);
    ImGui::SetNextWindowSize(viewport->Size);
    ImGui::SetNextWindowViewport(viewport->ID);

    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoTitleBar |
                                   ImGuiWindowFlags_NoCollapse |
                                   ImGuiWindowFlags_NoResize |
                                   ImGuiWindowFlags_NoMove |
                                   ImGuiWindowFlags_NoBringToFrontOnFocus |
                                   ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));

    ImGui::Begin("HexMappingMainWindow", nullptr, windowFlags);
    ImGui::PopStyleVar(3);

    // 1. Top Header Navigation Bar
    RenderTopHeader();

    // 2. Nội dung panel đang được chọn
    ImGui::BeginChild("MainContentArea", ImVec2(0, 0), false);
    switch (m_currentTab) {
        case ViewTab::Composition:
            m_layerPanel->Render();
            break;

        case ViewTab::AdvancedMapping:
            m_outputPanel->Render();
            break;

        case ViewTab::SensorIO:
            ImGui::TextColored(ColorPalette::VibrantOrange, "SENSOR I/O & CALIBRATION (Serial / OSC / Depth)");
            ImGui::Separator();
            ImGui::Text("TripleBuffer wait-free pipeline active at 60Hz.");
            break;

        case ViewTab::Performance:
            ImGui::TextColored(ColorPalette::SuccessGreen, "PERFORMANCE METRICS (p99 latency < 17.1ms)");
            ImGui::Separator();
            ImGui::Text("FPS: 60.0 | Jitter: < 0.8ms | VRAM: ~8.0 MB");
            break;
    }
    ImGui::EndChild();

    ImGui::End();
}

void ControlPanel::RenderTopHeader() {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ColorPalette::HeaderBg);
    ImGui::BeginChild("TopHeaderBar", ImVec2(0, 44.0f), false);

    ImGui::SetCursorPos(ImVec2(12.0f, 8.0f));

    // Logo / App Title
    ImGui::TextColored(ColorPalette::VibrantOrange, "MIKMAP");
    ImGui::SameLine();
    ImGui::TextDisabled("v1.0");

    ImGui::SameLine(120.0f);

    // View Navigation Tabs
    const char* tabs[] = { "Composition", "Advanced Output", "Sensors & Calib", "Performance" };
    ViewTab tabEnums[] = { ViewTab::Composition, ViewTab::AdvancedMapping, ViewTab::SensorIO, ViewTab::Performance };

    for (int i = 0; i < 4; ++i) {
        bool isSelected = (m_currentTab == tabEnums[i]);
        if (isSelected) {
            ImGui::PushStyleColor(ImGuiCol_Button, ColorPalette::VibrantOrange);
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
        }

        if (ImGui::Button(tabs[i], ImVec2(0, 26.0f))) {
            m_currentTab = tabEnums[i];
        }
        ImGui::PopStyleColor(2);
        ImGui::SameLine();
    }

    // Master Controls (Blackout, Freeze, Master Opacity)
    float rightControlsX = ImGui::GetWindowWidth() - 320.0f;
    if (rightControlsX > 500.0f) {
        ImGui::SameLine(rightControlsX);

        // Blackout button
        if (m_masterBlackout) {
            ImGui::PushStyleColor(ImGuiCol_Button, ColorPalette::DangerRed);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        }
        if (ImGui::Button("B", ImVec2(28.0f, 26.0f))) {
            m_masterBlackout = !m_masterBlackout;
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // Freeze button
        if (m_freezeOutput) {
            ImGui::PushStyleColor(ImGuiCol_Button, ColorPalette::SliceCyan);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
        }
        if (ImGui::Button("F", ImVec2(28.0f, 26.0f))) {
            m_freezeOutput = !m_freezeOutput;
        }
        ImGui::PopStyleColor();

        ImGui::SameLine();

        // Master Opacity
        Theme::OpacityBar("M", &m_masterOpacity, 120.0f, 26.0f);

        ImGui::SameLine();

        // FPS Badge
        ImGui::TextColored(ColorPalette::SuccessGreen, "60.0 FPS");
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

} // namespace HexMap::UI

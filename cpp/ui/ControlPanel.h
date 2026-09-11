#pragma once

#include <imgui.h>
#include <memory>
#include "panels/OutputPanel.h"
#include "panels/LayerPanel.h"

namespace HexMap::UI {

enum class ViewTab {
    Composition,
    AdvancedMapping,
    SensorIO,
    Performance
};

class ControlPanel {
public:
    ControlPanel();
    ~ControlPanel() = default;

    // Render toàn bộ UI chính (được gọi từ ofApp::draw() hoặc main render thread)
    void Render();

private:
    void RenderTopHeader();

    ViewTab m_currentTab = ViewTab::AdvancedMapping;
    bool m_masterBlackout = false;
    bool m_freezeOutput = false;
    float m_masterOpacity = 1.0f;
    float m_bpm = 120.0f;

    std::unique_ptr<OutputPanel> m_outputPanel;
    std::unique_ptr<LayerPanel> m_layerPanel;
};

} // namespace HexMap::UI

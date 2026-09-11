#pragma once

#include <imgui.h>
#include <string>
#include <vector>

namespace HexMap::UI {

struct ClipItem {
    std::string id;
    std::string name;
    bool active = false;
    float position = 0.0f;
    float duration = 10.0f;
    std::string filename;
};

struct LayerItem {
    std::string id;
    std::string name;
    float opacity = 1.0f;
    int blendMode = 0; // 0: Alpha, 1: Add, 2: Screen, 3: Multiply
    std::vector<ClipItem> clips;
};

class LayerPanel {
public:
    LayerPanel();
    ~LayerPanel() = default;

    void Render();

private:
    void RenderClipCell(LayerItem& layer, ClipItem& clip, int layerIdx, int clipIdx, float width, float height);

    std::vector<LayerItem> m_layers;
    int m_columnCount = 4;
};

} // namespace HexMap::UI

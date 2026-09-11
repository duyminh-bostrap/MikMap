#pragma once

#include <imgui.h>
#include <string>
#include <vector>
#include <memory>
#include <array>

namespace HexMap::UI {

struct MaskModel {
    std::string id;
    std::string name;
    bool inverted = false;
    float feather = 0.0f;
    std::vector<ImVec2> controlPoints;
};

struct SliceModel {
    std::string id;
    std::string name;
    bool visible = true;
    ImVec4 inputRect = ImVec4(0, 0, 1920, 1080); // x, y, w, h
    std::array<ImVec2, 4> outputQuad;            // tl, tr, br, bl (keystone)
    std::vector<MaskModel> masks;
};

struct ScreenModel {
    std::string id;
    std::string name;
    std::string outputDevice = "Projector 1";
    int width = 1920;
    int height = 1080;
    int fps = 60;
    bool edgeBlending = true;
    std::vector<SliceModel> slices;
};

class OutputPanel {
public:
    OutputPanel();
    ~OutputPanel() = default;

    // Render toàn bộ panel Advanced Output
    void Render();

    // Dữ liệu mô hình mapping
    std::vector<ScreenModel>& GetScreens() { return m_screens; }

private:
    void RenderLeftHierarchyTree(float width);
    void RenderCenterStage();
    void RenderRightProperties(float width);

    // Helpers cho stage
    void DrawStageGrid(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float zoom);
    void DrawSliceQuad(ImDrawList* drawList, const ImVec2& stagePos, float scale, SliceModel& slice, bool isSelected);

    // State
    std::vector<ScreenModel> m_screens;
    int m_activeScreenIndex = 0;
    std::string m_activeSliceId;
    std::string m_activeMaskId;

    bool m_isInputMode = false; // false = Output Routing, true = Input Selection
    float m_zoom = 0.45f;
    ImVec2 m_panOffset = ImVec2(40.0f, 40.0f);
    int m_draggingCornerIndex = -1; // 0: tl, 1: tr, 2: br, 3: bl
};

} // namespace HexMap::UI

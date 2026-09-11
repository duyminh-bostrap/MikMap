#include "LayerPanel.h"
#include "../Theme.h"
#include <imgui_internal.h>

namespace HexMap::UI {

LayerPanel::LayerPanel() {
    // Khởi tạo 3 layer mẫu với các clip giống web app
    for (int l = 1; l <= 3; ++l) {
        LayerItem layer;
        layer.id = "layer-" + std::to_string(l);
        layer.name = "Layer " + std::to_string(l);
        layer.opacity = (l == 1) ? 1.0f : 0.8f;
        layer.blendMode = 0;

        for (int c = 1; c <= m_columnCount; ++c) {
            ClipItem clip;
            clip.id = "clip-" + std::to_string(l) + "-" + std::to_string(c);
            clip.name = "Visual " + std::to_string((l - 1) * m_columnCount + c);
            clip.active = (c == 1); // Cột 1 kích hoạt mặc định
            clip.position = 2.5f;
            clip.duration = 12.0f;
            layer.clips.push_back(clip);
        }
        m_layers.push_back(layer);
    }
}

void LayerPanel::Render() {
    ImGui::BeginChild("CompositionLayerPanel", ImVec2(0, 0), true);

    // Cột tiêu đề các Column Trigger
    ImGui::TextColored(ColorPalette::VibrantOrange, "COMPOSITION / DECK");
    ImGui::SameLine();
    ImGui::TextDisabled("| 4 Columns x %d Layers", (int)m_layers.size());
    ImGui::Separator();
    ImGui::Spacing();

    const float headerWidth = 140.0f;
    const float clipCellWidth = 130.0f;
    const float clipCellHeight = 75.0f;

    // Header Trigger Columns
    ImGui::Dummy(ImVec2(headerWidth, 24.0f));
    for (int col = 1; col <= m_columnCount; ++col) {
        ImGui::SameLine();
        char colLabel[32];
        snprintf(colLabel, sizeof(colLabel), "Column %d", col);
        if (ImGui::Button(colLabel, ImVec2(clipCellWidth, 24.0f))) {
            // Trigger toàn bộ clip ở cột này
            for (auto& layer : m_layers) {
                for (size_t c = 0; c < layer.clips.size(); ++c) {
                    layer.clips[c].active = ((int)c == col - 1);
                }
            }
        }
    }

    ImGui::Separator();

    // Render từng hàng Layer
    for (size_t lIdx = 0; lIdx < m_layers.size(); ++lIdx) {
        LayerItem& layer = m_layers[lIdx];
        ImGui::PushID(layer.id.c_str());

        // 1. Cột điều khiển Layer (Tên + Opacity Bar + Blend Mode)
        ImGui::BeginGroup();
        ImGui::TextColored(ColorPalette::BurntBrownText, "%s", layer.name.c_str());

        // Thanh kéo Opacity tuỳ chỉnh
        char opId[32];
        snprintf(opId, sizeof(opId), "##Op%d", (int)lIdx);
        Theme::OpacityBar(opId, &layer.opacity, headerWidth - 10.0f, 16.0f);

        // Blend Mode combo
        const char* blendModes[] = { "Alpha", "Add", "Screen", "Multiply" };
        ImGui::SetNextItemWidth(headerWidth - 10.0f);
        ImGui::Combo("##Blend", &layer.blendMode, blendModes, IM_ARRAYSIZE(blendModes));
        ImGui::EndGroup();

        // 2. Các ô Clip trong Layer
        for (size_t cIdx = 0; cIdx < layer.clips.size(); ++cIdx) {
            ImGui::SameLine();
            RenderClipCell(layer, layer.clips[cIdx], (int)lIdx, (int)cIdx, clipCellWidth, clipCellHeight);
        }

        ImGui::Separator();
        ImGui::PopID();
    }

    ImGui::EndChild();
}

void LayerPanel::RenderClipCell(LayerItem& layer, ClipItem& clip, int layerIdx, int clipIdx, float width, float height) {
    ImGui::PushID(clip.id.c_str());

    // =========================================================================
    // COLOR PALETTE THEO YÊU CẦU:
    // - Clip Active: Vibrant Orange (#FF7F50)
    // - Clip Inactive: Deep Burnt Brown / Espresso (#3B1D0E bg, #4A2411 border, #E8C4A2 text)
    // =========================================================================
    if (clip.active) {
        ImGui::PushStyleColor(ImGuiCol_Button, ColorPalette::VibrantOrange);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColorPalette::OrangeHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColorPalette::OrangeActive);
        ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(1.0f, 0.65f, 0.45f, 1.0f));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ColorPalette::BurntBrownBg);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ColorPalette::BurntBrownHover);
        ImGui::PushStyleColor(ImGuiCol_ButtonActive, ColorPalette::BurntBrownBorder);
        ImGui::PushStyleColor(ImGuiCol_Border, ColorPalette::BurntBrownBorder);
    }
    ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);

    ImVec2 pos = ImGui::GetCursorScreenPos();
    if (ImGui::Button("##ClipButton", ImVec2(width, height))) {
        // Toggle clip trong layer (chỉ 1 clip active mỗi layer)
        for (auto& c : layer.clips) {
            c.active = false;
        }
        clip.active = true;
    }

    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Tiêu đề Clip
    ImVec2 titlePos = ImVec2(pos.x + 8.0f, pos.y + 6.0f);
    ImU32 textCol = clip.active ? IM_COL32(0, 0, 0, 255) : IM_COL32(232, 196, 162, 255);
    drawList->AddText(titlePos, textCol, clip.name.c_str());

    // Progress Bar transport dưới đáy ô clip
    if (clip.active) {
        float progress = clip.position / clip.duration;
        ImVec2 pMin = ImVec2(pos.x + 4.0f, pos.y + height - 8.0f);
        ImVec2 pMax = ImVec2(pos.x + width - 4.0f, pos.y + height - 4.0f);
        drawList->AddRectFilled(pMin, pMax, IM_COL32(0, 0, 0, 100), 2.0f);
        drawList->AddRectFilled(pMin, ImVec2(pMin.x + (pMax.x - pMin.x) * progress, pMax.y),
                                IM_COL32(0, 0, 0, 220), 2.0f);
    }

    ImGui::PopStyleVar();
    ImGui::PopStyleColor(4);

    ImGui::PopID();
}

} // namespace HexMap::UI

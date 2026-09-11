#include "OutputPanel.h"
#include "../Theme.h"
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>

namespace HexMap::UI {

OutputPanel::OutputPanel() {
    // Khởi tạo Screen 1 mặc định giống React Initial State
    ScreenModel defaultScreen;
    defaultScreen.id = "screen-1";
    defaultScreen.name = "Screen 1";
    defaultScreen.outputDevice = "Projector 1";
    defaultScreen.width = 1920;
    defaultScreen.height = 1080;
    defaultScreen.fps = 60;
    defaultScreen.edgeBlending = true;

    SliceModel defaultSlice;
    defaultSlice.id = "slice-1";
    defaultSlice.name = "Center Wall";
    defaultSlice.visible = true;
    defaultSlice.inputRect = ImVec4(0, 0, 1920, 1080);
    defaultSlice.outputQuad = {
        ImVec2(100.0f, 100.0f),
        ImVec2(700.0f, 120.0f),
        ImVec2(680.0f, 500.0f),
        ImVec2(120.0f, 480.0f)
    };

    MaskModel defaultMask;
    defaultMask.id = "mask-1";
    defaultMask.name = "Arch Curve";
    defaultMask.inverted = false;
    defaultMask.feather = 8.0f;
    defaultMask.controlPoints = {
        ImVec2(150, 150), ImVec2(350, 120), ImVec2(550, 150),
        ImVec2(550, 350), ImVec2(150, 350)
    };

    defaultSlice.masks.push_back(defaultMask);
    defaultScreen.slices.push_back(defaultSlice);
    m_screens.push_back(defaultScreen);

    m_activeSliceId = defaultSlice.id;
}

void OutputPanel::Render() {
    ImVec2 availableSize = ImGui::GetContentRegionAvail();
    const float leftWidth = 240.0f;
    const float rightWidth = 260.0f;

    // Split thành 3 cột: Trái (Tree + 3 nút) | Giữa (Stage) | Phải (Properties)
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));

    // CỘT TRÁI
    RenderLeftHierarchyTree(leftWidth);
    ImGui::SameLine();

    // CỘT GIỮA
    RenderCenterStage();
    ImGui::SameLine();

    // CỘT PHẢI
    RenderRightProperties(rightWidth);

    ImGui::PopStyleVar();
}

void OutputPanel::RenderLeftHierarchyTree(float width) {
    ImGui::BeginChild("MappingTreeContainer", ImVec2(width, 0), true, ImGuiWindowFlags_NoScrollbar);

    // Tiêu đề Tree
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ColorPalette::HeaderBg);
    ImGui::BeginChild("TreeHeader", ImVec2(0, 32.0f), false);
    ImGui::SetCursorPosY(7.0f);
    ImGui::SetCursorPosX(10.0f);
    ImGui::TextColored(ColorPalette::TextMuted, "MAPPING TREE");
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // Vùng scroll danh sách Screens -> Slices -> Masks
    float contentHeight = ImGui::GetContentRegionAvail().y - 110.0f; // Chừa chỗ cho 3 nút ở dưới
    ImGui::BeginChild("TreeScrollArea", ImVec2(0, contentHeight), false, ImGuiWindowFlags_AlwaysVerticalScrollbar);

    for (size_t sIdx = 0; sIdx < m_screens.size(); ++sIdx) {
        ScreenModel& screen = m_screens[sIdx];
        bool isScreenActive = (m_activeScreenIndex == (int)sIdx);

        ImGui::PushID(screen.id.c_str());

        // Screen Item
        if (isScreenActive) {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ColorPalette::VibrantOrange);
        } else {
            ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
        }

        std::string screenLabel = "[S] " + screen.name;
        if (ImGui::Selectable(screenLabel.c_str(), isScreenActive)) {
            m_activeScreenIndex = (int)sIdx;
            if (!screen.slices.empty()) {
                m_activeSliceId = screen.slices[0].id;
            }
        }
        ImGui::PopStyleColor(2);

        // Delete screen nếu có > 1 screen
        if (m_screens.size() > 1) {
            ImGui::SameLine(width - 45.0f);
            if (ImGui::SmallButton("X")) {
                m_screens.erase(m_screens.begin() + sIdx);
                if (m_activeScreenIndex >= (int)m_screens.size()) {
                    m_activeScreenIndex = (int)m_screens.size() - 1;
                }
                ImGui::PopID();
                break;
            }
        }

        // Slices của Screen
        ImGui::Indent(16.0f);
        for (auto& slice : screen.slices) {
            ImGui::PushID(slice.id.c_str());
            bool isSliceActive = (slice.id == m_activeSliceId);

            if (isSliceActive) {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.22f, 0.12f, 0.08f, 0.8f));
                ImGui::PushStyleColor(ImGuiCol_Text, ColorPalette::VibrantOrange);
            } else {
                ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ColorPalette::TextMuted);
            }

            std::string sliceLabel = "|- " + slice.name;
            if (ImGui::Selectable(sliceLabel.c_str(), isSliceActive)) {
                m_activeScreenIndex = (int)sIdx;
                m_activeSliceId = slice.id;
                m_activeMaskId = "";
            }
            ImGui::PopStyleColor(2);

            // Visibility toggle
            ImGui::SameLine(width - 45.0f);
            if (ImGui::SmallButton(slice.visible ? "V" : "-")) {
                slice.visible = !slice.visible;
            }

            // Masks của Slice
            ImGui::Indent(16.0f);
            for (auto& mask : slice.masks) {
                ImGui::PushID(mask.id.c_str());
                bool isMaskActive = (mask.id == m_activeMaskId && isSliceActive);

                if (isMaskActive) {
                    ImGui::PushStyleColor(ImGuiCol_Text, ColorPalette::MaskYellow);
                } else {
                    ImGui::PushStyleColor(ImGuiCol_Text, ColorPalette::TextMuted);
                }

                std::string maskLabel = "* " + mask.name;
                if (ImGui::Selectable(maskLabel.c_str(), isMaskActive)) {
                    m_activeScreenIndex = (int)sIdx;
                    m_activeSliceId = slice.id;
                    m_activeMaskId = mask.id;
                }
                ImGui::PopStyleColor();
                ImGui::PopID();
            }
            ImGui::Unindent(16.0f);

            ImGui::PopID();
        }
        ImGui::Unindent(16.0f);

        ImGui::PopID();
    }
    ImGui::EndChild();

    // =========================================================================
    // KHU VỰC CỐ ĐỊNH Ở GÓC DƯỚI: ĐÚNG 3 NÚT TÁC VỤ THEO YÊU CẦU CỦA USER
    // 1. Add Screen
    // 2. Add Slice
    // 3. Add Mask
    // =========================================================================
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ColorPalette::HeaderBg);
    ImGui::BeginChild("BottomActionDock", ImVec2(0, 105.0f), true);

    // 1. Nút Add Screen (Nổi bật màu cam phong cách MikMap)
    if (Theme::AccentButton("Add Screen", ColorPalette::VibrantOrange, ImVec2(-1, 26.0f))) {
        ScreenModel newScreen;
        newScreen.id = "screen-" + std::to_string(m_screens.size() + 1);
        newScreen.name = "Screen " + std::to_string(m_screens.size() + 1);
        newScreen.outputDevice = "Projector " + std::to_string(m_screens.size() + 1);
        newScreen.width = 1920;
        newScreen.height = 1080;
        newScreen.fps = 60;
        newScreen.edgeBlending = true;

        SliceModel newSlice;
        newSlice.id = "slice-" + std::to_string(m_screens.size() + 1) + "-1";
        newSlice.name = "Slice 1";
        newSlice.visible = true;
        newSlice.inputRect = ImVec4(0, 0, 1920, 1080);
        newSlice.outputQuad = {
            ImVec2(100, 100), ImVec2(700, 100),
            ImVec2(700, 500), ImVec2(100, 500)
        };
        newScreen.slices.push_back(newSlice);

        m_screens.push_back(newScreen);
        m_activeScreenIndex = (int)m_screens.size() - 1;
        m_activeSliceId = newSlice.id;
    }

    ImGui::Spacing();

    // 2. Nút Add Slice
    if (Theme::SecondaryButton("+ Add Slice", ImVec2(-1, 24.0f))) {
        if (!m_screens.empty()) {
            ScreenModel& curScreen = m_screens[m_activeScreenIndex];
            SliceModel sl;
            sl.id = "slice-" + std::to_string(curScreen.slices.size() + 1);
            sl.name = "Slice " + std::to_string(curScreen.slices.size() + 1);
            sl.visible = true;
            sl.inputRect = ImVec4(100, 100, 800, 600);
            sl.outputQuad = {
                ImVec2(200, 200), ImVec2(600, 200),
                ImVec2(600, 500), ImVec2(200, 500)
            };
            curScreen.slices.push_back(sl);
            m_activeSliceId = sl.id;
        }
    }

    ImGui::Spacing();

    // 3. Nút Add Mask
    if (Theme::AccentButton("Add Mask", ColorPalette::MaskYellow, ImVec2(-1, 24.0f))) {
        if (!m_screens.empty()) {
            ScreenModel& curScreen = m_screens[m_activeScreenIndex];
            for (auto& sl : curScreen.slices) {
                if (sl.id == m_activeSliceId) {
                    MaskModel m;
                    m.id = "mask-" + std::to_string(sl.masks.size() + 1);
                    m.name = "Mask " + std::to_string(sl.masks.size() + 1);
                    m.inverted = false;
                    m.feather = 0.0f;
                    m.controlPoints = {
                        ImVec2(150, 150), ImVec2(400, 150),
                        ImVec2(400, 400), ImVec2(150, 400)
                    };
                    sl.masks.push_back(m);
                    m_activeMaskId = m.id;
                    break;
                }
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::EndChild();
}

void OutputPanel::RenderCenterStage() {
    ImVec2 stageSize = ImVec2(ImGui::GetContentRegionAvail().x - 260.0f, 0);
    ImGui::BeginChild("MappingCenterStage", stageSize, true, ImGuiWindowFlags_NoScrollbar);

    // =========================================================================
    // STAGE TOOLBAR
    // Bao gồm: Switch Input/Output | Screen Tab Bar (cuộn ngang chống tràn) | Reset Warp
    // =========================================================================
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ColorPalette::HeaderBg);
    ImGui::BeginChild("StageToolbar", ImVec2(0, 36.0f), false);

    ImGui::SetCursorPos(ImVec2(8.0f, 5.0f));

    // Input vs Output toggle
    if (m_isInputMode) {
        ImGui::PushStyleColor(ImGuiCol_Button, ColorPalette::SliceCyan);
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
    }
    if (ImGui::Button("Input Selection", ImVec2(105, 24))) m_isInputMode = true;
    ImGui::PopStyleColor();

    ImGui::SameLine();

    if (!m_isInputMode) {
        ImGui::PushStyleColor(ImGuiCol_Button, ColorPalette::VibrantOrange);
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
    } else {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    }
    if (ImGui::Button("Output Routing", ImVec2(105, 24))) m_isInputMode = false;
    ImGui::PopStyleColor(2);

    ImGui::SameLine();
    ImGui::TextDisabled("|");
    ImGui::SameLine();

    // =========================================================================
    // FIX TRÀN: HORIZONTALLY SCROLLABLE SCREEN TAB BAR
    // Khi có nhiều screen, thanh này tự cuộn mượt bằng con lăn chuột, không tràn UI
    // =========================================================================
    if (m_screens.size() > 1) {
        float maxTabBarWidth = std::min(400.0f, ImGui::GetContentRegionAvail().x - 140.0f);
        ImGui::BeginChild("ScreenTabsHorizontalScroll", ImVec2(maxTabBarWidth, 26.0f), false,
                          ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);

        // Hỗ trợ cuộn ngang bằng con lăn chuột
        float wheel = ImGui::GetIO().MouseWheel;
        if (ImGui::IsWindowHovered() && wheel != 0.0f) {
            ImGui::SetScrollX(ImGui::GetScrollX() - wheel * 35.0f);
        }

        for (size_t i = 0; i < m_screens.size(); ++i) {
            if (i > 0) ImGui::SameLine();

            bool isCurrent = (m_activeScreenIndex == (int)i);
            if (isCurrent) {
                ImGui::PushStyleColor(ImGuiCol_Button, ColorPalette::VibrantOrange);
                ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0, 0, 0, 1));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, ColorPalette::TextMuted);
            }

            if (ImGui::Button(m_screens[i].name.c_str(), ImVec2(0, 22.0f))) {
                m_activeScreenIndex = (int)i;
            }
            ImGui::PopStyleColor(2);
        }
        ImGui::EndChild();
        ImGui::SameLine();
    }

    // Reset Warp button
    if (ImGui::Button("Reset Warp", ImVec2(80, 24))) {
        if (!m_screens.empty()) {
            for (auto& sl : m_screens[m_activeScreenIndex].slices) {
                if (sl.id == m_activeSliceId) {
                    sl.outputQuad = {
                        ImVec2(100, 100), ImVec2(700, 100),
                        ImVec2(700, 500), ImVec2(100, 500)
                    };
                    break;
                }
            }
        }
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();

    // =========================================================================
    // CANVAS STAGE (OpenGL / ImDrawList)
    // =========================================================================
    ImVec2 canvasPos = ImGui::GetCursorScreenPos();
    ImVec2 canvasSize = ImGui::GetContentRegionAvail();
    ImDrawList* drawList = ImGui::GetWindowDrawList();

    // Nền tối canvas
    drawList->AddRectFilled(canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y),
                            IM_COL32(10, 10, 10, 255));

    // Lưới toạ độ
    DrawStageGrid(drawList, canvasPos, ImVec2(canvasPos.x + canvasSize.x, canvasPos.y + canvasSize.y), m_zoom);

    // Vẽ slices và cho phép kéo corner pins
    if (!m_screens.empty()) {
        ScreenModel& activeScreen = m_screens[m_activeScreenIndex];
        for (auto& slice : activeScreen.slices) {
            bool isSelected = (slice.id == m_activeSliceId);
            DrawSliceQuad(drawList, canvasPos, 1.0f, slice, isSelected);
        }
    }

    ImGui::EndChild();
}

void OutputPanel::DrawStageGrid(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float zoom) {
    float step = 50.0f * (zoom / 0.5f);
    if (step < 20.0f) step = 20.0f;

    ImU32 gridColor = IM_COL32(30, 30, 30, 150);
    for (float x = min.x; x < max.x; x += step) {
        drawList->AddLine(ImVec2(x, min.y), ImVec2(x, max.y), gridColor);
    }
    for (float y = min.y; y < max.y; y += step) {
        drawList->AddLine(ImVec2(min.x, y), ImVec2(max.x, y), gridColor);
    }
}

void OutputPanel::DrawSliceQuad(ImDrawList* drawList, const ImVec2& stagePos, float scale, SliceModel& slice, bool isSelected) {
    ImVec2 p[4];
    for (int i = 0; i < 4; ++i) {
        p[i] = ImVec2(stagePos.x + slice.outputQuad[i].x * scale, stagePos.y + slice.outputQuad[i].y * scale);
    }

    // Quad Outline
    ImU32 lineColor = isSelected ? IM_COL32(255, 127, 80, 255) : IM_COL32(80, 80, 80, 200);
    drawList->AddQuad(p[0], p[1], p[2], p[3], lineColor, 2.0f);

    // Quad Fill (nhẹ)
    ImU32 fillColor = isSelected ? IM_COL32(255, 127, 80, 25) : IM_COL32(50, 50, 50, 15);
    drawList->AddQuadFilled(p[0], p[1], p[2], p[3], fillColor);

    // Tên slice ở tâm
    ImVec2 center = ImVec2((p[0].x + p[1].x + p[2].x + p[3].x) * 0.25f,
                           (p[0].y + p[1].y + p[2].y + p[3].y) * 0.25f);
    drawList->AddText(center, IM_COL32(200, 200, 200, 255), slice.name.c_str());

    // Corner Pin Drag Handles khi đang chọn
    if (isSelected) {
        ImVec2 mousePos = ImGui::GetIO().MousePos;
        bool isMouseDown = ImGui::IsMouseDown(ImGuiMouseButton_Left);

        const char* cornerNames[4] = { "TL", "TR", "BR", "BL" };
        for (int i = 0; i < 4; ++i) {
            float dist = std::hypot(mousePos.x - p[i].x, mousePos.y - p[i].y);
            bool isHovered = (dist < 12.0f);

            if (isHovered && isMouseDown && m_draggingCornerIndex == -1) {
                m_draggingCornerIndex = i;
            }

            if (m_draggingCornerIndex == i) {
                slice.outputQuad[i] = ImVec2((mousePos.x - stagePos.x) / scale,
                                             (mousePos.y - stagePos.y) / scale);
                if (!isMouseDown) {
                    m_draggingCornerIndex = -1;
                }
            }

            ImU32 handleColor = (m_draggingCornerIndex == i || isHovered) ?
                                IM_COL32(255, 255, 255, 255) : IM_COL32(255, 127, 80, 255);
            drawList->AddCircleFilled(p[i], 6.0f, handleColor);
            drawList->AddCircle(p[i], 7.0f, IM_COL32(0, 0, 0, 200), 12, 1.5f);
            drawList->AddText(ImVec2(p[i].x + 8, p[i].y - 8), IM_COL32(255, 127, 80, 255), cornerNames[i]);
        }
    }
}

void OutputPanel::RenderRightProperties(float width) {
    ImGui::BeginChild("PropertiesInspector", ImVec2(width, 0), true);

    ImGui::TextColored(ColorPalette::VibrantOrange, "PROPERTIES");
    ImGui::Separator();
    ImGui::Spacing();

    if (m_screens.empty()) {
        ImGui::EndChild();
        return;
    }

    ScreenModel& activeScreen = m_screens[m_activeScreenIndex];
    SliceModel* activeSlice = nullptr;
    for (auto& sl : activeScreen.slices) {
        if (sl.id == m_activeSliceId) {
            activeSlice = &sl;
            break;
        }
    }

    // 1. Slice Properties
    if (activeSlice) {
        ImGui::TextColored(ColorPalette::TextMuted, "SLICE SETTINGS");
        char nameBuf[128];
        snprintf(nameBuf, sizeof(nameBuf), "%s", activeSlice->name.c_str());
        if (ImGui::InputText("Name##Slice", nameBuf, sizeof(nameBuf))) {
            activeSlice->name = nameBuf;
        }

        ImGui::Spacing();
        ImGui::Text("Input Rect (Source UV):");
        float rect[4] = { activeSlice->inputRect.x, activeSlice->inputRect.y,
                          activeSlice->inputRect.z, activeSlice->inputRect.w };
        if (ImGui::DragFloat4("X / Y / W / H", rect, 1.0f, 0.0f, 3840.0f, "%.0f")) {
            activeSlice->inputRect = ImVec4(rect[0], rect[1], rect[2], rect[3]);
        }

        ImGui::Spacing();
        ImGui::Text("Corner Pins (H_w):");
        const char* cLabels[4] = { "TL", "TR", "BR", "BL" };
        for (int i = 0; i < 4; ++i) {
            float pt[2] = { activeSlice->outputQuad[i].x, activeSlice->outputQuad[i].y };
            if (ImGui::DragFloat2(cLabels[i], pt, 1.0f, 0.0f, 3840.0f, "%.0f")) {
                activeSlice->outputQuad[i] = ImVec2(pt[0], pt[1]);
            }
        }
    }

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();

    // 2. Screen Output Info
    ImGui::TextColored(ColorPalette::TextMuted, "SCREEN OUTPUT INFO");
    char screenNameBuf[128];
    snprintf(screenNameBuf, sizeof(screenNameBuf), "%s", activeScreen.name.c_str());
    if (ImGui::InputText("Screen Name", screenNameBuf, sizeof(screenNameBuf))) {
        activeScreen.name = screenNameBuf;
    }

    char deviceBuf[128];
    snprintf(deviceBuf, sizeof(deviceBuf), "%s", activeScreen.outputDevice.c_str());
    if (ImGui::InputText("Output Device", deviceBuf, sizeof(deviceBuf))) {
        activeScreen.outputDevice = deviceBuf;
    }

    ImGui::Text("Resolution: %dx%d @ %dHz", activeScreen.width, activeScreen.height, activeScreen.fps);

    if (ImGui::Checkbox("Edge Blending", &activeScreen.edgeBlending)) {
        // Toggle edge blending shader
    }

    ImGui::EndChild();
}

} // namespace HexMap::UI

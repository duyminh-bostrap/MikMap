import React, { useState } from 'react';
import { X, Copy, Check, Code2, Download } from 'lucide-react';

interface CppCodeModalProps {
  isOpen: boolean;
  onClose: () => void;
}

const CPP_FILES = [
  {
    name: 'Theme.h',
    path: 'cpp/ui/Theme.h',
    code: `#pragma once

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
    static void SetupMikMapTheme();
    static bool OpacityBar(const char* label, float* value, float width = -1.0f, float height = 18.0f);
    static bool PrimaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
    static bool SecondaryButton(const char* label, const ImVec2& size = ImVec2(0, 0));
    static bool AccentButton(const char* label, const ImVec4& color, const ImVec2& size = ImVec2(0, 0));
};

} // namespace HexMap::UI`
  },
  {
    name: 'OutputPanel.h',
    path: 'cpp/ui/panels/OutputPanel.h',
    code: `#pragma once

#include <imgui.h>
#include <string>
#include <vector>
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
    ImVec4 inputRect = ImVec4(0, 0, 1920, 1080);
    std::array<ImVec2, 4> outputQuad; // tl, tr, br, bl (keystone)
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

    void Render();
    std::vector<ScreenModel>& GetScreens() { return m_screens; }

private:
    void RenderLeftHierarchyTree(float width);
    void RenderCenterStage();
    void RenderRightProperties(float width);

    void DrawStageGrid(ImDrawList* drawList, const ImVec2& min, const ImVec2& max, float zoom);
    void DrawSliceQuad(ImDrawList* drawList, const ImVec2& stagePos, float scale, SliceModel& slice, bool isSelected);

    std::vector<ScreenModel> m_screens;
    int m_activeScreenIndex = 0;
    std::string m_activeSliceId;
    std::string m_activeMaskId;

    bool m_isInputMode = false;
    float m_zoom = 0.45f;
    int m_draggingCornerIndex = -1;
};

} // namespace HexMap::UI`
  },
  {
    name: 'OutputPanel.cpp',
    path: 'cpp/ui/panels/OutputPanel.cpp',
    code: `// Xem file cpp/ui/panels/OutputPanel.cpp trong project
// Chứa đầy đủ:
// 1. Cột trái: Hierarchy tree + 3 nút tác vụ cố định góc dưới (Add Screen, Add Slice, Add Mask)
// 2. Cột giữa: Stage Toolbar + Thanh tab Screen cuộn ngang (horizontal scroll) chống tràn khi có nhiều màn hình
// 3. Kéo chốt corner pin 4 góc (TL, TR, BR, BL) tương tác mượt mà trên canvas
// 4. Cột phải: Inspector thuộc tính Slice & Screen`
  },
  {
    name: 'LayerPanel.cpp',
    path: 'cpp/ui/panels/LayerPanel.cpp',
    code: `// Xem file cpp/ui/panels/LayerPanel.cpp trong project
// Chứa ma trận Composition:
// - Ô Clip kích hoạt: Màu cam rực rỡ (#FF7F50)
// - Ô Clip không kích hoạt: Màu nâu trầm espresso (#3B1D0E)
// - Thanh kéo Opacity từng layer (Theme::OpacityBar) & Blend mode`
  },
  {
    name: 'ControlPanel.cpp',
    path: 'cpp/ui/ControlPanel.cpp',
    code: `// Xem file cpp/ui/ControlPanel.cpp trong project
// Nối các panel và tích hợp trực tiếp vào main loop của openFrameworks:
// - Header điều hướng (Composition, Advanced Output, Sensors, Performance)
// - Master controls (Blackout B, Freeze F, Master Opacity, FPS badge)`
  },
  {
    name: 'Homography.h',
    path: 'cpp/core/math/Homography.h',
    code: `#pragma once

#include "Mat3.h"
#include "Vec2.h"
#include <array>
#include <optional>

namespace HexMap::Core::Math {

class Homography {
public:
    // Tính ma trận Homography biến đổi 4 điểm nguồn (src) sang 4 điểm đích (dst) bằng DLT
    static std::optional<Mat3> find4Point(const std::array<Vec2, 4>& src,
                                          const std::array<Vec2, 4>& dst);
};

} // namespace HexMap::Core::Math`
  },
  {
    name: 'CMakeLists.txt',
    path: 'cpp/CMakeLists.txt',
    code: `cmake_minimum_required(VERSION 3.20)
project(HexMapping VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CORE_SOURCES
    core/math/Mat3.cpp
    core/math/Homography.cpp
    core/model/WarpCornerPin.cpp
)

set(UI_SOURCES
    ui/Theme.cpp
    ui/panels/OutputPanel.cpp
    ui/panels/LayerPanel.cpp
    ui/ControlPanel.cpp
)

include_directories(
    \${CMAKE_CURRENT_SOURCE_DIR}
    \${CMAKE_CURRENT_SOURCE_DIR}/core
    \${CMAKE_CURRENT_SOURCE_DIR}/ui
)

add_library(HexMapCore STATIC \${CORE_SOURCES})
# add_executable(HexMapping app/main.cpp \${UI_SOURCES})`
  },
  {
    name: 'main.cpp',
    path: 'cpp/app/main.cpp',
    code: `#include <iostream>
#include "../ui/Theme.h"
#include "../ui/ControlPanel.h"
#include "../core/math/Homography.h"

int main() {
    std::cout << "MikMap / HexMapping C++20 Native Engine Ready.\\n";
    HexMap::UI::ControlPanel controlPanel;
    // Main loop running at 60 FPS on desktop
    return 0;
}`
  }
];

export const CppCodeModal: React.FC<CppCodeModalProps> = ({ isOpen, onClose }) => {
  const [selectedFileIdx, setSelectedFileIdx] = useState(0);
  const [copied, setCopied] = useState(false);

  if (!isOpen) return null;

  const currentFile = CPP_FILES[selectedFileIdx];

  const handleCopy = () => {
    navigator.clipboard.writeText(currentFile.code);
    setCopied(true);
    setTimeout(() => setCopied(false), 2000);
  };

  const handleDownload = () => {
    const blob = new Blob([currentFile.code], { type: 'text/plain;charset=utf-8' });
    const url = URL.createObjectURL(blob);
    const a = document.createElement('a');
    a.href = url;
    a.download = currentFile.name;
    a.click();
    URL.revokeObjectURL(url);
  };

  return (
    <div className="fixed inset-0 z-50 flex items-center justify-center bg-black/80 backdrop-blur-xs p-4">
      <div className="bg-[#121212] border border-[#2e2e2e] rounded-xl w-full max-w-4xl h-[85vh] flex flex-col shadow-2xl overflow-hidden animate-in fade-in zoom-in-95 duration-150">
        {/* Header */}
        <div className="h-12 bg-[#181818] border-b border-[#2a2a2a] px-4 flex items-center justify-between shrink-0">
          <div className="flex items-center gap-2.5">
            <div className="p-1 rounded bg-[#FF7F50]/15 text-[#FF7F50]">
              <Code2 size={16} />
            </div>
            <div>
              <span className="text-xs font-bold text-white tracking-wide">
                C++20 & Dear ImGui Source Files
              </span>
              <span className="text-[10px] text-[#888] ml-2 font-mono">
                HexMapping / MikMap Architecture
              </span>
            </div>
          </div>
          <button
            onClick={onClose}
            className="p-1 text-[#888] hover:text-white hover:bg-[#252525] rounded transition-colors cursor-pointer"
          >
            <X size={16} />
          </button>
        </div>

        {/* File Tabs Bar */}
        <div className="bg-[#0e0e0e] border-b border-[#222] px-3 py-2 flex items-center justify-between shrink-0">
          <div className="flex items-center gap-1.5 overflow-x-auto">
            {CPP_FILES.map((f, idx) => (
              <button
                key={f.name}
                onClick={() => setSelectedFileIdx(idx)}
                className={`px-3 py-1 rounded text-xs font-mono font-bold transition-all cursor-pointer ${
                  selectedFileIdx === idx
                    ? 'bg-[#FF7F50] text-black shadow-xs'
                    : 'text-[#888] hover:text-white hover:bg-[#1a1a1a]'
                }`}
              >
                {f.name}
              </button>
            ))}
          </div>

          <div className="flex items-center gap-2 shrink-0">
            <button
              onClick={handleCopy}
              className="flex items-center gap-1.5 px-3 py-1 rounded bg-[#1c1c1c] hover:bg-[#252525] text-white text-xs font-medium border border-[#333] transition-colors cursor-pointer"
            >
              {copied ? <Check size={12} className="text-[#06D6A0]" /> : <Copy size={12} />}
              <span>{copied ? 'Copied' : 'Copy'}</span>
            </button>
            <button
              onClick={handleDownload}
              className="flex items-center gap-1.5 px-3 py-1 rounded bg-[#1c1c1c] hover:bg-[#252525] text-[#FF7F50] text-xs font-medium border border-[#FF7F50]/30 transition-colors cursor-pointer"
            >
              <Download size={12} />
              <span>Download</span>
            </button>
          </div>
        </div>

        {/* Code Content */}
        <div className="flex-1 bg-[#0a0a0a] p-4 overflow-auto font-mono text-xs text-[#d4d4d4] leading-relaxed select-text">
          <div className="text-[11px] text-[#666] mb-3 pb-2 border-b border-[#222]">
            Location in project: <span className="text-[#FF7F50]">{currentFile.path}</span>
          </div>
          <pre className="whitespace-pre overflow-x-auto font-mono">
            {currentFile.code}
          </pre>
        </div>

        {/* Footer info */}
        <div className="h-10 bg-[#161616] border-t border-[#2a2a2a] px-4 flex items-center justify-between text-[11px] text-[#888] shrink-0">
          <span>Tất cả file C++ đã được lưu trong thư mục <code className="text-[#FF7F50] font-mono">/cpp/ui/</code> của dự án.</span>
          <button
            onClick={onClose}
            className="px-3 py-1 rounded bg-[#252525] hover:bg-[#333] text-white font-medium cursor-pointer"
          >
            Đóng
          </button>
        </div>
      </div>
    </div>
  );
};

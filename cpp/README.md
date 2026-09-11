# HexMapping / MikMap C++ UI Layer (Dear ImGui)

Thư mục này chứa toàn bộ mã nguồn giao diện **C++20 (Dear ImGui)** được chuyển thể hoàn chỉnh từ bản thiết kế UI MikMap, tuân thủ 100% kiến trúc được quy định trong `architecture.md` và `features.md`:

## Cấu trúc tập tin:
- `ui/Theme.h` & `ui/Theme.cpp`:
  - Thiết lập bảng màu chuẩn MikMap: Màu cam chủ đạo (`#FF7F50`), màu nâu trầm/espresso (`#3B1D0E`), viền (`#4A2411`), màu mặt nạ vàng (`#FFD166`), màu slice cyan (`#118AB2`).
  - Widget kéo Opacity tuỳ chỉnh `Theme::OpacityBar()`.
  - Các hàm tiện ích button bo góc và màu sắc đồng bộ.
- `ui/panels/OutputPanel.h` & `ui/panels/OutputPanel.cpp`:
  - **Tree View bên trái**: Danh sách Screens -> Slices -> Masks.
  - **3 nút tác vụ cố định góc dưới**: `Add Screen`, `+ Add Slice`, `Add Mask`.
  - **Stage Toolbar**: Switch Input/Output, **thanh chuyển đổi Screen cuộn ngang (horizontal scroll)** với con lăn chuột và tự động cuộn chống tràn tuyệt đối khi có nhiều screen, nút Reset Warp.
  - **Stage Canvas tương tác**: Vẽ lưới toạ độ, hình học quad keystone và 4 chốt corner pin (`TL`, `TR`, `BR`, `BL`) kéo thả trực tiếp bằng chuột.
  - **Inspector thuộc tính bên phải**: Cài đặt Slice (Input Rect, Corner Pins H_w) và Screen Output Info (Tên màn hình, thiết bị đầu ra, độ phân giải, Edge Blending).
- `ui/panels/LayerPanel.h` & `ui/panels/LayerPanel.cpp`:
  - Ma trận Clip 4 cột × N Layer.
  - Clip đang kích hoạt màu cam (`#FF7F50`), clip không kích hoạt màu nâu trầm/espresso (`#3B1D0E`).
  - Thanh kéo Opacity từng layer và menu lựa chọn Blend Mode.
- `ui/ControlPanel.h` & `ui/ControlPanel.cpp`:
  - Thanh Header điều hướng (Composition, Advanced Output, Sensors, Performance), Master Blackout (B), Freeze (F), Master Opacity, và hiển thị FPS.

## Cách tích hợp vào dự án openFrameworks / Visual Studio:
1. Sao chép thư mục `cpp/ui/` vào thư mục `src/ui/` trong project openFrameworks (`HexMapping`).
2. Trong `ofApp.h`:
   ```cpp
   #include "ui/ControlPanel.h"
   // ...
   HexMap::UI::ControlPanel m_controlPanel;
   ```
3. Trong `ofApp.cpp`:
   ```cpp
   void ofApp::setup() {
       // Khởi tạo ImGui với theme MikMap
       gui.setup();
       HexMap::UI::Theme::SetupMikMapTheme();
   }

   void ofApp::draw() {
       gui.begin();
       m_controlPanel.Render();
       gui.end();
   }
   ```

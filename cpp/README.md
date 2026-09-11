# HexMapping / MikMap — 100% Thuần C++20 Project

Dự án này hiện đã có **toàn bộ mã nguồn C++20 hoàn chỉnh**, không còn phụ thuộc vào Vite hay React cho ứng dụng thực tế.

## 📁 Cấu trúc cây thư mục C++ (/cpp):

```
cpp/
├─ CMakeLists.txt                 # Script build C++20 độc lập (MSVC / GCC / Clang)
├─ app/
│  └─ main.cpp                    # Điểm khởi chạy Native Desktop
├─ core/                          # Tầng Core: C++20 thuần, 0 phụ thuộc thư viện ngoài
│  ├─ math/
│  │  ├─ Vec2.h                   # Vector 2D constexpr
│  │  ├─ Mat3.h / .cpp            # Ma trận đồng nhất 3x3, nghịch đảo, determinant
│  │  └─ Homography.h / .cpp      # Thuật toán DLT giải Keystone & biến dạng 4 điểm
│  └─ model/
│     ├─ IWarp.h                  # Hợp đồng forward() và inverse() cho Warp
│     ├─ WarpCornerPin.h / .cpp   # Biến dạng góc 4 điểm (Keystone)
│     ├─ Slice.h                  # Đối tượng Slice với toạ độ Source UV và Warp
│     ├─ Screen.h                 # Màn hình đầu ra (Projector) với danh sách Slices
│     └─ Composition.h            # Composition tổng (Layers, Clips, Screens)
└─ ui/                            # Tầng Giao diện Dear ImGui
   ├─ Theme.h / .cpp              # Bảng màu MikMap (Vibrant Orange #FF7F50 + Burnt Brown #3B1D0E), widget OpacityBar
   ├─ ControlPanel.h / .cpp       # Điều hướng chính, Master Controls (B/F/Opacity), FPS counter
   └─ panels/
      ├─ OutputPanel.h / .cpp     # Advanced Output: Tree view, 3 nút cố định góc dưới, tab Screen cuộn ngang chống tràn, kéo corner pin trực tiếp
      └─ LayerPanel.h / .cpp      # Clip matrix 4 cột x N layer, blend modes, thanh kéo opacity
```

---

## 🚀 Cách chạy trên máy cá nhân (Windows / macOS / Linux):

### Cách 1: Sử dụng CMake (Cực kỳ nhanh và độc lập)
```bash
cd cpp
cmake -B build
cmake --build build --config Release
```

### Cách 2: Tích hợp vào dự án openFrameworks / Visual Studio 2022
1. Mở thư mục `D:\2026\Mike\openFrameworks\apps\myApps\HexMapping\`
2. Sao chép thư mục `cpp/core/` và `cpp/ui/` vào thư mục `src/` của dự án openFrameworks:
   - `cpp/core/` $\to$ `src/core/`
   - `cpp/ui/` $\to$ `src/ui/`
3. Trong `src/ofApp.h`:
   ```cpp
   #pragma once
   #include "ofMain.h"
   #include "ofxImGui.h"
   #include "ui/ControlPanel.h"

   class ofApp : public ofBaseApp {
   public:
       void setup();
       void update();
       void draw();

       ofxImGui::Gui gui;
       HexMap::UI::ControlPanel controlPanel;
   };
   ```
4. Trong `src/ofApp.cpp`:
   ```cpp
   #include "ofApp.h"
   #include "ui/Theme.h"

   void ofApp::setup() {
       ofSetFrameRate(60);
       gui.setup();
       HexMap::UI::Theme::SetupMikMapTheme();
   }

   void ofApp::update() {
       // Core update
   }

   void ofApp::draw() {
       gui.begin();
       controlPanel.Render();
       gui.end();
   }
   ```
5. Mở file `HexMapping.sln` trong Visual Studio 2022 và bấm **F5 (Start Debugging)**.

---

## 💡 Lưu ý về môi trường Google AI Studio Cloud:
Trong môi trường đám mây này (Google Cloud Run), hệ thống luôn cần một tiến trình web nhẹ chạy ở cổng 3000 để duy trì kết nối Live Preview cho trình duyệt của bạn (và cho phép bạn bấm nút **Export ZIP** hoặc kết nối GitHub bất cứ lúc nào). Toàn bộ mã nguồn desktop thực tế bạn cần đã nằm hoàn toàn trong thư mục `/cpp/`!

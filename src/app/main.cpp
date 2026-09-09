// ════════════════════════════════════════════════════════════════════════
//  app/main.cpp — điểm vào (I1 F2)
//
//  Hai cửa sổ:
//    · Control — trên màn hình làm việc, chứa toàn bộ ImGui
//    · Output  — trên máy chiếu, borderless, KHÔNG có gì ngoài nội dung
//
//  Chúng chia sẻ GL context nên FBO canvas dựng một lần dùng được cho cả
//  hai. Cửa sổ output cố ý KHÔNG có viền và không có UI: bất cứ thứ gì
//  vẽ lên đó đều bị máy chiếu chiếu ra cho khán giả thấy.
// ════════════════════════════════════════════════════════════════════════
#include "app/AppController.h"

#include "ofAppGLFWWindow.h"
#include "ofMain.h"

int main(int argc, char** argv) {
    // Dùng ofGLFWWindowSettings chứ không phải ofGLWindowSettings:
    // shareContextWith và decorated chỉ có ở lớp dẫn xuất GLFW.
    // Thiếu shareContextWith thì hai cửa sổ có GL context riêng, và FBO
    // canvas dựng ở cửa sổ này sẽ KHÔNG vẽ được ở cửa sổ kia.

    // ── Cửa sổ control ─────────────────────────────────────────────────
    ofGLFWWindowSettings controlSettings;
    controlSettings.setGLVersion(3, 2);
    controlSettings.setSize(1320, 880);
    controlSettings.windowMode = OF_WINDOW;
    controlSettings.title = "HexMapping - Control";
    auto controlWindow = ofCreateWindow(controlSettings);

    // ── Cửa sổ output ──────────────────────────────────────────────────
    ofGLFWWindowSettings outputSettings;
    outputSettings.setGLVersion(3, 2);
    outputSettings.setSize(1280, 720);
    outputSettings.windowMode = OF_WINDOW;
    outputSettings.title = "HexMapping - OUTPUT";
    outputSettings.shareContextWith = controlWindow;   // dùng chung FBO/texture

    // F2 — borderless: máy chiếu không được thấy thanh tiêu đề.
    outputSettings.decorated = false;

    // Đặt BÊN PHẢI cửa sổ control để không bị che khuất. Khi cắm máy
    // chiếu, kéo sang màn hình 2 rồi bấm F11 để vào fullscreen.
    outputSettings.setPosition(glm::vec2(1340, 60));
    auto outputWindow = ofCreateWindow(outputSettings);

    auto app = std::make_shared<hexmap::AppController>();
    app->setOutputWindow(outputWindow);

    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--demo") app->setDemoMode(true);
    }

    // Cửa sổ output không có ofApp riêng — ta gắn thẳng vào sự kiện của nó.
    // Cách này giữ toàn bộ trạng thái trong MỘT controller, thay vì tách
    // ra hai app rồi phải đồng bộ với nhau.
    ofAddListener(outputWindow->events().draw,
                  app.get(), &hexmap::AppController::drawOutput);
    ofAddListener(outputWindow->events().mousePressed,
                  app.get(), &hexmap::AppController::outputMousePressed);
    ofAddListener(outputWindow->events().mouseDragged,
                  app.get(), &hexmap::AppController::outputMouseDragged);
    ofAddListener(outputWindow->events().mouseReleased,
                  app.get(), &hexmap::AppController::outputMouseReleased);
    ofAddListener(outputWindow->events().mouseMoved,
                  app.get(), &hexmap::AppController::outputMouseMoved);

    // ★ Phim tat phai chay tren CA HAI cua so. Chi dang ky cho cua so
    //   control thi khi nguoi van hanh dang thao tac o cua so output,
    //   moi phim tat se im lang khong lam gi.
    ofAddListener(outputWindow->events().keyPressed,
                  app.get(), &hexmap::AppController::outputKeyPressed);

    ofRunApp(controlWindow, app);
    ofRunMainLoop();
    return 0;
}

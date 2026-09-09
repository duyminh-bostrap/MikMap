// ════════════════════════════════════════════════════════════════════════
//  tools/spike_hap — SPIKE TEST cho rủi ro R1
//
//  CÂU HỎI CẦN TRẢ LỜI:
//    Phát được bao nhiêu luồng video 4K HAP đồng thời mà vẫn giữ 60fps
//    trên máy này (RTX A4000)?
//
//  Đây là mã DÙNG MỘT LẦN. Mục đích duy nhất là đo, không phải để tái sử
//  dụng. Nếu kết quả tốt → kiến trúc video trong architecture.md đứng
//  vững. Nếu xấu → phải đổi phương án NGAY, trước khi xây tiếp.
// ════════════════════════════════════════════════════════════════════════
#include "ofMain.h"
#include "ofApp.h"

int main(int argc, char** argv) {
    // --bench : quét 1..6 luồng, ghi báo cáo ra file, thoát.
    //           Dùng để lấy số liệu ra ngoài vì đây là app GUI.
    // (không cờ) : chế độ tương tác, bấm phím 1-8 để đổi số luồng.
    bool bench = false;
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--bench") bench = true;
    }

    ofGLWindowSettings settings;
    settings.setGLVersion(3, 2);
    settings.setSize(1600, 900);
    settings.windowMode = OF_WINDOW;
    settings.title = "HexMapping — Spike Test R1: 4K HAP playback";

    auto window = ofCreateWindow(settings);
    auto app = std::make_shared<ofApp>();
    if (bench) app->enableBenchmark();

    ofRunApp(window, app);
    ofRunMainLoop();
    return 0;
}

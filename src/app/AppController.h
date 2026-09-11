// ════════════════════════════════════════════════════════════════════════
//  app/AppController.h — ★ nơi DUY NHẤT bốn tầng gặp nhau
//
//      core/   mô hình dữ liệu + toán      (không biết GL)
//      io/     thread sensor               (không biết model)
//      render/ GPU                          (chỉ đọc model)
//      ui/     ImGui                        (đọc model, phát lệnh)
//
//  Mọi phụ thuộc chéo đi qua đây. Nhờ vậy bốn tầng kia test được độc lập
//  và không tầng nào biết đến tầng khác.
//
//  ── Hai cửa sổ ───────────────────────────────────────────────────────
//  Control (màn hình 1) và Output (máy chiếu). Chúng chia sẻ GL context
//  nên FBO canvas dựng một lần dùng được cho cả hai.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/calib/SensorMapper.h"
#include "core/calib/SensorRouting.h"
#include "core/filter/OneEuroFilter.h"
#include "core/filter/PointTracker.h"
#include "core/model/ProjectIO.h"
#include "core/util/AppSettings.h"
#include "io/ISensorSource.h"
#include "render/MediaCache.h"
#include "render/RenderEngine.h"
#include "ui/ControlPanel.h"

#include "ofMain.h"

#include <deque>
#include <map>
#include <memory>
#include <vector>

namespace hexmap {

class AppController : public ofBaseApp {
public:
    void setOutputWindow(std::shared_ptr<ofAppBaseWindow> w) { m_outputWindow = std::move(w); }

    /// --demo: bat mock sensor va auto-calibrate ngay khi khoi dong.
    void setDemoMode(bool v) { m_demoMode = v; }

    // ── Vòng đời cửa sổ control ────────────────────────────────────────
    void setup() override;
    void update() override;
    void draw() override;
    void exit() override;
    void keyPressed(int key) override;

    // ── Cửa sổ output (gắn qua ofAddListener) ──────────────────────────
    void drawOutput(ofEventArgs& args);
    void outputMousePressed(ofMouseEventArgs& args);
    void outputMouseDragged(ofMouseEventArgs& args);
    void outputMouseReleased(ofMouseEventArgs& args);
    void outputMouseMoved(ofMouseEventArgs& args);

    /// Phim bam tren cua so OUTPUT.
    ///
    /// Khong co ham nay thi phim tat chi chay khi cua so control co focus.
    /// Giua show, nguoi van hanh keo handle tren cua so output xong thi
    /// moi phim tat im lang khong lam gi — va ho se tuong phan mem treo.
    void outputKeyPressed(ofKeyEventArgs& args);

private:
    void buildDefaultProject();
    void pollSensor();
    void applyUiActions(UiActions& a);
    void updateStats();

    /// Ghi so lieu hieu nang ra file moi vai giay.
    ///
    /// Doc FPS bang mat tren man hinh khong dang tin: so nhay lien tuc va
    /// chinh viec chup man hinh cung lam tut FPS. Co ban ghi thi doi chieu
    /// duoc giua cac lan chay, va khi chay show that ma bi giat thi co du
    /// lieu de lan nguoc nguyen nhan.
    void writePerfLog();

    void startSensor(int typeIndex);
    void stopSensor();

    /// Suy ra H_s tu pham vi da biet cua MockSource — thu ca chuoi
    /// sensor -> mapping ma khong can phan cung.
    void autoCalibrateMock();

    /// Danh sach man hinh vat ly dang cam.
    struct DisplayInfo {
        int  index = 0;
        int  x = 0, y = 0, w = 0, h = 0;
        bool isPrimary = false;
        std::string name;
    };
    std::vector<DisplayInfo> enumerateDisplays() const;

    /// Day cua so output sang man hinh N roi vao fullscreen.
    /// Day la cach dua hinh ra may chieu ma khong phai keo tay.
    void sendOutputToDisplay(int displayIndex);

    /// Cua so output doi kich thuoc (vao/thoat fullscreen, keo vien, doi
    /// man hinh) -> co gian mapping theo TI LE de giu nguyen hinh dang.
    ///
    /// Khong lam viec nay thi vao fullscreen 1920x1080 tu cua so 854x480
    /// se de lai 3/4 may chieu MAU DEN — va khong co thong bao loi nao.
    void syncScreenToOutputSize();

    /// Ap cai dat xuong cac he thong that (vsync, cache budget, ngon ngu).
    /// Goi khi khoi dong va moi lan nguoi dung doi cai dat.
    void applySettings();

    /// Nạp một file media vào ô clip. Dùng chung cho hộp thoại chọn file
    /// (I6) và cho trình duyệt thư viện — hai lối vào, MỘT chỗ quyết định
    /// loại media và cảnh báo HAP, để hai lối không lệch nhau.
    void assignClip(int layer, int column, const std::string& path);

    /// G17 — thuc thi mot lan kich hoat vung cam ung.
    void executeTrigger(const TriggerHit& hit);

    /// Tìm handle góc gần con trỏ nhất trong bán kính cho trước.
    int  pickHandle(const Vec2& mouse, double radiusPx) const;
    Slice* activeSlice();
    Screen* activeScreen();

    // ── Trạng thái ─────────────────────────────────────────────────────
    Project      m_project;
    RenderEngine m_render;
    MediaCache   m_cache;
    ControlPanel m_panel;
    EditState    m_edit;
    PerfStats    m_stats;
    UiActions    m_actions;

    SensorMapper m_mapper;
    std::unique_ptr<ISensorSource> m_sensor;
    int m_sensorTypeIndex = 0;
    bool m_demoMode = false;

    /// Project duoc NAP tu file hay moi dung mac dinh. Quyet dinh co
    /// duoc phep tu dieu chinh do phan giai screen theo cua so hay khong.
    bool m_projectWasLoaded = false;

    /// Kich thuoc cua so output lan truoc, de phat hien thay doi.
    Vec2 m_lastOutputSize{0.0, 0.0};

    /// Điểm sensor đã ánh xạ sang canvas — để vẽ overlay (G13).
    std::vector<Vec2> m_mappedPoints;
    /// Điểm sensor THÔ mới nhất — wizard calibration cần giá trị này.
    std::vector<Vec2> m_rawPoints;

    /// G17 — thong ke de hien tren UI.
    int m_triggerCount = 0;
    std::string m_lastTriggerName;

    // ── G11 G12: lam sach du lieu sensor ───────────────────────────────
    /// Gan ID ben vung truoc, roi moi loc. Thu tu nay BAT BUOC: loc ma
    /// khong co ID on dinh se tron quy dao cua hai ngon tay vao nhau.
    /// G18 — nguon nao di qua ho so nao, toi screen nao.
    /// Dung CHI SO chu khong phai con tro: vector cua Project cap phat lai
    /// khi them screen, va moi con tro dang giu se thanh treo (xem F22).
    SensorRoutingTable m_routes;

    /// Dung lai bang dinh tuyen. Goi sau MOI thay doi ve calibration hoac
    /// danh sach screen.
    void rebuildSensorRoutes();

    PointTracker m_tracker;

    /// Mot bo loc RIENG cho tung ID. Dung chung mot bo loc cho moi diem
    /// se lam chung keo nhau ve phia trung binh.
    std::map<uint32_t, OneEuroFilter2D> m_filters;

    bool m_filterEnabled = true;
    int  m_trackCount = 0;

    std::shared_ptr<ofAppBaseWindow> m_outputWindow;

    // ── Đo hiệu năng (G9) ──────────────────────────────────────────────
    std::deque<double> m_frameTimes;
    std::deque<double> m_sensorLatencies;
    static constexpr size_t kHistory = 600;
    uint64_t m_lastFrameNs = 0;

    std::string m_projectPath = "bin/data/projects/default.hexmap";

    /// Man hinh vat ly dang nhan output. -1 = chua dua ra dau.
    /// Chi de HIEN TRANG THAI tren thanh tren cung; nguon su that ve vi
    /// tri cua so van la ban than cua so.
    int m_outputDisplayIndex = -1;

    AppSettings m_settings;
    std::string m_settingsPath = "settings.json";

    float m_perfLogTimer = 0.0f;
    int   m_perfLogCount = 0;
};

} // namespace hexmap

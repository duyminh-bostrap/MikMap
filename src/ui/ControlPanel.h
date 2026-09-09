// ════════════════════════════════════════════════════════════════════════
//  ui/ControlPanel.h — bảng điều khiển Dear ImGui (I4)
//
//  ── Quy tắc phụ thuộc (architecture.md §1) ───────────────────────────
//  ui/ chỉ ĐỌC model và PHÁT LỆNH. Nó không bao giờ gọi render/ và
//  không tự thực hiện hành động có hệ quả (lưu file, mở cổng mạng).
//
//  Những việc đó được gom vào struct UiActions; AppController — nơi duy
//  nhất bốn tầng gặp nhau — mới là chỗ thực thi. Nhờ vậy panel test được
//  và không kéo theo phụ thuộc vòng.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/calib/CalibrationProfile.h"
#include "core/model/ProjectIO.h"
#include "render/RenderEngine.h"

#include "ofMain.h"
#include "ofxImGui.h"
// ofxImGui.h co y KHONG include ImHelpers.h (xem ghi chu trong file do).
// Ta can no cho ofxImGui::AddImage().
#include "ImHelpers.h"

#include <string>
#include <vector>

namespace hexmap {

/// Số liệu hiệu năng — PerfPanel (G9).
/// architecture.md §10.6: độ trễ phải được ĐO, không được ĐOÁN.
struct PerfStats {
    double fps = 0.0;
    double frameAvgMs = 0.0;
    double frameP99Ms = 0.0;

    bool     sensorConnected = false;
    double   sensorLatencyAvgMs = 0.0;
    double   sensorLatencyP99Ms = 0.0;
    uint64_t framesDropped = 0;
    uint64_t ringOverflow = 0;
    uint64_t packetsMalformed = 0;
    int      touchCount = 0;
    int      trackedCount = 0;   ///< G12 — so diem co ID ben vung
    bool     filterEnabled = true;

    int    layersDrawn = 0;
    int    slicesDrawn = 0;
    int    mediaLoaded = 0;
    int    mediaEvictions = 0;
    size_t vramBytes = 0;
};

/// Lệnh do người dùng phát ra, AppController thực thi.
struct UiActions {
    bool saveProject = false;
    bool loadProject = false;
    bool newProject  = false;

    bool startSensor = false;
    bool stopSensor  = false;
    int  sensorTypeIndex = -1;      ///< -1 = không đổi; 0 = Mock, 1 = OSC

    bool calibAddPoint  = false;    ///< chạm điểm hiện tại của wizard
    bool calibSolve     = false;
    bool calibClear     = false;
    bool calibNextTarget = false;

    /// Calibrate TU DONG cho MockSource. Pham vi toa do cua mock la da
    /// biet, nen suy ra duoc H_s ma khong can cham tay.
    /// Muc dich: cho phep thu TOAN BO chuoi sensor -> mapping ma khong
    /// can phan cung (architecture.md §10.7).
    bool calibAutoMock = false;

    /// Hiện dấu thập lên máy chiếu (G6). Giá trị trạng thái, không phải
    /// lệnh một lần — panel phản chiếu checkbox xuống đây mỗi frame.
    bool calibShowTarget = false;

    /// I6 — mở hộp thoại chọn file để gán media vào một ô clip.
    bool browseForClip = false;
    int  browseLayer = -1;
    int  browseColumn = -1;

    /// G17 — them / xoa vung cam ung.
    bool addTriggerZone = false;
    int  removeTriggerZone = -1;

    bool toggleFullscreen = false;
    bool resetActiveSliceWarp = false;
    int  convertWarpTo = -1;        ///< -1 = không đổi; 0 = CornerPin, 1 = Mesh
    int  addSliceToScreen = -1;
    int  removeSliceIndex = -1;
};

class ControlPanel {
public:
    void setup();
    void shutdown();

    /// Vẽ toàn bộ UI. Có thể SỬA TRỰC TIẾP model (opacity, tên, cờ) —
    /// đó là thao tác không hệ quả. Việc có hệ quả thì điền vào actions.
    void draw(Project& project,
              EditState& edit,
              const PerfStats& stats,
              const ofTexture* canvasPreview,
              UiActions& actions);

    /// G17 — so lan kich hoat + ten vung gan nhat, de hien tren UI.
    void setTriggerInfo(int count, const std::string& lastName) {
        m_triggerCount = count;
        m_lastTriggerName = lastName;
    }

    /// Chỉ số điểm calibration wizard đang yêu cầu người dùng chạm.
    int  calibTargetIndex() const { return m_calibTarget; }
    void setCalibTargetIndex(int i) { m_calibTarget = i; }

    int  activeScreenIndex() const { return m_activeScreen; }

    /// G6 — co hien dau thap calibration len may chieu khong.
    /// La NGUON SU THAT DUY NHAT: ca checkbox lan phim tat 'c' deu doc
    /// va ghi vao day. Neu de moi noi giu mot ban sao, phim tat se bi
    /// checkbox ghi de moi frame.
    bool calibShowTarget() const { return m_calibShowTarget; }
    void setCalibShowTarget(bool v) { m_calibShowTarget = v; }
    void setStatusMessage(const std::string& msg, bool isError = false);

private:
    void drawMenuBar(UiActions& a);
    void drawClipGrid(Project& p);
    void drawLayerPanel(Project& p);
    void drawScreenPanel(Project& p, EditState& edit, UiActions& a);
    void drawSensorPanel(Project& p, const PerfStats& s, UiActions& a);
    void drawCalibrationPanel(Project& p, const PerfStats& s, UiActions& a);
    void drawPerfPanel(const PerfStats& s);
    void drawPreview(const ofTexture* tex);

    /// C3-C8 + D1-D6 — thuoc tinh cua clip DANG CHON.
    void drawClipPanel(Project& p);

    /// G17 — danh sach vung cam ung.
    void drawTriggerZonePanel(Project& p, UiActions& a);

    ofxImGui::Gui m_gui;
    bool m_ready = false;

    /// O clip dang chon de xem/sua thuoc tinh. Khac voi o dang PHAT:
    /// nguoi van hanh can chinh clip sap dung ma khong lam gian doan
    /// clip dang chieu.
    int m_selLayer = -1;
    int m_selColumn = -1;

    int m_activeScreen = 0;
    int m_calibTarget = 0;
    int m_selectedCalib = 0;
    bool m_calibShowTarget = true;

    std::string m_status;
    bool m_statusIsError = false;
    float m_statusTimer = 0.0f;

    /// Yeu cau mo file browser, ghi nhan trong luc ve luoi clip roi
    /// chuyen ra UiActions o cuoi draw().
    int m_triggerCount = 0;
    std::string m_lastTriggerName;
    int m_selZone = -1;

    bool m_pendingBrowse = false;
    int  m_browseLayer = -1;
    int  m_browseColumn = -1;
};

} // namespace hexmap

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
#include "core/util/AppSettings.h"
#include "ui/Localization.h"
#include "ui/Theme.h"
#include "render/RenderEngine.h"

#include "ofMain.h"
#include "ofxImGui.h"
// ofxImGui.h co y KHONG include ImHelpers.h (xem ghi chu trong file do).
// Ta can no cho ofxImGui::AddImage().
#include "ImHelpers.h"

#include <functional>
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

    /// Gán thẳng một file từ trình duyệt media, không qua hộp thoại.
    /// Rỗng = không có yêu cầu nào.
    std::string assignMediaPath;
    int assignLayer = -1;
    int assignColumn = -1;

    /// G17 — them / xoa vung cam ung.
    bool addTriggerZone = false;
    int  removeTriggerZone = -1;

    bool toggleFullscreen = false;

    /// Day cua so output sang man hinh nay roi fullscreen. -1 = khong doi.
    int  sendOutputToDisplay = -1;

    /// Thoat fullscreen, dua cua so output ve che do cua so (de keo duoc).
    bool outputWindowed = false;

    /// Cai dat: luu ra file / dat lai mac dinh.
    bool saveSettings = false;
    bool resetSettings = false;
    /// Cai dat vua doi -> AppController ap dung (vsync, cache budget...).
    bool settingsChanged = false;
    /// F12 — tạo mặt nạ cho slice đang chọn (nếu chưa có).
    bool addMask = false;

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

    /// Danh sach man hinh vat ly, de menu Output liet ke.
    struct DisplayEntry {
        int index = 0;
        int w = 0, h = 0;
        bool isPrimary = false;
        std::string name;
    };
    void setDisplays(std::vector<DisplayEntry> d) { m_displays = std::move(d); }

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

    /// Cai dat ung dung. ControlPanel doc va SUA truc tiep (thao tac
    /// khong he qua); viec LUU ra file do AppController lam.
    void setSettings(AppSettings* s) { m_settings = s; }

    /// Màn hình vật lý đang nhận output, để thanh trên cùng hiện trạng
    /// thái. -1 = chưa đưa ra máy chiếu nào.
    void setActiveOutputDisplay(int index) { m_activeOutputDisplay = index; }

    /// Điểm chạm đã ánh xạ sang không gian OUTPUT, cho khung nhìn sensor.
    void setSensorPoints(std::vector<Vec2> pts) { m_sensorPoints = std::move(pts); }

    /// Ô clip đang chọn để xem thuộc tính — AppController đọc cặp này để
    /// biết cần "peek" (không ép nạp) media nào cho ô XEM TRƯỚC.
    /// -1 nghĩa là chưa chọn ô nào.
    int selectedLayer() const { return m_selLayer; }
    int selectedColumn() const { return m_selColumn; }

    /// Texture xem trước của clip đang chọn — AppController tự lấy bằng
    /// MediaCache::peek() (KHÔNG ép nạp) rồi đưa vào đây mỗi frame.
    ///
    /// ★ ControlPanel không được tự lấy texture: render/ (nơi giữ
    ///   MediaCache) nằm NGOÀI vùng phụ thuộc cho phép của ui/ — xem
    ///   architecture.md §1. Texture phải được AppController, nơi DUY
    ///   NHẤT bốn tầng gặp nhau, chuẩn bị sẵn rồi đưa vào.
    void setClipPreview(const ofTexture* tex) { m_clipPreviewTex = tex; }

public:
    /// Ba trang chính. Thay cho các cửa sổ nổi trước đây.
    ///
    /// ── Vì sao chuyển sang bố cục cố định ────────────────────────────
    /// Cửa sổ nổi tự do nghe thì linh hoạt, nhưng trong phòng tối, giữa
    /// buổi diễn, người vận hành không có thời gian sắp lại bàn làm việc.
    /// Bố cục cố định nghĩa là mọi thứ LUÔN ở đúng chỗ cũ — và không bao
    /// giờ có chuyện một bảng trôi ra ngoài màn hình hoặc bị che mất.
    enum class View { Composition = 0, Mapping, Sensor };

    View view() const { return m_view; }
    void setView(View v) { m_view = v; }

private:
    void drawTopBar(const PerfStats& s, UiActions& a);

    void drawCompositionView(Project& p, EditState& edit, const PerfStats& s,
                             const ofTexture* canvasTex, UiActions& a);
    void drawMappingView(Project& p, EditState& edit,
                         const ofTexture* canvasTex, UiActions& a);
    void drawSensorView(Project& p, const PerfStats& s, UiActions& a);

    /// Trình duyệt media bên trái trang Composition.
    void drawBrowserPanel(Project& p, UiActions& a);

    /// Hai màn hình xem: nội dung canvas và vùng thực sự ra máy chiếu.
    void drawMonitors(Project& p, const PerfStats& s, const ofTexture* canvasTex);

    /// Bảng thuộc tính bên phải trang Composition.
    void drawInspector(Project& p);

    /// Tab LAYER trong bảng thuộc tính: opacity + blend + transition +
    /// solo/bypass của layer đang chọn (m_activeLayerRow), tách khỏi
    /// tab CLIP để hai khái niệm không lẫn vào nhau trên cùng một bảng.
    void drawLayerProperties(Project& p);

    /// Lưới layer × cột, layer control dính bên trái.
    void drawLayersDeck(Project& p);

    /// Thanh Opacity DUNG CHUNG cho ca luoi layer, nam o hang tieu de
    /// (dinh, luon thay khi cuon). Mot dong: nhan trai + rang co GRADIENT
    /// (lam -> cam) + phan tram phai + vach mau bam ben trai — khac voi
    /// theme::opacityBar (nhan tren mot dong RIENG) vi day la ban thiet
    /// ke rieng cho vi tri nay, chi dung MOT lan.
    /// @return true neu value01 vua doi
    bool drawSharedOpacityBar(const char* label, double* value01, ImU32 fillColor);

    /// Thanh công cụ trang Mapping.
    void drawMapToolbar(Project& p, EditState& edit, UiActions& a);

    /// Bảng thuộc tính slice bên phải trang Mapping.
    void drawSliceSettings(Project& p, EditState& edit, UiActions& a);

    /// ★ Chế độ INPUT: kéo VÙNG LẤY của slice trên canvas.
    ///
    /// Trước đây chỉ chỉnh được đầu ra (keystone); vùng lấy phải gõ số.
    /// Nhưng hai việc đó là hai nửa của cùng một thao tác — "lấy phần
    /// nào của hình" và "đặt nó ở đâu trên vật thể" — nên chúng dùng
    /// chung một khung nhìn và một công tắc chuyển, đúng như bản thiết kế.
    void drawInputEditor(Project& p, EditState& edit, const ofTexture* canvasTex);

    /// Quét lại thư mục media. Gọi thưa, không phải mỗi frame.
    void rescanMedia();

    /// Chuyển sang / rời khỏi font mono cho số liệu.
    /// An toàn khi font mono không nạp được (nullptr) — khi đó không làm gì.
    void pushMono();
    void popMono();

    void drawScreenPanel(Project& p, EditState& edit, UiActions& a);

    /// F12 — bảng mặt nạ bezier của slice đang chọn.
    void drawMaskPanel(Slice& s, EditState& edit);

    /// Cây SCREEN SETUP: Screen → Slice → Mask.
    void drawScreenTree(Project& p, EditState& edit, UiActions& a);

    /// Thuộc tính của một Screen (khác với thuộc tính Slice).
    void drawScreenProperties(Screen& sc, EditState& edit, UiActions& a);

    /// F12 — phần tương tác chuột với nút mặt nạ, tách khỏi
    /// drawMappingEditor để hàm đó không phình ra khó đọc.
    /// @param toWidget  contentUV → toạ độ màn hình của widget
    /// @return true nếu đã "nuốt" cú bấm (không để nó chọn slice khác)
    bool handleMaskEditing(Slice& s, EditState& edit,
                           const std::function<ImVec2(const Vec2&)>& toWidget,
                           const std::function<Vec2(const ImVec2&)>& toOutput,
                           ImDrawList* dl, bool hovered, bool active);
    void drawSensorPanel(Project& p, const PerfStats& s, UiActions& a);
    void drawCalibrationPanel(Project& p, const PerfStats& s, UiActions& a);
    void drawPerfPanel(const PerfStats& s);
    void drawSettingsPanel(UiActions& a);

    /// Nap font co glyph tieng Viet.
    ///
    /// Font mac dinh cua ImGui chi co ASCII — day la ly do toan bo UI
    /// truoc gio phai viet KHONG DAU. Nap Inter kem dai glyph tieng Viet
    /// thi hien duoc day du dau, va cung san sang cho ngon ngu khac.
    void loadFont();

    /// C3-C8 + D1-D6 — thuoc tinh cua clip DANG CHON.
    void drawClipPanel(Project& p);

    /// G17 — danh sach vung cam ung.
    void drawTriggerZonePanel(Project& p, UiActions& a);

    /// ★ Trinh chinh mapping NGAY TRONG cua so chinh.
    ///
    /// Truoc day muon keo goc slice phai thao tac tren cua so output —
    /// nghia la phai nhin sang may chieu, hoac de cua so output nam tren
    /// man hinh lam viec roi lai phai day sang may chieu luc dien. Cac
    /// phan mem mapping deu cho chinh ngay trong app; day la ban tuong duong.
    void drawMappingEditor(Project& p, EditState& edit, const ofTexture* canvasTex);

    ofxImGui::Gui m_gui;
    bool m_ready = false;

    View m_view = View::Composition;

    /// Trang Mapping: đang chỉnh vùng LẤY hay vùng RA.
    enum class MapMode { Input = 0, Output };
    MapMode m_mapMode = MapMode::Output;
    bool m_showMapSidebar = true;
    bool m_showMapTree = true;

    /// ★ Item đang chọn trong cây — quyết định bảng bên phải hiện gì.
    ///
    /// Trước đây danh sách slice nằm lẫn trong bảng thuộc tính và mặt nạ
    /// là một mục gấp trong đó. Nghĩa là không có chỗ nào nhìn được TOÀN
    /// BỘ cấu trúc một lần — mà đó lại chính là thứ người vận hành cần
    /// khi dùng nhiều máy chiếu: cái gì đang chiếu ra đâu.
    enum class SelKind { Screen = 0, Slice, Mask };
    SelKind m_selKind = SelKind::Slice;

    /// Trình duyệt media — danh sách file trong bin/data/media.
    /// Quét đĩa là thao tác I/O nên KHÔNG làm mỗi frame; chỉ khi mở trang
    /// hoặc khi người dùng bấm quét lại.
    std::vector<std::string> m_mediaFiles;
    bool m_mediaScanned = false;

    /// O clip dang chon de xem/sua thuoc tinh. Khac voi o dang PHAT:
    /// nguoi van hanh can chinh clip sap dung ma khong lam gian doan
    /// clip dang chieu.
    int m_selLayer = -1;
    int m_selColumn = -1;

    /// Texture xem trước cho o clip dang chon — AppController peek() roi
    /// dua vao day moi frame qua setClipPreview(). nullptr = media chua
    /// nap (hoac chua chon o nao) — luc do hien cho trong roi thay vi
    /// ep nap chi de xem truoc.
    const ofTexture* m_clipPreviewTex = nullptr;

    /// O dang mo menu chuot phai (Chon file / Duplicate / Copy / Paste /
    /// Xoa). Ghi lai LUC MO vi popup ve o mot cho khac trong cay ImGui,
    /// sau khi vong lap qua tung o da ket thuc — khong con L/c cua vong
    /// lap de dung nua.
    int m_ctxMenuLayer = -1;
    int m_ctxMenuColumn = -1;

    /// "Copy" luu MOT ban sao clip o day; "Paste" doc lai. La du lieu
    /// THUAN (core::Clip khong so huu texture/decoder — xem Clip.h), nen
    /// ui/ giu ban sao trong bo nho la an toan, khong pha quy tac phu
    /// thuoc kien truc.
    Clip m_clipClipboard;
    bool m_hasClipClipboard = false;

    /// ★ Layer đang chọn để xem NHANH — RIÊNG với layer của clip đang
    ///   chọn (m_selLayer ở trên).
    ///
    ///   Bấm vào TÊN một layer (không phải một ô clip) chỉ để xem/chỉnh
    ///   opacity của layer đó qua thanh dùng chung phía trên bảng — thao
    ///   tác này không nên vô tình đổi luôn clip đang xem trong bảng
    ///   THUỘC TÍNH, nên phải là một biến khác với m_selLayer/m_selColumn.
    int m_activeLayerRow = -1;

    /// Tab CLIP / LAYER trong bảng thuộc tính (trang Composition).
    /// Đổi sang Layer mỗi khi người dùng bấm chọn layer khác — người
    /// vận hành vừa chọn layer thì gần như chắc chắn muốn xem NGAY
    /// thuộc tính của layer đó, không phải clip cũ đang xem dở.
    enum class InspectorTab { Clip = 0, Layer };
    InspectorTab m_inspectorTab = InspectorTab::Layer;

    int m_activeScreen = 0;
    int m_calibTarget = 0;
    int m_selectedCalib = 0;
    bool m_calibShowTarget = true;

    std::string m_status;
    bool m_statusIsError = false;
    float m_statusTimer = 0.0f;

    /// Trang thai keo trong trinh chinh mapping — RIENG voi cua so output,
    /// de hai noi khong tranh nhau mot bien.
    int  m_mapDragPoint = -1;
    bool m_mapShowContent = true;

    /// Che do sua vung LAY: -1 khong keo, 0 goc tren-trai, 1 goc duoi-phai,
    /// 2 ca khoi.
    int  m_inputDragPart = -1;

    /// Cua so Cai dat — thu duy nhat con NOI, vi no khong thuoc luong lam
    /// viec luc dien.
    bool m_showSettings = false;

    int m_activeOutputDisplay = -1;
    std::vector<Vec2> m_sensorPoints;



    /// Phan chieu tu PerfStats, de menu bar hien duoc trang thai sensor.
    bool m_sensorRunning = false;
    std::vector<DisplayEntry> m_displays;
    AppSettings* m_settings = nullptr;
    bool m_fontLoaded = false;

    int m_triggerCount = 0;
    std::string m_lastTriggerName;
    int m_selZone = -1;

    bool m_pendingBrowse = false;
    int  m_browseLayer = -1;
    int  m_browseColumn = -1;
};

} // namespace hexmap

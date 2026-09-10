#include "app/AppController.h"

#include "core/model/WarpCornerPin.h"
#include "core/model/WarpMesh.h"
#include "core/util/Clock.h"
#include "ui/Localization.h"
#include "io/sources/MockSource.h"
#include "io/sources/OscSource.h"

#include <GLFW/glfw3.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <sstream>

namespace hexmap {
namespace {

double percentile(std::deque<double> v, double p) {
    if (v.empty()) return 0.0;
    std::sort(v.begin(), v.end());
    const size_t i = std::min(v.size() - 1,
                              static_cast<size_t>(static_cast<double>(v.size()) * p));
    return v[i];
}

double average(const std::deque<double>& v) {
    if (v.empty()) return 0.0;
    double s = 0.0;
    for (const double x : v) s += x;
    return s / static_cast<double>(v.size());
}

/// Điểm mục tiêu của wizard calibration, trong contentUV của slice.
const Vec2 kCalibTargets[4] = {
    {0.15, 0.15}, {0.85, 0.15}, {0.85, 0.85}, {0.15, 0.85},
};

} // namespace

// ═══════════════════════════════════════════════════════════════════════

void AppController::setup() {
    ofSetVerticalSync(true);
    ofBackground(24);
    ofSetFrameRate(0);

    // ★ PHAI goi TRUOC khi cap phat FBO.
    // oF mac dinh dung GL_TEXTURE_RECTANGLE (toa do tinh bang PIXEL).
    // Hai cho trong du an nay doi GL_TEXTURE_2D (toa do CHUAN HOA 0..1):
    //   · ImGui khong ve duoc texture rectangle — preview bao loi
    //   · RenderEngine::drawSliceGeometry chia toa do cho kich thuoc
    //     texture, tuc gia dinh toa do chuan hoa
    // Bo dong nay thi ca hai cung sai, va sai theo kieu kho lan ra.
    ofDisableArbTex();

    // Nap cai dat TRUOC panel.setup(): ngon ngu va font phai dung ngay
    // tu frame dau, khong the doi frame sau roi nap lai atlas font.
    {
        std::string warn;
        if (!m_settings.load(ofToDataPath(m_settingsPath, true), warn) && !warn.empty()) {
            ofLogWarning("settings") << warn;
        }
        i18n::setLanguage(m_settings.language);
    }

    buildDefaultProject();

    m_render.setup(m_project.composition.canvasSize);
    m_panel.setSettings(&m_settings);
    m_panel.setup();
    applySettings();

    m_mapper.setCalibration(m_project.calibrations.empty()
                            ? nullptr : &m_project.calibrations[0]);
    m_mapper.setScreen(m_project.screens.empty() ? nullptr : &m_project.screens[0]);

    // ★ Voi project MAC DINH, dong bo do phan giai screen theo cua so
    //   output that.
    //
    //   Neu khong: screen khai 1920x1080 nhung cua so chi 1280x720, va
    //   slice se trai RA NGOAI vung nhin thay. Trieu chung rat kho hieu —
    //   diem sensor bien mat, mot phan noi dung bi cat — ma khong co thong
    //   bao loi nao.
    //
    //   KHONG lam dieu nay voi project da NAP: do phan giai o do la thiet
    //   ke co chu dich cua nguoi dung cho may chieu that.
    if (!m_projectWasLoaded && m_outputWindow && !m_project.screens.empty()) {
        const Vec2 winSize{static_cast<double>(m_outputWindow->getWidth()),
                           static_cast<double>(m_outputWindow->getHeight())};
        if (winSize.x > 0.0 && winSize.y > 0.0) {
            Screen& sc = m_project.screens[0];
            sc.resolution = winSize;
            for (Slice& sl : sc.slices) {
                sl.warp()->resetToRect(Vec2{0.0, 0.0}, winSize);
            }
        }
    }

    if (m_outputWindow) {
        m_lastOutputSize = Vec2{static_cast<double>(m_outputWindow->getWidth()),
                                static_cast<double>(m_outputWindow->getHeight())};
    }

    m_edit.activeSliceIndex = 0;
    m_lastFrameNs = static_cast<uint64_t>(Clock::nowNs());

    // Tu dua output ra may chieu neu nguoi dung da chon san. Dung san
    // khau thi khong ai muon bam lai moi lan mo phan mem.
    if (m_settings.defaultOutputDisplay >= 0) {
        sendOutputToDisplay(m_settings.defaultOutputDisplay);
    }

    // --demo: bat sensor gia lap va calibrate san. Dung de xem ngay ca
    // chuoi sensor -> mapping ma khong phai bam gi, va de kiem chung
    // tu dong duoc.
    if (m_demoMode) {
        startSensor(0);
        autoCalibrateMock();

        // Dung san hai vung cam ung de --demo the hien duoc CA CHUOI,
        // ke ca G17.
        //
        // ★ Vi tri phai nam TREN DUONG DI cua diem mock, khong phai o
        //   giua canvas. MockPattern::Circle chay quy dao ban kinh 35%
        //   quanh tam, nen mot vung o giua se KHONG BAO GIO bi di vao —
        //   quy dao luon nam ngoai no.
        if (m_project.triggerZones.zones.empty()
            && m_project.composition.columnCount() > 2) {
            const Vec2 cs = m_project.composition.canvasSize;

            // Diem mock o canvas: (0.5 + 0.35·cos θ, 0.5 + 0.35·sin θ)
            // theo ti le. Dat vung ngay tren quy dao, o hai ben trai/phai.
            auto addZone = [&](const char* name, double cxRatio, int col) {
                TriggerZone z;
                z.name = name;
                z.size   = Vec2{cs.x * 0.14, cs.y * 0.26};
                z.origin = Vec2{cs.x * cxRatio - z.size.x * 0.5,
                                cs.y * 0.5     - z.size.y * 0.5};
                z.action = TriggerAction::TriggerColumn;
                z.targetColumn = col;
                z.cooldownSec = 0.8;
                m_project.triggerZones.zones.push_back(z);
            };

            addZone("Zone phai", 0.85, 1);   // quy dao di qua o theta ~ 0
            addZone("Zone trai", 0.15, 2);   // ... va o theta ~ 180 do
        }

        // F12 — dat san mot mat na elip. Cung ly do voi hai vung cam ung
        // o tren: --demo phai the hien duoc CA CHUOI ma khong phai bam gi,
        // va phai kiem chung tu dong duoc (architecture.md §10.7).
        //
        // Chi dat khi slice CHUA co mat na, de --demo tren mot project
        // that khong de len thiet ke cua nguoi dung.
        if (!m_project.screens.empty() && m_project.screens[0].sliceCount() > 0) {
            Slice& s0 = m_project.screens[0].slices[0];
            if (s0.mask.nodes.empty()) {
                s0.mask = BezierMask::ellipse(4);
                s0.mask.feather = 0.04;
            }
        }
    }
}

void AppController::buildDefaultProject() {
    // Nếu có project mặc định thì nạp; không thì dựng một cái tối thiểu
    // để ứng dụng không bao giờ khởi động vào màn hình trống rỗng.
    Project loaded;
    const LoadResult r = projectio::load(ofToDataPath(m_projectPath, true), loaded);
    if (r.ok) {
        m_project = std::move(loaded);
        m_projectWasLoaded = true;
        for (const std::string& w : r.warnings) ofLogWarning("project") << w;
        return;
    }
    m_projectWasLoaded = false;

    m_project = Project{};
    m_project.name = "Untitled";
    m_project.composition = Composition(3, 8, 2);
    m_project.composition.canvasSize = Vec2{1920.0, 1080.0};

    Screen sc(0, "Projector 1", Vec2{1920.0, 1080.0});
    sc.addFullScreenSlice(m_project.composition.canvasSize);
    m_project.screens.push_back(std::move(sc));

    CalibrationProfile cal;
    cal.name = "Sensor 1";
    cal.targetScreenId = 0;
    m_project.calibrations.push_back(std::move(cal));

    // Nạp sẵn file HAP mẫu nếu có — để mở app lên là thấy hình ngay,
    // thay vì phải đi tìm file trước khi biết phần mềm có chạy không.
    const char* samples[] = {
        "media/4k_detail_hap_q.mov",
        "media/4k_detail_hap.mov",
        "media/4k_noisy_hap_q.mov",
    };
    int col = 0;
    for (const char* s : samples) {
        if (!ofFile::doesFileExist(ofToDataPath(s, true))) continue;
        Clip c;
        c.name = ofFilePath::getBaseName(s);
        c.media.type = MediaType::Video;
        c.media.path = s;
        c.media.durationSec = 5.0;
        c.transport.durationSec = 5.0;
        m_project.composition.deck(0).setClip(0, col++, c);
    }

    // Phat luon clip dau tien — CHI voi project mac dinh moi tao.
    // Mo app len thay ngay hinh, thay vi man hinh den khien nguoi dung
    // khong biet phan mem co chay khong.
    //
    // Co y KHONG lam dieu nay khi NAP project: ProjectIO khong luu trang
    // thai dang phat, va tu phat khi mo file la hanh vi gay bat ngo.
    if (col > 0 && m_settings.autoPlayFirstClip) {
        m_project.composition.triggerClip(0, 0);
    }
}

// ═══════════════════════════════════════════════════════════════════════

void AppController::update() {
    const auto nowNs = static_cast<uint64_t>(Clock::nowNs());
    const double dtMs = static_cast<double>(nowNs - m_lastFrameNs) / 1.0e6;
    m_lastFrameNs = nowNs;

    if (dtMs > 0.0 && dtMs < 1000.0) {
        m_frameTimes.push_back(dtMs);
        if (m_frameTimes.size() > kHistory) m_frameTimes.pop_front();
    }

    const double dtSec = ofGetLastFrameTime();

    syncScreenToOutputSize();

    pollSensor();
    m_project.composition.update(dtSec);
    m_cache.update();
    m_render.renderComposition(m_project.composition, m_cache);
    m_cache.collectGarbage();

    updateStats();
    writePerfLog();
}

void AppController::writePerfLog() {
    if (!m_settings.perfLogEnabled) return;

    m_perfLogTimer += static_cast<float>(ofGetLastFrameTime());
    if (m_perfLogTimer < static_cast<float>(m_settings.perfLogIntervalSec)) return;
    m_perfLogTimer = 0.0f;

    // Bo qua ban ghi dau: giay dau tien luon xau vi con dang nap media,
    // dung shader va cap phat FBO. Ghi vao chi lam nhieu so lieu.
    if (m_perfLogCount++ == 0) return;

    std::ostringstream line;
    line << std::fixed << std::setprecision(2)
         << "fps=" << m_stats.fps
         << "  frame_avg=" << m_stats.frameAvgMs << "ms"
         << "  frame_p99=" << m_stats.frameP99Ms << "ms"
         << "  layers=" << m_stats.layersDrawn
         << "  slices=" << m_stats.slicesDrawn
         << "  media=" << m_stats.mediaLoaded
         << "  vram=" << (m_stats.vramBytes / (1024 * 1024)) << "MB"
         << "  sensor=" << (m_stats.sensorConnected ? "on" : "off")
         << std::endl;

    std::ofstream f(ofToDataPath("perf.log", true), std::ios::app);
    if (f) f << line.str();
}

void AppController::pollSensor() {
    m_mappedPoints.clear();
    m_rawPoints.clear();
    if (!m_sensor) return;

    // ── Kênh sự kiện: rút hết, có GIỚI HẠN ─────────────────────────────
    // Không giới hạn thì một trận bão sự kiện sẽ kéo dài frame này vô hạn.
    // Kenh su kien: rut het co GIOI HAN. Khong gioi han thi mot tran bao
    // su kien se keo dai frame nay vo han.
    //
    // G17 KHONG dung kenh nay: vung cam ung phat hien "diem DI VAO vung"
    // tu kenh trang thai, vi sensor tracking chi phat Down mot lan duy
    // nhat khi diem xuat hien. Xem TriggerZone.h.
    TouchEvent ev;
    int drained = 0;
    while (drained < 256 && m_sensor->events().pop(ev)) {
        ++drained;
    }

    // ── Kênh trạng thái: chỉ lấy frame MỚI NHẤT ────────────────────────
    if (!m_sensor->frames().consume()) return;

    const SensorFrame& f = m_sensor->frames().readSlot();

    // ★ Đo độ trễ thật — architecture.md §3.6.
    const double latencyMs = static_cast<double>(Clock::nowNs() - f.tCaptureNs) / 1.0e6;
    if (latencyMs >= 0.0 && latencyMs < 5000.0) {
        m_sensorLatencies.push_back(latencyMs);
        if (m_sensorLatencies.size() > kHistory) m_sensorLatencies.pop_front();
    }

    // ★ Biến đổi toạ độ ở ĐÂY, trên render thread — architecture.md §3.5.
    m_edit.sensorOutputPoints.clear();

    const double tSec = Clock::nsToSec(Clock::nowNs());

    // ── G12 rồi G11: gán ID bền vững TRƯỚC, lọc SAU ────────────────────
    // Thứ tự này bắt buộc. Lọc mà không có ID ổn định sẽ trộn quỹ đạo
    // của hai ngón tay vào nhau — mỗi frame bộ lọc lại nhận một điểm
    // khác và tưởng đó là cùng một vật đang nhảy loạn.
    std::vector<Vec2> observed;
    observed.reserve(static_cast<size_t>(f.count));
    for (int i = 0; i < f.count; ++i) {
        observed.push_back(Vec2{f.points[i].x, f.points[i].y});
    }

    m_tracker.update(observed, tSec);
    m_trackCount = static_cast<int>(m_tracker.activeTracks().size());

    // Giải phóng bộ lọc của điểm đã biến mất hẳn — nếu không, map này
    // phình ra vô hạn trong một show dài.
    for (const uint32_t lostId : m_tracker.justLost()) {
        m_filters.erase(lostId);
    }

    for (const TrackedPoint& tp : m_tracker.activeTracks()) {
        Vec2 raw = tp.position;
        if (m_filterEnabled) {
            raw = m_filters[tp.id].filter(tp.position, tSec);
        }
        m_rawPoints.push_back(raw);

        const MappedPoint mp = m_mapper.map(raw);
        if (!mp.valid) continue;

        m_mappedPoints.push_back(mp.canvasPx);
        // G13 — toa do may chieu, de ve cham xanh len output. Day la cach
        // kiem chung calibration bang MAT: cham vao vat the, thay cham roi
        // dung cho tay minh.
        m_edit.sensorOutputPoints.push_back(mp.outputPx);
    }

    // ★ G17 — DAY LA DIEM DEN CUA CA DU AN.
    // Diem da o trong khong gian canvas; tim vung nao vua duoc di vao
    // roi thuc thi hanh dong.
    const double nowSec = Clock::nsToSec(Clock::nowNs());
    for (const TriggerHit& hit : m_project.triggerZones.update(m_mappedPoints, nowSec)) {
        executeTrigger(hit);
    }
}

void AppController::updateStats() {
    m_stats.fps        = ofGetFrameRate();
    m_stats.frameAvgMs = average(m_frameTimes);
    m_stats.frameP99Ms = percentile(m_frameTimes, 0.99);

    m_stats.sensorConnected = (m_sensor && m_sensor->status() == SourceStatus::Running);
    m_stats.sensorLatencyAvgMs = average(m_sensorLatencies);
    m_stats.sensorLatencyP99Ms = percentile(m_sensorLatencies, 0.99);
    m_stats.touchCount = static_cast<int>(m_rawPoints.size());
    m_stats.trackedCount = m_trackCount;
    m_stats.filterEnabled = m_filterEnabled;

    if (m_sensor) {
        m_stats.framesDropped = m_sensor->frames().droppedCount();
        m_stats.ringOverflow  = m_sensor->events().overflowCount();
        if (auto* osc = dynamic_cast<OscSource*>(m_sensor.get())) {
            m_stats.packetsMalformed = osc->packetsMalformed();
        }
    }

    m_stats.layersDrawn    = m_render.lastLayersDrawn();
    m_stats.slicesDrawn    = m_render.lastSlicesDrawn();
    m_stats.mediaLoaded    = m_cache.loadedCount();
    m_stats.mediaEvictions = m_cache.evictionCount();
    m_stats.vramBytes      = m_cache.estimatedVramBytes();
}

// ═══════════════════════════════════════════════════════════════════════

void AppController::draw() {
    m_actions = UiActions{};
    m_panel.setTriggerInfo(m_triggerCount, m_lastTriggerName);

    // Doc lai danh sach man hinh moi frame: nguoi dung co the cam/rut
    // may chieu giua chung, va menu phai phan anh dung thuc te.
    std::vector<ControlPanel::DisplayEntry> entries;
    for (const DisplayInfo& d : enumerateDisplays()) {
        ControlPanel::DisplayEntry e;
        e.index = d.index; e.w = d.w; e.h = d.h;
        e.isPrimary = d.isPrimary; e.name = d.name;
        entries.push_back(e);
    }
    m_panel.setDisplays(std::move(entries));
    m_panel.draw(m_project, m_edit, m_stats,
                 &m_render.canvasFbo().getTexture(), m_actions);
    applyUiActions(m_actions);
}

void AppController::drawOutput(ofEventArgs&) {
    if (m_project.screens.empty()) { ofClear(0, 0, 0, 255); return; }

    // Doc trang thai THAT cua cua so moi frame, khong dua vao co noi bo.
    // Nguoi dung co the vao/thoat fullscreen bang F11, bang menu, hoac
    // bang phim he thong — chi co ban than cua so biet chac.
    m_edit.outputIsFullscreen =
        (m_outputWindow && m_outputWindow->getWindowMode() == OF_FULLSCREEN);

    m_render.renderScreen(m_project.screens[0], m_edit);

    // Vung cam ung cung la overlay chinh sua — an khi dang chieu.
    if (!m_edit.outputIsFullscreen) {
        m_render.drawTriggerZones(m_project.screens[0], m_project.triggerZones, m_edit);
    }
}

// ═══════════════════════════════════════════════════════════════════════

Screen* AppController::activeScreen() {
    if (m_project.screens.empty()) return nullptr;
    return &m_project.screens[0];
}

Slice* AppController::activeSlice() {
    Screen* sc = activeScreen();
    if (sc == nullptr) return nullptr;
    if (m_edit.activeSliceIndex < 0 || m_edit.activeSliceIndex >= sc->sliceCount()) {
        return nullptr;
    }
    return &sc->slices[static_cast<size_t>(m_edit.activeSliceIndex)];
}

int AppController::pickHandle(const Vec2& mouse, double radiusPx) const {
    if (m_project.screens.empty()) return -1;
    const Screen& sc = m_project.screens[0];
    if (m_edit.activeSliceIndex < 0 || m_edit.activeSliceIndex >= sc.sliceCount()) return -1;

    const IWarp* w = sc.slices[static_cast<size_t>(m_edit.activeSliceIndex)].warp();
    if (w == nullptr) return -1;

    // Dung API tong quat: corner pin cho 4 diem, mesh cho ca luoi.
    // Code o day khong can biet dang keo loai warp nao (F6).
    const int n = w->controlPointCount();
    int best = -1;
    double bestDist = radiusPx;

    for (int k = 0; k < n; ++k) {
        const double d = w->controlPointAt(k).distanceTo(mouse);
        if (d < bestDist) { bestDist = d; best = k; }
    }
    return best;
}

void AppController::outputMouseMoved(ofMouseEventArgs& args) {
    if (!m_edit.showOverlay) { m_edit.hoveredHandle = -1; return; }
    m_edit.hoveredHandle = pickHandle(Vec2{args.x, args.y}, 16.0);
}

void AppController::outputMousePressed(ofMouseEventArgs& args) {
    if (!m_edit.showOverlay) return;

    const Vec2 m{args.x, args.y};
    m_edit.draggedHandle = pickHandle(m, 16.0);

    // Bấm vào chỗ trống trong một slice khác → chọn slice đó.
    if (m_edit.draggedHandle < 0) {
        Screen* sc = activeScreen();
        if (sc != nullptr) {
            const int hit = sc->hitTest(m);
            if (hit >= 0) m_edit.activeSliceIndex = hit;
        }
    }
}

void AppController::outputMouseDragged(ofMouseEventArgs& args) {
    if (m_edit.draggedHandle < 0) return;

    Slice* s = activeSlice();
    if (s == nullptr || s->warp() == nullptr) return;

    // Hoat dong cho MOI loai warp (F6). Voi corner pin, setControlPointAt
    // TU CHOI vi tri lam tu giac lom hoac tu cat va giu nguyen trang thai
    // cu — nguoi dung thay handle "bat nguoc lai", va overlay doi vien
    // sang DO de ho hieu vi sao (xem RenderEngine::drawEditOverlay).
    s->warp()->setControlPointAt(m_edit.draggedHandle, Vec2{args.x, args.y});
}

void AppController::outputMouseReleased(ofMouseEventArgs&) {
    m_edit.draggedHandle = -1;
}

// ═══════════════════════════════════════════════════════════════════════

void AppController::startSensor(int typeIndex) {
    stopSensor();

    if (typeIndex == 1) {
        OscConfig cfg;
        cfg.port = 9000;
        auto src = std::make_unique<OscSource>(cfg);
        if (!src->start()) {
            m_panel.setStatusMessage("OSC: " + src->lastError(), true);
            return;
        }
        m_sensor = std::move(src);
        m_panel.setStatusMessage("OSC dang nghe cong 9000");
    } else {
        MockConfig cfg;
        cfg.pointCount = 3;
        cfg.rateHz = 120.0;
        cfg.pattern = MockPattern::Circle;
        // Phạm vi mm — cố ý KHÁC thang pixel, để lộ ra ngay nếu quên calibrate.
        cfg.maxX = 2000.0;
        cfg.maxY = 1200.0;
        auto src = std::make_unique<MockSource>(cfg);
        src->start();
        m_sensor = std::move(src);
        m_panel.setStatusMessage("Mock sensor dang chay (3 diem)");
    }

    m_sensorLatencies.clear();
}

void AppController::stopSensor() {
    if (m_sensor) m_sensor->stop();
    m_sensor.reset();
    m_mappedPoints.clear();
    m_rawPoints.clear();
    m_edit.sensorOutputPoints.clear();
    m_project.triggerZones.resetRuntimeState();
    m_tracker.reset();
    m_filters.clear();
    m_trackCount = 0;
}

void AppController::executeTrigger(const TriggerHit& hit) {
    Composition& comp = m_project.composition;

    switch (hit.action) {
    case TriggerAction::TriggerClip:   comp.triggerClip(hit.layer, hit.column); break;
    case TriggerAction::TriggerColumn: comp.triggerColumn(hit.column);          break;
    case TriggerAction::ClearLayer:    comp.layer(hit.layer).clear();           break;
    case TriggerAction::ClearAll:      comp.clearAll();                         break;
    case TriggerAction::None:
    default:
        return;
    }

    ++m_triggerCount;
    if (hit.zoneIndex >= 0 && hit.zoneIndex < m_project.triggerZones.count()) {
        m_lastTriggerName =
            m_project.triggerZones.zones[static_cast<size_t>(hit.zoneIndex)].name;
    }
}

void AppController::applySettings() {
    i18n::setLanguage(m_settings.language);
    ofSetVerticalSync(m_settings.vsync);
    m_cache.setBudget(m_settings.mediaCacheBudget);
}

void AppController::syncScreenToOutputSize() {
    if (!m_outputWindow || m_project.screens.empty()) return;

    const Vec2 now{static_cast<double>(m_outputWindow->getWidth()),
                   static_cast<double>(m_outputWindow->getHeight())};
    if (now.x < 2.0 || now.y < 2.0) return;

    // Frame dau: chi ghi nhan, khong co gian (chua co moc de so).
    if (m_lastOutputSize.x < 2.0) { m_lastOutputSize = now; return; }
    if (std::abs(now.x - m_lastOutputSize.x) < 1.0
        && std::abs(now.y - m_lastOutputSize.y) < 1.0) return;

    Screen& sc = m_project.screens[0];

    const double sx = now.x / std::max(1.0, sc.resolution.x);
    const double sy = now.y / std::max(1.0, sc.resolution.y);

    // Co gian TUNG DIEM DIEU KHIEN theo ti le, thay vi dat lai hinh chu
    // nhat. Nguoi van hanh co the da mat nua tieng can chinh tren cua so
    // nho; vao fullscreen ma mat het cong do thi khong dung duoc.
    for (Slice& sl : sc.slices) {
        IWarp* w = sl.warp();
        if (w == nullptr) continue;
        const int n = w->controlPointCount();
        for (int k = 0; k < n; ++k) {
            const Vec2 p = w->controlPointAt(k);
            w->setControlPointAt(k, Vec2{p.x * sx, p.y * sy});
        }
    }

    sc.resolution = now;
    m_lastOutputSize = now;

    // Calibration H_s anh xa sang KHONG GIAN OUTPUT, nen doi do phan giai
    // lam no sai. Bao cho nguoi dung biet thay vi de ho phat hien luc
    // cham vao vat the ma hieu ung ra sai cho.
    if (!m_project.calibrations.empty() && m_project.calibrations[0].isValid()) {
        m_panel.setStatusMessage(
            "Do phan giai output doi -> calibration sensor can lam lai", true);
    }
}

std::vector<AppController::DisplayInfo> AppController::enumerateDisplays() const {
    std::vector<DisplayInfo> out;

    // oF chi cho biet man hinh CHINH (ofGetScreenWidth). Muon biet co bao
    // nhieu man hinh va chung nam o dau thi phai hoi thang GLFW.
    int count = 0;
    GLFWmonitor** monitors = glfwGetMonitors(&count);
    if (monitors == nullptr) return out;

    GLFWmonitor* primary = glfwGetPrimaryMonitor();

    for (int i = 0; i < count; ++i) {
        DisplayInfo d;
        d.index = i;
        glfwGetMonitorPos(monitors[i], &d.x, &d.y);

        if (const GLFWvidmode* mode = glfwGetVideoMode(monitors[i])) {
            d.w = mode->width;
            d.h = mode->height;
        }

        d.isPrimary = (monitors[i] == primary);
        const char* n = glfwGetMonitorName(monitors[i]);
        d.name = (n != nullptr) ? n : "Display";
        out.push_back(d);
    }
    return out;
}

void AppController::sendOutputToDisplay(int displayIndex) {
    if (!m_outputWindow) return;

    const auto displays = enumerateDisplays();
    if (displayIndex < 0 || displayIndex >= static_cast<int>(displays.size())) {
        m_panel.setStatusMessage("Khong thay man hinh so "
                                 + std::to_string(displayIndex + 1), true);
        return;
    }

    const DisplayInfo& d = displays[static_cast<size_t>(displayIndex)];

    // Thoat fullscreen truoc: dang fullscreen thi khong di chuyen duoc.
    m_outputWindow->setFullscreen(false);
    m_outputWindow->setWindowPosition(d.x + 40, d.y + 40);
    m_outputWindow->setWindowShape(std::max(320, d.w - 80),
                                   std::max(240, d.h - 80));

    // Khong tu dat do phan giai o day: syncScreenToOutputSize() se thay
    // cua so doi kich thuoc va co gian mapping theo ti le. Mot cho lo
    // viec nay thay vi hai, de hai cho khong cho ra ket qua khac nhau.
    m_outputWindow->setFullscreen(true);
    m_panel.setStatusMessage("Output -> man hinh " + std::to_string(displayIndex + 1)
                             + " (" + d.name + ", "
                             + std::to_string(d.w) + "x" + std::to_string(d.h) + ")");
}

void AppController::autoCalibrateMock() {
    auto* mock = dynamic_cast<MockSource*>(m_sensor.get());
    if (mock == nullptr) {
        m_panel.setStatusMessage(
            "Auto-calibrate chi dung duoc voi Mock sensor (bam M de bat)", true);
        return;
    }
    if (m_project.calibrations.empty()) return;

    Slice* s = activeSlice();
    if (s == nullptr) {
        m_panel.setStatusMessage("Chua chon slice nao", true);
        return;
    }

    // Pham vi toa do cua mock la DA BIET, va 4 goc slice cung vay.
    // Ghep chung lai la ra H_s — dung 4 cap tuong ung y het khi nguoi
    // van hanh cham tay, chi khac la khong phai cham.
    const MockConfig& cfg = mock->config();
    const Vec2 sensorCorners[4] = {
        {cfg.minX, cfg.minY}, {cfg.maxX, cfg.minY},
        {cfg.maxX, cfg.maxY}, {cfg.minX, cfg.maxY},
    };
    const Vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};

    CalibrationProfile& cal = m_project.calibrations[0];
    cal.clearPairs();
    for (int k = 0; k < 4; ++k) {
        cal.addPair(sensorCorners[k], s->contentToOutput(uv[k]));
    }

    const HomographyResult r = cal.solve();
    m_panel.setStatusMessage(
        r.ok ? "Auto-calibrate xong — sai so " + std::to_string(r.rmsError) + " px"
             : std::string("Auto-calibrate that bai: ") + r.message,
        !r.ok);
}

void AppController::applyUiActions(UiActions& a) {
    if (a.sensorTypeIndex >= 0) m_sensorTypeIndex = a.sensorTypeIndex;
    if (a.startSensor) startSensor(m_sensorTypeIndex);
    if (a.stopSensor)  stopSensor();

    if (a.toggleFullscreen && m_outputWindow) {
        m_outputWindow->toggleFullscreen();
    }

    if (a.settingsChanged) applySettings();

    if (a.resetSettings) {
        m_settings = AppSettings{};
        applySettings();
        m_panel.setStatusMessage(TR("set.reset"));
    }

    if (a.saveSettings) {
        std::string err;
        if (m_settings.save(ofToDataPath(m_settingsPath, true), err)) {
            m_panel.setStatusMessage(TR("set.saved"));
        } else {
            m_panel.setStatusMessage(err, true);
        }
    }

    if (a.sendOutputToDisplay >= 0) sendOutputToDisplay(a.sendOutputToDisplay);

    if (a.outputWindowed && m_outputWindow) {
        m_outputWindow->setFullscreen(false);
        m_panel.setStatusMessage("Output ve che do cua so — keo duoc bang thanh tieu de");
    }

    if (a.saveProject) {
        std::string err;
        const std::string path = ofToDataPath(m_projectPath, true);
        if (projectio::save(path, m_project, err)) {
            m_panel.setStatusMessage("Da luu: " + path);
        } else {
            m_panel.setStatusMessage("Luu that bai: " + err, true);
        }
    }

    if (a.loadProject) {
        Project loaded;
        const LoadResult r = projectio::load(ofToDataPath(m_projectPath, true), loaded);
        if (r.ok) {
            m_project = std::move(loaded);
            m_cache.clear();
            m_render.resizeCanvas(m_project.composition.canvasSize);
            m_mapper.setCalibration(m_project.calibrations.empty()
                                    ? nullptr : &m_project.calibrations[0]);
            m_mapper.setScreen(m_project.screens.empty() ? nullptr : &m_project.screens[0]);
            m_edit.activeSliceIndex = 0;
            m_project.triggerZones.resetRuntimeState();
            m_triggerCount = 0;
            m_lastTriggerName.clear();

            std::string msg = "Da nap project";
            if (!r.warnings.empty()) {
                msg += " (" + std::to_string(r.warnings.size()) + " canh bao)";
                for (const std::string& w : r.warnings) ofLogWarning("project") << w;
            }
            m_panel.setStatusMessage(msg, !r.warnings.empty());
        } else {
            m_panel.setStatusMessage("Nap that bai: " + r.error, true);
        }
    }

    if (a.newProject) {
        m_project = Project{};
        m_project.composition = Composition(3, 8, 2);
        Screen sc(0, "Projector 1", Vec2{1920.0, 1080.0});
        sc.addFullScreenSlice(m_project.composition.canvasSize);
        m_project.screens.push_back(std::move(sc));
        m_project.calibrations.emplace_back();
        m_cache.clear();
        m_mapper.setCalibration(&m_project.calibrations[0]);
        m_mapper.setScreen(&m_project.screens[0]);
    }

    // ── Slice ──────────────────────────────────────────────────────────
    if (a.addSliceToScreen >= 0
        && a.addSliceToScreen < static_cast<int>(m_project.screens.size())) {
        Screen& sc = m_project.screens[static_cast<size_t>(a.addSliceToScreen)];
        sc.addFullScreenSlice(m_project.composition.canvasSize);
        m_edit.activeSliceIndex = sc.sliceCount() - 1;
    }

    if (a.removeSliceIndex >= 0) {
        if (Screen* sc = activeScreen()) {
            sc->removeSlice(a.removeSliceIndex);
            m_edit.activeSliceIndex = std::min(m_edit.activeSliceIndex,
                                               sc->sliceCount() - 1);
        }
    }

    if (a.resetActiveSliceWarp) {
        if (Slice* s = activeSlice()) {
            if (Screen* sc = activeScreen()) {
                s->warp()->resetToRect(Vec2{0.0, 0.0}, sc->resolution);
            }
        }
    }

    if (a.convertWarpTo >= 0) {
        if (Slice* s = activeSlice()) {
            s->convertWarp(a.convertWarpTo == 1 ? WarpType::Mesh : WarpType::CornerPin,
                           6, 6);
        }
    }

    // ── G17: them / xoa vung cam ung ───────────────────────────────────
    if (a.addTriggerZone) {
        TriggerZone z;
        z.name = "Zone " + std::to_string(m_project.triggerZones.count() + 1);
        // Dat giua canvas, kich thuoc 1/4 — de nhin thay ngay va keo chinh.
        const Vec2 cs = m_project.composition.canvasSize;
        z.size   = Vec2{cs.x * 0.25, cs.y * 0.25};
        z.origin = Vec2{(cs.x - z.size.x) * 0.5, (cs.y - z.size.y) * 0.5};
        z.action = TriggerAction::TriggerClip;
        m_project.triggerZones.zones.push_back(z);
    }

    if (a.removeTriggerZone >= 0
        && a.removeTriggerZone < m_project.triggerZones.count()) {
        auto& zs = m_project.triggerZones.zones;
        zs.erase(zs.begin() + a.removeTriggerZone);
    }

    // ── I6: chon file gan vao o clip ───────────────────────────────────
    if (a.browseForClip) {
        const ofFileDialogResult res =
            ofSystemLoadDialog("Chon video HAP hoac anh", false);
        if (res.bSuccess) {
            const std::string ext = ofToLower(ofFilePath::getFileExt(res.filePath));

            Clip c;
            c.name = ofFilePath::getBaseName(res.filePath);
            c.media.path = res.filePath;

            if (ext == "png" || ext == "jpg" || ext == "jpeg"
                || ext == "gif" || ext == "tga" || ext == "bmp") {
                c.media.type = MediaType::Image;
            } else {
                c.media.type = MediaType::Video;

                // ★ I9 — canh bao khi file kha nang cao KHONG phai HAP.
                // Day la nguyen nhan so 1 khien show bi tut fps: nguoi
                // van hanh keo mot file .mp4 vao roi khong hieu tai sao
                // tu 60 xuong 12 fps.
                if (ext != "mov" && m_settings.warnNonHapMedia) {
                    m_panel.setStatusMessage(
                        "Canh bao: '." + ext + "' kha nang cao KHONG phai HAP. "
                        "Dung tools/encode_hap.ps1 de chuyen doi.", true);
                }
            }

            m_project.composition.deck(m_project.composition.viewedDeck())
                .setClip(a.browseLayer, a.browseColumn, c);
        }
    }

    // ── Wizard calibration (G6) ────────────────────────────────────────
    if (m_project.calibrations.empty()) {
        m_edit.calibrating = false;
        return;
    }

    // ★ G6 — dua diem muc tieu xuong RenderEngine de ve dau thap len
    //   may chieu. Khong co no, nguoi van hanh khong biet cham vao dau.
    m_edit.calibrating   = a.calibShowTarget;
    m_edit.calibStep     = m_panel.calibTargetIndex();
    m_edit.calibTargetUV = kCalibTargets[std::clamp(m_edit.calibStep, 0, 3)];

    CalibrationProfile& cal = m_project.calibrations[0];

    if (a.calibClear) {
        cal.clearPairs();
        m_panel.setCalibTargetIndex(0);
        m_panel.setStatusMessage("Da xoa het diem calibration");
    }

    if (a.calibNextTarget) {
        m_panel.setCalibTargetIndex((m_panel.calibTargetIndex() + 1) % 4);
    }

    if (a.calibAddPoint) {
        if (m_rawPoints.empty()) {
            m_panel.setStatusMessage("Chua thay diem cham nao — sensor dang chay chua?", true);
        } else if (Slice* s = activeSlice()) {
            const int t = m_panel.calibTargetIndex();
            // Điểm đích trên MÁY CHIẾU, suy từ contentUV qua warp của slice.
            const Vec2 outputPx = s->contentToOutput(kCalibTargets[t]);
            cal.addPair(m_rawPoints[0], outputPx);

            m_panel.setCalibTargetIndex((t + 1) % 4);
            m_panel.setStatusMessage("Da ghi diem " + std::to_string(t + 1) + "/4");
        }
    }

    if (a.calibAutoMock) autoCalibrateMock();

    if (a.calibSolve) {
        const HomographyResult r = cal.solve();
        if (r.ok) {
            m_panel.setStatusMessage(
                "Calibrate xong — sai so " + std::to_string(r.rmsError) + " px",
                !cal.isAccurate(3.0));
        } else {
            m_panel.setStatusMessage(std::string("Calibrate that bai: ") + r.message, true);
        }
    }
}

void AppController::keyPressed(int key) {
    // Phim tat cho luc CHAY SHOW: tay tren ban phim, khong phai chuot.
    // Giua show khong ai di tim checkbox trong ImGui.
    switch (key) {
    case OF_KEY_F11:
        if (m_outputWindow) m_outputWindow->toggleFullscreen();
        break;

    case ' ':
        // I5 — Show Mode: tat/bat toan bo overlay chinh sua.
        m_edit.showOverlay = !m_edit.showOverlay;
        break;

    case 'g':
        m_edit.showGrid = !m_edit.showGrid;      // F14
        break;

    case 'm':
        // Bat/tat sensor gia lap — tien khi thu nhanh ma chua cam phan cung.
        if (m_sensor) stopSensor();
        else          startSensor(0);
        break;

    case 'o':
        if (m_sensor) stopSensor();
        else          startSensor(1);            // OSC
        break;

    case 'c':
        // G6 — dau thap calibration. Ghi vao PANEL chu khong vao m_edit:
        // applyUiActions() dong bo m_edit tu panel moi frame, nen ghi
        // thang vao m_edit se bi ghi de ngay lap tuc.
        m_panel.setCalibShowTarget(!m_panel.calibShowTarget());
        break;

    case 'p':
        // G13 — cham xanh cua diem sensor tren output.
        m_edit.showSensorPoints = !m_edit.showSensorPoints;
        break;

    case 'a':
        autoCalibrateMock();
        break;

    case 'f':
        // G11 — bat/tat loc, de so sanh truc tiep muc rung.
        m_filterEnabled = !m_filterEnabled;
        m_panel.setStatusMessage(m_filterEnabled ? "Loc nhieu: BAT" : "Loc nhieu: TAT");
        break;

    default:
        break;
    }
}

void AppController::outputKeyPressed(ofKeyEventArgs& args) {
    keyPressed(args.key);
}

void AppController::exit() {
    stopSensor();
    m_panel.shutdown();
    m_cache.clear();
}

} // namespace hexmap

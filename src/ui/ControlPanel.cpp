#include "ui/ControlPanel.h"

#include "ui/Localization.h"
#include "ui/Theme.h"

#include "core/model/WarpCornerPin.h"
#include "core/model/WarpMesh.h"

#include <algorithm>
#include <cfloat>
#include <cstddef>
#include <cstdio>

namespace hexmap {
namespace {

const char* kBlendNames[] = {
    "Normal", "Add", "Multiply", "Screen", "Overlay", "SoftLight",
    "HardLight", "Difference", "Subtract", "Lighten", "Darken",
};

/// 4 điểm calibration mặc định: các góc, thụt vào 15% để người vận hành
/// chạm được thoải mái mà không bị mép vật thể cản.
const Vec2 kCalibTargets[4] = {
    {0.15, 0.15}, {0.85, 0.15}, {0.85, 0.85}, {0.15, 0.85},
};

/// Ghep nhan da dich voi mot ID CO DINH: "Nhan###id".
///
/// ★ ImGui lay chuoi tieu de lam ID cua so. Neu de tieu de doi theo ngon
///   ngu thi doi ngon ngu = ImGui thay mot cua so HOAN TOAN MOI: vi tri,
///   kich thuoc, trang thai thu gon deu ve mac dinh, va bo cuc nguoi dung
///   sap xep bi xoa sach. Phan sau `###` la ID that va khong bao gio doi.
std::string winTitle(const char* key, const char* id) {
    return std::string(i18n::t(key)) + "###" + id;
}

/// Nhãn ở TRÊN, widget chiếm hết chiều rộng còn lại.
///
/// ★ Bảng thuộc tính bên phải chỉ rộng 280px. Kiểu mặc định của ImGui
///   đặt nhãn BÊN PHẢI widget, nên "Vùng lấy - kích thước" bị cắt mất
///   nửa sau — và người dùng không đoán được ô số đó là gì.
void labelAbove(const char* text) {
    ImGui::TextUnformatted(text);
    ImGui::SetNextItemWidth(-FLT_MIN);
}

std::string formatBytes(size_t b) {
    char buf[48];
    if (b > 1024ull * 1024ull * 1024ull) {
        std::snprintf(buf, sizeof(buf), "%.2f GB", static_cast<double>(b) / (1024.0 * 1024.0 * 1024.0));
    } else if (b > 1024ull * 1024ull) {
        std::snprintf(buf, sizeof(buf), "%.1f MB", static_cast<double>(b) / (1024.0 * 1024.0));
    } else {
        std::snprintf(buf, sizeof(buf), "%.0f KB", static_cast<double>(b) / 1024.0);
    }
    return buf;
}

} // namespace

void ControlPanel::setup() {
    m_gui.setup(nullptr, true);

    // Nap font SAU gui.setup(): luc do ImGui context da ton tai nen
    // io.Fonts moi dung duoc. ofxImGui se dung atlas nay o frame dau.
    loadFont();

    // Bang mau MikMap de len theme mac dinh cua ofxImGui.
    // Goi SAU setup() vi setTheme() cua ofxImGui ghi de ImGuiStyle.
    theme::apply();

    m_ready = true;
}

void ControlPanel::shutdown() {
    if (m_ready) m_gui.exit();
    m_ready = false;
}

void ControlPanel::setStatusMessage(const std::string& msg, bool isError) {
    m_status = msg;
    m_statusIsError = isError;
    m_statusTimer = 6.0f;
}

// ═══════════════════════════════════════════════════════════════════════

void ControlPanel::draw(Project& project,
                        EditState& edit,
                        const PerfStats& stats,
                        const ofTexture* canvasPreview,
                        UiActions& actions) {
    if (!m_ready) return;

    m_statusTimer = std::max(0.0f, m_statusTimer - static_cast<float>(ofGetLastFrameTime()));
    m_sensorRunning = stats.sensorConnected;

    m_gui.begin();

    // ── Vo ung dung: MOT cua so day man hinh, khong vien ───────────────
    //
    // ★ Bo cuc CO DINH thay cho cac cua so noi truoc day.
    //
    //   Cua so noi tu do nghe thi linh hoat, nhung trong phong toi giua
    //   buoi dien, nguoi van hanh khong co thoi gian sap lai ban lam viec
    //   — va mot bang troi ra ngoai man hinh hoac bi che mat la chuyen
    //   xay ra that. Bo cuc co dinh nghia la moi thu LUON o dung cho cu.
    const ImGuiViewport* vp = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(vp->WorkPos);
    ImGui::SetNextWindowSize(vp->WorkSize);

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, theme::v4(theme::BgApp));

    const ImGuiWindowFlags rootFlags =
        ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize
        | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse
        | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus
        | ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    if (ImGui::Begin("##mikmap_root", nullptr, rootFlags)) {
        drawTopBar(stats, actions);

        ImGui::PushStyleVar(ImGuiStyleVar_ChildBorderSize, 0.0f);
        ImGui::BeginChild("##content", ImVec2(0, 0), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar);

        switch (m_view) {
        case View::Mapping: drawMappingView(project, edit, canvasPreview, actions); break;
        case View::Sensor:  drawSensorView(project, stats, actions);                break;
        default:            drawCompositionView(project, edit, stats,
                                                canvasPreview, actions);            break;
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleColor();
    ImGui::PopStyleVar();

    // ── Cua so Cai dat: van la cua so NOI, co chu dich ─────────────────
    //
    // No khong thuoc luong lam viec luc dien — mo ra, chinh, dong lai.
    // Danh cho no mot cho co dinh trong bo cuc la lay mat dien tich cua
    // nhung thu dung suot buoi.
    if (m_showSettings) {
        ImGui::SetNextWindowSize(ImVec2(460, 560), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(1.0f);
        if (ImGui::Begin(winTitle("win.settings", "settings").c_str(), &m_showSettings)) {
            drawSettingsPanel(actions);
        }
        ImGui::End();
    }

    if (m_pendingBrowse) {
        actions.browseForClip = true;
        actions.browseLayer   = m_browseLayer;
        actions.browseColumn  = m_browseColumn;
        m_pendingBrowse = false;
    }

    m_gui.end();
}

// ═══════════════════════════════════════════════════════════════════════
//  Thanh dieu huong tren cung
// ═══════════════════════════════════════════════════════════════════════

void ControlPanel::drawTopBar(const PerfStats& s, UiActions& a) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgPanel));
    ImGui::BeginChild("##topbar", ImVec2(0, theme::TopBarH), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar);

    // Vien duoi — tach thanh dieu huong khoi vung lam viec.
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p0 = ImGui::GetWindowPos();
        const float w = ImGui::GetWindowWidth();
        dl->AddLine(ImVec2(p0.x, p0.y + theme::TopBarH - 1.0f),
                    ImVec2(p0.x + w, p0.y + theme::TopBarH - 1.0f), theme::Border);
    }

    ImGui::SetCursorPos(ImVec2(14.0f, (theme::TopBarH - ImGui::GetFrameHeight()) * 0.5f));

    // ── Thuong hieu ────────────────────────────────────────────────────
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 p = ImGui::GetCursorScreenPos();
        const float cy = p.y + ImGui::GetFrameHeight() * 0.5f;
        dl->AddRectFilled(ImVec2(p.x, cy - 8.0f), ImVec2(p.x + 16.0f, cy + 8.0f),
                          theme::Primary, 3.0f);
        dl->AddRectFilled(ImVec2(p.x + 5.5f, cy - 2.5f), ImVec2(p.x + 10.5f, cy + 2.5f),
                          theme::BgPanel, 1.0f);
        ImGui::Dummy(ImVec2(22.0f, ImGui::GetFrameHeight()));
    }
    ImGui::SameLine(0.0f, 6.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted("MIKMAP");

    // ── Project ────────────────────────────────────────────────────────
    //
    // ★ Thanh menu cu da bi ba tab thay the — va suyt keo theo ca loi Luu
    //   / Mo project. Ban thiet ke khong ve cho nao cho viec nay, nhung
    //   mot phan mem khong luu duoc cong viec thi khong dung duoc; nen no
    //   quay lai duoi dang mot nut bat popup, khong chiem cho thuong truc.
    ImGui::SameLine(0.0f, 16.0f);
    if (theme::tabButton(TR("menu.project"), false, ImVec2(78, 0), theme::Text)) {
        ImGui::OpenPopup("##projectmenu");
    }
    if (ImGui::BeginPopup("##projectmenu")) {
        if (ImGui::MenuItem(TR("menu.project.new")))               a.newProject  = true;
        if (ImGui::MenuItem(TR("menu.project.open"), "Ctrl+O"))    a.loadProject = true;
        if (ImGui::MenuItem(TR("menu.project.save"), "Ctrl+S"))    a.saveProject = true;
        ImGui::Separator();
        if (ImGui::BeginMenu(TR("menu.output.sendto"))) {
            if (m_displays.empty()) {
                ImGui::TextDisabled("%s", TR("menu.output.nodisplay"));
            }
            for (const DisplayEntry& d : m_displays) {
                char lbl[128];
                std::snprintf(lbl, sizeof(lbl), "%d. %s  %dx%d%s",
                              d.index + 1, d.name.c_str(), d.w, d.h,
                              d.isPrimary ? TR("menu.output.primary") : "");
                if (ImGui::MenuItem(lbl)) a.sendOutputToDisplay = d.index;
            }
            ImGui::EndMenu();
        }
        if (ImGui::MenuItem(TR("menu.output.windowed")))            a.outputWindowed  = true;
        if (ImGui::MenuItem(TR("menu.output.fullscreen"), "F11"))   a.toggleFullscreen = true;
        ImGui::Separator();
        if (ImGui::MenuItem(TR("menu.sensor.mock"), "M")) {
            a.sensorTypeIndex = 0; a.startSensor = true;
        }
        if (ImGui::MenuItem(TR("menu.sensor.osc"), "O")) {
            a.sensorTypeIndex = 1; a.startSensor = true;
        }
        if (ImGui::MenuItem(TR("menu.sensor.stop")))            a.stopSensor   = true;
        if (ImGui::MenuItem(TR("menu.sensor.autocal"), "A"))    a.calibAutoMock = true;
        ImGui::MenuItem(TR("menu.sensor.crosshair"), "C", &m_calibShowTarget);
        ImGui::EndPopup();
    }

    // ── Ba tab ─────────────────────────────────────────────────────────
    ImGui::SameLine(0.0f, 16.0f);
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgSunken));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3, 3));
        ImGui::BeginChild("##tabs", ImVec2(512.0f, theme::TopBarH - 12.0f),
                          ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);

        const ImVec2 tsz(164.0f, 0.0f);
        if (theme::tabButton(TR("nav.composition"), m_view == View::Composition, tsz)) {
            m_view = View::Composition;
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (theme::tabButton(TR("nav.mapping"), m_view == View::Mapping, tsz)) {
            m_view = View::Mapping;
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (theme::tabButton(TR("nav.sensor"), m_view == View::Sensor, tsz)) {
            m_view = View::Sensor;
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    // ── Trang thai ben phai ────────────────────────────────────────────
    //
    // FPS va do phan giai dau ra hien THUONG TRUC, khong phai mo bang moi
    // thay: hai con so nay la thu duy nhat cho biet buoi dien co on khong,
    // va luc chung tut thi khong ai co thoi gian di tim cho de xem.
    {
        char fpsBuf[48];
        std::snprintf(fpsBuf, sizeof(fpsBuf), "FPS %.1f", s.fps);

        std::string outBuf = TR("nav.nooutput");
        for (const DisplayEntry& d : m_displays) {
            if (d.index != m_activeOutputDisplay) continue;
            char b[96];
            std::snprintf(b, sizeof(b), "OUTPUT %d: %dx%d", d.index + 1, d.w, d.h);
            outBuf = b;
        }

        const float gearW  = 34.0f;
        const float fpsW   = ImGui::CalcTextSize(fpsBuf).x + 24.0f;
        const float outW   = ImGui::CalcTextSize(outBuf.c_str()).x + 24.0f;
        const float rightW = fpsW + outW + gearW + 30.0f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                      ImGui::GetWindowWidth() - rightW));

        // Chi so FPS doi mau theo nguong 60: xanh = dat, vang = khong.
        const bool ok = (s.fps >= 55.0);
        ImGui::PushStyleColor(ImGuiCol_Text, theme::v4(ok ? theme::Success : theme::Warning));
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(fpsBuf);
        ImGui::PopStyleColor();

        ImGui::SameLine(0.0f, 18.0f);
        theme::statusDot(m_activeOutputDisplay >= 0 ? theme::Success : theme::TextFaint);
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", outBuf.c_str());

        ImGui::SameLine(0.0f, 12.0f);
        if (theme::tabButton(TR("nav.settings"), m_showSettings, ImVec2(gearW, 0))) {
            m_showSettings = !m_showSettings;
        }
    }

    // ── Thong bao tam thoi ─────────────────────────────────────────────
    if (m_statusTimer > 0.0f && !m_status.empty()) {
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        const ImVec2 tsz = ImGui::CalcTextSize(m_status.c_str());
        const ImVec2 wp  = ImGui::GetMainViewport()->WorkPos;
        const float  ww  = ImGui::GetMainViewport()->WorkSize.x;

        const ImVec2 p((wp.x + ww - tsz.x) * 0.5f, wp.y + theme::TopBarH + 10.0f);
        const ImU32 c = m_statusIsError ? theme::Danger : theme::Success;

        dl->AddRectFilled(ImVec2(p.x - 14.0f, p.y - 7.0f),
                          ImVec2(p.x + tsz.x + 14.0f, p.y + tsz.y + 7.0f),
                          theme::BgCard, 5.0f);
        dl->AddRect(ImVec2(p.x - 14.0f, p.y - 7.0f),
                    ImVec2(p.x + tsz.x + 14.0f, p.y + tsz.y + 7.0f),
                    theme::alpha(c, 0.6f), 5.0f, 0, 1.5f);
        dl->AddText(p, c, m_status.c_str());
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

// ═══════════════════════════════════════════════════════════════════════
//  TRANG 1 — COMPOSITION
// ═══════════════════════════════════════════════════════════════════════

void ControlPanel::drawCompositionView(Project& p, EditState& edit,
                                       const PerfStats& s,
                                       const ofTexture* canvasTex,
                                       UiActions& a) {
    (void)edit;

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 200.0f || avail.y < 200.0f) return;

    // Nua tren cho cong cu, nua duoi cho luoi clip. Sàn 300px cho nua
    // tren: duoi nguong do hai man hinh xem nho toi muc vo dung.
    const float topH = std::max(300.0f, avail.y * theme::CompTopRatio);
    const float botH = std::max(120.0f, avail.y - topH - 6.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

    // ── Hang tren ──────────────────────────────────────────────────────
    ImGui::BeginChild("##comp_top", ImVec2(0, topH), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar);
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgPanel));
        ImGui::BeginChild("##browser", ImVec2(theme::BrowserW, 0), ImGuiChildFlags_Borders);
        drawBrowserPanel(p, a);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SameLine();

        const float midW = std::max(240.0f,
            ImGui::GetContentRegionAvail().x - theme::InspectorW - 6.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgApp));
        ImGui::BeginChild("##monitors", ImVec2(midW, 0), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);
        drawMonitors(p, s, canvasTex);
        ImGui::EndChild();
        ImGui::PopStyleColor();

        ImGui::SameLine();

        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgPanel));
        ImGui::BeginChild("##inspector", ImVec2(0, 0), ImGuiChildFlags_Borders);
        drawInspector(p);
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }
    ImGui::EndChild();

    // ── Hang duoi: layer × cot, chiem HET chieu ngang ──────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgPanel));
    ImGui::BeginChild("##deck", ImVec2(0, botH), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    drawLayersDeck(p);
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();
}

// ── Trinh duyet media ──────────────────────────────────────────────────

void ControlPanel::rescanMedia() {
    m_mediaFiles.clear();

    ofDirectory dir(ofToDataPath("media", true));
    if (!dir.exists()) { m_mediaScanned = true; return; }

    dir.allowExt("mov");
    dir.allowExt("mp4");
    dir.allowExt("png");
    dir.allowExt("jpg");
    dir.allowExt("jpeg");
    dir.listDir();
    dir.sort();

    for (std::size_t i = 0; i < dir.size(); ++i) {
        m_mediaFiles.push_back(dir.getPath(i));
    }
    m_mediaScanned = true;
}

void ControlPanel::drawBrowserPanel(Project& p, UiActions& a) {
    (void)p;

    // Quet dia la I/O — chi lam lan dau, hoac khi nguoi dung yeu cau.
    if (!m_mediaScanned) rescanMedia();

    ImGui::TextUnformatted(TR("browser.title"));
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 34.0f);
    if (theme::tabButton("<>", false, ImVec2(24, 0), theme::Info)) rescanMedia();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("browser.rescan"));

    ImGui::Separator();
    theme::sectionLabel(TR("browser.media"));

    if (m_mediaFiles.empty()) {
        ImGui::TextDisabled("%s", TR("browser.empty"));
    }

    const bool hasTarget = (m_selLayer >= 0 && m_selColumn >= 0);

    ImGui::BeginChild("##medialist", ImVec2(0, -76.0f));
    for (std::size_t i = 0; i < m_mediaFiles.size(); ++i) {
        const std::string name = ofFilePath::getFileName(m_mediaFiles[i]);
        const std::string ext  = ofToLower(ofFilePath::getFileExt(m_mediaFiles[i]));

        // ★ .mov gan nhu chac chan la HAP trong du an nay; dinh dang khac
        //   thi bao mau VANG ngay tren danh sach. Canh bao sau khi da keo
        //   vao thi da muon — luc do fps da tut giua buoi dien.
        const bool likelyHap = (ext == "mov");
        ImGui::PushStyleColor(ImGuiCol_Text,
            theme::v4(likelyHap ? theme::Text : theme::Warning));

        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(name.c_str(), false, 0, ImVec2(0, 20))) {
            if (hasTarget) {
                a.assignMediaPath = m_mediaFiles[i];
                a.assignLayer     = m_selLayer;
                a.assignColumn    = m_selColumn;
            } else {
                setStatusMessage(TR("browser.noTarget"), true);
            }
        }
        ImGui::PopID();
        ImGui::PopStyleColor();

        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s\n%s", m_mediaFiles[i].c_str(),
                              likelyHap ? TR("browser.hapOk") : TR("browser.hapWarn"));
        }
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (hasTarget) {
        ImGui::TextDisabled(TR("browser.target"), m_selLayer + 1, m_selColumn + 1);
    } else {
        ImGui::TextDisabled("%s", TR("browser.noTarget"));
    }

    if (theme::outlineButton(TR("browser.browse"), theme::Info,
                             ImVec2(-FLT_MIN, 0))) {
        if (hasTarget) {
            m_pendingBrowse = true;
            m_browseLayer   = m_selLayer;
            m_browseColumn  = m_selColumn;
        } else {
            setStatusMessage(TR("browser.noTarget"), true);
        }
    }
}

// ── Hai man hinh xem ───────────────────────────────────────────────────

void ControlPanel::drawMonitors(Project& p, const PerfStats& s,
                                const ofTexture* canvasTex) {
    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 120.0f || avail.y < 80.0f) return;

    const float halfW = (avail.x - 8.0f) * 0.5f;

    auto monitor = [&](const char* id, const char* label, ImU32 labelCol,
                       bool showSliceRects, const char* badge, ImU32 badgeCol) {
        ImGui::BeginChild(id, ImVec2(halfW, 0), ImGuiChildFlags_None,
                          ImGuiWindowFlags_NoScrollbar);

        ImGui::PushStyleColor(ImGuiCol_Text, theme::v4(labelCol));
        ImGui::TextUnformatted(label);
        ImGui::PopStyleColor();

        if (badge != nullptr) {
            const float bw = ImGui::CalcTextSize(badge).x + 14.0f;
            ImGui::SameLine();
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                          ImGui::GetWindowWidth() - bw - 8.0f));
            ImGui::PushStyleColor(ImGuiCol_Text, theme::v4(badgeCol));
            ImGui::TextUnformatted(badge);
            ImGui::PopStyleColor();
        }

        const ImVec2 box = ImGui::GetContentRegionAvail();
        if (box.x < 20.0f || box.y < 20.0f) { ImGui::EndChild(); return; }

        const ImVec2 org = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(org, ImVec2(org.x + box.x, org.y + box.y),
                          IM_COL32(0, 0, 0, 255), 4.0f);

        if (canvasTex != nullptr && canvasTex->isAllocated()) {
            const float tw = canvasTex->getWidth();
            const float th = canvasTex->getHeight();
            const float sc = std::min(box.x / tw, box.y / th);
            const float dw = tw * sc, dh = th * sc;
            const ImVec2 tl(org.x + (box.x - dw) * 0.5f, org.y + (box.y - dh) * 0.5f);

            dl->AddImage(GetImTextureID(*canvasTex), tl, ImVec2(tl.x + dw, tl.y + dh));

            // ★ Man hinh phai ve them VUNG LAY cua tung slice.
            //   Day la khac biet that giua hai o: ben trai la toan bo
            //   canvas, ben phai cho biet phan nao cua canvas THUC SU
            //   duoc chieu ra. Khong co no thi hai o giong het nhau va
            //   cai thu hai vo nghia.
            if (showSliceRects && !p.screens.empty()) {
                const Vec2 cs = p.composition.canvasSize;
                if (cs.x > 0.0 && cs.y > 0.0) {
                    for (const Screen& sc2 : p.screens) {
                        if (!sc2.enabled) continue;
                        for (int si : sc2.visibleSlices()) {
                            const Slice& sl = sc2.slices[static_cast<std::size_t>(si)];
                            const ImVec2 a0(
                                tl.x + static_cast<float>(sl.inputOrigin.x / cs.x) * dw,
                                tl.y + static_cast<float>(sl.inputOrigin.y / cs.y) * dh);
                            const ImVec2 a1(
                                a0.x + static_cast<float>(sl.inputSize.x / cs.x) * dw,
                                a0.y + static_cast<float>(sl.inputSize.y / cs.y) * dh);
                            dl->AddRect(a0, a1, theme::alpha(theme::Primary, 0.9f), 0.0f, 0, 1.5f);
                            if (!sl.name.empty()) {
                                dl->AddText(ImVec2(a0.x + 4.0f, a0.y + 2.0f),
                                            theme::alpha(theme::Primary, 0.9f), sl.name.c_str());
                            }
                        }
                    }
                }
            }
        } else {
            const char* msg = TR("preview.nocontent");
            const ImVec2 tsz = ImGui::CalcTextSize(msg);
            dl->AddText(ImVec2(org.x + (box.x - tsz.x) * 0.5f,
                               org.y + (box.y - tsz.y) * 0.5f),
                        theme::TextFaint, msg);
        }

        dl->AddRect(org, ImVec2(org.x + box.x, org.y + box.y),
                    showSliceRects ? theme::alpha(theme::Primary, 0.45f) : theme::Border,
                    4.0f);
        ImGui::EndChild();
    };

    char fps[32];
    std::snprintf(fps, sizeof(fps), "%.1f FPS", s.fps);

    monitor("##mon_preview", TR("mon.preview"), theme::TextDim, false, nullptr, 0);
    ImGui::SameLine();
    monitor("##mon_active", TR("mon.active"), theme::Primary, true, fps,
            (s.fps >= 55.0) ? theme::Success : theme::Warning);
}

// ── Bang thuoc tinh ────────────────────────────────────────────────────

void ControlPanel::drawInspector(Project& p) {
    ImGui::TextUnformatted(TR("inspector.title"));
    ImGui::Separator();
    drawClipPanel(p);
}

// ── Luoi layer × cot ───────────────────────────────────────────────────

void ControlPanel::drawLayersDeck(Project& p) {
    Composition& comp = p.composition;

    // ── Thanh cong cu deck ─────────────────────────────────────────────
    ImGui::AlignTextToFramePadding();
    ImGui::TextUnformatted(TR("comp.deck"));
    for (int d = 0; d < comp.deckCount(); ++d) {
        ImGui::SameLine(0.0f, 6.0f);
        const bool act = (d == comp.viewedDeck());
        if (theme::tabButton(comp.deck(d).name.c_str(), act, ImVec2(76, 0))) {
            comp.setViewedDeck(d);   // A9 — chi doi deck dang XEM
        }
    }

    ImGui::SameLine(0.0f, 18.0f);
    if (theme::outlineButton(TR("comp.clearall"), theme::Danger, ImVec2(90, 0))) {
        comp.clearAll();
    }
    ImGui::SameLine(0.0f, 14.0f);
    ImGui::AlignTextToFramePadding();
    ImGui::TextDisabled("%s", TR("comp.hint"));

    const int cols   = comp.columnCount();
    const int layers = comp.layerCount();
    if (cols <= 0 || layers <= 0) {
        ImGui::TextDisabled("%s", TR("comp.empty"));
        return;
    }

    // ── Bang: cot layer DINH ben trai, hang tieu de DINH tren ──────────
    //
    // ★ Dung ImGui table voi ScrollFreeze thay vi tu cuon tung hang.
    //   Neu moi hang tu cuon rieng, keo hang nay thi hang kia dung yen va
    //   cac o clip lech cot nhau — luoi mat het y nghia.
    const ImGuiTableFlags tf = ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY
                             | ImGuiTableFlags_BordersInnerV
                             | ImGuiTableFlags_SizingFixedFit;

    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(2, 2));
    if (ImGui::BeginTable("##deckgrid", cols + 1, tf)) {
        ImGui::TableSetupColumn("##layer", ImGuiTableColumnFlags_WidthFixed,
                                theme::LayerCtrlW);
        for (int c = 0; c < cols; ++c) {
            char id[24];
            std::snprintf(id, sizeof(id), "##col%d", c);
            ImGui::TableSetupColumn(id, ImGuiTableColumnFlags_WidthFixed, theme::ClipW);
        }
        ImGui::TableSetupScrollFreeze(1, 1);

        // Hang tieu de: bam so cot = phat ca cot (A6).
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers, 22.0f);
        ImGui::TableSetColumnIndex(0);
        for (int c = 0; c < cols; ++c) {
            ImGui::TableSetColumnIndex(c + 1);
            ImGui::PushID(20000 + c);
            char lbl[24];
            std::snprintf(lbl, sizeof(lbl), "COL %d", c + 1);
            if (theme::tabButton(lbl, false, ImVec2(-FLT_MIN, 18.0f), theme::Primary)) {
                comp.triggerColumn(c);
            }
            if (ImGui::IsItemHovered()) ImGui::SetTooltip(TR("comp.triggercol"), c + 1);
            ImGui::PopID();
        }

        // Layer ve tu TREN xuong: chi so cao = tren cung, giong Resolume.
        for (int L = layers - 1; L >= 0; --L) {
            Layer& layer = comp.layer(L);
            ImGui::TableNextRow(ImGuiTableRowFlags_None, theme::LayerRowH);
            ImGui::PushID(3000 + L);

            // ── Cot dieu khien layer ───────────────────────────────────
            ImGui::TableSetColumnIndex(0);
            {
                // ★ Ten CAT NGAN ve be rong co dinh, roi ba nut dat o mot
                //   moc CO DINH tinh tu dau o.
                //
                //   Khong dung GetContentRegionAvail() o day: trong mot o
                //   cua ImGui table no KHONG tra ve be rong cua o, va ba
                //   nut bi day ra ngoai cot roi bi cat mat — nhin thi
                //   tuong cot qua hep, that ra la phep tinh sai.
                char nameBuf[16];
                std::snprintf(nameBuf, sizeof(nameBuf), "%-9.9s", layer.name.c_str());

                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(nameBuf);

                constexpr float kBtnBlockW = 24.0f * 3.0f + 3.0f * 2.0f;
                constexpr float kBtnAnchor = 132.0f;   // < LayerCtrlW - kBtnBlockW
                ImGui::SameLine(0.0f, std::max(6.0f,
                    kBtnAnchor - ImGui::CalcTextSize(nameBuf).x));

                const ImVec2 bs(24.0f, 20.0f);
                if (theme::tabButton("S", layer.solo, bs, theme::Info)) layer.solo = !layer.solo;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.solo.tip"));
                ImGui::SameLine(0.0f, 3.0f);
                if (theme::tabButton("B", layer.bypass, bs, theme::Danger)) layer.bypass = !layer.bypass;
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.bypass.tip"));
                ImGui::SameLine(0.0f, 3.0f);
                if (theme::tabButton("X", false, bs, theme::Danger)) layer.clear();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.clear.tip"));

                // ★ Model luu 0..1 nhung nguoi van hanh nghi bang PHAN TRAM.
                //   Dua thang gia tri 0..1 vao dinh dang "%.0f%%" cho ra
                //   "1%" khi opacity = 1.0 — sai hoan toan va rat de tin.
                float opPct = static_cast<float>(layer.opacity * 100.0);
                ImGui::SetNextItemWidth(theme::LayerCtrlW - 8.0f);
                if (ImGui::SliderFloat("##op", &opPct, 0.0f, 100.0f, "OPACITY  %.0f%%",
                                       ImGuiSliderFlags_AlwaysClamp)) {
                    layer.opacity = opPct / 100.0;
                }

                ImGui::SetNextItemWidth((theme::LayerCtrlW - 12.0f) * 0.58f);
                int blend = static_cast<int>(layer.blend);
                if (ImGui::Combo("##blend", &blend, kBlendNames, IM_ARRAYSIZE(kBlendNames))) {
                    layer.blend = static_cast<BlendMode>(blend);
                }
                ImGui::SameLine(0.0f, 4.0f);
                ImGui::SetNextItemWidth((theme::LayerCtrlW - 12.0f) * 0.38f);
                float tr = static_cast<float>(layer.transitionDuration);
                if (ImGui::DragFloat("##tr", &tr, 0.01f, 0.0f, 10.0f, "%.2fs")) {
                    layer.transitionDuration = std::max(0.0f, tr);
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.transition.tip"));
            }

            // ── Cac o clip ─────────────────────────────────────────────
            for (int c = 0; c < cols; ++c) {
                ImGui::TableSetColumnIndex(c + 1);
                ImGui::PushID(c);

                const Clip& clip = comp.deck(comp.viewedDeck()).clip(L, c);
                const bool playing  = (layer.activeDeck == comp.viewedDeck()
                                       && layer.activeColumn == c);
                const bool selected = (L == m_selLayer && c == m_selColumn);
                const bool empty    = clip.isEmpty();

                ImU32 bg     = theme::alpha(theme::BgSunken, 0.9f);
                ImU32 border = theme::alpha(theme::Border, 0.8f);
                if (playing)      { bg = theme::alpha(theme::Primary, 0.16f); border = theme::Primary; }
                else if (!empty)  { bg = theme::BgCard;                       border = theme::BorderLit; }
                if (selected)     { border = theme::Warning; }

                ImGui::PushStyleColor(ImGuiCol_Button,        theme::v4(bg));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::v4(theme::alpha(theme::Info, 0.20f)));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  theme::v4(theme::alpha(theme::Info, 0.32f)));
                ImGui::PushStyleColor(ImGuiCol_Border,        theme::v4(border));

                if (ImGui::Button("##cell", ImVec2(-FLT_MIN, theme::LayerRowH - 8.0f))) {
                    // ★ Bam MOT lan = CHON, khong phat.
                    //
                    //   Truoc day bam la phat ngay. Nhung nguoi van hanh
                    //   phai chinh duoc clip SAP dung ma khong lam gian
                    //   doan clip dang chieu — bam nham mot o giua buoi
                    //   dien la khan gia thay ngay.
                    //   Phat = bam DUP, hoac bam so cot.
                    m_selLayer  = L;
                    m_selColumn = c;
                    if (empty) {
                        m_pendingBrowse = true;
                        m_browseLayer   = L;
                        m_browseColumn  = c;
                    }
                }
                if (!empty && ImGui::IsItemHovered()
                    && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
                    comp.triggerClip(L, c);
                }

                const ImVec2 r0 = ImGui::GetItemRectMin();
                const ImVec2 r1 = ImGui::GetItemRectMax();
                ImDrawList* dl = ImGui::GetWindowDrawList();

                if (empty) {
                    const ImVec2 ts = ImGui::CalcTextSize("+");
                    dl->AddText(ImVec2((r0.x + r1.x - ts.x) * 0.5f,
                                       (r0.y + r1.y - ts.y) * 0.5f),
                                theme::alpha(theme::TextFaint, 0.9f), "+");
                } else {
                    dl->PushClipRect(r0, r1, true);
                    dl->AddText(ImVec2(r0.x + 5.0f, r0.y + 4.0f), theme::Text,
                                clip.name.c_str());
                    dl->PopClipRect();

                    if (playing) {
                        // Thanh tien do: nguoi van hanh phai biet clip con
                        // bao lau de chuyen tiep dung nhip.
                        const float t = static_cast<float>(
                            std::clamp(clip.transport.position, 0.0, 1.0));
                        dl->AddRectFilled(ImVec2(r0.x, r1.y - 3.0f),
                                          ImVec2(r0.x + (r1.x - r0.x) * t, r1.y),
                                          theme::Primary);
                    }
                }

                ImGui::PopStyleColor(4);

                if (!empty && ImGui::IsItemHovered()) {
                    ImGui::SetTooltip("%s\n%s\n%s", clip.name.c_str(),
                                      clip.media.path.c_str(), TR("comp.cellhint"));
                }
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
}

// ═══════════════════════════════════════════════════════════════════════
//  TRANG 2 — ADVANCED MAPPING
// ═══════════════════════════════════════════════════════════════════════

void ControlPanel::drawMapToolbar(Project& p, EditState& edit, UiActions& a) {
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgPanel));
    ImGui::BeginChild("##maptools", ImVec2(0, theme::ToolBarH), ImGuiChildFlags_None,
                      ImGuiWindowFlags_NoScrollbar);
    {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        const ImVec2 wp = ImGui::GetWindowPos();
        dl->AddLine(ImVec2(wp.x, wp.y + theme::ToolBarH - 1.0f),
                    ImVec2(wp.x + ImGui::GetWindowWidth(), wp.y + theme::ToolBarH - 1.0f),
                    theme::Border);
    }

    ImGui::SetCursorPos(ImVec2(10.0f, (theme::ToolBarH - ImGui::GetFrameHeight()) * 0.5f));

    // ── Trai: them / xoa slice ─────────────────────────────────────────
    if (theme::outlineButton(TR("adv.addslice"), theme::Success, ImVec2(0, 0))) {
        a.addSliceToScreen = m_activeScreen;
    }
    ImGui::SameLine(0.0f, 6.0f);
    if (theme::outlineButton(TR("adv.delete"), theme::Danger, ImVec2(0, 0))) {
        a.removeSliceIndex = edit.activeSliceIndex;
    }

    ImGui::SameLine(0.0f, 14.0f);
    if (theme::tabButton(TR("map.showcontent"), m_mapShowContent, ImVec2(0, 0), theme::Info)) {
        m_mapShowContent = !m_mapShowContent;
    }
    ImGui::SameLine(0.0f, 6.0f);
    if (theme::tabButton(TR("adv.grid"), edit.showGrid, ImVec2(0, 0), theme::Info)) {
        edit.showGrid = !edit.showGrid;
    }
    ImGui::SameLine(0.0f, 6.0f);
    if (theme::tabButton(TR("mask.edit"), edit.maskEditMode, ImVec2(0, 0), theme::Success)) {
        edit.maskEditMode = !edit.maskEditMode;
        m_mapDragPoint = -1;
        edit.maskDraggedNode = -1;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("mask.edit.tip"));

    // ── Giua: cong tac INPUT / OUTPUT ──────────────────────────────────
    //
    // ★ Hai nua cua CUNG mot thao tac, nen dung chung mot khung nhin.
    //   "Lay phan nao cua hinh" (INPUT) va "dat no o dau tren vat the"
    //   (OUTPUT). Truoc day chi chinh duoc OUTPUT bang chuot, con INPUT
    //   phai go so — trong khi ca hai deu la viec keo hinh chu nhat.
    {
        constexpr float kModeW = 320.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - kModeW) * 0.5f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgSunken));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3, 3));
        ImGui::BeginChild("##mapmode", ImVec2(kModeW, theme::ToolBarH - 10.0f),
                          ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);

        const ImVec2 msz(152.0f, 0.0f);
        if (theme::tabButton(TR("map.mode.input"), m_mapMode == MapMode::Input,
                             msz, theme::Success)) {
            m_mapMode = MapMode::Input;
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (theme::tabButton(TR("map.mode.output"), m_mapMode == MapMode::Output,
                             msz, theme::Primary)) {
            m_mapMode = MapMode::Output;
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    // ── Phai: dua ra may chieu + an/hien bang ──────────────────────────
    {
        const float rightW = 330.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                      ImGui::GetWindowWidth() - rightW));

        if (theme::outlineButton(TR("menu.output.fullscreen"), theme::Primary, ImVec2(0, 0))) {
            a.toggleFullscreen = true;
        }
        ImGui::SameLine(0.0f, 6.0f);
        if (theme::outlineButton(TR("menu.output.windowed"), theme::Info, ImVec2(0, 0))) {
            a.outputWindowed = true;
        }
        ImGui::SameLine(0.0f, 6.0f);
        if (theme::tabButton("[ ]", m_showMapSidebar, ImVec2(30, 0), theme::Text)) {
            m_showMapSidebar = !m_showMapSidebar;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("map.sidebar"));
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
    (void)p;
}

void ControlPanel::drawMappingView(Project& p, EditState& edit,
                                   const ofTexture* canvasTex, UiActions& a) {
    drawMapToolbar(p, edit, a);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

    const float sideW = m_showMapSidebar ? theme::InspectorW : 0.0f;
    const float workW = std::max(200.0f,
        ImGui::GetContentRegionAvail().x - sideW - (m_showMapSidebar ? 6.0f : 0.0f));

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgApp));
    ImGui::BeginChild("##workspace", ImVec2(workW, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    {
        // Nhan cho biet dang nhin vao KHONG GIAN NAO. Hai che do trong rat
        // giong nhau — deu la hinh chu nhat tren nen toi — nen thieu nhan
        // thi rat de chinh nham khong gian.
        theme::statusDot(m_mapMode == MapMode::Input ? theme::Success : theme::Primary);
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", m_mapMode == MapMode::Input
                                  ? TR("map.mode.input.note") : TR("map.mode.output.note"));

        if (m_mapMode == MapMode::Input) drawInputEditor(p, edit, canvasTex);
        else                             drawMappingEditor(p, edit, canvasTex);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    if (m_showMapSidebar) {
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgPanel));
        ImGui::BeginChild("##sliceset", ImVec2(0, 0), ImGuiChildFlags_Borders);
        drawSliceSettings(p, edit, a);
        ImGui::EndChild();
        ImGui::PopStyleColor();
    }

    ImGui::PopStyleVar();
}

void ControlPanel::drawSliceSettings(Project& p, EditState& edit, UiActions& a) {
    ImGui::TextUnformatted(TR("slice.settings"));
    ImGui::Separator();
    drawScreenPanel(p, edit, a);
}

// ── Che do INPUT: keo vung LAY tren canvas ─────────────────────────────

void ControlPanel::drawInputEditor(Project& p, EditState& edit,
                                   const ofTexture* canvasTex) {
    if (p.screens.empty()) { ImGui::TextDisabled("%s", TR("map.noscreen")); return; }

    m_activeScreen = std::clamp(m_activeScreen, 0,
                                static_cast<int>(p.screens.size()) - 1);
    Screen& sc = p.screens[static_cast<std::size_t>(m_activeScreen)];

    const Vec2 cs = p.composition.canvasSize;
    if (cs.x <= 0.0 || cs.y <= 0.0) return;

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 60.0f || avail.y < 60.0f) return;

    const float scale = std::min(avail.x * 0.92f / static_cast<float>(cs.x),
                                 avail.y * 0.92f / static_cast<float>(cs.y));
    const float dw = static_cast<float>(cs.x) * scale;
    const float dh = static_cast<float>(cs.y) * scale;

    const ImVec2 cur = ImGui::GetCursorScreenPos();
    const ImVec2 org(cur.x + (avail.x - dw) * 0.5f, cur.y + (avail.y - dh) * 0.5f);

    auto toWidget = [&](const Vec2& v) {
        return ImVec2(org.x + static_cast<float>(v.x) * scale,
                      org.y + static_cast<float>(v.y) * scale);
    };
    auto toCanvas = [&](const ImVec2& v) {
        return Vec2{(v.x - org.x) / scale, (v.y - org.y) / scale};
    };

    ImGui::InvisibleButton("##inputcanvas", avail);
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    dl->AddRectFilled(org, ImVec2(org.x + dw, org.y + dh), IM_COL32(10, 10, 12, 255));

    if (canvasTex != nullptr && canvasTex->isAllocated()) {
        dl->AddImage(GetImTextureID(*canvasTex), org, ImVec2(org.x + dw, org.y + dh));
    }
    dl->AddRect(org, ImVec2(org.x + dw, org.y + dh), theme::BorderLit);

    // ── Vung lay cua tung slice ────────────────────────────────────────
    const int n = sc.sliceCount();
    int hitCorner = -1;   // 0 = goc tren-trai, 1 = goc duoi-phai, 2 = ca khoi
    int hitSlice  = -1;

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    constexpr float kGrab = 11.0f;

    for (int i = 0; i < n; ++i) {
        const Slice& s = sc.slices[static_cast<std::size_t>(i)];
        const ImVec2 a0 = toWidget(s.inputOrigin);
        const ImVec2 a1 = toWidget(Vec2{s.inputOrigin.x + s.inputSize.x,
                                        s.inputOrigin.y + s.inputSize.y});

        const bool sel = (i == edit.activeSliceIndex);
        const ImU32 col = sel ? theme::Success : theme::alpha(theme::Info, 0.75f);

        dl->AddRectFilled(a0, a1, theme::alpha(col, sel ? 0.14f : 0.07f));
        dl->AddRect(a0, a1, col, 0.0f, 0, sel ? 2.0f : 1.0f);
        if (!s.name.empty()) {
            dl->AddText(ImVec2(a0.x + 5.0f, a0.y - 16.0f), col, s.name.c_str());
        }

        if (!sel || !(hovered || active)) continue;

        // Hai tay nam goc + than khoi. Du de keo va thay doi kich thuoc
        // ma khong can 8 tay nam nhu trinh sua anh — o day chi can vung
        // chu nhat, khong xoay.
        dl->AddRectFilled(ImVec2(a0.x - 4, a0.y - 4), ImVec2(a0.x + 4, a0.y + 4), col);
        dl->AddRectFilled(ImVec2(a1.x - 4, a1.y - 4), ImVec2(a1.x + 4, a1.y + 4), col);

        // Ten `near` KHONG dung duoc: windows.h dinh nghia no thanh macro
        // rong, va loi bao ra la "auto: no variable declared" — hoan toan
        // khong goi y gi den nguyen nhan that.
        auto grabbed = [&](const ImVec2& q) {
            return std::abs(q.x - mouse.x) < kGrab && std::abs(q.y - mouse.y) < kGrab;
        };
        if      (grabbed(a0)) { hitCorner = 0; hitSlice = i; }
        else if (grabbed(a1)) { hitCorner = 1; hitSlice = i; }
        else if (mouse.x > a0.x && mouse.x < a1.x
                 && mouse.y > a0.y && mouse.y < a1.y) { hitCorner = 2; hitSlice = i; }
    }

    // ── Tuong tac ──────────────────────────────────────────────────────
    if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if (hitSlice >= 0) {
            m_inputDragPart = hitCorner;
        } else {
            m_inputDragPart = -1;
            // Bam vao cho trong: chon slice co vung lay chua diem do.
            const Vec2 cp = toCanvas(mouse);
            for (int i = n - 1; i >= 0; --i) {
                const Slice& s = sc.slices[static_cast<std::size_t>(i)];
                if (cp.x >= s.inputOrigin.x && cp.x <= s.inputOrigin.x + s.inputSize.x
                    && cp.y >= s.inputOrigin.y && cp.y <= s.inputOrigin.y + s.inputSize.y) {
                    edit.activeSliceIndex = i;
                    break;
                }
            }
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) m_inputDragPart = -1;

    if (m_inputDragPart >= 0 && edit.activeSliceIndex >= 0
        && edit.activeSliceIndex < n
        && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {

        Slice& s = sc.slices[static_cast<std::size_t>(edit.activeSliceIndex)];
        const ImVec2 d = ImGui::GetIO().MouseDelta;
        const Vec2 dv{d.x / scale, d.y / scale};

        if (m_inputDragPart == 2) {
            s.inputOrigin.x += dv.x;
            s.inputOrigin.y += dv.y;
        } else if (m_inputDragPart == 0) {
            // Keo goc tren-trai: goc duoi-phai dung yen, nen kich thuoc
            // phai bu lai. Chan o 8px de vung khong bi bop ve 0 roi bien
            // mat khoi khung nhin.
            const double nx = std::min(s.inputOrigin.x + dv.x,
                                       s.inputOrigin.x + s.inputSize.x - 8.0);
            const double ny = std::min(s.inputOrigin.y + dv.y,
                                       s.inputOrigin.y + s.inputSize.y - 8.0);
            s.inputSize.x += s.inputOrigin.x - nx;
            s.inputSize.y += s.inputOrigin.y - ny;
            s.inputOrigin  = Vec2{nx, ny};
        } else {
            s.inputSize.x = std::max(8.0, s.inputSize.x + dv.x);
            s.inputSize.y = std::max(8.0, s.inputSize.y + dv.y);
        }
    }

    ImGui::SetCursorScreenPos(ImVec2(org.x, org.y + dh + 6.0f));
    ImGui::TextDisabled("%s", TR("map.input.hint"));
}

// ═══════════════════════════════════════════════════════════════════════
//  TRANG 3 — SENSOR I/O
// ═══════════════════════════════════════════════════════════════════════

void ControlPanel::drawSensorView(Project& p, const PerfStats& s, UiActions& a) {
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

    // ── Trai: quan ly thiet bi ─────────────────────────────────────────
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgPanel));
    ImGui::BeginChild("##devices", ImVec2(300.0f, 0), ImGuiChildFlags_Borders);
    {
        ImGui::TextUnformatted(TR("sen.devices"));
        ImGui::Separator();
        drawSensorPanel(p, s, a);

        ImGui::Spacing();
        theme::sectionLabel(TR("perf.title"));
        drawPerfPanel(s);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Giua: khung nhin diem cham ─────────────────────────────────────
    ImGui::SameLine();
    const float rightW = 340.0f;
    const float midW = std::max(200.0f,
        ImGui::GetContentRegionAvail().x - rightW - 6.0f);

    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgSunken));
    ImGui::BeginChild("##radar", ImVec2(midW, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    {
        // So diem cham da hien o bang THIET BI ben trai; lap lai o day chi
        // them nhieu. Cai khung nay can la TRANG THAI dang chay hay khong.
        theme::statusDot(s.sensorConnected ? theme::Success : theme::TextFaint);
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", s.sensorConnected ? TR("sen.running") : TR("sen.stopped"));

        const ImVec2 avail = ImGui::GetContentRegionAvail();
        if (avail.x > 60.0f && avail.y > 60.0f && !p.screens.empty()) {
            const Screen& sc = p.screens[static_cast<std::size_t>(
                std::clamp(m_activeScreen, 0, static_cast<int>(p.screens.size()) - 1))];

            const float rw = static_cast<float>(std::max(1.0, sc.resolution.x));
            const float rh = static_cast<float>(std::max(1.0, sc.resolution.y));
            const float k = std::min(avail.x * 0.94f / rw, avail.y * 0.94f / rh);
            const float dw = rw * k, dh = rh * k;

            const ImVec2 cur = ImGui::GetCursorScreenPos();
            const ImVec2 org(cur.x + (avail.x - dw) * 0.5f, cur.y + (avail.y - dh) * 0.5f);
            ImDrawList* dl = ImGui::GetWindowDrawList();

            dl->AddRectFilled(org, ImVec2(org.x + dw, org.y + dh), IM_COL32(6, 6, 8, 255));

            // Luoi dan huong — de uoc luong vi tri diem cham bang mat.
            for (int i = 1; i < 8; ++i) {
                const float t = static_cast<float>(i) / 8.0f;
                dl->AddLine(ImVec2(org.x + dw * t, org.y),
                            ImVec2(org.x + dw * t, org.y + dh),
                            theme::alpha(theme::Info, 0.13f));
                dl->AddLine(ImVec2(org.x, org.y + dh * t),
                            ImVec2(org.x + dw, org.y + dh * t),
                            theme::alpha(theme::Info, 0.13f));
            }
            dl->AddRect(org, ImVec2(org.x + dw, org.y + dh), theme::Border);

            // Vien tung slice.
            for (int i = 0; i < sc.sliceCount(); ++i) {
                const Slice& sl = sc.slices[static_cast<std::size_t>(i)];
                const IWarp* w = sl.warp();
                if (w == nullptr) continue;
                ImVec2 q[4];
                const Vec2 uv[4] = {{0, 0}, {1, 0}, {1, 1}, {0, 1}};
                for (int c = 0; c < 4; ++c) {
                    const Vec2 o = w->forward(uv[c]);
                    q[c] = ImVec2(org.x + static_cast<float>(o.x) * k,
                                  org.y + static_cast<float>(o.y) * k);
                }
                dl->AddPolyline(q, 4, theme::alpha(theme::Primary, 0.55f),
                                ImDrawFlags_Closed, 1.5f);
            }

            // Diem cham da anh xa — day la bang chung calibration dung
            // hay sai: cham vao vat the, cham phai roi dung cho tay minh.
            for (const Vec2& v : m_sensorPoints) {
                const ImVec2 q(org.x + static_cast<float>(v.x) * k,
                               org.y + static_cast<float>(v.y) * k);
                dl->AddCircleFilled(q, 5.0f, theme::alpha(theme::Success, 0.85f));
                dl->AddCircle(q, 11.0f, theme::Success, 0, 1.5f);
            }

            ImGui::Dummy(avail);
        }
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    // ── Phai: calibration + vung cam ung ───────────────────────────────
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgPanel));
    ImGui::BeginChild("##calib", ImVec2(0, 0), ImGuiChildFlags_Borders);
    {
        ImGui::TextUnformatted(TR("cal.section"));
        ImGui::Separator();
        drawCalibrationPanel(p, s, a);

        ImGui::Spacing();
        ImGui::Separator();
        drawTriggerZonePanel(p, a);
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();

    ImGui::PopStyleVar();
}

// ── F1 F3 F5 F15 F16: screen & slice ───────────────────────────────────

void ControlPanel::drawScreenPanel(Project& p, EditState& edit, UiActions& a) {
    if (p.screens.empty()) { ImGui::TextDisabled("%s", TR("map.noscreen")); return; }

    m_activeScreen = std::clamp(m_activeScreen, 0, static_cast<int>(p.screens.size()) - 1);

    // ★ Bang nay rong 280px. Moi dong PHAI vua trong do.
    //
    //   ImGui khong cat bot noi dung tran ra: no NOI RONG vung noi dung
    //   cua child. Hau qua khong hien nhien chut nao — moi widget dat be
    //   rong bang "-FLT_MIN" (het cho con lai) se an theo be rong da noi
    //   ay va thanh ra rong hon khung nhin, tuc la CA BANG bi cat, khong
    //   rieng dong gay ra chuyen do.
    for (size_t i = 0; i < p.screens.size(); ++i) {
        const bool active = (static_cast<int>(i) == m_activeScreen);
        ImGui::PushID(static_cast<int>(i));
        if (theme::tabButton(p.screens[i].name.c_str(), active,
                             ImVec2(-FLT_MIN, 0), theme::Info)) {
            m_activeScreen = static_cast<int>(i);
        }
        ImGui::PopID();
    }

    Screen& sc = p.screens[static_cast<size_t>(m_activeScreen)];
    ImGui::Separator();

    ImGui::Checkbox(TR("adv.overlay"), &edit.showOverlay);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TR("adv.overlay.tip"));
    }

    if (theme::outlineButton(TR("adv.addslice"), theme::Success, ImVec2(-FLT_MIN, 0))) {
        a.addSliceToScreen = m_activeScreen;
    }

    ImGui::Separator();
    theme::sectionLabel(TR("adv.slicelist"));

    for (int i = 0; i < sc.sliceCount(); ++i) {
        Slice& s = sc.slices[static_cast<size_t>(i)];
        ImGui::PushID(3000 + i);

        // Hang 1: chon + ten. Hang 2: bat/solo/xoa. Nhoi ca sau thu vao
        // mot hang la thu da lam bang tran ngang.
        const bool selected = (i == edit.activeSliceIndex);
        if (ImGui::RadioButton("##sel", selected)) edit.activeSliceIndex = i;
        ImGui::SameLine();

        char nameBuf[64];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", s.name.c_str());
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) s.name = nameBuf;

        ImGui::Indent(22.0f);
        ImGui::Checkbox(TR("common.on"), &s.enabled);
        ImGui::SameLine();
        ImGui::Checkbox(TR("adv.solo"), &s.solo);
        ImGui::SameLine();
        if (ImGui::SmallButton(TR("adv.delete"))) a.removeSliceIndex = i;
        ImGui::Unindent(22.0f);

        if (!s.warp()->isInvertible()) {
            ImGui::PushStyleColor(ImGuiCol_Text, theme::v4(theme::Danger));
            ImGui::TextWrapped("%s", TR("adv.broken"));
            ImGui::PopStyleColor();
        }
        ImGui::PopID();
    }

    if (edit.activeSliceIndex < 0 || edit.activeSliceIndex >= sc.sliceCount()) {
        ImGui::TextDisabled("%s", TR("adv.noslice"));
        return;
    }

    Slice& s = sc.slices[static_cast<size_t>(edit.activeSliceIndex)];
    ImGui::Separator();
    ImGui::TextWrapped(TR("adv.selected"), s.name.c_str());

    // F4 — vùng lấy trên canvas
    float ox[2] = {static_cast<float>(s.inputOrigin.x), static_cast<float>(s.inputOrigin.y)};
    labelAbove(TR("adv.inputorigin"));
    if (ImGui::DragFloat2("##inorg", ox, 1.0f)) {
        s.inputOrigin = Vec2{ox[0], ox[1]};
    }
    float sz[2] = {static_cast<float>(s.inputSize.x), static_cast<float>(s.inputSize.y)};
    labelAbove(TR("adv.inputsize"));
    if (ImGui::DragFloat2("##insz", sz, 1.0f, 1.0f, 16384.0f)) {
        s.inputSize = Vec2{std::max(1.0f, sz[0]), std::max(1.0f, sz[1])};
    }

    // Đổi loại warp
    int warpType = (s.warp()->type() == WarpType::Mesh) ? 1 : 0;
    const char* warpNames[] = {"Corner pin", "Mesh"};
    labelAbove(TR("adv.warptype"));
    if (ImGui::Combo("##warptype", &warpType, warpNames, 2)) {
        a.convertWarpTo = warpType;
    }
    if (ImGui::Button(TR("adv.reset"), ImVec2(-FLT_MIN, 0))) a.resetActiveSliceWarp = true;

    // ── F19: hieu chinh mau rieng cho slice ────────────────────────────
    if (ImGui::TreeNode(TR("adv.color"))) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", TR("adv.color.note"));
        ImGui::PopTextWrapPos();

        float br = static_cast<float>(s.color.brightness);
        labelAbove(TR("adv.color.brightness"));
        if (ImGui::SliderFloat("##br", &br, -1.0f, 1.0f)) s.color.brightness = br;

        float ct = static_cast<float>(s.color.contrast);
        labelAbove(TR("adv.color.contrast"));
        if (ImGui::SliderFloat("##ct", &ct, 0.0f, 3.0f)) s.color.contrast = ct;

        float gm = static_cast<float>(s.color.gamma);
        labelAbove(TR("adv.color.gamma"));
        if (ImGui::SliderFloat("##gm", &gm, 0.1f, 4.0f)) s.color.gamma = gm;

        float gain[3] = {static_cast<float>(s.color.gainR),
                         static_cast<float>(s.color.gainG),
                         static_cast<float>(s.color.gainB)};
        labelAbove(TR("adv.color.rgb"));
        if (ImGui::ColorEdit3("##rgb", gain,
                              ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)) {
            s.color.gainR = gain[0];
            s.color.gainG = gain[1];
            s.color.gainB = gain[2];
        }

        float op = static_cast<float>(s.color.opacity);
        labelAbove(TR("adv.color.opacity"));
        if (ImGui::SliderFloat("##sop", &op, 0.0f, 1.0f)) s.color.opacity = op;

        if (ImGui::Button(TR("adv.color.reset"))) s.color.reset();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", s.color.isIdentity() ? TR("adv.color.default")
                                                      : TR("adv.color.edited"));

        ImGui::TreePop();
    }

    // ── F12: mat na bezier ────────────────────────────────────────────
    if (ImGui::TreeNode(TR("mask.title"))) {
        drawMaskPanel(s, edit);
        ImGui::TreePop();
    }

    // ── F20: hoa vien de ghep nhieu may chieu ─────────────────────────
    if (ImGui::TreeNode(TR("adv.softedge"))) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled(TR("adv.edge.note1"));
        ImGui::TextDisabled(TR("adv.edge.note2"));
        ImGui::PopTextWrapPos();

        float e[4] = {static_cast<float>(s.softEdge.left),
                      static_cast<float>(s.softEdge.right),
                      static_cast<float>(s.softEdge.top),
                      static_cast<float>(s.softEdge.bottom)};
        bool changed = false;
        labelAbove(TR("adv.edge.left"));
        changed |= ImGui::SliderFloat("##el", &e[0], 0.0f, 0.5f, "%.3f");
        labelAbove(TR("adv.edge.right"));
        changed |= ImGui::SliderFloat("##er", &e[1], 0.0f, 0.5f, "%.3f");
        labelAbove(TR("adv.edge.top"));
        changed |= ImGui::SliderFloat("##et", &e[2], 0.0f, 0.5f, "%.3f");
        labelAbove(TR("adv.edge.bottom"));
        changed |= ImGui::SliderFloat("##eb", &e[3], 0.0f, 0.5f, "%.3f");
        if (changed) {
            s.softEdge.left = e[0]; s.softEdge.right = e[1];
            s.softEdge.top  = e[2]; s.softEdge.bottom = e[3];
        }

        float g = static_cast<float>(s.softEdge.gamma);
        labelAbove(TR("adv.edge.gamma"));
        if (ImGui::SliderFloat("##eg", &g, 0.2f, 4.0f)) s.softEdge.gamma = g;
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TR("adv.edge.gamma.tip"));
        }

        float lum = static_cast<float>(s.softEdge.luminance);
        labelAbove(TR("adv.edge.mid"));
        if (ImGui::SliderFloat("##em", &lum, 0.0f, 1.0f)) s.softEdge.luminance = lum;

        if (ImGui::Button(TR("adv.edge.reset"))) s.softEdge.reset();
        ImGui::SameLine();
        ImGui::TextDisabled("%s", s.softEdge.isIdentity() ? TR("adv.edge.off")
                                                        : TR("adv.edge.on"));

        ImGui::TreePop();
    }

    // F15 — nhập toạ độ góc bằng SỐ, không chỉ kéo chuột.
    // Cần thiết khi căn chính xác theo bản vẽ, hoặc khi máy chiếu ở xa
    // không với tay tới được.
    if (s.warp()->type() == WarpType::CornerPin) {
        auto* cp = static_cast<WarpCornerPin*>(s.warp());
        ImGui::Text("%s", TR("adv.corners"));
        const char* cornerNames[4] = {TR("adv.corner.tl"), TR("adv.corner.tr"),
                                      TR("adv.corner.br"), TR("adv.corner.bl")};
        for (int k = 0; k < 4; ++k) {
            ImGui::PushID(4000 + k);
            float c[2] = {static_cast<float>(cp->corner(k).x),
                          static_cast<float>(cp->corner(k).y)};
                labelAbove(cornerNames[k]);
            if (ImGui::DragFloat2("##corner", c, 0.5f)) {
                if (!cp->setCorner(k, Vec2{c[0], c[1]})) {
                    // Bị từ chối vì tứ giác sẽ lõm hoặc tự cắt.
                    setStatusMessage(TR("adv.corner.rejected"), true);
                }
            }
            ImGui::PopID();
        }
    } else {
        auto* mesh = static_cast<WarpMesh*>(s.warp());
        int dims[2] = {mesh->cols(), mesh->rows()};
        labelAbove(TR("adv.meshgrid"));
        if (ImGui::DragInt2("##meshdim", dims, 0.2f, 1, 32)) {
            mesh->resize(dims[0], dims[1]);   // F11 — giữ nguyên hình đã kéo
        }
        ImGui::TextDisabled("%s", TR("adv.mesh.hint"));
    }
}

// ── F12: bang mat na bezier ────────────────────────────────────────────

void ControlPanel::drawMaskPanel(Slice& s, EditState& edit) {
    BezierMask& m = s.mask;

    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextDisabled("%s", TR("mask.note"));
    ImGui::PopTextWrapPos();

    if (ImGui::Checkbox(TR("mask.enabled"), &m.enabled)) {
        // Bat mat na khi chua co hinh thi khong thay gi thay doi va nguoi
        // dung tuong phan mem hong. Dung san hinh chu nhat de co cai ma keo.
        if (m.enabled && m.nodes.size() < 3) {
            const bool wasInvert = m.invert;
            const double wasFeather = m.feather;
            m = BezierMask::rectangle(0.10);
            m.invert  = wasInvert;
            m.feather = wasFeather;
            edit.maskEditMode = true;
        }
    }

    ImGui::SameLine();
    if (ImGui::Checkbox(TR("mask.edit"), &edit.maskEditMode)) {
        // Vao che do sua thi bo chon diem keystone dang keo do, neu khong
        // cu keo tiep theo se di chuyen goc keystone thay vi nut mat na.
        m_mapDragPoint = -1;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("mask.edit.tip"));

    if (ImGui::Checkbox(TR("mask.invert"), &m.invert)) {}

    float f = static_cast<float>(m.feather);
    labelAbove(TR("mask.feather"));
    if (ImGui::SliderFloat("##feather", &f, 0.0f, 0.5f, "%.3f")) {
        m.feather = f;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("mask.feather.tip"));

    ImGui::Separator();

    if (ImGui::Button(TR("mask.shape.rect"), ImVec2(-FLT_MIN, 0))) {
        const bool inv = m.invert; const double ft = m.feather;
        m = BezierMask::rectangle(0.10);
        m.invert = inv; m.feather = ft;
        edit.maskEditMode = true;
    }
    if (ImGui::Button(TR("mask.shape.ellipse"), ImVec2(-FLT_MIN, 0))) {
        const bool inv = m.invert; const double ft = m.feather;
        m = BezierMask::ellipse(4);
        m.invert = inv; m.feather = ft;
        edit.maskEditMode = true;
    }
    if (ImGui::Button(TR("mask.clear"), ImVec2(-FLT_MIN, 0))) {
        m.reset();
        edit.maskEditMode = false;
        edit.maskDraggedNode = -1;
        edit.maskHoveredNode = -1;
    }

    if (m.nodes.empty()) {
        ImGui::TextDisabled("%s", TR("mask.empty"));
    } else {
        ImGui::Text(TR("mask.nodes"), static_cast<int>(m.nodes.size()));
        if (m.nodes.size() < 3) {
            ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "%s", TR("mask.tooFew"));
        }
    }

    if (edit.maskEditMode) {
        ImGui::PushTextWrapPos(0.0f);
        ImGui::TextDisabled("%s", TR("mask.hint"));
        ImGui::TextDisabled("%s", TR("mask.hint.handle"));
        ImGui::PopTextWrapPos();
    }
}

// ── G4 G9 G13: sensor ──────────────────────────────────────────────────

void ControlPanel::drawSensorPanel(Project& p, const PerfStats& s, UiActions& a) {
    (void)p;

    ImGui::Text("%s", TR("menu.sensor"));

    static int sensorType = 0;
    // Cot nay chi rong 300px: nhan len tren, nut chiem het be ngang.
    // Xep ngang ca combo + nhan + nut + trang thai thi dong bi cat.
    const char* types[] = {TR("sen.type.mock"), TR("sen.type.osc")};
    labelAbove(TR("sen.source"));
    if (ImGui::Combo("##sensrc", &sensorType, types, 2)) a.sensorTypeIndex = sensorType;

    if (s.sensorConnected) {
        if (theme::outlineButton(TR("sen.stop"), theme::Danger, ImVec2(-FLT_MIN, 0))) {
            a.stopSensor = true;
        }
        theme::statusDot(theme::Success);
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(theme::v4(theme::Success), "%s", TR("sen.running"));
    } else {
        if (theme::outlineButton(TR("sen.run"), theme::Success, ImVec2(-FLT_MIN, 0))) {
            a.startSensor = true;
        }
        theme::statusDot(theme::TextFaint, false);
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextDisabled("%s", TR("sen.stopped"));
    }

    ImGui::Text(TR("sen.points"), s.touchCount, s.trackedCount);
    ImGui::TextDisabled(TR("sen.filter"),
                       s.filterEnabled ? TR("common.on.caps") : TR("common.off.caps"));
}

// ── G6 G10: wizard calibration ─────────────────────────────────────────

void ControlPanel::drawCalibrationPanel(Project& p, const PerfStats& s, UiActions& a) {
    (void)s;

    if (p.calibrations.empty()) {
        ImGui::TextDisabled("%s", TR("cal.none"));
        return;
    }

    m_selectedCalib = std::clamp(m_selectedCalib, 0,
                                 static_cast<int>(p.calibrations.size()) - 1);
    CalibrationProfile& cal = p.calibrations[static_cast<size_t>(m_selectedCalib)];

    ImGui::Text(TR("cal.title"), cal.name.c_str());

    // Hướng dẫn từng bước — người vận hành không cần đọc tài liệu.
    m_calibTarget = std::clamp(m_calibTarget, 0, 3);
    ImGui::PushTextWrapPos(0.0f);
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                       TR("cal.step"), m_calibTarget + 1);
    ImGui::PopTextWrapPos();
    ImGui::TextDisabled(TR("cal.target"),
                        kCalibTargets[m_calibTarget].x, kCalibTargets[m_calibTarget].y);

    // ★ G6 — khong co dau thap thi wizard vo dung: nguoi van hanh khong
    //   biet phai cham vao dau tren vat the that.
    ImGui::Checkbox(TR("cal.crosshair"), &m_calibShowTarget);
    a.calibShowTarget = m_calibShowTarget;

    if (ImGui::Button(TR("cal.auto"))) a.calibAutoMock = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TR("cal.auto.tip"));
    }
    ImGui::Separator();

    if (ImGui::Button(TR("cal.add"))) a.calibAddPoint = true;
    ImGui::SameLine();
    if (ImGui::Button(TR("cal.skip"))) a.calibNextTarget = true;
    ImGui::SameLine();
    if (ImGui::Button(TR("cal.solve"))) a.calibSolve = true;
    ImGui::SameLine();
    if (ImGui::Button(TR("cal.clear"))) a.calibClear = true;

    ImGui::Text(TR("cal.recorded"),
                static_cast<int>(cal.pairCount()),
                static_cast<int>(cal.enabledPairCount()));

    // ★ G10 — sai số phải hiện ra. Không có nó, người vận hành thấy hiệu
    //   ứng lệch chỗ mà đi chỉnh keystone (sai chỗ) thay vì calibrate lại.
    if (cal.isValid()) {
        const bool good = cal.isAccurate(3.0);
        ImGui::TextColored(good ? ImVec4(0.4f, 1.0f, 0.5f, 1.0f)
                                : ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                           TR("cal.error"),
                           cal.rmsError(), cal.maxError(),
                           good ? TR("cal.good") : TR("cal.bad"));
        ImGui::Text(TR("cal.inlier"), cal.inlierCount(),
                    static_cast<int>(cal.enabledPairCount()));
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "%s", TR("cal.notyet"));
        if (!cal.message().empty()) ImGui::TextWrapped("%s", cal.message().c_str());
    }

    // Danh sách điểm — tắt được điểm xấu mà không phải chạm lại từ đầu.
    if (ImGui::TreeNode(TR("cal.points"))) {
        const auto& flags = cal.outlierFlags();
        for (size_t i = 0; i < cal.pairCount(); ++i) {
            ImGui::PushID(static_cast<int>(5000 + i));
            bool en = cal.pairs()[i].enabled;
            if (ImGui::Checkbox("##en", &en)) cal.setPairEnabled(i, en);
            ImGui::SameLine();

            const bool isOutlier = (i < flags.size() && flags[i]);
            if (isOutlier) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::Text(TR("cal.pair"),
                        static_cast<int>(i),
                        cal.pairs()[i].src.x, cal.pairs()[i].src.y,
                        cal.pairs()[i].dst.x, cal.pairs()[i].dst.y,
                        isOutlier ? TR("cal.outlier") : "");
            if (isOutlier) ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::TreePop();
    }
}

// ── G9: PerfPanel ──────────────────────────────────────────────────────

void ControlPanel::drawPerfPanel(const PerfStats& s) {
    const bool ok60 = (s.frameP99Ms > 0.0 && s.frameP99Ms <= 16.7);

    ImGui::TextColored(ok60 ? ImVec4(0.4f, 1.0f, 0.5f, 1.0f)
                            : ImVec4(1.0f, 0.6f, 0.3f, 1.0f),
                       "FPS %.1f", s.fps);
    ImGui::Text(TR("perf.frame"), s.frameAvgMs, s.frameP99Ms);
    ImGui::Separator();

    // ★ architecture.md §10.6 — do, khong doan.
    if (s.sensorConnected) {
        ImGui::Text("%s", TR("perf.latency"));
        ImGui::Text("  avg %.2f ms   p99 %.2f ms",
                    s.sensorLatencyAvgMs, s.sensorLatencyP99Ms);
        ImGui::TextDisabled("%s", TR("perf.latency.note"));
        ImGui::TextDisabled("%s", TR("perf.latency.note2"));
    } else {
        ImGui::TextDisabled("%s", TR("perf.nosensor"));
    }

    ImGui::Separator();
    ImGui::Text(TR("perf.dropped"),
                static_cast<unsigned long long>(s.framesDropped));

    // Ring overflow khac 0 = render thread qua tai, su kien BI MAT.
    if (s.ringOverflow > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                           TR("perf.overflow.bad"),
                           static_cast<unsigned long long>(s.ringOverflow));
    } else {
        ImGui::Text("%s", TR("perf.overflow"));
    }

    if (s.packetsMalformed > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                           TR("perf.malformed"),
                           static_cast<unsigned long long>(s.packetsMalformed));
    }

    ImGui::Separator();
    ImGui::Text(TR("perf.drawn"), s.layersDrawn, s.slicesDrawn);
    ImGui::Text(TR("perf.media"), s.mediaLoaded, s.mediaEvictions);
    ImGui::Text(TR("perf.vram"), formatBytes(s.vramBytes).c_str());
}

// ── C3-C8 + D1-D6: thuoc tinh clip dang chon ───────────────────────────

void ControlPanel::drawClipPanel(Project& p) {
    Composition& comp = p.composition;

    if (m_selLayer < 0 || m_selColumn < 0) {
        ImGui::TextDisabled("%s", TR("clip.nosel"));
        return;
    }

    Clip* c = comp.deck(comp.viewedDeck()).clipPtr(m_selLayer, m_selColumn);
    if (c == nullptr || c->isEmpty()) {
        ImGui::TextDisabled("%s", TR("clip.empty"));
        return;
    }

    ImGui::Text(TR("clip.where"), m_selLayer + 1, m_selColumn + 1);

    char nameBuf[96];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", c->name.c_str());
    if (ImGui::InputText(TR("clip.name"), nameBuf, sizeof(nameBuf))) c->name = nameBuf;

    ImGui::TextWrapped("%s", c->media.path.c_str());
    ImGui::TextDisabled(TR("clip.mediainfo"),
                        c->media.size.x, c->media.size.y, c->media.durationSec);

    // ── C1 C8: transport ───────────────────────────────────────────────
    ImGui::SeparatorText(TR("clip.transport"));

    if (ImGui::Button(c->transport.isPlaying() ? TR("clip.pause") : TR("clip.play"))) {
        c->transport.togglePlay();
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("clip.stop"))) c->transport.stop();

    // C8 — keo tua. Chi ghi khi nguoi dung THUC SU keo; neu ghi moi frame
    // thi se de len dau phat dang chay va clip dung yen tai cho.
    float pos = static_cast<float>(c->transport.position);
    if (ImGui::SliderFloat(TR("clip.playhead"), &pos, 0.0f, 1.0f, "%.3f")) {
        c->transport.seekNormalized(pos);
    }

    // ── C4: chieu phat ─────────────────────────────────────────────────
    const char* dirNames[] = {TR("clip.dir.fwd"), TR("clip.dir.rev"), TR("clip.dir.pp")};
    int dir = static_cast<int>(c->transport.direction);
    if (ImGui::Combo(TR("clip.direction"), &dir, dirNames, 3)) {
        c->transport.direction = static_cast<PlayDirection>(dir);
    }

    // ── C5: toc do ─────────────────────────────────────────────────────
    float sp = static_cast<float>(c->transport.speed);
    if (ImGui::DragFloat(TR("clip.speed"), &sp, 0.01f, -4.0f, 4.0f, "%.2fx")) {
        c->transport.speed = sp;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("1x")) c->transport.speed = 1.0;

    // ── C6: cat dau/cuoi ───────────────────────────────────────────────
    float trim[2] = {static_cast<float>(c->transport.inPoint),
                     static_cast<float>(c->transport.outPoint)};
    if (ImGui::DragFloat2(TR("clip.inout"), trim, 0.005f, 0.0f, 1.0f, "%.3f")) {
        // setTrim tu hoan doi khi vao nguoc va giu khoang toi thieu, nen
        // keo hai handle chong len nhau khong gay chia cho 0.
        c->transport.setTrim(trim[0], trim[1]);
    }

    // ── C7: autopilot ──────────────────────────────────────────────────
    const char* endNames[] = {TR("clip.end.loop"), TR("clip.end.stop"),
                              TR("clip.end.hold"), TR("clip.end.next"),
                              TR("clip.end.rand")};
    int ea = static_cast<int>(c->transport.endAction);
    if (ImGui::Combo(TR("clip.endaction"), &ea, endNames, 5)) {
        c->transport.endAction = static_cast<EndAction>(ea);
    }

    // ── C3: kieu kich hoat ─────────────────────────────────────────────
    const char* trigNames[] = {TR("clip.trig.toggle"), TR("clip.trig.piano")};
    int ts = static_cast<int>(c->triggerStyle);
    if (ImGui::Combo(TR("clip.trigger"), &ts, trigNames, 2)) {
        c->triggerStyle = static_cast<TriggerStyle>(ts);
    }

    // ── D1-D6: bien doi ────────────────────────────────────────────────
    ImGui::SeparatorText(TR("clip.transform"));

    float posXY[2] = {static_cast<float>(c->transform.position.x),
                      static_cast<float>(c->transform.position.y)};
    if (ImGui::DragFloat2(TR("clip.position"), posXY, 1.0f)) {
        c->transform.position = Vec2{posXY[0], posXY[1]};
    }

    float scXY[2] = {static_cast<float>(c->transform.scale.x),
                     static_cast<float>(c->transform.scale.y)};
    if (ImGui::DragFloat2(TR("clip.scale"), scXY, 0.01f, -10.0f, 10.0f)) {
        c->transform.scale = Vec2{scXY[0], scXY[1]};
    }

    float rotDeg = static_cast<float>(c->transform.rotation * 180.0 / 3.14159265358979);
    if (ImGui::DragFloat(TR("clip.rotation"), &rotDeg, 0.5f, -360.0f, 360.0f, "%.1f")) {
        c->transform.rotation = rotDeg * 3.14159265358979 / 180.0;
    }

    ImGui::Checkbox(TR("clip.fliph"), &c->transform.flipH);
    ImGui::SameLine();
    ImGui::Checkbox(TR("clip.flipv"), &c->transform.flipV);

    if (ImGui::Button(TR("clip.resetxform"))) c->transform.reset();

    // ── D3 D4: hoa tron ────────────────────────────────────────────────
    ImGui::SeparatorText(TR("clip.blending"));

    float op = static_cast<float>(c->opacity);
    if (ImGui::SliderFloat(TR("clip.opacity"), &op, 0.0f, 1.0f)) c->opacity = op;

    int bl = static_cast<int>(c->blend);
    if (ImGui::Combo(TR("clip.blend"), &bl, kBlendNames, IM_ARRAYSIZE(kBlendNames))) {
        c->blend = static_cast<BlendMode>(bl);
    }
}

// ── G17: vung cam ung ──────────────────────────────────────────────────

void ControlPanel::drawTriggerZonePanel(Project& p, UiActions& a) {
    ImGui::Text(TR("zone.title"), m_triggerCount);
    if (!m_lastTriggerName.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f),
                           TR("zone.last"), m_lastTriggerName.c_str());
    }

    if (ImGui::Button(TR("zone.add"))) a.addTriggerZone = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TR("zone.add.tip"));
    }

    auto& zones = p.triggerZones.zones;
    if (zones.empty()) {
        ImGui::TextDisabled("%s", TR("zone.none"));
        return;
    }

    const char* actionNames[] = {TR("zone.act.none"), TR("zone.act.clip"),
                                 TR("zone.act.column"), TR("zone.act.clearlayer"),
                                 TR("zone.act.clearall")};

    for (int i = 0; i < static_cast<int>(zones.size()); ++i) {
        TriggerZone& z = zones[static_cast<size_t>(i)];
        ImGui::PushID(7000 + i);

        // To sang khi dang co ngon tay trong vung — phan hoi truc quan
        // quan trong nhat khi can chinh.
        if (z.occupied) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.6f, 0.35f, 1.0f));

        const bool open = ImGui::CollapsingHeader(
            (z.name + (z.occupied ? TR("zone.touching") : "")).c_str());

        if (z.occupied) ImGui::PopStyleColor();

        if (open) {
            ImGui::Checkbox(TR("zone.enabled"), &z.enabled);
            ImGui::SameLine();
            if (ImGui::SmallButton(TR("zone.delete"))) a.removeTriggerZone = i;

            char nb[64];
            std::snprintf(nb, sizeof(nb), "%s", z.name.c_str());
            if (ImGui::InputText(TR("zone.name"), nb, sizeof(nb))) z.name = nb;

            float o[2] = {static_cast<float>(z.origin.x), static_cast<float>(z.origin.y)};
            if (ImGui::DragFloat2(TR("zone.position"), o, 1.0f)) z.origin = Vec2{o[0], o[1]};

            float sz[2] = {static_cast<float>(z.size.x), static_cast<float>(z.size.y)};
            if (ImGui::DragFloat2(TR("zone.size"), sz, 1.0f, 1.0f, 16384.0f)) {
                z.size = Vec2{std::max(1.0f, sz[0]), std::max(1.0f, sz[1])};
            }

            int act = static_cast<int>(z.action);
            if (ImGui::Combo(TR("zone.action"), &act, actionNames, 5)) {
                z.action = static_cast<TriggerAction>(act);
            }

            if (z.action == TriggerAction::TriggerClip
                || z.action == TriggerAction::ClearLayer) {
                ImGui::DragInt(TR("zone.layer"), &z.targetLayer, 0.1f, 0,
                               std::max(0, p.composition.layerCount() - 1));
            }
            if (z.action == TriggerAction::TriggerClip
                || z.action == TriggerAction::TriggerColumn) {
                ImGui::DragInt(TR("zone.column"), &z.targetColumn, 0.1f, 0,
                               std::max(0, p.composition.columnCount() - 1));
            }

            float cd = static_cast<float>(z.cooldownSec);
            if (ImGui::DragFloat(TR("zone.cooldown"), &cd, 0.01f, 0.0f, 5.0f, "%.2fs")) {
                z.cooldownSec = std::max(0.0f, cd);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s", TR("zone.cooldown.tip"));
            }
        }
        ImGui::PopID();
    }
}

// ── F12: keo nut mat na trong cua so Mapping ───────────────────────────
//
// Tach khoi drawMappingEditor de ham do khong phinh ra kho doc: hai bo
// tuong tac (goc keystone / nut mat na) dung chung khung nhin nhung logic
// hoan toan khac nhau.

bool ControlPanel::handleMaskEditing(
        Slice& s, EditState& edit,
        const std::function<ImVec2(const Vec2&)>& toWidget,
        const std::function<Vec2(const ImVec2&)>& toOutput,
        ImDrawList* dl, bool hovered, bool active) {

    BezierMask& m = s.mask;
    const int n = static_cast<int>(m.nodes.size());

    const ImVec2 mouse = ImGui::GetIO().MousePos;
    auto uvToWidget = [&](const Vec2& uv) { return toWidget(s.contentToOutput(uv)); };
    auto dist = [](const ImVec2& a, const ImVec2& b) {
        return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y));
    };

    // ── Ve duong bien ──────────────────────────────────────────────────
    if (n >= 2) {
        std::vector<Vec2> poly;
        m.flatten(poly);

        std::vector<ImVec2> pts;
        pts.reserve(poly.size());
        for (const Vec2& uv : poly) pts.push_back(uvToWidget(uv));

        // Mau khac han vien slice (vang) va vien hong (do): ba thu nay
        // deu la duong khep kin nen phai phan biet duoc bang mau.
        const ImU32 maskCol = m.enabled ? IM_COL32(120, 255, 200, 255)
                                        : IM_COL32(120, 255, 200, 110);
        dl->AddPolyline(pts.data(), static_cast<int>(pts.size()), maskCol,
                        ImDrawFlags_Closed, 2.0f);
    }

    if (n == 0) return false;

    // ── Tim thu duoi con tro ───────────────────────────────────────────
    //
    // Uu tien TAY NAM hon diem neo: tay nam thuong nam gan neo, va neu
    // neo thang thi khong bao gio cham duoc vao tay nam.
    constexpr float kGrab = 12.0f;

    int hitNode = -1;
    int hitPart = 0;                 // 0 neo, 1 tay nam vao, 2 tay nam ra
    float best = kGrab;

    if (hovered || active) {
        for (int i = 0; i < n; ++i) {
            const MaskNode& nd = m.nodes[static_cast<size_t>(i)];
            if (nd.isCorner()) continue;
            const float dIn  = dist(uvToWidget(nd.inPoint()),  mouse);
            const float dOut = dist(uvToWidget(nd.outPoint()), mouse);
            if (dIn  < best) { best = dIn;  hitNode = i; hitPart = 1; }
            if (dOut < best) { best = dOut; hitNode = i; hitPart = 2; }
        }
        for (int i = 0; i < n; ++i) {
            const float d = dist(uvToWidget(m.nodes[static_cast<size_t>(i)].point), mouse);
            if (d < best) { best = d; hitNode = i; hitPart = 0; }
        }
    }
    edit.maskHoveredNode = (hitPart == 0) ? hitNode : -1;

    bool consumed = false;

    // ── Chuot phai: xoa nut ────────────────────────────────────────────
    if (hovered && hitNode >= 0 && hitPart == 0
        && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
        if (!m.removeNode(hitNode)) {
            setStatusMessage(TR("mask.cannotRemove"), true);
        }
        edit.maskDraggedNode = -1;
        return true;
    }

    // ── Chuot trai ─────────────────────────────────────────────────────
    if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        const ImGuiIO& io = ImGui::GetIO();

        if (hitNode >= 0) {
            MaskNode& nd = m.nodes[static_cast<size_t>(hitNode)];

            if (io.KeyShift && hitPart == 0) {
                // Duoi thang: bo hai tay nam, nut thanh goc nhon.
                nd.inHandle  = Vec2{0.0, 0.0};
                nd.outHandle = Vec2{0.0, 0.0};
                consumed = true;

            } else if (io.KeyCtrl && hitPart == 0 && nd.isCorner()) {
                // Moc tay nam ra khoi mot nut goc. Huong ban dau lay theo
                // duong noi hai nut ke — de nguoi dung keo tiep tu mot the
                // hop ly thay vi tu vector 0 (khong biet keo ve huong nao).
                const MaskNode& prev = m.nodes[static_cast<size_t>((hitNode - 1 + n) % n)];
                const MaskNode& next = m.nodes[static_cast<size_t>((hitNode + 1) % n)];
                Vec2 dir{(next.point.x - prev.point.x) * 0.15,
                         (next.point.y - prev.point.y) * 0.15};
                if (dir.x == 0.0 && dir.y == 0.0) dir = Vec2{0.08, 0.0};
                nd.outHandle = dir;
                nd.inHandle  = Vec2{-dir.x, -dir.y};
                edit.maskDraggedNode = hitNode;
                edit.maskDraggedPart = 2;
                consumed = true;

            } else {
                edit.maskDraggedNode = hitNode;
                edit.maskDraggedPart = hitPart;
                consumed = true;
            }

        } else if (n >= 2) {
            // Bam LEN DUONG -> chen nut o dung cho bam.
            //
            // Do khoang cach bang PIXEL chu khong bang don vi UV: tren mot
            // slice 1920x200, lech 0.02 UV la 38px theo truc nay va 4px
            // theo truc kia — mot nguong tinh bang UV se bat rat lech.
            double t = 0.0, du = 0.0;
            Vec2 uv;
            if (s.outputToContent(toOutput(mouse), uv)) {
                const int seg = m.closestSegment(uv, t, du);
                if (seg >= 0) {
                    const float dPx = dist(uvToWidget(m.pointOnSegment(seg, t)), mouse);
                    if (dPx < kGrab) {
                        if (m.insertNodeOnSegment(seg, t) < 0) {
                            setStatusMessage(TR("mask.full"), true);
                        }
                        consumed = true;
                    }
                }
            }
        }
    }

    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) edit.maskDraggedNode = -1;

    // ── Keo ────────────────────────────────────────────────────────────
    if (edit.maskDraggedNode >= 0 && edit.maskDraggedNode < n
        && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {

        MaskNode& nd = m.nodes[static_cast<size_t>(edit.maskDraggedNode)];

        Vec2 uv;
        // ★ Nut mat na song trong contentUV [0,1], va outputToContent tu
        //   choi diem ngoai khoang do. Keo chuot ra ngoai slice thi nut
        //   dung lai o bien — dung, vi mot nut ngoai slice khong co nghia
        //   gi: texture mat na chi phu [0,1].
        if (s.outputToContent(toOutput(mouse), uv)) {
            if (edit.maskDraggedPart == 0) {
                nd.point = uv;
            } else {
                const Vec2 h{uv.x - nd.point.x, uv.y - nd.point.y};

                // Keo mot tay nam thi tay nam kia doi xung theo — duong
                // cong di qua nut mot cach TRON. Doi voi bong vat the
                // that (cot tron, mai vom) day gan nhu luon la thu can;
                // muon goc nhon thi Shift+bam de duoi thang ca hai.
                if (edit.maskDraggedPart == 2) {
                    nd.outHandle = h;
                    nd.inHandle  = Vec2{-h.x, -h.y};
                } else {
                    nd.inHandle  = h;
                    nd.outHandle = Vec2{-h.x, -h.y};
                }
            }
        }
        consumed = true;
    }

    // ── Ve nut va tay nam ──────────────────────────────────────────────
    for (int i = 0; i < n; ++i) {
        const MaskNode& nd = m.nodes[static_cast<size_t>(i)];
        const ImVec2 p = uvToWidget(nd.point);

        if (!nd.isCorner()) {
            const ImVec2 a = uvToWidget(nd.inPoint());
            const ImVec2 b = uvToWidget(nd.outPoint());
            dl->AddLine(p, a, IM_COL32(120, 255, 200, 120), 1.0f);
            dl->AddLine(p, b, IM_COL32(120, 255, 200, 120), 1.0f);
            dl->AddCircleFilled(a, 3.5f, IM_COL32(120, 255, 200, 200));
            dl->AddCircleFilled(b, 3.5f, IM_COL32(120, 255, 200, 200));
        }

        ImU32 c = IM_COL32(120, 255, 200, 255);
        if (i == edit.maskDraggedNode)      c = IM_COL32(255, 255, 255, 255);
        else if (i == edit.maskHoveredNode) c = IM_COL32(210, 255, 235, 255);

        dl->AddCircleFilled(p, 6.0f, c);
        dl->AddCircleFilled(p, 2.4f, IM_COL32(20, 20, 20, 255));
    }

    return consumed;
}

// ── ★ Trinh chinh mapping NGAY TRONG cua so chinh ─────────────────────

void ControlPanel::drawMappingEditor(Project& p, EditState& edit,
                                     const ofTexture* canvasTex) {
    if (p.screens.empty()) { ImGui::TextDisabled("%s", TR("map.noscreen")); return; }

    m_activeScreen = std::clamp(m_activeScreen, 0,
                                static_cast<int>(p.screens.size()) - 1);
    Screen& sc = p.screens[static_cast<size_t>(m_activeScreen)];

    // Cac cong tac (hien noi dung / luoi test / chinh mat na) da nam
    // tren THANH CONG CU cua trang Mapping. De ca hai noi thi nguoi dung
    // gap hai o tich cho cung mot thu — va se co luc chung nhin nhu khong
    // dong bo, du thuc ra cung tro toi mot bien.
    ImGui::TextDisabled("%s", edit.maskEditMode ? TR("mask.hint") : TR("map.hint"));

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x < 40.0f || avail.y < 40.0f) return;

    const float resW = static_cast<float>(std::max(1.0, sc.resolution.x));
    const float resH = static_cast<float>(std::max(1.0, sc.resolution.y));

    // Vua khung, GIU TI LE. Khong keo gian: xem mapping o ti le sai thi
    // moi thao tac can chinh deu lech so voi thuc te.
    //
    // ★ Chua LE quanh vung ve.
    //   Can chinh mapping thuong xuyen phai keo goc RA NGOAI mep man hinh
    //   — vd hinh chieu len vat the lon hon khung chieu. Neu ve vua khit
    //   khung thi diem keo ra ngoai se bien mat khoi vung nhin va khong
    //   bam lai duoc: nguoi dung ket, phai bam Reset lam lai tu dau.
    constexpr float kMargin = 0.22f;   // 22% moi ben
    const float usableW = avail.x * (1.0f - kMargin);
    const float usableH = avail.y * (1.0f - kMargin);

    const float scale = std::min(usableW / resW, usableH / resH);
    const float drawW = resW * scale;
    const float drawH = resH * scale;

    const ImVec2 cur = ImGui::GetCursorScreenPos();
    const ImVec2 origin(cur.x + (avail.x - drawW) * 0.5f,
                        cur.y + (avail.y - drawH) * 0.5f);

    auto toWidget = [&](const Vec2& v) {
        return ImVec2(origin.x + static_cast<float>(v.x) * scale,
                      origin.y + static_cast<float>(v.y) * scale);
    };
    auto toScreenSpace = [&](const ImVec2& v) {
        return Vec2{(v.x - origin.x) / scale, (v.y - origin.y) / scale};
    };

    ImGui::InvisibleButton("##mapcanvas", avail);
    const bool hovered = ImGui::IsItemHovered();
    const bool active  = ImGui::IsItemActive();

    ImDrawList* dl = ImGui::GetWindowDrawList();

    dl->AddRectFilled(origin, ImVec2(origin.x + drawW, origin.y + drawH),
                      IM_COL32(18, 18, 22, 255));

    // ── Noi dung xem thu ───────────────────────────────────────────────
    if (m_mapShowContent && canvasTex != nullptr && canvasTex->isAllocated()) {
        for (const int si : sc.visibleSlices()) {
            const Slice& s = sc.slices[static_cast<size_t>(si)];
            const IWarp* w = s.warp();
            if (w == nullptr) continue;

            const Vec2 uv[4] = {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}};
            ImVec2 pos[4];
            ImVec2 tc[4];
            for (int k = 0; k < 4; ++k) {
                pos[k] = toWidget(w->forward(uv[k]));
                const Vec2 cv = s.contentToCanvas(uv[k]);
                tc[k] = ImVec2(static_cast<float>(cv.x) / canvasTex->getWidth(),
                               static_cast<float>(cv.y) / canvasTex->getHeight());
            }
            // AddImageQuad noi suy AFFINE, khong phai phoi canh — nen day
            // chi la XEM THU cho de hinh dung. Hinh THAT do GPU ve o cua
            // so output moi dung phoi canh.
            dl->AddImageQuad(GetImTextureID(*canvasTex),
                             pos[0], pos[1], pos[2], pos[3],
                             tc[0], tc[1], tc[2], tc[3]);
        }
    }

    dl->AddRect(origin, ImVec2(origin.x + drawW, origin.y + drawH),
                IM_COL32(110, 110, 120, 255));

    // ── Vien tung slice ────────────────────────────────────────────────
    for (int i = 0; i < sc.sliceCount(); ++i) {
        const Slice& s = sc.slices[static_cast<size_t>(i)];
        const IWarp* w = s.warp();
        if (w == nullptr) continue;

        const bool isActive = (i == edit.activeSliceIndex);

        ImU32 col = IM_COL32(90, 140, 200, 170);
        if (!w->isInvertible()) col = IM_COL32(255, 60, 60, 255);
        else if (isActive)      col = IM_COL32(255, 200, 60, 255);
        if (!s.enabled)         col = IM_COL32(90, 90, 90, 120);

        const int sub = std::max(1, w->defaultSubdivisions());
        std::vector<ImVec2> pts;
        for (int k = 0; k <= sub; ++k)
            pts.push_back(toWidget(w->forward({static_cast<double>(k) / sub, 0.0})));
        for (int k = 1; k <= sub; ++k)
            pts.push_back(toWidget(w->forward({1.0, static_cast<double>(k) / sub})));
        for (int k = sub - 1; k >= 0; --k)
            pts.push_back(toWidget(w->forward({static_cast<double>(k) / sub, 1.0})));
        for (int k = sub - 1; k >= 1; --k)
            pts.push_back(toWidget(w->forward({0.0, static_cast<double>(k) / sub})));

        dl->AddPolyline(pts.data(), static_cast<int>(pts.size()), col,
                        ImDrawFlags_Closed, isActive ? 2.5f : 1.5f);

        // F12 — duong mat na hien CA khi khong o che do sua: no quyet
        // dinh phan nao thuc su duoc chieu, nen nhin vao khung mapping ma
        // khong thay no thi hieu sai hoan toan ve thu dang ra may chieu.
        if (s.mask.nodes.size() >= 2) {
            std::vector<Vec2> mpoly;
            s.mask.flatten(mpoly);
            std::vector<ImVec2> mpts;
            mpts.reserve(mpoly.size());
            for (const Vec2& uv : mpoly) mpts.push_back(toWidget(s.contentToOutput(uv)));

            const ImU32 mc = s.mask.enabled ? IM_COL32(120, 255, 200, 200)
                                            : IM_COL32(120, 255, 200, 70);
            dl->AddPolyline(mpts.data(), static_cast<int>(mpts.size()), mc,
                            ImDrawFlags_Closed, 1.5f);
        }

        if (!s.name.empty()) {
            dl->AddText(ImVec2(pts[0].x + 4.0f, pts[0].y + 2.0f), col, s.name.c_str());
        }
    }

    // ── Diem dieu khien cua slice dang chon ────────────────────────────
    if (edit.activeSliceIndex < 0 || edit.activeSliceIndex >= sc.sliceCount()) {
        dl->AddText(ImVec2(origin.x + 8.0f, origin.y + 8.0f),
                    IM_COL32(255, 255, 255, 150), TR("map.selecthint"));
        return;
    }

    Slice& s = sc.slices[static_cast<size_t>(edit.activeSliceIndex)];
    IWarp* w = s.warp();
    if (w == nullptr) return;

    // ── F12: che do sua mat na ─────────────────────────────────────────
    //
    // ★ Khi bat, phan keystone ben duoi KHONG chay chut nao — khong ve
    //   handle, khong bat chuot. Neu de ca hai cung nhan chuot thi mot cu
    //   keo hut se xo lech goc keystone da can xong, tuc pha hong dung
    //   thu ton cong nhat de lam lai, ngay giua luc dang can mat na.
    if (edit.maskEditMode) {
        handleMaskEditing(s, edit, toWidget, toScreenSpace, dl, hovered, active);
        return;
    }
    edit.maskDraggedNode = -1;
    edit.maskHoveredNode = -1;

    const int n = w->controlPointCount();
    const float r = (n > 25) ? 4.0f : 6.5f;

    int nearest = -1;
    if (hovered || active) {
        const ImVec2 m = ImGui::GetIO().MousePos;
        float best = 14.0f;
        for (int k = 0; k < n; ++k) {
            const ImVec2 q = toWidget(w->controlPointAt(k));
            const float d = std::sqrt((q.x - m.x) * (q.x - m.x)
                                    + (q.y - m.y) * (q.y - m.y));
            if (d < best) { best = d; nearest = k; }
        }
    }

    if (active && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        m_mapDragPoint = nearest;

        // Bam vao cho trong -> chon slice khac.
        if (nearest < 0) {
            const int hit = sc.hitTest(toScreenSpace(ImGui::GetIO().MousePos));
            if (hit >= 0) edit.activeSliceIndex = hit;
        }
    }
    if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) m_mapDragPoint = -1;

    // setControlPointAt TU CHOI vi tri lam tu giac lom hoac tu cat va giu
    // nguyen trang thai cu — nguoi dung thay diem "bat nguoc lai", va
    // vien doi sang DO de hieu vi sao.
    if (m_mapDragPoint >= 0 && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        Vec2 target = toScreenSpace(ImGui::GetIO().MousePos);

        // Kep vao pham vi mo rong. Cho phep RA NGOAI man hinh — day la
        // nhu cau that khi chieu len vat the lon hon khung chieu — nhung
        // khong cho bay di vo han: keo qua nhanh ra ngoai cua so la diem
        // di xa hang nghin pixel va khong tim lai duoc.
        const double limX = sc.resolution.x * 0.5;
        const double limY = sc.resolution.y * 0.5;
        target.x = std::clamp(target.x, -limX, sc.resolution.x + limX);
        target.y = std::clamp(target.y, -limY, sc.resolution.y + limY);

        w->setControlPointAt(m_mapDragPoint, target);
    }

    int outsideCount = 0;
    for (int k = 0; k < n; ++k) {
        const Vec2  cp = w->controlPointAt(k);
        const ImVec2 q = toWidget(cp);

        // Diem nam ngoai khung may chieu: phan noi dung o do se KHONG
        // duoc chieu ra. Danh dau bang vong tron rong de nguoi dung biet
        // day la co y hay lo tay.
        const bool outside = (cp.x < 0.0 || cp.y < 0.0
                              || cp.x > sc.resolution.x || cp.y > sc.resolution.y);
        if (outside) ++outsideCount;

        ImU32 c = IM_COL32(255, 200, 60, 255);
        if (k == m_mapDragPoint)    c = IM_COL32(255, 255, 255, 255);
        else if (k == nearest)      c = IM_COL32(255, 235, 120, 255);
        else if (outside)           c = IM_COL32(255, 140, 60, 255);

        dl->AddCircleFilled(q, r, c);
        dl->AddCircleFilled(q, r * 0.4f, IM_COL32(20, 20, 20, 255));
        if (outside) dl->AddCircle(q, r + 3.5f, IM_COL32(255, 140, 60, 220), 0, 1.5f);
    }

    if (outsideCount > 0) {
        char msg[96];
        std::snprintf(msg, sizeof(msg), TR("map.outside"), outsideCount);
        dl->AddText(ImVec2(origin.x + 6.0f, origin.y + drawH + 6.0f),
                    IM_COL32(255, 140, 60, 255), msg);
    }
}

// ── Nap font co glyph tieng Viet ──────────────────────────────────────

void ControlPanel::loadFont() {
    if (m_fontLoaded) return;

    // Dai glyph: ImGui co san GetGlyphRangesVietnamese() — ASCII + Latin mo
    // rong + toan bo dau tieng Viet. Nap dai rong hon (vd Chinese) se lam
    // atlas phinh len hang chuc MB ma chua dung toi.
    const ImWchar* ranges = ImGui::GetIO().Fonts->GetGlyphRangesVietnamese();

    ImFontConfig cfg;
    cfg.OversampleH = 2;
    cfg.OversampleV = 1;
    cfg.PixelSnapH  = true;

    // Thu theo thu tu: font dong goi kem -> font he thong -> font mac dinh.
    //
    // Font dong goi kem duoc uu tien vi no giong nhau tren moi may. Font
    // he thong chi la luoi an toan khi ai do xoa mat thu muc fonts/.
    const std::string candidates[] = {
        ofToDataPath("fonts/Inter.ttf", true),
        "C:/Windows/Fonts/segoeui.ttf",
        "C:/Windows/Fonts/tahoma.ttf",
    };

    for (const std::string& path : candidates) {
        if (!ofFile::doesFileExist(path)) continue;

        // ★ PHAI di qua m_gui.addFont(), khong duoc goi thang
        //   io.Fonts->AddFontFromFileTTF().
        //
        //   Goi thang thi font VAO duoc atlas nhung atlas khong bao gio
        //   duoc build lai va nap len GPU, va no cung khong duoc dat lam
        //   font mac dinh — nen ImGui van ve bang font ProggyClean cu.
        //   Trieu chung rat de doc nham: ProggyClean co Latin-1 nen "Cai"
        //   hien dung dau `a`, chi rieng `d` va `at` (ngoai Latin-1) moi
        //   thanh dau hoi. Nhin qua tuong la font thieu glyph, that ra la
        //   font moi chua he duoc dung.
        if (m_gui.addFont(path, 16.0f, &cfg, ranges, /*setAsDefault*/ true) != nullptr) {
            m_fontLoaded = true;
            ofLogNotice("ControlPanel") << "Font: " << path;
            return;
        }
    }

    // Khong nap duoc font nao: van chay duoc, chi la dau tieng Viet hien
    // thanh o vuong. Bao ro thay vi de nguoi dung tu doan.
    ofLogWarning("ControlPanel")
        << "Khong nap duoc font co glyph tieng Viet - dau se hien sai. "
           "Kiem tra bin/data/fonts/Inter.ttf";
}

// ── Cua so Cai dat ────────────────────────────────────────────────────

void ControlPanel::drawSettingsPanel(UiActions& a) {
    if (m_settings == nullptr) { ImGui::TextDisabled("-"); return; }
    AppSettings& st = *m_settings;

    // ── Ngon ngu ───────────────────────────────────────────────────────
    ImGui::SeparatorText(TR("set.language"));

    // Ten ngon ngu KHONG dich - moi ngon ngu tu goi ten no bang chinh
    // no. Nguoi dung lo chon nham ngon ngu khong doc duoc se khong tim
    // duong ve neu danh sach hien bang thu tieng ho vua chon nham.
    const char* langNames[] = {"Tiếng Việt", "English"};
    int lang = static_cast<int>(st.language);
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::Combo("##lang", &lang, langNames, 2)) {
        st.language = static_cast<Language>(lang);
        i18n::setLanguage(st.language);
        a.settingsChanged = true;
    }
    ImGui::TextDisabled("%s", TR("set.language.note"));

    const int missing = i18n::missingCount();
    if (missing > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                           "%s: %d", TR("set.missing"), missing);
    }

    // ── Output ─────────────────────────────────────────────────────────
    ImGui::SeparatorText(TR("set.output"));

    {
        std::string cur = TR("set.output.none");
        for (const DisplayEntry& d : m_displays) {
            if (d.index == st.defaultOutputDisplay) {
                cur = std::to_string(d.index + 1) + ". " + d.name;
            }
        }
        ImGui::SetNextItemWidth(240.0f);
        if (ImGui::BeginCombo(TR("set.output.default"), cur.c_str())) {
            if (ImGui::Selectable(TR("set.output.none"), st.defaultOutputDisplay < 0)) {
                st.defaultOutputDisplay = -1;
                a.settingsChanged = true;
            }
            for (const DisplayEntry& d : m_displays) {
                char lbl[128];
                std::snprintf(lbl, sizeof(lbl), "%d. %s  %dx%d",
                              d.index + 1, d.name.c_str(), d.w, d.h);
                if (ImGui::Selectable(lbl, st.defaultOutputDisplay == d.index)) {
                    st.defaultOutputDisplay = d.index;
                    a.settingsChanged = true;
                }
            }
            ImGui::EndCombo();
        }
    }

    if (ImGui::Checkbox(TR("set.vsync"), &st.vsync)) a.settingsChanged = true;

    // ── Media ──────────────────────────────────────────────────────────
    ImGui::SeparatorText(TR("set.media"));

    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::SliderInt(TR("set.media.budget"), &st.mediaCacheBudget, 1, 64)) {
        a.settingsChanged = true;
    }
    ImGui::TextDisabled("%s", TR("set.media.budget.note"));

    if (ImGui::Checkbox(TR("set.media.warnhap"), &st.warnNonHapMedia)) {
        a.settingsChanged = true;
    }

    // ── Chan doan ──────────────────────────────────────────────────────
    ImGui::SeparatorText(TR("set.diag"));

    if (ImGui::Checkbox(TR("set.perflog"), &st.perfLogEnabled)) a.settingsChanged = true;

    if (st.perfLogEnabled) {
        float iv = static_cast<float>(st.perfLogIntervalSec);
        ImGui::SetNextItemWidth(180.0f);
        if (ImGui::SliderFloat(TR("set.perflog.interval"), &iv, 0.5f, 60.0f, "%.1f")) {
            st.perfLogIntervalSec = iv;
            a.settingsChanged = true;
        }
    }
    ImGui::TextDisabled("%s", TR("set.perflog.note"));

    // ── Khoi dong ──────────────────────────────────────────────────────
    ImGui::SeparatorText(TR("set.startup"));
    if (ImGui::Checkbox(TR("set.autoplay"), &st.autoPlayFirstClip)) a.settingsChanged = true;

    // ── Luu ────────────────────────────────────────────────────────────
    ImGui::Separator();
    if (ImGui::Button(TR("set.save")))  a.saveSettings = true;
    ImGui::SameLine();
    if (ImGui::Button(TR("set.reset"))) a.resetSettings = true;
}


} // namespace hexmap

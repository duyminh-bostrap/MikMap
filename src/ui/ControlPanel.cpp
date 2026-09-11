#include "ui/ControlPanel.h"

#include "ui/Localization.h"
#include "ui/Theme.h"
#include "ui/IconsLucide.h"

#include "core/model/WarpCornerPin.h"
#include "core/model/WarpMesh.h"
#include "core/math/Snapping.h"

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
        drawTopBar(edit, stats, actions);

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

void ControlPanel::drawTopBar(EditState& edit, const PerfStats& s, UiActions& a) {
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

    // ★ Tam trung tam theo TRUC DOC cua ca thanh. Moi phan tu sau day tu
    //   dat lai Y quanh moc nay ngay TRUOC khi ve, khong dua vao co che
    //   can dong tu dong cua ImGui::SameLine().
    //
    //   Ly do phai lam vay: hang nay tron nhieu co chu khac nhau (18px
    //   cho MIKMAP, 12px cho nut, 11px cho tab, font mono rieng cho so
    //   lieu). SameLine() chi mang theo Y cua DIEM BAT DAU dong — no
    //   khong tu can giua cac phan tu co chieu cao khac nhau tren cung
    //   mot hang. Bo qua buoc nay se ra dung thu nguoi dung da thay:
    //   chu tren thanh menu nam LECH nhau, khong thang hang.
    const float cy = theme::TopBarH * 0.5f;
    auto centerV = [&](float itemH) {
        ImGui::SetCursorPosY(cy - itemH * 0.5f);
    };

    // Chieu cao mot "hang" o co chu mac dinh — dung lam moc chung cho
    // cac phan tu trang tri (icon, cham trang thai) khong tu co chieu
    // cao rieng.
    const float rowH = ImGui::GetFrameHeight();

    // Le trai/phai thanh dieu huong: px-5 = 20px trong ban thiet ke pug.
    constexpr float kTopBarPadX = 20.0f;
    ImGui::SetCursorPosX(kTopBarPadX);
    centerV(rowH);

    // ── Thuong hieu — CHINH NO la nut mo menu Project ───────────────────
    //
    // ★ Bo rieng nut "Du an": mot nut chu rieng biet chiem cho va lam
    //   hang nhin loang. Logo + chu MIKMAP la thu DUY NHAT khong doi theo
    //   trang dang xem, nen la noi hop ly nhat de dat menu — giong cach
    //   phan lon app desktop dat menu chinh sau logo.
    //
    //   Ve y het truoc gio (khong doi hinh dang), chi phu them MOT vung
    //   bam vo hinh de vua giu nguyen mat vua bat duoc click. Ve TRUOC,
    //   bam SAU: InvisibleButton phai nam o vi tri DA BIET, nen phai tinh
    //   truoc kich thuoc icon + khoang cach + chu roi moi dat no.
    {
        theme::pushBold(theme::fs::Brand);
        const float textW = ImGui::CalcTextSize("MIKMAP").x;
        theme::popFont();

        // Icon w-5 h-5 = 20x20, cham trong w-2 h-2 = 8x8, khoang cach toi
        // chu gap-2.5 = 10px — dung so do ban thiet ke pug, khong doan.
        constexpr float kIconW = 20.0f;
        constexpr float kGap   = 10.0f;
        const float totalW = kIconW + kGap + textW;

        const ImVec2 p0 = ImGui::GetCursorScreenPos();
        const bool clicked = ImGui::InvisibleButton("##brandmenu", ImVec2(totalW, rowH));
        const bool hovered = ImGui::IsItemHovered();
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        ImDrawList* dl = ImGui::GetWindowDrawList();
        const float iconCy = p0.y + rowH * 0.5f;
        const ImU32 iconCol = hovered ? theme::Warning : theme::Primary;
        dl->AddRectFilled(ImVec2(p0.x, iconCy - 10.0f), ImVec2(p0.x + kIconW, iconCy + 10.0f),
                          iconCol, 3.0f);
        dl->AddRectFilled(ImVec2(p0.x + 6.0f, iconCy - 4.0f), ImVec2(p0.x + 14.0f, iconCy + 4.0f),
                          theme::BgPanel, 1.0f);

        dl->PushClipRectFullScreen();
        theme::pushBold(theme::fs::Brand);
        const ImU32 textCol = hovered ? theme::Warning : theme::Text;
        const ImVec2 textPos(p0.x + kIconW + kGap,
                             p0.y + (rowH - ImGui::GetTextLineHeight()) * 0.5f);
        // ★ Chu MIKMAP trong ban thiet ke la font-black (wght 900); font
        //   nang nhat da cat san chi la Bold (700). Gia det chu bang cach
        //   ve chong nhieu lan lech nua-pixel theo luoi 3x3 (thay vi 2x2) —
        //   net day ro ret hon han Bold thuong, gan sat Black.
        for (float dy = -0.7f; dy <= 0.7f; dy += 0.7f) {
            for (float dx = -0.7f; dx <= 0.7f; dx += 0.7f) {
                dl->AddText(ImVec2(textPos.x + dx, textPos.y + dy), textCol, "MIKMAP");
            }
        }
        theme::popFont();
        dl->PopClipRect();

        if (clicked) ImGui::OpenPopup("##projectmenu");
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
    //
    // ★ Moi tab TU CO theo chinh chu cua no (ImVec2(0,0)), khong ep chung
    //   mot be rong co dinh. "ADVANCED MAPPING" dai hon han "SENSOR I/O";
    //   ep bang nhau thi hai tab ngan bi thua khoang trong hai ben va
    //   nhin lech tam. Container ngoai dung AutoResizeX de tu co theo
    //   tong be rong that cua ba tab, khong phai mot con so doan truoc.
    //
    //   So do LAY DUNG tu ban thiet ke pug, khong doan: gap-8 (32px) tu
    //   logo toi khoi tab, p-1 (4px) vien trong khoi, px-4 py-1.5 (16/6px)
    //   moi nut, khong co gap giua ba nut, chu text-xs (12px).
    constexpr float kTabPadX = 16.0f, kTabPadY = 6.0f, kGroupPad = 4.0f;
    ImGui::SameLine(0.0f, 32.0f);

    theme::pushBold(theme::fs::Body);
    const float tabLineH = ImGui::GetTextLineHeight();
    theme::popFont();
    const float tabBtnH  = tabLineH + kTabPadY * 2.0f;
    const float tabsH    = tabBtnH + kGroupPad * 2.0f;

    centerV(tabsH);
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgSunken));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kGroupPad, kGroupPad));
        ImGui::BeginChild("##tabs", ImVec2(0.0f, tabsH),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX,
                          ImGuiWindowFlags_NoScrollbar);

        char lbl[96];
        theme::pushBold(theme::fs::Body);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(kTabPadX, kTabPadY));

        std::snprintf(lbl, sizeof(lbl), "%s  %s", ICON_LC_LAYERS, TR("nav.composition"));
        if (theme::tabButton(lbl, m_view == View::Composition, ImVec2(0, 0))) {
            m_view = View::Composition;
        }
        ImGui::SameLine(0.0f, 0.0f);
        std::snprintf(lbl, sizeof(lbl), "%s  %s", ICON_LC_MONITOR_PLAY, TR("nav.mapping"));
        if (theme::tabButton(lbl, m_view == View::Mapping, ImVec2(0, 0))) {
            m_view = View::Mapping;
        }
        ImGui::SameLine(0.0f, 0.0f);
        std::snprintf(lbl, sizeof(lbl), "%s  %s", ICON_LC_ACTIVITY, TR("nav.sensor"));
        if (theme::tabButton(lbl, m_view == View::Sensor, ImVec2(0, 0))) {
            m_view = View::Sensor;
        }

        ImGui::PopStyleVar();
        theme::popFont();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    // ── Thanh cong cu nhanh: Show Mode / Test Grid / Auto Calib / Toan man hinh ──
    //
    // ★ Show Mode va Test Grid da co san TRANG THAI (EditState::showOverlay,
    //   showGrid) va logic ve o RenderEngine tu truoc — chi thieu cho de
    //   bam TOI, khong phai lam moi tu dau. Show Mode la NGUOC voi
    //   showOverlay (bam "Show Mode" = AN het handle chinh sua di).
    ImGui::SameLine(0.0f, 12.0f);
    centerV(tabsH);
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgSunken));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kGroupPad, kGroupPad));
        ImGui::BeginChild("##quicktools", ImVec2(0.0f, tabsH),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX,
                          ImGuiWindowFlags_NoScrollbar);
        theme::pushSemiBold(theme::fs::Tiny);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(9.0f, 5.0f));

        char qlbl[64];
        std::snprintf(qlbl, sizeof(qlbl), "%s  %s", ICON_LC_EYE, TR("topbar.showmode"));
        if (theme::tabButton(qlbl, !edit.showOverlay, ImVec2(0, 0), theme::Success)) {
            edit.showOverlay = !edit.showOverlay;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("topbar.showmode.tip"));

        ImGui::SameLine(0.0f, 2.0f);
        std::snprintf(qlbl, sizeof(qlbl), "%s  %s", ICON_LC_GRID_3X3, TR("topbar.testgrid"));
        if (theme::tabButton(qlbl, edit.showGrid, ImVec2(0, 0), theme::Warning)) {
            edit.showGrid = !edit.showGrid;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("topbar.testgrid.tip"));

        ImGui::SameLine(0.0f, 2.0f);
        std::snprintf(qlbl, sizeof(qlbl), "%s  %s", ICON_LC_WAND_2, TR("topbar.autocalib"));
        if (theme::tabButton(qlbl, false, ImVec2(0, 0), theme::Info)) {
            a.calibAutoMock = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("topbar.autocalib.tip"));

        ImGui::PopStyleVar();
        theme::popFont();

        ImGui::SameLine(0.0f, 4.0f);
        if (theme::toolButton(ICON_LC_MAXIMIZE_2, TR("topbar.fullscreen.tip"))) {
            a.toggleFullscreen = true;
        }

        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    // ── Du an: luu / mo (bieu tuong tat) + ngon ngu ─────────────────────
    ImGui::SameLine(0.0f, 8.0f);
    centerV(tabsH);
    {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgSunken));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(kGroupPad, kGroupPad));
        ImGui::BeginChild("##projectio", ImVec2(0.0f, tabsH),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX,
                          ImGuiWindowFlags_NoScrollbar);
        if (theme::toolButton(ICON_LC_DOWNLOAD, TR("topbar.save.tip"))) a.saveProject = true;
        ImGui::SameLine(0.0f, 2.0f);
        if (theme::toolButton(ICON_LC_UPLOAD, TR("topbar.load.tip")))   a.loadProject = true;
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    ImGui::SameLine(0.0f, 8.0f);
    centerV(tabsH);
    {
        char langLbl[16];
        std::snprintf(langLbl, sizeof(langLbl), "%s  %s", ICON_LC_GLOBE,
                      i18n::language() == Language::English ? "EN" : "VI");
        theme::pushBold(theme::fs::Small);
        if (theme::tabButton(langLbl, false, ImVec2(0, tabsH))) {
            i18n::setLanguage(i18n::language() == Language::English
                              ? Language::Vietnamese : Language::English);
        }
        theme::popFont();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("topbar.lang.tip"));
    }

    // ── Trang thai ben phai ────────────────────────────────────────────
    //
    // FPS va do phan giai dau ra hien THUONG TRUC, khong phai mo bang moi
    // thay: hai con so nay la thu duy nhat cho biet buoi dien co on khong,
    // va luc chung tut thi khong ai co thoi gian di tim cho de xem.
    {
        char fpsBuf[48];
        std::snprintf(fpsBuf, sizeof(fpsBuf), "FPS: %.1f", s.fps);

        // ★ Do tre CHI co nghia khi co sensor dang chay — khong bia so
        //   khi chua ket noi gi ca (architecture.md §10.6: do tre phai
        //   duoc DO, khong duoc DOAN).
        char latBuf[48];
        if (s.sensorConnected) {
            std::snprintf(latBuf, sizeof(latBuf), "%s %.1fms", TR("topbar.latency"),
                          s.sensorLatencyAvgMs);
        } else {
            std::snprintf(latBuf, sizeof(latBuf), "%s --", TR("topbar.latency"));
        }

        std::string outBuf = TR("nav.nooutput");
        for (const DisplayEntry& d : m_displays) {
            if (d.index != m_activeOutputDisplay) continue;
            char b[96];
            std::snprintf(b, sizeof(b), "OUTPUT %d: %dx%d", d.index + 1, d.w, d.h);
            outBuf = b;
        }

        // Uoc luong be rong bang CHINH font se dung de ve (mono), khong
        // phai font mac dinh — neu khong be rong tinh se sai mot chut so
        // voi chu that, va nhom trang thai ben phai khong khop sat mep
        // cua so nhu du dinh.
        theme::pushMono(theme::fs::Small);
        const float fpsW = ImGui::CalcTextSize(fpsBuf).x + 18.0f;
        const float latW = ImGui::CalcTextSize(latBuf).x;
        const float outW = ImGui::CalcTextSize(outBuf.c_str()).x;
        theme::popFont();

        const float gearW  = 30.0f;
        const float rightW = fpsW + latW + outW + gearW + 64.0f;

        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                      ImGui::GetWindowWidth() - rightW));

        // ★ So lieu dung font MONO.
        //
        //   Chu so cua font ti le co be rong khac nhau, nen mot gia tri
        //   doi 60 lan/giay se nhay qua nhay lai va rat kho doc luot —
        //   dung luc can liec nhanh xem fps co tut khong.
        const bool ok = (s.fps >= 55.0);

        // Chip co vien, giong ban thiet ke.
        {
            theme::pushMono(theme::fs::Small);
            centerV(ImGui::GetFrameHeight());
            const ImVec2 tsz = ImGui::CalcTextSize(fpsBuf);
            const ImVec2 p0  = ImGui::GetCursorScreenPos();
            const float  pad = 9.0f;
            const float  h   = ImGui::GetFrameHeight();
            ImDrawList* dl = ImGui::GetWindowDrawList();
            dl->AddRectFilled(p0, ImVec2(p0.x + tsz.x + pad * 2.0f, p0.y + h),
                              theme::BgSunken, 3.0f);
            dl->AddRect(p0, ImVec2(p0.x + tsz.x + pad * 2.0f, p0.y + h),
                        theme::Border, 3.0f);
            dl->AddText(ImVec2(p0.x + pad, p0.y + (h - tsz.y) * 0.5f),
                        ok ? theme::Success : theme::Warning, fpsBuf);
            ImGui::Dummy(ImVec2(tsz.x + pad * 2.0f, h));
            theme::popFont();
        }

        ImGui::SameLine(0.0f, 14.0f);
        centerV(ImGui::GetTextLineHeight());
        theme::pushMono(theme::fs::Small);
        ImGui::TextColored(theme::v4(s.sensorConnected ? theme::Success : theme::TextFaint),
                           "%s", latBuf);
        theme::popFont();

        ImGui::SameLine(0.0f, 18.0f);
        centerV(ImGui::GetTextLineHeight());
        theme::statusDot(m_activeOutputDisplay >= 0 ? theme::Success : theme::TextFaint);
        ImGui::SameLine(0.0f, 6.0f);
        theme::pushMono(theme::fs::Small);
        centerV(ImGui::GetTextLineHeight());
        ImGui::TextDisabled("%s", outBuf.c_str());
        theme::popFont();

        ImGui::SameLine(0.0f, 12.0f);
        centerV(ImGui::GetFrameHeight());
        if (theme::tabButton(ICON_LC_SETTINGS_2, m_showSettings, ImVec2(gearW, 0))) {
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

void ControlPanel::pushMono() { theme::pushMono(theme::fs::Body); }
void ControlPanel::popMono()  { theme::popFont(); }

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

    theme::panelHeader(ICON_LC_FOLDER_OPEN, TR("browser.title"), theme::Info);

    theme::sectionLabel(TR("browser.media"));
    ImGui::SameLine();
    ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 42.0f);
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeight() - 2.0f);
    if (theme::tabButton(ICON_LC_REFRESH_CW, false, ImVec2(26, 0), theme::Info)) rescanMedia();
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("browser.rescan"));
    ImGui::SetCursorPosX(ImGui::GetStyle().WindowPadding.x);

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

        const bool isImage = (ext == "png" || ext == "jpg" || ext == "jpeg");
        char row[192];
        std::snprintf(row, sizeof(row), "%s  %s",
                      isImage ? ICON_LC_IMAGE : ICON_LC_FILM, name.c_str());

        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Selectable(row, false, 0, ImVec2(0, 20))) {
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

    char browseLbl[96];
    std::snprintf(browseLbl, sizeof(browseLbl), "%s  %s",
                  ICON_LC_FOLDER_OPEN, TR("browser.browse"));
    if (theme::outlineButton(browseLbl, theme::Info, ImVec2(-FLT_MIN, 0))) {
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

    // Chieu cao thanh tieu de moi man hinh — vien tren 2px MAU RIENG cho
    // tung o, dung mau de phan biet PREVIEW (lam) voi DANG CHIEU (cam)
    // ngay ca khi nhin luot qua, khong can doc chu.
    // Khop voi ban thiet ke mikmap_UI_pug: h-5 = 20px.
    constexpr float kHeadH = 20.0f;

    auto monitor = [&](const char* id, const char* label, ImU32 labelCol,
                       bool showSliceRects, const char* badge, ImU32 badgeCol,
                       bool pulse, const ofTexture* tex) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgCard));
        ImGui::BeginChild(id, ImVec2(halfW, 0), ImGuiChildFlags_Borders,
                          ImGuiWindowFlags_NoScrollbar);
        ImGui::PopStyleColor();

        // ── Thanh tieu de: vien tren mau, chua nhan + badge ────────────
        {
            ImDrawList* dl = ImGui::GetWindowDrawList();
            const ImVec2 wp = ImGui::GetWindowPos();
            const float ww = ImGui::GetWindowWidth();
            dl->AddLine(ImVec2(wp.x, wp.y), ImVec2(wp.x + ww, wp.y), labelCol, 2.0f);

            ImGui::SetCursorPosY(6.0f);
            theme::pushBold(theme::fs::Tiny);
            ImGui::SetCursorPosX(10.0f);
            if (pulse) {
                theme::statusDot(labelCol);
                ImGui::SameLine(0.0f, 5.0f);
            }
            ImGui::PushStyleColor(ImGuiCol_Text, theme::v4(labelCol));
            ImGui::AlignTextToFramePadding();
            ImGui::TextUnformatted(label);
            ImGui::PopStyleColor();

            if (badge != nullptr) {
                const float bw = ImGui::CalcTextSize(badge).x;
                ImGui::SameLine();
                ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                              ImGui::GetWindowWidth() - bw - 10.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, theme::v4(badgeCol));
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(badge);
                ImGui::PopStyleColor();
            }
            theme::popFont();
            ImGui::SetCursorPosY(kHeadH);
        }

        const ImVec2 box = ImGui::GetContentRegionAvail();
        if (box.x < 20.0f || box.y < 20.0f) { ImGui::EndChild(); return; }

        const ImVec2 org = ImGui::GetCursorScreenPos();
        ImDrawList* dl = ImGui::GetWindowDrawList();
        dl->AddRectFilled(org, ImVec2(org.x + box.x, org.y + box.y),
                          IM_COL32(0, 0, 0, 255), 4.0f);

        if (tex != nullptr && tex->isAllocated()) {
            const float tw = tex->getWidth();
            const float th = tex->getHeight();
            const float sc = std::min(box.x / tw, box.y / th);
            const float dw = tw * sc, dh = th * sc;
            const ImVec2 tl(org.x + (box.x - dw) * 0.5f, org.y + (box.y - dh) * 0.5f);

            dl->AddImage(GetImTextureID(*tex), tl, ImVec2(tl.x + dw, tl.y + dh));

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

    // ★ PREVIEW la texture cua CLIP DANG CHON (m_clipPreviewTex, do
    //   AppController bom vao qua setClipPreview() moi frame) — KHONG
    //   phai canvas dang chieu. Chon o rong / chua chon gi thi tex la
    //   nullptr, o nay ve "chua co noi dung" thay vi lap lai ACTIVE.
    monitor("##mon_preview", TR("mon.preview"), theme::Info, false, nullptr, 0,
            false, m_clipPreviewTex);
    ImGui::SameLine();
    monitor("##mon_active", TR("mon.active"), theme::Primary, true, fps,
            (s.fps >= 55.0) ? theme::Success : theme::Warning, true, canvasTex);
}

// ── Bang thuoc tinh ────────────────────────────────────────────────────

void ControlPanel::drawInspector(Project& p) {
    // ★ Hai tab CLIP / LAYER thay cho MOT tieu de co dinh.
    //
    //   Truoc day bang nay chi hien thuoc tinh CLIP. Nhung nguoi van hanh
    //   cung can xem/sua opacity, blend, transition cua LAYER ma khong
    //   phai keo mat xuong hang layer o luoi ben duoi — hai khai niem
    //   khac nhau (clip = mot o trong luoi, layer = ca mot hang) nen
    //   dung TAB de tach ro, khong nhoi chung mot bang.
    const float tabW = (ImGui::GetContentRegionAvail().x - 2.0f) * 0.5f;

    const int layerNum = std::clamp(m_activeLayerRow, 0,
                                    std::max(0, p.composition.layerCount() - 1)) + 1;
    char layerTabLbl[24];
    std::snprintf(layerTabLbl, sizeof(layerTabLbl), TR("inspector.tab.layer"), layerNum);

    if (theme::tabButton(TR("inspector.tab.clip"), m_inspectorTab == InspectorTab::Clip,
                         ImVec2(tabW, 0), theme::Warning)) {
        m_inspectorTab = InspectorTab::Clip;
    }
    ImGui::SameLine(0.0f, 2.0f);
    if (theme::tabButton(layerTabLbl, m_inspectorTab == InspectorTab::Layer,
                         ImVec2(tabW, 0), theme::Primary)) {
        m_inspectorTab = InspectorTab::Layer;
    }
    ImGui::Separator();

    if (m_inspectorTab == InspectorTab::Clip) drawClipPanel(p);
    else                                      drawLayerProperties(p);
}

void ControlPanel::drawLayerProperties(Project& p) {
    Composition& comp = p.composition;
    if (comp.layerCount() <= 0) {
        ImGui::TextDisabled("%s", TR("comp.empty"));
        return;
    }
    m_activeLayerRow = std::clamp(m_activeLayerRow, 0, comp.layerCount() - 1);
    Layer& layer = comp.layer(m_activeLayerRow);

    char title[32];
    std::snprintf(title, sizeof(title), TR("layer.props.title"), m_activeLayerRow + 1);
    theme::pushBold(theme::fs::Body);
    ImGui::TextColored(theme::v4(theme::Primary), "%s", title);
    theme::popFont();

    ImGui::Checkbox(TR("layer.props.solo"), &layer.solo);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.solo.tip"));
    ImGui::SameLine(0.0f, 14.0f);
    ImGui::Checkbox(TR("layer.props.bypass"), &layer.bypass);
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.bypass.tip"));

    ImGui::Spacing();

    const bool layerPlaying = (layer.activeDeck == comp.viewedDeck()
                              && layer.activeColumn >= 0);
    double opVal = layer.opacity;
    if (theme::opacityBar("##layerop", TR("layer.opacity"), &opVal,
                          layerPlaying ? theme::Primary : theme::Info,
                          layerPlaying, ImGui::GetContentRegionAvail().x)) {
        layer.opacity = opVal;
    }

    ImGui::Spacing();
    labelAbove(TR("clip.blend"));
    int blend = static_cast<int>(layer.blend);
    if (ImGui::Combo("##layerblend", &blend, kBlendNames, IM_ARRAYSIZE(kBlendNames))) {
        layer.blend = static_cast<BlendMode>(blend);
    }

    labelAbove(TR("layer.transition.label"));
    float tr = static_cast<float>(layer.transitionDuration);
    if (ImGui::DragFloat("##layertr", &tr, 0.01f, 0.0f, 10.0f, "%.2fs")) {
        layer.transitionDuration = std::max(0.0f, tr);
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.transition.tip"));
}

// ── Luoi layer × cot ───────────────────────────────────────────────────

bool ControlPanel::drawSharedOpacityBar(const char* label, double* value01,
                                        ImU32 fillColor) {
    const float w = ImGui::GetContentRegionAvail().x;
    constexpr float kH = 26.0f;

    ImVec2 p0 = ImGui::GetCursorScreenPos();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    // Vach mau ben trai — cung mot ngon ngu voi vach chon trong cay
    // SCREEN SETUP: bao "cai nay dang duoc dieu khien".
    dl->AddRectFilled(p0, ImVec2(p0.x + 3.0f, p0.y + kH), fillColor);

    // Vung bam la CA hang (de bam trung), nhung phan tinh gia tri chi
    // xet toa do trong RANH (giua nhan va % ben phai).
    ImGui::InvisibleButton("##sharedop_hit", ImVec2(w, kH));
    bool changed = false;

    const float labelW = 40.0f;
    const float pctW   = 32.0f;
    const float trackX0 = p0.x + 10.0f + labelW;
    const float trackX1 = p0.x + w - pctW - 6.0f;
    const float trackW  = std::max(20.0f, trackX1 - trackX0);
    const float trackH  = 6.0f;
    const float trackY  = p0.y + (kH - trackH) * 0.5f;

    if (ImGui::IsItemActive() && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float mx = ImGui::GetIO().MousePos.x;
        double t = static_cast<double>((mx - trackX0) / trackW);
        t = std::clamp(t, 0.0, 1.0);
        if (t != *value01) { *value01 = t; changed = true; }
    }

    theme::pushBold(theme::fs::Micro);
    dl->AddText(theme::fonts().bold, theme::fs::Micro,
                ImVec2(p0.x + 10.0f, p0.y + (kH - ImGui::GetTextLineHeight()) * 0.5f),
                fillColor, label);
    theme::popFont();

    dl->AddRectFilled(ImVec2(trackX0, trackY), ImVec2(trackX0 + trackW, trackY + trackH),
                      theme::BgSunken, trackH * 0.5f);
    dl->AddRect(ImVec2(trackX0, trackY), ImVec2(trackX0 + trackW, trackY + trackH),
                theme::Border, trackH * 0.5f);

    // Gradient Info -> mau da chon: dung MULTICOLOR de tai hien dung
    // linear-gradient(90deg, info, primary) cua ban thiet ke.
    const float fillW = static_cast<float>(*value01) * trackW;
    if (fillW > 0.5f) {
        dl->AddRectFilledMultiColor(ImVec2(trackX0, trackY),
                                    ImVec2(trackX0 + fillW, trackY + trackH),
                                    theme::Info, fillColor, fillColor, theme::Info);
    }

    constexpr float kThumbR = 5.0f;
    ImVec2 thumb(std::clamp(trackX0 + fillW, trackX0, trackX0 + trackW),
                (trackY + trackY + trackH) * 0.5f);
    dl->AddCircleFilled(thumb, kThumbR, IM_COL32(255, 255, 255, 255));

    char pct[16];
    std::snprintf(pct, sizeof(pct), "%.0f%%", std::clamp(*value01, 0.0, 1.0) * 100.0);
    theme::pushMono(theme::fs::Micro);
    const ImVec2 pctSize = ImGui::CalcTextSize(pct);
    dl->AddText(ImVec2(p0.x + w - pctSize.x - 6.0f,
                       p0.y + (kH - pctSize.y) * 0.5f),
                theme::Text, pct);
    theme::popFont();

    return changed;
}

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
    // ★ Nut nay GIU NGUYEN chu, khong rut thanh icon tran.
    //
    //   No dung MOI layer cung luc — khan gia thay ngay. Mot icon khong
    //   nhan chi doc duoc neu nguoi dung da biet no la gi, va "da biet"
    //   khong phai thu dang dat cuoc khi hau qua la man hinh den giua
    //   buoi dien. Icon hop voi thao tac lap lai va co the hoan tac;
    //   khong hop voi thao tac pha huy.
    char clearLbl[64];
    std::snprintf(clearLbl, sizeof(clearLbl), "%s  %s",
                  ICON_LC_SQUARE, TR("comp.clearall"));
    if (theme::outlineButton(clearLbl, theme::Danger, ImVec2(0, 0))) {
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

        // ★ Hang tieu de gio mang CA thanh Opacity dung chung (cot 0)
        //   lan so cot COL 1..N (cac cot con lai). Hai thu nay o CUNG MOT
        //   hang vi ca hai deu la "hang tieu de" — cot 0 khong con de
        //   trong nua.
        //
        //   Truoc day moi layer co MOT thanh do mo rieng, chiem het mot
        //   hang trong o 90px cua no. Ban thiet ke doi sang MOT thanh
        //   DUNG CHUNG, luon hien o tren cung, dieu khien layer NAO DANG
        //   DUOC CHON (bam vao ten layer de chon) — do la ly do no nam o
        //   hang tieu de (dinh, luon thay) chu khong nam trong tung hang.
        // Khop voi ban thiet ke mikmap_UI_pug: thanh opacity dung chung o
        // sticky-left la h-10 = 40px (ImGui table can MOT chieu cao chung
        // cho ca hang tieu de nen dung cung so do cho cot COL ben phai).
        ImGui::TableNextRow(ImGuiTableRowFlags_Headers, 40.0f);
        ImGui::TableSetColumnIndex(0);
        {
            m_activeLayerRow = std::clamp(m_activeLayerRow, 0, layers - 1);
            Layer& activeLayer = comp.layer(m_activeLayerRow);
            const bool playing = (activeLayer.activeDeck == comp.viewedDeck()
                                  && activeLayer.activeColumn >= 0);

            char lbl[24];
            std::snprintf(lbl, sizeof(lbl), TR("layer.opacity.short"), m_activeLayerRow + 1);

            double opVal = activeLayer.opacity;
            if (drawSharedOpacityBar(lbl, &opVal,
                                    playing ? theme::Primary : theme::Info)) {
                activeLayer.opacity = opVal;
            }
        }
        for (int c = 0; c < cols; ++c) {
            ImGui::TableSetColumnIndex(c + 1);
            ImGui::PushID(20000 + c);
            char lbl[24];
            std::snprintf(lbl, sizeof(lbl), "COL %d", c + 1);
            theme::pushBold(theme::fs::Micro);
            const bool hitCol = theme::tabButton(lbl, false, ImVec2(-FLT_MIN, 18.0f),
                                                 theme::Primary);
            theme::popFont();
            if (hitCol) {
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

                // ★ Bam TEN layer -> chon lam "layer dang xem": thanh
                //   Opacity dung chung o tren doi sang dieu khien layer
                //   nay, va bang THUOC TINH nhay sang tab LAYER cua no —
                //   dung dieu nguoi van hanh mong doi khi vua bam chon
                //   mot layer khac.
                const bool isActiveRow = (L == m_activeLayerRow);
                theme::pushBold(theme::fs::Body);
                const bool hitName = theme::tabButton(nameBuf, isActiveRow,
                                                      ImVec2(0, 0), theme::Primary);
                theme::popFont();
                if (hitName) {
                    m_activeLayerRow = L;
                    m_inspectorTab   = InspectorTab::Layer;
                }

                constexpr float kBtnBlockW = 24.0f * 3.0f + 3.0f * 2.0f;
                constexpr float kBtnAnchor = 132.0f;   // < LayerCtrlW - kBtnBlockW
                ImGui::SameLine(0.0f, std::max(6.0f,
                    kBtnAnchor - ImGui::CalcTextSize(nameBuf).x));

                // Chu cai S/B/X chi doc duoc neu da biet chung la gi. Icon
                // + tooltip vua gon hon vua tu giai thich.
                const ImVec2 bs(24.0f, 20.0f);
                if (theme::tabButton(ICON_LC_FOCUS, layer.solo, bs, theme::Info)) {
                    layer.solo = !layer.solo;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.solo.tip"));
                ImGui::SameLine(0.0f, 3.0f);
                if (theme::tabButton(ICON_LC_EYE_OFF, layer.bypass, bs, theme::Danger)) {
                    layer.bypass = !layer.bypass;
                }
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.bypass.tip"));
                ImGui::SameLine(0.0f, 3.0f);
                if (theme::tabButton(ICON_LC_SQUARE, false, bs, theme::Danger)) layer.clear();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.clear.tip"));

                // ── Hang "the": BLEND + thoi luong chuyen, gon lai ──────
                //
                // ★ Opacity da chuyen sang thanh dung chung o tren; hang
                //   nay gio chi con hai dieu khien, ep vao mot the phang
                //   giong ban thiet ke thay vi hai dong rieng nhu truoc.
                ImGui::Dummy(ImVec2(1.0f, 4.0f));   // khe tho, giong "justify-between"

                ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgSunken));
                ImGui::BeginChild("##badge", ImVec2(theme::LayerCtrlW - 8.0f, 24.0f),
                                  ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);
                theme::pushBold(theme::fs::Micro);
                ImGui::AlignTextToFramePadding();
                ImGui::TextDisabled("%s", TR("layer.blend.label"));
                theme::popFont();

                ImGui::SameLine(0.0f, 4.0f);
                ImGui::SetNextItemWidth(78.0f);
                int blend = static_cast<int>(layer.blend);
                theme::pushBold(theme::fs::Micro);
                if (ImGui::Combo("##blend", &blend, kBlendNames, IM_ARRAYSIZE(kBlendNames))) {
                    layer.blend = static_cast<BlendMode>(blend);
                }
                theme::popFont();

                ImGui::SameLine(0.0f, 4.0f);
                ImGui::SetNextItemWidth(-FLT_MIN);
                float tr = static_cast<float>(layer.transitionDuration);
                theme::pushMono(theme::fs::Micro);
                if (ImGui::DragFloat("##tr", &tr, 0.01f, 0.0f, 10.0f, "%.2fs")) {
                    layer.transitionDuration = std::max(0.0f, tr);
                }
                theme::popFont();
                if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.transition.tip"));
                ImGui::EndChild();
                ImGui::PopStyleColor();
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

                // ★ Vien BorderLit (xam sang) cho o co clip nhin giong het
                //   mot khung DANG CHON du khong ai chon ca — dung dung
                //   mau Border binh thuong nhu ban thiet ke pug (border
                //   gray-800, gan nhu chim vao nen), chi Warning/Primary
                //   moi duoc noi bat that su (dang chon / dang phat).
                ImU32 bg     = theme::alpha(theme::BgSunken, 0.9f);
                ImU32 border = theme::alpha(theme::Border, 0.6f);
                if (playing)      { bg = theme::alpha(theme::Primary, 0.16f); border = theme::Primary; }
                else if (!empty)  { bg = theme::BgCard;                       border = theme::Border; }
                if (selected)     { border = theme::Warning; }

                ImGui::PushStyleColor(ImGuiCol_Button,        theme::v4(bg));
                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, theme::v4(theme::alpha(theme::Info, 0.20f)));
                ImGui::PushStyleColor(ImGuiCol_ButtonActive,  theme::v4(theme::alpha(theme::Info, 0.32f)));
                ImGui::PushStyleColor(ImGuiCol_Border,        theme::v4(border));

                // ★ Hai vung bam TACH RIENG, dung nhu anh minh hoa yeu cau:
                //   o MAU (thumbnail, phia tren) CHI de phat/chon — khong
                //   keo-tha, khong menu chuot phai, khong tooltip hover; o
                //   DEN (thanh nhan, phia duoi) moi la noi giu-keo, bam
                //   phai mo menu. Gop hai thu vao MOT nut duy nhat truoc
                //   day khien "chi dinh bam" o vung anh cung bi hieu nham
                //   thanh keo — tach han la cach fix dung goc.
                constexpr float kLabelH = 20.0f;
                constexpr float kProgH  = 3.0f;
                constexpr float kBlackH = kLabelH + kProgH;
                const float totalH = theme::LayerRowH - 8.0f;
                const float thumbH = std::max(0.0f, totalH - kBlackH);

                ImVec2 r0 = ImGui::GetCursorScreenPos();

                // ★ ImGui tu chen ItemSpacing.y GIUA hai nut xep chong —
                //   khong triet tieu thi o co clip cao hon o rong dung
                //   dung 1 khoang spacing, va lo ra mot khe nen bang o
                //   giua vung anh voi thanh nhan. Ep spacing = 0 CHI cho
                //   hai nut nay, tra lai binh thuong ngay sau do.
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(ImGui::GetStyle().ItemSpacing.x, 0.0f));

                if (!empty) {
                    if (ImGui::Button("##play", ImVec2(-FLT_MIN, thumbH))) {
                        // ★ Phat NGAY (len song that) VA chon lam muc tieu
                        //   xem truoc luon — nguoi van hanh vua bam la thay
                        //   ca hai man hinh PREVIEW lan ACTIVE cung khop,
                        //   khong phai bam them lan nua o thanh den.
                        comp.triggerClip(L, c);
                        m_selLayer  = L;
                        m_selColumn = c;
                    }
                }

                const float blackH = empty ? totalH : kBlackH;
                if (ImGui::Button("##black", ImVec2(-FLT_MIN, blackH))) {
                    m_selLayer  = L;
                    m_selColumn = c;
                }
                ImGui::PopStyleVar();

                const ImVec2 r1 = ImGui::GetItemRectMax();
                if (empty) r0 = ImGui::GetItemRectMin();   // o rong: mot nut duy nhat
                ImDrawList* dl = ImGui::GetWindowDrawList();

                // ── Keo-tha CHI TU/VAO o DEN: giu roi keo tha sang o khac de DOI CHO ──
                //
                // ★ Ke muc tieu da co clip thi HOAN DOI (khong ghi de mat du
                //   lieu); ke rong thi chi CHUYEN toi (o nguon thanh rong).
                if (!empty && ImGui::BeginDragDropSource()) {
                    const int payload[2] = { L, c };
                    ImGui::SetDragDropPayload("MIKMAP_CLIP", payload, sizeof(payload));
                    ImGui::TextUnformatted(clip.name.c_str());
                    ImGui::EndDragDropSource();
                }
                if (ImGui::BeginDragDropTarget()) {
                    dl->AddRect(r0, r1, theme::Warning, ImGui::GetStyle().FrameRounding,
                               0, 2.0f);
                    if (const ImGuiPayload* pl = ImGui::AcceptDragDropPayload("MIKMAP_CLIP")) {
                        const int* src = static_cast<const int*>(pl->Data);
                        const int srcL = src[0], srcC = src[1];
                        // ★ Chi cho keo trong CUNG mot layer — moi layer la
                        //   mot deck doc lap, keo cheo layer se lam mo hinh
                        //   "layer nao dang phat cot nao" roi tung, kho sua
                        //   dung cho ca hai layer cung luc.
                        if (srcL == L && srcC != c) {
                            Deck& deck = comp.deck(comp.viewedDeck());
                            const Clip moved = deck.clip(srcL, srcC);
                            const Clip here  = deck.clip(L, c);
                            const bool wasSwap = !here.isEmpty();
                            deck.setClip(L, c, moved);
                            if (wasSwap) deck.setClip(srcL, srcC, here);
                            else         deck.clearClip(srcL, srcC);

                            // ★ BAT BUOC: neu o nguon/dich dang la o LAYER
                            //   NAY dang phat, phai doi activeColumn theo
                            //   clip — khong thi layer tro vao mot o vua bi
                            //   xoa/doi du lieu va ra man hinh DEN ma khong
                            //   ai biet vi sao (day la loi that da gap).
                            const bool onThisDeck = (layer.activeDeck == comp.viewedDeck());
                            if (onThisDeck && layer.activeColumn == srcC) {
                                layer.activeColumn = c;
                            } else if (wasSwap && onThisDeck && layer.activeColumn == c) {
                                layer.activeColumn = srcC;
                            }
                        }
                    }
                    ImGui::EndDragDropTarget();
                }

                // ── Menu chuot phai (o DEN): chon file / nhan ban / sao chep / dan / xoa / doi ten / thong tin ──
                if (ImGui::BeginPopupContextItem("##clipctx")) {
                    if (ImGui::MenuItem(TR("clip.ctx.choose"))) {
                        m_pendingBrowse = true;
                        m_browseLayer   = L;
                        m_browseColumn  = c;
                    }
                    if (!empty) {
                        // Nhan ban: tim cot rong ke tiep tren CUNG layer nay,
                        // khong ghi de len o da co du lieu.
                        int dupCol = -1;
                        for (int k = 1; k < cols && dupCol < 0; ++k) {
                            const int cand = (c + k) % cols;
                            if (comp.deck(comp.viewedDeck()).clip(L, cand).isEmpty()) dupCol = cand;
                        }
                        if (ImGui::MenuItem(TR("clip.ctx.duplicate"), nullptr, false, dupCol >= 0)) {
                            comp.deck(comp.viewedDeck()).setClip(L, dupCol, clip);
                        }
                        if (ImGui::MenuItem(TR("clip.ctx.copy"))) {
                            m_clipClipboard    = clip;
                            m_hasClipClipboard = true;
                        }
                    }
                    if (m_hasClipClipboard) {
                        if (ImGui::MenuItem(TR("clip.ctx.paste"))) {
                            comp.deck(comp.viewedDeck()).setClip(L, c, m_clipClipboard);
                        }
                    }
                    if (!empty) {
                        ImGui::Separator();
                        if (ImGui::MenuItem(TR("clip.ctx.rename"))) {
                            m_renameLayer  = L;
                            m_renameColumn = c;
                            std::snprintf(m_renameBuf, sizeof(m_renameBuf), "%s", clip.name.c_str());
                            m_pendingRename = true;
                        }
                        if (ImGui::MenuItem(TR("clip.ctx.info"))) {
                            m_renameLayer  = L;
                            m_renameColumn = c;
                            m_pendingInfo  = true;
                        }
                        ImGui::Separator();
                        if (ImGui::MenuItem(TR("clip.ctx.delete"))) {
                            comp.deck(comp.viewedDeck()).clearClip(L, c);
                        }
                    }
                    ImGui::EndPopup();
                }

                if (empty) {
                    // ★ O rong khong ve gi ca — van bam CHON duoc (xu ly o
                    //   tren), nhung khong hien dau cong hay noi dung nao,
                    //   dung nhu ban thiet ke pug (o rong la mot khoang
                    //   toi tron, khong co "+" giua o).
                } else {
                    // ── Anh dai dien gia lap: chuyen sac cheo. Chua co thumbnail
                    //   that (phai giai ma mot frame roi thu nho — viec cho
                    //   MediaCache, khong phai cho tang giao dien), nhung mot
                    //   o clip co mau van de phan biet hon han mot o den tron.
                    //
                    //   Bo cuc khop ban thiet ke mikmap_UI_pug: vung anh phia
                    //   tren, thanh nhan h-5 (20px) phia duoi chua ten clip +
                    //   icon mat, thanh tien do h-1 (3px) o day cung khi dang
                    //   phat.
                    const ImVec2 thumb1(r1.x, r1.y - kBlackH);

                    const ImU32 c0 = playing ? theme::alpha(theme::Primary, 0.55f)
                                             : theme::alpha(theme::Info,    0.45f);
                    const ImU32 c1 = playing ? theme::alpha(theme::Warning, 0.30f)
                                             : theme::alpha(theme::Success, 0.25f);
                    dl->AddRectFilledMultiColor(r0, thumb1, c0, c1, c1, c0);
                    if (playing) {
                        dl->AddCircleFilled(ImVec2(r1.x - 9.0f, r0.y + 9.0f), 3.0f,
                                            theme::Primary);
                    }

                    // Thanh nhan (o DEN): CHI ve, khong tu tao vung bam rieng
                    // nua — ca thanh nay da la nut "##black" o tren roi.
                    const bool isPreviewSel = (L == m_selLayer && c == m_selColumn);
                    const ImVec2 lbl0(r0.x, thumb1.y);
                    const ImVec2 lbl1(r1.x, r1.y - kProgH);
                    dl->AddRectFilled(lbl0, lbl1, isPreviewSel
                                      ? theme::alpha(theme::Info, 0.28f)
                                      : theme::alpha(theme::BgCard, 0.95f));

                    dl->PushClipRect(r0, ImVec2(r1.x - 16.0f, lbl1.y), true);
                    dl->AddText(theme::fonts().bold, theme::fs::Micro,
                                ImVec2(r0.x + 5.0f, lbl0.y + (kLabelH - theme::fs::Micro) * 0.5f),
                                isPreviewSel ? theme::Info : theme::Text, clip.name.c_str());
                    dl->PopClipRect();

                    dl->AddText(ImVec2(r1.x - 15.0f, lbl0.y + (kLabelH - theme::fs::Micro) * 0.5f),
                               isPreviewSel ? theme::Info : theme::alpha(theme::TextDim, 0.8f),
                               ICON_LC_EYE);

                    if (playing) {
                        // Thanh tien do: nguoi van hanh phai biet clip con
                        // bao lau de chuyen tiep dung nhip.
                        const float t = static_cast<float>(
                            std::clamp(clip.transport.position, 0.0, 1.0));
                        dl->AddRectFilled(ImVec2(r0.x, r1.y - kProgH),
                                          ImVec2(r0.x + (r1.x - r0.x) * t, r1.y),
                                          theme::Primary);
                    }
                }

                // ★ Cac lop phu (thumbnail/nhan/tien do) ve SAU nut, nen de
                //   ra ngoai r0..r1 la de len DUNG vien ImGui vua ve — phai
                //   ve lai vien MOT LAN NUA tren cung, khong thi vien bi
                //   nuot mat (dung Fill roi moi Border, khong nguoc lai).
                dl->AddRect(r0, r1, border, ImGui::GetStyle().FrameRounding);

                ImGui::PopStyleColor(4);
                ImGui::PopID();
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();

    // ── Doi ten / Thong tin clip — mo TU menu chuot phai (o DEN) ───────
    //
    // ★ OpenPopup phai goi O DAY, ngoai vong lap qua tung o, vi BeginPopup
    //   ben duoi cung o day: hai ben phai CUNG mot tang ID-stack moi khop
    //   (vong lap dung PushID(L)/PushID(c) rieng cho tung o).
    if (m_pendingRename) { ImGui::OpenPopup("##renameclip"); m_pendingRename = false; }
    if (ImGui::BeginPopup("##renameclip")) {
        theme::sectionLabel(TR("clip.ctx.rename"));
        ImGui::SetNextItemWidth(220.0f);
        const bool enter = ImGui::InputText("##renamebuf", m_renameBuf, sizeof(m_renameBuf),
                                            ImGuiInputTextFlags_EnterReturnsTrue);
        const bool ok = enter || ImGui::Button(TR("dialog.ok"));
        ImGui::SameLine();
        const bool cancel = ImGui::Button(TR("dialog.cancel"));
        if (ok) {
            if (Clip* cptr = comp.deck(comp.viewedDeck()).clipPtr(m_renameLayer, m_renameColumn)) {
                cptr->name = m_renameBuf;
            }
            ImGui::CloseCurrentPopup();
        } else if (cancel) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    if (m_pendingInfo) { ImGui::OpenPopup("##clipinfo"); m_pendingInfo = false; }
    if (ImGui::BeginPopup("##clipinfo")) {
        theme::sectionLabel(TR("clip.ctx.info"));
        const Clip& info = comp.deck(comp.viewedDeck()).clip(m_renameLayer, m_renameColumn);
        ImGui::Text(TR("clip.where"), m_renameLayer + 1, m_renameColumn + 1);
        ImGui::Text("%s", info.name.c_str());
        ImGui::TextDisabled("%s", info.media.path.c_str());
        theme::pushMono(theme::fs::Small);
        ImGui::Text(TR("clip.mediainfo"), info.media.size.x, info.media.size.y,
                   info.media.durationSec);
        theme::popFont();
        ImGui::EndPopup();
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  TRANG 2 — ADVANCED MAPPING
// ═══════════════════════════════════════════════════════════════════════

// ── Cay SCREEN SETUP ───────────────────────────────────────────────────
//
// ★ Ba cap: Screen -> Slice -> Mask, moi cap MOT MAU.
//
//   Truoc day danh sach slice nam lan trong bang thuoc tinh, va mat na
//   la mot muc gap trong do. Nghia la khong co cho nao nhin duoc TOAN
//   BO cau truc mot lan — ma do lai chinh la thu nguoi van hanh can khi
//   dung nhieu may chieu: cai gi dang chieu ra dau.

void ControlPanel::drawScreenTree(Project& p, EditState& edit, UiActions& a) {
    theme::panelHeader(ICON_LC_LIST_TREE, TR("tree.title"), theme::Info);

    if (p.screens.empty()) {
        ImGui::TextDisabled("%s", TR("map.noscreen"));
        return;
    }
    m_activeScreen = std::clamp(m_activeScreen, 0,
                                static_cast<int>(p.screens.size()) - 1);

    // ★ Nut mat ngay tren hang, o CANH PHAI: bat/tat man chieu hoac slice
    //   ma khong phai bam vao roi lan xuong bang thuoc tinh moi thay cong
    //   tac. Ve SAU treeRow (InvisibleButton cua no phu het ca hang) nen
    //   vung nho nay o tren cung, an click truoc — dung ky thuat da dung
    //   cho o clip.
    auto visToggle = [&](bool& enabled) {
        const ImVec2 r0 = ImGui::GetItemRectMin();
        const ImVec2 r1 = ImGui::GetItemRectMax();
        constexpr float kEyeW = 22.0f;
        ImGui::SetCursorScreenPos(ImVec2(r1.x - kEyeW, r0.y));
        ImGui::InvisibleButton("##vis", ImVec2(kEyeW, r1.y - r0.y));
        const bool hov = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) enabled = !enabled;
        const char* icon = enabled ? ICON_LC_EYE : ICON_LC_EYE_OFF;
        const ImU32 col = hov ? theme::Text
                              : (enabled ? theme::TextDim : theme::alpha(theme::TextFaint, 0.8f));
        const ImVec2 isz = ImGui::CalcTextSize(icon);
        ImGui::GetWindowDrawList()->AddText(
            ImVec2(r1.x - kEyeW + (kEyeW - isz.x) * 0.5f, r0.y + (r1.y - r0.y - isz.y) * 0.5f),
            col, icon);
        if (hov) ImGui::SetTooltip("%s", TR("tree.vis.tip"));
    };

    for (int si = 0; si < static_cast<int>(p.screens.size()); ++si) {
        Screen& sc = p.screens[static_cast<std::size_t>(si)];
        ImGui::PushID(si);

        const bool screenSel = (si == m_activeScreen && m_selKind == SelKind::Screen);
        if (theme::treeRow(ICON_LC_MONITOR, sc.name.c_str(), 0, screenSel, theme::Info)) {
            m_activeScreen = si;
            m_selKind = SelKind::Screen;
        }
        visToggle(sc.enabled);

        for (int li = 0; li < sc.sliceCount(); ++li) {
            Slice& sl = sc.slices[static_cast<std::size_t>(li)];
            ImGui::PushID(li);

            const bool sliceSel = (si == m_activeScreen && li == edit.activeSliceIndex
                                   && m_selKind == SelKind::Slice);

            // Slice hong (warp khong nghich dao duoc) hien mau DO ngay tren
            // cay: khong phai mo tung slice ra moi biet cai nao gay loi.
            const ImU32 col = (sl.warp() != nullptr && !sl.warp()->isInvertible())
                              ? theme::Danger : theme::Primary;

            if (theme::treeRow(ICON_LC_CROP, sl.name.c_str(), 1, sliceSel, col)) {
                m_activeScreen = si;
                edit.activeSliceIndex = li;
                m_selKind = SelKind::Slice;
            }
            visToggle(sl.enabled);

            if (!sl.mask.nodes.empty()) {
                const bool maskSel = (si == m_activeScreen && li == edit.activeSliceIndex
                                      && m_selKind == SelKind::Mask);
                char lbl[64];
                std::snprintf(lbl, sizeof(lbl), "%s (%d)", TR("tree.mask"),
                              static_cast<int>(sl.mask.nodes.size()));

                if (theme::treeRow(ICON_LC_SCISSORS, lbl, 2, maskSel, theme::Warning)) {
                    m_activeScreen = si;
                    edit.activeSliceIndex = li;
                    m_selKind = SelKind::Mask;

                    // Chon mat na = muon sua no. Bat luon che do sua thay
                    // vi bat nguoi dung tim them mot cong tac nua.
                    edit.maskEditMode = true;
                    m_mapDragPoint = -1;
                }
            }
            ImGui::PopID();
        }
        ImGui::PopID();
    }
    (void)a;
}

// ── Bang thuoc tinh: doi theo LOAI item dang chon ──────────────────────

void ControlPanel::drawSliceSettings(Project& p, EditState& edit, UiActions& a) {
    if (p.screens.empty()) {
        theme::panelHeader(ICON_LC_CROP, TR("slice.settings"), theme::Primary);
        ImGui::TextDisabled("%s", TR("map.noscreen"));
        return;
    }
    m_activeScreen = std::clamp(m_activeScreen, 0,
                                static_cast<int>(p.screens.size()) - 1);
    Screen& sc = p.screens[static_cast<std::size_t>(m_activeScreen)];

    switch (m_selKind) {
    case SelKind::Screen:
        theme::panelHeader(ICON_LC_MONITOR, TR("screen.settings"), theme::Info);
        drawScreenProperties(sc, edit, a);
        break;

    case SelKind::Mask:
        theme::panelHeader(ICON_LC_SCISSORS, TR("mask.settings"), theme::Warning);
        if (edit.activeSliceIndex >= 0 && edit.activeSliceIndex < sc.sliceCount()) {
            drawMaskPanel(sc.slices[static_cast<std::size_t>(edit.activeSliceIndex)], edit);
        } else {
            ImGui::TextDisabled("%s", TR("adv.noslice"));
        }
        break;

    default:
        theme::panelHeader(ICON_LC_CROP, TR("slice.settings"), theme::Primary);
        drawScreenPanel(p, edit, a);
        break;
    }
}

// ── Thuoc tinh cua SCREEN ──────────────────────────────────────────────

void ControlPanel::drawScreenProperties(Screen& sc, EditState& edit, UiActions& a) {
    (void)edit;

    char nameBuf[64];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", sc.name.c_str());
    labelAbove(TR("screen.name"));
    if (ImGui::InputText("##scname", nameBuf, sizeof(nameBuf))) sc.name = nameBuf;

    ImGui::Checkbox(TR("screen.enabled"), &sc.enabled);

    ImGui::Separator();
    theme::sectionLabel(TR("screen.routing"));

    // Do phan giai ao cua screen. So lieu -> font mono.
    ImGui::TextDisabled("%s", TR("screen.resolution"));
    ImGui::SameLine();
    pushMono();
    ImGui::TextColored(theme::v4(theme::Success), "%.0f x %.0f",
                       sc.resolution.x, sc.resolution.y);
    popMono();

    for (const DisplayEntry& d : m_displays) {
        ImGui::PushID(d.index);
        char lbl[128];
        std::snprintf(lbl, sizeof(lbl), "%s %d. %s  %dx%d",
                      ICON_LC_MONITOR, d.index + 1, d.name.c_str(), d.w, d.h);
        if (theme::tabButton(lbl, d.index == m_activeOutputDisplay,
                             ImVec2(-FLT_MIN, 0), theme::Info)) {
            a.sendOutputToDisplay = d.index;
        }
        ImGui::PopID();
    }
    if (m_displays.empty()) ImGui::TextDisabled("%s", TR("menu.output.nodisplay"));

    ImGui::Separator();
    theme::sectionLabel(TR("screen.slices"));

    // Danh sach slice cua screen nay — bam de chon, khong phai quay ra cay.
    for (int i = 0; i < sc.sliceCount(); ++i) {
        ImGui::PushID(5000 + i);
        char lbl[80];
        std::snprintf(lbl, sizeof(lbl), "%s %s", ICON_LC_CROP,
                      sc.slices[static_cast<std::size_t>(i)].name.c_str());
        if (theme::tabButton(lbl, false, ImVec2(-FLT_MIN, 0), theme::Primary)) {
            edit.activeSliceIndex = i;
            m_selKind = SelKind::Slice;
        }
        ImGui::PopID();
    }

    if (theme::outlineButton(TR("adv.addslice"), theme::Success, ImVec2(-FLT_MIN, 0))) {
        a.addSliceToScreen = m_activeScreen;
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  Thanh cong cu trang Mapping
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

    // ── Trai ───────────────────────────────────────────────────────────
    if (theme::iconButton(ICON_LC_LIST_TREE, m_showMapTree)) {
        m_showMapTree = !m_showMapTree;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("tree.toggle"));

    ImGui::SameLine(0.0f, 10.0f);
    if (theme::iconButton(ICON_LC_MOUSE_POINTER_2, !edit.maskEditMode, theme::Text)) {
        edit.maskEditMode = false;
        edit.maskDraggedNode = -1;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("tool.select"));

    ImGui::SameLine(0.0f, 4.0f);
    if (theme::iconButton(ICON_LC_SCISSORS, edit.maskEditMode, theme::Warning)) {
        edit.maskEditMode = !edit.maskEditMode;
        m_mapDragPoint = -1;
        edit.maskDraggedNode = -1;
        if (edit.maskEditMode) m_selKind = SelKind::Mask;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("mask.edit.tip"));

    // F21 — nam canh hai cong cu kia vi no cung la mot CHE DO cua thao tac
    // keo, khong phai mot lenh chay mot lan.
    ImGui::SameLine(0.0f, 4.0f);
    if (theme::iconButton(ICON_LC_MAGNET, m_snapEnabled, theme::Success)) {
        m_snapEnabled = !m_snapEnabled;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("tool.snap.tip"));

    // ── Dai chon nhanh SCREEN (chi hien khi co hon 1 man chieu) ─────────
    //
    // ★ Them Screen/Slice/Mask da chuyen xuong day cot cay — o day gio
    //   chi con dai pill de NHAY nhanh giua cac may chieu ma khong phai
    //   voi tay sang cot cay, giong ban thiet ke tham khao.
    if (p.screens.size() > 1) {
        ImGui::SameLine(0.0f, 12.0f);
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgSunken));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(2, 2));
        ImGui::BeginChild("##screenpills", ImVec2(0.0f, theme::ToolBarH - 10.0f),
                          ImGuiChildFlags_Borders | ImGuiChildFlags_AutoResizeX,
                          ImGuiWindowFlags_NoScrollbar);
        theme::pushBold(theme::fs::Micro);
        for (int i = 0; i < static_cast<int>(p.screens.size()); ++i) {
            if (i > 0) ImGui::SameLine(0.0f, 0.0f);
            ImGui::PushID(6000 + i);
            if (theme::tabButton(p.screens[static_cast<size_t>(i)].name.c_str(),
                                 i == m_activeScreen, ImVec2(0, 0), theme::Primary)) {
                m_activeScreen = i;
            }
            ImGui::PopID();
        }
        theme::popFont();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    // ── Giua: cong tac VUNG LAY / DUONG RA ─────────────────────────────
    //
    // ★ Hai nua cua CUNG mot thao tac, nen dung chung mot khung nhin:
    //   "lay phan nao cua hinh" (INPUT) va "dat no o dau tren vat the"
    //   (OUTPUT). Truoc day chi chinh duoc OUTPUT bang chuot, con INPUT
    //   phai go so — trong khi ca hai deu la viec keo hinh chu nhat.
    {
        constexpr float kModeW = 330.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX((ImGui::GetWindowWidth() - kModeW) * 0.5f);

        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgSunken));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(3, 3));
        ImGui::BeginChild("##mapmode", ImVec2(kModeW, theme::ToolBarH - 10.0f),
                          ImGuiChildFlags_Borders, ImGuiWindowFlags_NoScrollbar);

        const ImVec2 msz(157.0f, 0.0f);
        theme::pushBold(theme::fs::Small);
        if (theme::tabButton(TR("map.mode.input"), m_mapMode == MapMode::Input,
                             msz, theme::Success)) {
            m_mapMode = MapMode::Input;
        }
        ImGui::SameLine(0.0f, 2.0f);
        if (theme::tabButton(TR("map.mode.output"), m_mapMode == MapMode::Output,
                             msz, theme::Primary)) {
            m_mapMode = MapMode::Output;
        }

        theme::popFont();
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
    }

    // ── Phai ───────────────────────────────────────────────────────────
    {
        const float rightW = 250.0f + 150.0f;
        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(),
                                      ImGui::GetWindowWidth() - rightW));

        // Reset khung keystone/mesh cua slice dang chon ve full-frame.
        char resetLbl[48];
        std::snprintf(resetLbl, sizeof(resetLbl), "%s  %s", ICON_LC_ROTATE_CCW, TR("adv.reset"));
        if (theme::tabButton(resetLbl, false, ImVec2(0, 0))) {
            a.resetActiveSliceWarp = true;
        }
        ImGui::SameLine(0.0f, 4.0f);
        if (theme::toolButton(ICON_LC_TRASH_2, TR("adv.delete"), theme::Danger)) {
            a.removeSliceIndex = edit.activeSliceIndex;
        }
        ImGui::SameLine(0.0f, 10.0f);

        if (theme::tabButton(TR("tool.testcard"), edit.showGrid, ImVec2(0, 0), theme::Info)) {
            edit.showGrid = !edit.showGrid;
        }
        ImGui::SameLine(0.0f, 8.0f);

        // ★ Nut NEN DAC duy nhat cua ca giao dien.
        //
        //   Ban thiet ke danh mau nen cam cho rieng "Apply". O day no
        //   nghia la DUA HINH RA MAY CHIEU — hanh dong quan trong nhat
        //   cua ca trang, va la thu nguoi van hanh tim luc gap.
        if (theme::filledButton(TR("tool.apply"), theme::Primary, ImVec2(78, 0))) {
            a.toggleFullscreen = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("tool.apply.tip"));

        ImGui::SameLine(0.0f, 10.0f);
        if (theme::iconButton(ICON_LC_LAYOUT_GRID, m_showMapSidebar)) {
            m_showMapSidebar = !m_showMapSidebar;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("map.sidebar"));
    }

    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void ControlPanel::drawMappingView(Project& p, EditState& edit,
                                   const ofTexture* canvasTex, UiActions& a) {
    drawMapToolbar(p, edit, a);

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(6, 6));

    // ── Trai: cay SCREEN SETUP ─────────────────────────────────────────
    //
    // ★ Ba nut Them Screen/Slice/Mask ghim o DAY cot cay, khong nam lan
    //   trong thanh cong cu tren dau — day la cho nguoi dung nhin vao
    //   DAU TIEN khi can them mot muc moi vao cay, giong ban thiet ke
    //   tham khao (MikMap_Web AdvancedMappingView).
    if (m_showMapTree) {
        ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgPanel));
        ImGui::BeginChild("##screentree", ImVec2(theme::TreeW, 0), ImGuiChildFlags_Borders);
        {
            constexpr float kFooterH = 98.0f;
            ImGui::BeginChild("##screentree_scroll", ImVec2(0, -kFooterH));
            drawScreenTree(p, edit, a);
            ImGui::EndChild();

            ImGui::Separator();
            char lbl[64];
            std::snprintf(lbl, sizeof(lbl), "%s  %s", ICON_LC_MONITOR, TR("adv.addscreen"));
            if (theme::outlineButton(lbl, theme::Primary, ImVec2(-FLT_MIN, 0))) {
                a.addScreen = true;
            }
            std::snprintf(lbl, sizeof(lbl), "%s  %s", ICON_LC_PLUS, TR("tool.addslice"));
            if (theme::tabButton(lbl, false, ImVec2(-FLT_MIN, 0), theme::Success)) {
                a.addSliceToScreen = m_activeScreen;
            }
            std::snprintf(lbl, sizeof(lbl), "%s  %s", ICON_LC_SCISSORS, TR("tool.addmask"));
            if (theme::tabButton(lbl, false, ImVec2(-FLT_MIN, 0), theme::Warning)) {
                a.addMask = true;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleColor();
        ImGui::SameLine();
    }

    const float sideW = m_showMapSidebar ? theme::InspectorW : 0.0f;
    const float workW = std::max(200.0f,
        ImGui::GetContentRegionAvail().x - sideW - (m_showMapSidebar ? 6.0f : 0.0f));

    // Nen canvas #050505 theo bang mau — toi hon bang, de khung lam viec
    // lui ra sau va noi dung noi len.
    ImGui::PushStyleColor(ImGuiCol_ChildBg, theme::v4(theme::BgSunken));
    ImGui::BeginChild("##workspace", ImVec2(workW, 0), ImGuiChildFlags_Borders,
                      ImGuiWindowFlags_NoScrollbar);
    {
        // Nhan noi cho biet dang nhin vao KHONG GIAN NAO. Hai che do trong
        // rat giong nhau — deu la hinh chu nhat tren nen toi — nen thieu
        // nhan thi rat de chinh nham khong gian.
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
        theme::panelHeader(ICON_LC_CPU, TR("sen.devices"), theme::Info);
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
        theme::panelHeader(ICON_LC_WORKFLOW, TR("cal.section"), theme::Info);
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

    // ★ Danh sach slice (chon + ten + ON/solo/xoa) tung nam o day gio bo
    //   han: cay SCREEN SETUP da chon duoc slice (kem icon mat cho ON),
    //   thanh cong cu tren dau da co nut Xoa cho slice dang chon. Bang
    //   nay CHI con thuoc tinh cua MOT slice — cai dang duoc chon o cay.
    Screen& sc = p.screens[static_cast<size_t>(m_activeScreen)];

    if (edit.activeSliceIndex < 0 || edit.activeSliceIndex >= sc.sliceCount()) {
        ImGui::TextDisabled("%s", TR("adv.noslice"));
        return;
    }

    Slice& s = sc.slices[static_cast<size_t>(edit.activeSliceIndex)];

    char sliceNameBuf[64];
    std::snprintf(sliceNameBuf, sizeof(sliceNameBuf), "%s", s.name.c_str());
    labelAbove(TR("slice.name"));
    if (ImGui::InputText("##slicename", sliceNameBuf, sizeof(sliceNameBuf))) {
        s.name = sliceNameBuf;
    }

    if (s.warp() != nullptr && !s.warp()->isInvertible()) {
        ImGui::PushStyleColor(ImGuiCol_Text, theme::v4(theme::Danger));
        ImGui::TextWrapped("%s", TR("adv.broken"));
        ImGui::PopStyleColor();
    }
    ImGui::Separator();

    // ── F22: lấy nội dung từ đâu ───────────────────────────────────────
    {
        const int layerCount = p.composition.layerCount();

        int kind = (s.sourceKind == Slice::SourceKind::Layer) ? 1 : 0;
        const char* kinds[] = {TR("adv.src.comp"), TR("adv.src.layer")};
        labelAbove(TR("adv.source"));
        ImGui::SetNextItemWidth(-FLT_MIN);
        if (ImGui::Combo("##srckind", &kind, kinds, 2)) {
            s.sourceKind = (kind == 1) ? Slice::SourceKind::Layer
                                       : Slice::SourceKind::Composition;
            // Lần đầu chọn Layer mà chưa có chỉ số thì lấy layer trên
            // cùng — đó là layer người dùng hay đang làm việc nhất.
            if (s.sourceKind == Slice::SourceKind::Layer && s.sourceLayer < 0) {
                s.sourceLayer = std::max(0, layerCount - 1);
            }
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("adv.source.tip"));

        if (s.sourceKind == Slice::SourceKind::Layer) {
            int layer = std::clamp(s.sourceLayer, 0, std::max(0, layerCount - 1));
            labelAbove(TR("adv.src.which"));
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ImGui::DragInt("##srclayer", &layer, 0.1f, 0,
                               std::max(0, layerCount - 1))) {
                s.sourceLayer = layer;
            }

            // ★ Cảnh báo khi chỉ số trỏ vào layer không còn tồn tại.
            //   Không có dòng này thì slice lặng lẽ lùi về composition và
            //   người vận hành thấy "nó chiếu nhầm hình" mà không hiểu vì
            //   sao — trong khi nguyên nhân chỉ là đã xoá bớt layer.
            if (s.effectiveSourceLayer(layerCount) < 0) {
                ImGui::PushStyleColor(ImGuiCol_Text, theme::v4(theme::Warning));
                ImGui::TextWrapped("%s", TR("adv.src.missing"));
                ImGui::PopStyleColor();
            }
        }
    }

    // F4 — vùng lấy trên canvas
    float ox[2] = {static_cast<float>(s.inputOrigin.x), static_cast<float>(s.inputOrigin.y)};
    labelAbove(TR("adv.inputorigin"));
    theme::pushMono(theme::fs::Body);
    const bool inorg = ImGui::DragFloat2("##inorg", ox, 1.0f);
    theme::popFont();
    if (inorg) s.inputOrigin = Vec2{ox[0], ox[1]};
    float sz[2] = {static_cast<float>(s.inputSize.x), static_cast<float>(s.inputSize.y)};
    labelAbove(TR("adv.inputsize"));
    theme::pushMono(theme::fs::Body);
    const bool insz = ImGui::DragFloat2("##insz", sz, 1.0f, 1.0f, 16384.0f);
    theme::popFont();
    if (insz) s.inputSize = Vec2{std::max(1.0f, sz[0]), std::max(1.0f, sz[1])};

    // Đổi loại warp. Thứ tự PHẢI khớp với AppController::applyUiActions.
    int warpType = 0;
    switch (s.warp()->type()) {
    case WarpType::Mesh:   warpType = 1; break;
    case WarpType::Bezier: warpType = 2; break;
    default:               warpType = 0; break;
    }
    const char* warpNames[] = {"Corner pin", "Mesh", "Bezier"};
    labelAbove(TR("adv.warptype"));
    if (ImGui::Combo("##warptype", &warpType, warpNames, 3)) {
        a.convertWarpTo = warpType;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("adv.warptype.tip"));
    char rlbl[64];
    std::snprintf(rlbl, sizeof(rlbl), "%s  %s", ICON_LC_ROTATE_CCW, TR("adv.reset"));
    if (ImGui::Button(rlbl, ImVec2(-FLT_MIN, 0))) a.resetActiveSliceWarp = true;

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

        double opVal = s.color.opacity;
        if (theme::opacityBar("##sop", TR("adv.color.opacity"), &opVal,
                              theme::Primary, false)) {
            s.color.opacity = opVal;
        }

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
            theme::pushMono(theme::fs::Body);
            const bool moved = ImGui::DragFloat2("##corner", c, 0.5f);
            theme::popFont();
            if (moved) {
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

    char mlbl[64];
    std::snprintf(mlbl, sizeof(mlbl), "%s  %s", ICON_LC_SQUARE, TR("mask.shape.rect"));
    if (ImGui::Button(mlbl, ImVec2(-FLT_MIN, 0))) {
        const bool inv = m.invert; const double ft = m.feather;
        m = BezierMask::rectangle(0.10);
        m.invert = inv; m.feather = ft;
        edit.maskEditMode = true;
    }
    std::snprintf(mlbl, sizeof(mlbl), "%s  %s", ICON_LC_CIRCLE, TR("mask.shape.ellipse"));
    if (ImGui::Button(mlbl, ImVec2(-FLT_MIN, 0))) {
        const bool inv = m.invert; const double ft = m.feather;
        m = BezierMask::ellipse(4);
        m.invert = inv; m.feather = ft;
        edit.maskEditMode = true;
    }
    std::snprintf(mlbl, sizeof(mlbl), "%s  %s", ICON_LC_TRASH_2, TR("mask.clear"));
    if (ImGui::Button(mlbl, ImVec2(-FLT_MIN, 0))) {
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

    static int sensorType = 0;
    // Cot nay chi rong 300px: nhan len tren, nut chiem het be ngang.
    // Xep ngang ca combo + nhan + nut + trang thai thi dong bi cat.
    // Thu tu PHAI khop voi AppController::startSensor: 0 mock, 1 OSC, 2 TUIO.
    const char* types[] = {TR("sen.type.mock"), TR("sen.type.osc"),
                           TR("sen.type.tuio")};
    labelAbove(TR("sen.source"));
    if (ImGui::Combo("##sensrc", &sensorType, types, 3)) a.sensorTypeIndex = sensorType;

    if (s.sensorConnected) {
        char lbl[64];
        std::snprintf(lbl, sizeof(lbl), "%s  %s", ICON_LC_POWER, TR("sen.stop"));
        if (theme::outlineButton(lbl, theme::Danger, ImVec2(-FLT_MIN, 0))) {
            a.stopSensor = true;
        }
        theme::statusDot(theme::Success);
        ImGui::SameLine(0.0f, 6.0f);
        ImGui::AlignTextToFramePadding();
        ImGui::TextColored(theme::v4(theme::Success), "%s", TR("sen.running"));
    } else {
        char lbl[64];
        std::snprintf(lbl, sizeof(lbl), "%s  %s", ICON_LC_POWER, TR("sen.run"));
        if (theme::outlineButton(lbl, theme::Success, ImVec2(-FLT_MIN, 0))) {
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

    if (theme::toolButton(ICON_LC_CROSSHAIR, TR("cal.add"), theme::Success)) {
        a.calibAddPoint = true;
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (theme::toolButton(ICON_LC_SKIP_FORWARD, TR("cal.skip"), theme::TextDim)) {
        a.calibNextTarget = true;
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (theme::toolButton(ICON_LC_CHECK, TR("cal.solve"), theme::Primary)) {
        a.calibSolve = true;
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (theme::toolButton(ICON_LC_TRASH_2, TR("cal.clear"), theme::Danger)) {
        a.calibClear = true;
    }

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
    // Toan bo bang nay la SO LIEU — mono tu dau den cuoi.
    pushMono();
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
    popMono();
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

    // Ba nut dieu khien phat la ky hieu QUOC TE — tam giac, hai gach,
    // hinh vuong. Viet chu ra chi ton cho ma khong ro hon.
    const bool playing = c->transport.isPlaying();
    if (theme::toolButton(playing ? ICON_LC_PAUSE : ICON_LC_PLAY,
                          playing ? TR("clip.pause") : TR("clip.play"),
                          playing ? theme::Warning : theme::Success)) {
        c->transport.togglePlay();
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (theme::toolButton(ICON_LC_SQUARE, TR("clip.stop"), theme::Danger)) {
        c->transport.stop();
    }
    ImGui::SameLine(0.0f, 4.0f);
    if (theme::toolButton(ICON_LC_ROTATE_CCW, TR("clip.resetxform"), theme::Info)) {
        c->transform.reset();
    }

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

    // ── D3 D4: hoa tron ────────────────────────────────────────────────
    ImGui::SeparatorText(TR("clip.blending"));

    double opVal = c->opacity;
    if (theme::opacityBar("##clipop", TR("clip.opacity"), &opVal,
                          theme::Primary, false)) {
        c->opacity = opVal;
    }

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

    char zlbl[64];
    std::snprintf(zlbl, sizeof(zlbl), "%s  %s", ICON_LC_PLUS, TR("zone.add"));
    if (theme::outlineButton(zlbl, theme::Success, ImVec2(-FLT_MIN, 0))) {
        a.addTriggerZone = true;
    }
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
            if (theme::toolButton(ICON_LC_TRASH_2, TR("zone.delete"), theme::Danger)) {
                a.removeTriggerZone = i;
            }

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

    // ★ Rieng 4 goc Corner Pin ve TO va co NHAN toa do — day la thao tac
    //   can chinh xac nhat va lam nhieu nhat (keystone), nen phai de bat
    //   ra ngay, khong lan giua cac diem mesh nho. Mesh/Bezier van dung
    //   dau cham nho nhu cu, chi day nguoi dung.
    const bool isCornerPin = (s.warp()->type() == WarpType::CornerPin);
    const int n = w->controlPointCount();
    const float r = isCornerPin ? 11.0f : (n > 25 ? 4.0f : 6.5f);
    static const char* kCornerTag[4] = {"TL", "TR", "BR", "BL"};

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

        // ── F21: hut ve duong gion ─────────────────────────────────────
        //
        // ★ Giu ALT de TAM TAT hut. Bat buoc phai co: khi hai moc nam sat
        //   nhau, co dung mot vi tri chi cach moc vai pixel la viec that —
        //   vd co y de ho mot khe 3px giua hai slice. Khong co duong thoat
        //   thi nguoi dung phai tat hut o bang thuoc tinh roi bat lai, va
        //   giua buoi dien thi khong ai lam vay.
        const bool bypass = ImGui::GetIO().KeyAlt;
        if (m_snapEnabled && !bypass) {
            std::vector<double> gx, gy;

            // Moc 1 — khung may chieu: hai mep va duong giua.
            gx.push_back(0.0);
            gx.push_back(sc.resolution.x);
            gx.push_back(sc.resolution.x * 0.5);
            gy.push_back(0.0);
            gy.push_back(sc.resolution.y);
            gy.push_back(sc.resolution.y * 0.5);

            // Moc 2 — diem dieu khien CUA CAC SLICE KHAC. Day moi la moc
            // hay dung nhat: ghep hai may chieu thi mep phai cua slice nay
            // phai trung mep trai cua slice kia toi tung pixel.
            for (int si = 0; si < sc.sliceCount(); ++si) {
                if (si == edit.activeSliceIndex) continue;
                const IWarp* ow = sc.slices[static_cast<std::size_t>(si)].warp();
                if (ow == nullptr) continue;
                for (int k = 0; k < ow->controlPointCount(); ++k) {
                    const Vec2 q = ow->controlPointAt(k);
                    gx.push_back(q.x);
                    gy.push_back(q.y);
                }
            }

            // Moc 3 — cac diem KHAC cua chinh slice nay, tru diem dang keo.
            for (int k = 0; k < n; ++k) {
                if (k == m_mapDragPoint) continue;
                const Vec2 q = w->controlPointAt(k);
                gx.push_back(q.x);
                gy.push_back(q.y);
            }

            // ★ Nguong tinh theo PIXEL MAN HINH roi doi ve don vi output.
            //   Do chinh xac cua ban tay la hang so theo pixel man hinh;
            //   neu nguong tinh theo don vi output thi thu nho khung nhin
            //   se lam hut khong bao gio an, con phong to thi hut loan xa.
            const double thr = snapThresholdFor(8.0, static_cast<double>(scale));
            const SnapResult snapped = snapPoint(target, gx, gy, thr);
            target = snapped.position;

            // Ve duong gion dang hut, de nguoi dung THAY minh thang hang
            // voi cai gi — khong co no thi diem tu nhien "dinh" lai mot
            // cach kho hieu.
            if (snapped.snappedX) {
                const float sx = toWidget(Vec2{snapped.guideX, 0.0}).x;
                dl->AddLine(ImVec2(sx, origin.y),
                            ImVec2(sx, origin.y + drawH),
                            theme::alpha(theme::Warning, 0.75f), 1.0f);
            }
            if (snapped.snappedY) {
                const float sy = toWidget(Vec2{0.0, snapped.guideY}).y;
                dl->AddLine(ImVec2(origin.x, sy),
                            ImVec2(origin.x + drawW, sy),
                            theme::alpha(theme::Warning, 0.75f), 1.0f);
            }
        }

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

        if (isCornerPin) {
            // ★ Nhu ban thiet ke tham khao: quang sang + vien trang de noi
            //   bat tren moi nen, cong nhan "TL (x,y)" ngay duoi diem —
            //   nguoi van hanh doc duoc toa do ma khong phai lien mat qua
            //   bang thuoc tinh ben canh.
            dl->AddCircleFilled(q, r + 5.0f, theme::alpha(c, 0.22f));
            dl->AddCircleFilled(q, r, c);
            dl->AddCircle(q, r, IM_COL32(255, 255, 255, 235), 0, 2.5f);

            char lbl[48];
            std::snprintf(lbl, sizeof(lbl), "%s (%.0f,%.0f)", kCornerTag[k], cp.x, cp.y);
            theme::pushBold(theme::fs::Micro);
            const ImVec2 lsz = ImGui::CalcTextSize(lbl);
            const ImVec2 lp(q.x - lsz.x * 0.5f, q.y + r + 6.0f);
            dl->AddRectFilled(ImVec2(lp.x - 3.0f, lp.y - 1.0f),
                              ImVec2(lp.x + lsz.x + 3.0f, lp.y + lsz.y + 1.0f),
                              IM_COL32(10, 10, 10, 200), 2.0f);
            dl->AddText(lp, IM_COL32(255, 255, 255, 255), lbl);
            theme::popFont();
        } else {
            dl->AddCircleFilled(q, r, c);
            dl->AddCircleFilled(q, r * 0.4f, IM_COL32(20, 20, 20, 255));
        }
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

    ImGuiIO& io = ImGui::GetIO();
    const ImWchar* viRanges = io.Fonts->GetGlyphRangesVietnamese();

    // ★★ Loai vung PRIVATE USE khoi cac font CHU.
    //
    //   Inter chua 745 glyph o vung private-use (U+E000..F8FF) — bien the
    //   kieu chu ma du an nay khong dung. Lucide dat icon CUNG vung do.
    //
    //   ImGui 1.92 hoi cac nguon font THEO THU TU va nguon dau tien co
    //   glyph la thang. Inter dung truoc, nen no chiem mat 4 icon:
    //     U+E0AB  crop            -> Inter "d.subs"              (chu 'd')
    //     U+E13D  plus            -> Inter "dngb_ballotx.circled"(dau X)
    //     U+E14E  scissors        -> Inter "exclam.squared"      (dau !)
    //     U+E1C3  mouse-pointer-2 -> Inter "uni25AA.case"        (o vuong)
    //
    //   Trieu chung cuc ky de doc nham: MOT SO icon dung, mot so ra ky tu
    //   la — nhin nhu font icon hong hoac lech phien ban, trong khi ca hai
    //   font deu hoan toan binh thuong. Phai doc cmap ca hai file moi ra.
    //
    //   Mang phai SONG LAU bang font nen phai static.
    static const ImWchar kNoPrivateUse[] = { 0xE000, 0xF8FF, 0 };
    static const ImWchar kIconRanges[]   = { ICON_MIN_LC, ICON_MAX_16_LC, 0 };

    ImFontConfig txt;
    txt.OversampleH = 2;
    txt.OversampleV = 1;
    txt.PixelSnapH  = true;
    txt.GlyphExcludeRanges = kNoPrivateUse;

    ImFontConfig ico;
    ico.MergeMode        = true;
    ico.PixelSnapH       = true;
    ico.OversampleH      = 2;
    ico.OversampleV      = 1;
    ico.GlyphOffset      = ImVec2(0.0f, 2.0f);   // khop duong chan chu
    ico.GlyphMinAdvanceX = 16.0f;                // moi icon rong bang nhau

    // ── Nap MOT do dam: font chu + icon ghep vao chinh no ──────────────
    //
    // ★ Icon phai ghep vao TUNG do dam. MergeMode ghep vao font NAP GAN
    //   NHAT, nen neu chi ghep vao Regular thi moi cho dung chu dam se
    //   mat sach icon — va mat mot cach lang le, chi hien o vuong.
    auto loadWeight = [&](const char* file, bool asDefault) -> ImFont* {
        const std::string path = ofToDataPath(std::string("fonts/") + file, true);
        if (!ofFile::doesFileExist(path)) return nullptr;

        // ★ PHAI di qua m_gui.addFont(), khong duoc goi thang
        //   io.Fonts->AddFontFromFileTTF(): goi thang thi font vao duoc
        //   atlas nhung atlas khong bao gio duoc build lai va nap len GPU.
        ImFont* f = m_gui.addFont(path, theme::fs::Body, &txt, viRanges, asDefault);
        if (f == nullptr) return nullptr;

        const std::string iconPath = ofToDataPath("fonts/lucide.ttf", true);
        if (ofFile::doesFileExist(iconPath)) {
            m_gui.addFont(iconPath, theme::fs::Body, &ico, kIconRanges, false);
        }
        return f;
    };

    theme::Fonts fonts;
    fonts.regular  = loadWeight("Inter-Regular.ttf",  true);
    fonts.semibold = loadWeight("Inter-SemiBold.ttf", false);
    fonts.bold     = loadWeight("Inter-Bold.ttf",     false);

    if (fonts.regular == nullptr) {
        // Luoi an toan: font he thong. Mat do dam va mat icon, nhung van
        // doc duoc tieng Viet va van chay.
        ImFont* sys = nullptr;
        for (const char* p2 : { "C:/Windows/Fonts/segoeui.ttf",
                                "C:/Windows/Fonts/tahoma.ttf" }) {
            if (!ofFile::doesFileExist(p2)) continue;
            sys = m_gui.addFont(p2, theme::fs::Body, &txt, viRanges, true);
            if (sys != nullptr) break;
        }
        fonts.regular = fonts.semibold = fonts.bold = sys;
        ofLogWarning("ControlPanel")
            << "Thieu bin/data/fonts/Inter-*.ttf - dung font he thong, mat icon";
    }

    // ── Font mono cho SO LIEU ──────────────────────────────────────────
    //
    // Chu so cua font ti le co be rong khac nhau, nen mot gia tri doi 60
    // lan/giay se nhay qua nhay lai va rat kho doc luot — dung luc can
    // liec nhanh xem fps co tut khong.
    const std::string monoPath = ofToDataPath("fonts/RobotoMono.ttf", true);
    if (ofFile::doesFileExist(monoPath)) {
        ImFontConfig mono = txt;
        fonts.mono = m_gui.addFont(monoPath, theme::fs::Body, &mono, viRanges, false);
    }
    if (fonts.mono == nullptr) fonts.mono = fonts.regular;

    theme::setFonts(fonts);
    m_fontLoaded = (fonts.regular != nullptr);

    if (m_fontLoaded) {
        ofLogNotice("ControlPanel")
            << "Font: Inter 400/600/700 + lucide + RobotoMono";
    } else {
        ofLogError("ControlPanel") << "Khong nap duoc font nao";
    }
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
    char slbl[64];
    std::snprintf(slbl, sizeof(slbl), "%s  %s", ICON_LC_SAVE, TR("set.save"));
    if (theme::outlineButton(slbl, theme::Success, ImVec2(-FLT_MIN, 0))) {
        a.saveSettings = true;
    }
    std::snprintf(slbl, sizeof(slbl), "%s  %s", ICON_LC_ROTATE_CCW, TR("set.reset"));
    if (theme::outlineButton(slbl, theme::TextDim, ImVec2(-FLT_MIN, 0))) {
        a.resetSettings = true;
    }
}


} // namespace hexmap

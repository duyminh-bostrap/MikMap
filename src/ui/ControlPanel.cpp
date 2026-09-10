#include "ui/ControlPanel.h"

#include "ui/Localization.h"

#include "core/model/WarpCornerPin.h"
#include "core/model/WarpMesh.h"

#include <algorithm>
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

    drawMenuBar(actions);

    // Composition la cua so CHINH — khong co nut dong. Dong no di thi
    // khong con gi de lam.
    ImGui::SetNextWindowPos(ImVec2(8, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
    if (ImGui::Begin(winTitle("win.composition", "composition").c_str(),
                 nullptr, ImGuiWindowFlags_NoCollapse)) {
        drawClipGrid(project);
        ImGui::Separator();
        drawLayerPanel(project);
    }
    ImGui::End();

    // ── Advanced Output — mo tu menu Output ────────────────────────────
    if (m_showAdvancedOutput) {
        ImGui::SetNextWindowPos(ImVec2(576, 30), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 420), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(winTitle("win.advanced", "advout").c_str(), &m_showAdvancedOutput)) {
            drawScreenPanel(project, edit, actions);
        }
        ImGui::End();
    }

    // ★ Cua so Mapping — can chinh ngay trong app, khong phai sang cua
    //   so output. Dat mac dinh MO vi day la cong viec chinh.
    if (m_showMapping) {
        ImGui::SetNextWindowPos(ImVec2(576, 30), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(660, 500), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(winTitle("win.mapping", "mapping").c_str(), &m_showMapping)) {
            drawMappingEditor(project, edit, canvasPreview);
        }
        ImGui::End();
    }

    if (m_showPreview) {
        ImGui::SetNextWindowPos(ImVec2(8, 458), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(560, 380), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(winTitle("win.preview", "preview").c_str(), &m_showPreview)) {
            drawPreview(canvasPreview);
        }
        ImGui::End();
    }

    // ── Sensor — mo tu menu Sensor ─────────────────────────────────────
    if (m_showSensor) {
        ImGui::SetNextWindowPos(ImVec2(576, 458), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(400, 380), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(winTitle("win.sensor", "sensor").c_str(), &m_showSensor)) {
            drawSensorPanel(project, stats, actions);
            ImGui::Separator();
            drawCalibrationPanel(project, stats, actions);
            ImGui::Separator();
            drawTriggerZonePanel(project, actions);
        }
        ImGui::End();
    }

    if (m_showSettings) {
        ImGui::SetNextWindowSize(ImVec2(460, 560), ImGuiCond_FirstUseEver);

        // Nen DAC, khac cac panel khac.
        //
        // Cac panel lam viec nam canh nhau tren nen toi nen trong suot van
        // doc duoc. Cua so Cai dat thi luon NOI LEN TREN mot panel khac, va
        // luoi clip ben duoi xuyen qua lam chu gan nhu khong doc noi.
        ImGui::SetNextWindowBgAlpha(1.0f);

        if (ImGui::Begin(winTitle("win.settings", "settings").c_str(), &m_showSettings)) {
            drawSettingsPanel(actions);
        }
        ImGui::End();
    }

    if (m_showPerf) {
        ImGui::SetNextWindowPos(ImVec2(984, 30), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 320), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(winTitle("win.perf", "perf").c_str(), &m_showPerf)) {
            drawPerfPanel(stats);
        }
        ImGui::End();
    }

    if (m_showClip) {
        ImGui::SetNextWindowPos(ImVec2(984, 358), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2(300, 480), ImGuiCond_FirstUseEver);
        if (ImGui::Begin(winTitle("win.clip", "clip").c_str(), &m_showClip)) {
            drawClipPanel(project);
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

void ControlPanel::drawMenuBar(UiActions& a) {
    if (!ImGui::BeginMainMenuBar()) return;

    // ── Project ────────────────────────────────────────────────────────
    if (ImGui::BeginMenu(TR("menu.project"))) {
        if (ImGui::MenuItem(TR("menu.project.new")))               a.newProject  = true;
        if (ImGui::MenuItem(TR("menu.project.open"), "Ctrl+O")) a.loadProject = true;
        if (ImGui::MenuItem(TR("menu.project.save"), "Ctrl+S")) a.saveProject = true;
        ImGui::EndMenu();
    }

    // ── Output ─────────────────────────────────────────────────────────
    // Advanced Output la cua so rieng mo tu day, giong cach Resolume
    // tach no ra khoi khung lam viec chinh: no la cong cu DUNG SAN KHAU,
    // dung mot lan luc can chinh, khong phai thu nhin suot buoi dien.
    if (ImGui::BeginMenu(TR("menu.output"))) {
        ImGui::MenuItem(TR("menu.output.mapping"), nullptr, &m_showMapping);
        ImGui::MenuItem(TR("menu.output.advanced"), nullptr, &m_showAdvancedOutput);
        ImGui::Separator();

        // Liet ke man hinh vat ly. Day la cach dua hinh ra may chieu ma
        // KHONG phai keo cua so bang tay — thao tac vua kho vua de keo
        // nham vao khe giua hai man hinh.
        if (m_displays.empty()) {
            ImGui::TextDisabled("%s", TR("menu.output.nodisplay"));
        } else {
            ImGui::TextDisabled("%s", TR("menu.output.sendto"));
            for (const DisplayEntry& d : m_displays) {
                char label[128];
                std::snprintf(label, sizeof(label), "  %d. %s  %dx%d%s",
                              d.index + 1, d.name.c_str(), d.w, d.h,
                              d.isPrimary ? TR("menu.output.primary") : "");
                if (ImGui::MenuItem(label)) a.sendOutputToDisplay = d.index;
            }
        }

        ImGui::Separator();
        if (ImGui::MenuItem(TR("menu.output.windowed"))) a.outputWindowed = true;
        if (ImGui::MenuItem(TR("menu.output.fullscreen"), "F11")) a.toggleFullscreen = true;
        ImGui::EndMenu();
    }

    // ── Sensor ─────────────────────────────────────────────────────────
    // Gom moi thu lien quan sensor vao mot cho. Nhung lenh hay dung
    // (bat/tat, auto-calibrate) de thang o menu, khong bat mo cua so.
    if (ImGui::BeginMenu(TR("menu.sensor"))) {
        ImGui::MenuItem(TR("menu.sensor.panel"), nullptr, &m_showSensor);
        ImGui::Separator();

        if (ImGui::MenuItem(TR("menu.sensor.mock"), "M")) {
            a.sensorTypeIndex = 0;
            a.startSensor = true;
        }
        if (ImGui::MenuItem(TR("menu.sensor.osc"), "O")) {
            a.sensorTypeIndex = 1;
            a.startSensor = true;
        }
        if (ImGui::MenuItem(TR("menu.sensor.stop"))) a.stopSensor = true;

        ImGui::Separator();
        if (ImGui::MenuItem(TR("menu.sensor.autocal"), "A")) a.calibAutoMock = true;
        ImGui::MenuItem(TR("menu.sensor.crosshair"), "C", &m_calibShowTarget);
        ImGui::EndMenu();
    }

    // ── View ───────────────────────────────────────────────────────────
    if (ImGui::BeginMenu(TR("menu.settings"))) {
        ImGui::MenuItem(TR("menu.settings.open"), nullptr, &m_showSettings);
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu(TR("menu.view"))) {
        ImGui::MenuItem(TR("win.mapping"),     nullptr, &m_showMapping);
        ImGui::MenuItem(TR("win.preview"),     nullptr, &m_showPreview);
        ImGui::MenuItem(TR("win.clip"),        nullptr, &m_showClip);
        ImGui::MenuItem(TR("win.perf"),        nullptr, &m_showPerf);
        ImGui::EndMenu();
    }

    // ── Chi bao trang thai ben phai ────────────────────────────────────
    // Hien thang tren menu bar de nguoi van hanh biet sensor co chay
    // khong ma KHONG phai mo cua so Sensor ra xem.
    ImGui::Separator();
    if (m_sensorRunning) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "%s", TR("status.sensor.on"));
    } else {
        ImGui::TextDisabled("%s", TR("status.sensor.off"));
    }

    if (m_statusTimer > 0.0f && !m_status.empty()) {
        ImGui::Separator();
        if (m_statusIsError) ImGui::TextColored(ImVec4(1.0f, 0.4f, 0.4f, 1.0f), "%s", m_status.c_str());
        else                 ImGui::TextColored(ImVec4(0.5f, 1.0f, 0.6f, 1.0f), "%s", m_status.c_str());
    }

    ImGui::EndMainMenuBar();
}

// ── A4 A5 A6: lưới clip ────────────────────────────────────────────────

void ControlPanel::drawClipGrid(Project& p) {
    Composition& comp = p.composition;

    ImGui::Text("%s", TR("comp.deck"));
    ImGui::SameLine();
    for (int d = 0; d < comp.deckCount(); ++d) {
        if (d > 0) ImGui::SameLine();
        const bool active = (d == comp.viewedDeck());
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.55f, 0.85f, 1.0f));
        if (ImGui::Button(comp.deck(d).name.c_str())) {
            // A9 — chỉ đổi deck đang XEM, playback không bị ngắt.
            comp.setViewedDeck(d);
        }
        if (active) ImGui::PopStyleColor();
    }

    ImGui::Spacing();

    const int cols = comp.columnCount();
    const int layers = comp.layerCount();
    if (cols <= 0 || layers <= 0) { ImGui::TextDisabled("%s", TR("comp.empty")); return; }

    // Hàng tiêu đề cột — bấm để trigger CẢ CỘT (A6).
    ImGui::Dummy(ImVec2(74.0f, 1.0f));
    for (int c = 0; c < cols; ++c) {
        ImGui::SameLine();
        ImGui::PushID(10000 + c);
        if (ImGui::Button(std::to_string(c + 1).c_str(), ImVec2(46.0f, 22.0f))) {
            comp.triggerColumn(c);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip(TR("comp.triggercol"), c + 1);
        ImGui::PopID();
    }

    // Vẽ layer từ TRÊN xuống: chỉ số cao = trên cùng, giống Resolume.
    for (int L = layers - 1; L >= 0; --L) {
        const Layer& layer = comp.layer(L);
        ImGui::PushID(L);

        ImGui::AlignTextToFramePadding();
        ImGui::Text("%-8.8s", layer.name.c_str());

        for (int c = 0; c < cols; ++c) {
            ImGui::SameLine();
            ImGui::PushID(c);

            const Clip& clip = comp.deck(comp.viewedDeck()).clip(L, c);
            const bool isPlaying = (layer.activeDeck == comp.viewedDeck()
                                    && layer.activeColumn == c);

            if (clip.isEmpty()) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.14f, 0.16f, 1.0f));
            } else if (isPlaying) {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.70f, 0.35f, 1.0f));
            } else {
                ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.28f, 0.30f, 0.36f, 1.0f));
            }

            const std::string label = clip.isEmpty() ? "+" : clip.name.substr(0, 5);
            if (ImGui::Button(label.c_str(), ImVec2(46.0f, 26.0f))) {
                // Ctrl+bam, hoac bam vao o TRONG -> chon file (I6).
                // O trong ma bam thuong khong lam gi thi nguoi dung se
                // tuong phan mem hong; cho no mo file browser luon.
                if (ImGui::GetIO().KeyCtrl || clip.isEmpty()) {
                    m_pendingBrowse = true;
                    m_browseLayer = L;
                    m_browseColumn = c;
                } else {
                    comp.triggerClip(L, c);      // A5
                    m_selLayer = L;              // chon luon de xem thuoc tinh
                    m_selColumn = c;
                }
            }
            if (!clip.isEmpty() && ImGui::IsItemHovered()) {
                ImGui::SetTooltip("%s\n%s", clip.name.c_str(), clip.media.path.c_str());
            }

            ImGui::PopStyleColor();
            ImGui::PopID();
        }
        ImGui::PopID();
    }

    ImGui::Spacing();
    if (ImGui::Button(TR("comp.clearall"))) comp.clearAll();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", TR("comp.hint"));
}

// ── A7 A8 A11: layer ───────────────────────────────────────────────────

void ControlPanel::drawLayerPanel(Project& p) {
    Composition& comp = p.composition;

    float master = static_cast<float>(comp.masterOpacity);
    if (ImGui::SliderFloat(TR("comp.master"), &master, 0.0f, 1.0f)) {
        comp.masterOpacity = master;
    }

    for (int L = comp.layerCount() - 1; L >= 0; --L) {
        Layer& layer = comp.layer(L);
        ImGui::PushID(2000 + L);

        ImGui::SetNextItemWidth(110.0f);
        float op = static_cast<float>(layer.opacity);
        if (ImGui::SliderFloat("##op", &op, 0.0f, 1.0f, "%.2f")) layer.opacity = op;

        ImGui::SameLine();
        ImGui::SetNextItemWidth(100.0f);
        int blend = static_cast<int>(layer.blend);
        if (ImGui::Combo("##blend", &blend, kBlendNames, IM_ARRAYSIZE(kBlendNames))) {
            layer.blend = static_cast<BlendMode>(blend);
        }

        ImGui::SameLine();
        ImGui::Checkbox("S", &layer.solo);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.solo.tip"));

        ImGui::SameLine();
        ImGui::Checkbox("B", &layer.bypass);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.bypass.tip"));

        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        float tr = static_cast<float>(layer.transitionDuration);
        if (ImGui::DragFloat("##tr", &tr, 0.01f, 0.0f, 10.0f, "%.2fs")) {
            layer.transitionDuration = std::max(0.0f, tr);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TR("layer.transition.tip"));
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X")) layer.clear();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("layer.clear.tip"));

        ImGui::SameLine();
        ImGui::Text("%s", layer.name.c_str());

        ImGui::PopID();
    }
}

// ── F1 F3 F5 F15 F16: screen & slice ───────────────────────────────────

void ControlPanel::drawScreenPanel(Project& p, EditState& edit, UiActions& a) {
    if (p.screens.empty()) { ImGui::TextDisabled("%s", TR("map.noscreen")); return; }

    m_activeScreen = std::clamp(m_activeScreen, 0, static_cast<int>(p.screens.size()) - 1);

    for (size_t i = 0; i < p.screens.size(); ++i) {
        if (i > 0) ImGui::SameLine();
        const bool active = (static_cast<int>(i) == m_activeScreen);
        if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.35f, 0.55f, 0.85f, 1.0f));
        ImGui::PushID(static_cast<int>(i));
        if (ImGui::Button(p.screens[i].name.c_str())) m_activeScreen = static_cast<int>(i);
        ImGui::PopID();
        if (active) ImGui::PopStyleColor();
    }

    Screen& sc = p.screens[static_cast<size_t>(m_activeScreen)];
    ImGui::Separator();

    ImGui::Checkbox(TR("adv.overlay"), &edit.showOverlay);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("%s", TR("adv.overlay.tip"));
    }
    ImGui::SameLine();
    ImGui::Checkbox(TR("adv.grid"), &edit.showGrid);

    if (ImGui::Button(TR("adv.addslice"))) a.addSliceToScreen = m_activeScreen;

    ImGui::Separator();
    ImGui::Text("%s", TR("adv.slicelist"));

    for (int i = 0; i < sc.sliceCount(); ++i) {
        Slice& s = sc.slices[static_cast<size_t>(i)];
        ImGui::PushID(3000 + i);

        const bool selected = (i == edit.activeSliceIndex);
        if (ImGui::RadioButton("##sel", selected)) edit.activeSliceIndex = i;
        ImGui::SameLine();
        ImGui::Checkbox("##en", &s.enabled);
        ImGui::SameLine();
        ImGui::Checkbox(TR("adv.solo"), &s.solo);
        ImGui::SameLine();

        char nameBuf[64];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", s.name.c_str());
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) s.name = nameBuf;

        ImGui::SameLine();
        if (ImGui::SmallButton(TR("adv.delete"))) a.removeSliceIndex = i;

        if (!s.warp()->isInvertible()) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                               "%s", TR("adv.broken"));
        }
        ImGui::PopID();
    }

    if (edit.activeSliceIndex < 0 || edit.activeSliceIndex >= sc.sliceCount()) {
        ImGui::TextDisabled("%s", TR("adv.noslice"));
        return;
    }

    Slice& s = sc.slices[static_cast<size_t>(edit.activeSliceIndex)];
    ImGui::Separator();
    ImGui::Text(TR("adv.selected"), s.name.c_str());

    // F4 — vùng lấy trên canvas
    float ox[2] = {static_cast<float>(s.inputOrigin.x), static_cast<float>(s.inputOrigin.y)};
    if (ImGui::DragFloat2(TR("adv.inputorigin"), ox, 1.0f)) {
        s.inputOrigin = Vec2{ox[0], ox[1]};
    }
    float sz[2] = {static_cast<float>(s.inputSize.x), static_cast<float>(s.inputSize.y)};
    if (ImGui::DragFloat2(TR("adv.inputsize"), sz, 1.0f, 1.0f, 16384.0f)) {
        s.inputSize = Vec2{std::max(1.0f, sz[0]), std::max(1.0f, sz[1])};
    }

    // Đổi loại warp
    int warpType = (s.warp()->type() == WarpType::Mesh) ? 1 : 0;
    const char* warpNames[] = {"Corner pin", "Mesh"};
    if (ImGui::Combo(TR("adv.warptype"), &warpType, warpNames, 2)) {
        a.convertWarpTo = warpType;
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("adv.reset"))) a.resetActiveSliceWarp = true;

    // ── F19: hieu chinh mau rieng cho slice ────────────────────────────
    if (ImGui::TreeNode(TR("adv.color"))) {
        ImGui::TextDisabled("%s", TR("adv.color.note"));

        float br = static_cast<float>(s.color.brightness);
        if (ImGui::SliderFloat(TR("adv.color.brightness"), &br, -1.0f, 1.0f)) s.color.brightness = br;

        float ct = static_cast<float>(s.color.contrast);
        if (ImGui::SliderFloat(TR("adv.color.contrast"), &ct, 0.0f, 3.0f)) s.color.contrast = ct;

        float gm = static_cast<float>(s.color.gamma);
        if (ImGui::SliderFloat(TR("adv.color.gamma"), &gm, 0.1f, 4.0f)) s.color.gamma = gm;

        float gain[3] = {static_cast<float>(s.color.gainR),
                         static_cast<float>(s.color.gainG),
                         static_cast<float>(s.color.gainB)};
        if (ImGui::ColorEdit3(TR("adv.color.rgb"), gain,
                              ImGuiColorEditFlags_Float | ImGuiColorEditFlags_HDR)) {
            s.color.gainR = gain[0];
            s.color.gainG = gain[1];
            s.color.gainB = gain[2];
        }

        float op = static_cast<float>(s.color.opacity);
        if (ImGui::SliderFloat(TR("adv.color.opacity"), &op, 0.0f, 1.0f)) s.color.opacity = op;

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
        ImGui::TextDisabled(TR("adv.edge.note1"));
        ImGui::TextDisabled(TR("adv.edge.note2"));

        float e[4] = {static_cast<float>(s.softEdge.left),
                      static_cast<float>(s.softEdge.right),
                      static_cast<float>(s.softEdge.top),
                      static_cast<float>(s.softEdge.bottom)};
        bool changed = false;
        changed |= ImGui::SliderFloat(TR("adv.edge.left"),   &e[0], 0.0f, 0.5f, "%.3f");
        changed |= ImGui::SliderFloat(TR("adv.edge.right"),  &e[1], 0.0f, 0.5f, "%.3f");
        changed |= ImGui::SliderFloat(TR("adv.edge.top"),    &e[2], 0.0f, 0.5f, "%.3f");
        changed |= ImGui::SliderFloat(TR("adv.edge.bottom"), &e[3], 0.0f, 0.5f, "%.3f");
        if (changed) {
            s.softEdge.left = e[0]; s.softEdge.right = e[1];
            s.softEdge.top  = e[2]; s.softEdge.bottom = e[3];
        }

        float g = static_cast<float>(s.softEdge.gamma);
        if (ImGui::SliderFloat(TR("adv.edge.gamma"), &g, 0.2f, 4.0f)) s.softEdge.gamma = g;
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("%s", TR("adv.edge.gamma.tip"));
        }

        float lum = static_cast<float>(s.softEdge.luminance);
        if (ImGui::SliderFloat(TR("adv.edge.mid"), &lum, 0.0f, 1.0f)) s.softEdge.luminance = lum;

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
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::DragFloat2(cornerNames[k], c, 0.5f)) {
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
        if (ImGui::DragInt2(TR("adv.meshgrid"), dims, 0.2f, 1, 32)) {
            mesh->resize(dims[0], dims[1]);   // F11 — giữ nguyên hình đã kéo
        }
        ImGui::TextDisabled("%s", TR("adv.mesh.hint"));
    }
}

// ── F12: bang mat na bezier ────────────────────────────────────────────

void ControlPanel::drawMaskPanel(Slice& s, EditState& edit) {
    BezierMask& m = s.mask;

    ImGui::TextDisabled("%s", TR("mask.note"));

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
    ImGui::SetNextItemWidth(180.0f);
    if (ImGui::SliderFloat(TR("mask.feather"), &f, 0.0f, 0.5f, "%.3f")) {
        m.feather = f;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("mask.feather.tip"));

    ImGui::Separator();

    if (ImGui::Button(TR("mask.shape.rect"))) {
        const bool inv = m.invert; const double ft = m.feather;
        m = BezierMask::rectangle(0.10);
        m.invert = inv; m.feather = ft;
        edit.maskEditMode = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("mask.shape.ellipse"))) {
        const bool inv = m.invert; const double ft = m.feather;
        m = BezierMask::ellipse(4);
        m.invert = inv; m.feather = ft;
        edit.maskEditMode = true;
    }
    ImGui::SameLine();
    if (ImGui::Button(TR("mask.clear"))) {
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
        ImGui::TextDisabled("%s", TR("mask.hint"));
        ImGui::TextDisabled("%s", TR("mask.hint.handle"));
    }
}

// ── G4 G9 G13: sensor ──────────────────────────────────────────────────

void ControlPanel::drawSensorPanel(Project& p, const PerfStats& s, UiActions& a) {
    (void)p;

    ImGui::Text("%s", TR("menu.sensor"));

    static int sensorType = 0;
    const char* types[] = {TR("sen.type.mock"), TR("sen.type.osc")};
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::Combo(TR("sen.source"), &sensorType, types, 2)) a.sensorTypeIndex = sensorType;

    ImGui::SameLine();
    if (s.sensorConnected) {
        if (ImGui::Button(TR("sen.stop"))) a.stopSensor = true;
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "%s", TR("sen.running"));
    } else {
        if (ImGui::Button(TR("sen.run"))) a.startSensor = true;
        ImGui::SameLine();
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
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                       TR("cal.step"), m_calibTarget + 1);
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

    ImGui::Checkbox(TR("map.showcontent"), &m_mapShowContent);

    // F12 — cong tac che do sua mat na de NGAY o day, khong bat nguoi
    // dung mo Advanced Output moi bat duoc: viec sua mat na dien ra
    // trong chinh cua so nay.
    ImGui::SameLine();
    if (ImGui::Checkbox(TR("mask.edit"), &edit.maskEditMode)) {
        m_mapDragPoint = -1;         // dung keo goc keystone dang do
        edit.maskDraggedNode = -1;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", TR("mask.edit.tip"));

    ImGui::SameLine();
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

void ControlPanel::drawPreview(const ofTexture* tex) {
    if (tex == nullptr || !tex->isAllocated()) {
        ImGui::TextDisabled("%s", TR("preview.nocontent"));
        return;
    }

    const ImVec2 avail = ImGui::GetContentRegionAvail();
    const float aspect = tex->getWidth() / std::max(1.0f, tex->getHeight());

    float w = avail.x;
    float h = w / aspect;
    if (h > avail.y) { h = avail.y; w = h * aspect; }

    // Dung helper cua ofxImGui: ImTextureID la ImU64 o ImGui 1.92+, va
    // ban 1.92 doi cac ham ve sang ImTextureRef. Tu ep kieu se hong khi
    // nang cap ImGui; helper cua addon lo phan do.
    ofxImGui::AddImage(*tex, glm::vec2(w, h));
}

} // namespace hexmap

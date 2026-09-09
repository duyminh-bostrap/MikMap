#include "ui/ControlPanel.h"

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

    m_gui.begin();

    drawMenuBar(actions);

    ImGui::SetNextWindowPos(ImVec2(8, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 420), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Composition")) {
        drawClipGrid(project);
        ImGui::Separator();
        drawLayerPanel(project);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(576, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 420), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Output - Screen & Slice")) {
        drawScreenPanel(project, edit, actions);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(8, 458), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(560, 380), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Preview")) {
        drawPreview(canvasPreview);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(576, 458), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(400, 380), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Sensor & Calibration")) {
        drawSensorPanel(project, stats, actions);
        ImGui::Separator();
        drawCalibrationPanel(project, stats, actions);
        ImGui::Separator();
        drawTriggerZonePanel(project, actions);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(984, 30), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 320), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Performance")) {
        drawPerfPanel(stats);
    }
    ImGui::End();

    ImGui::SetNextWindowPos(ImVec2(984, 358), ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowSize(ImVec2(300, 480), ImGuiCond_FirstUseEver);
    if (ImGui::Begin("Clip")) {
        drawClipPanel(project);
    }
    ImGui::End();

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

    if (ImGui::BeginMenu("Project")) {
        if (ImGui::MenuItem("New"))            a.newProject = true;
        if (ImGui::MenuItem("Open...", "Ctrl+O")) a.loadProject = true;
        if (ImGui::MenuItem("Save",    "Ctrl+S")) a.saveProject = true;
        ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Output")) {
        if (ImGui::MenuItem("Toggle fullscreen", "F11")) a.toggleFullscreen = true;
        ImGui::EndMenu();
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

    ImGui::Text("Deck:");
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
    if (cols <= 0 || layers <= 0) { ImGui::TextDisabled("Luoi rong"); return; }

    // Hàng tiêu đề cột — bấm để trigger CẢ CỘT (A6).
    ImGui::Dummy(ImVec2(74.0f, 1.0f));
    for (int c = 0; c < cols; ++c) {
        ImGui::SameLine();
        ImGui::PushID(10000 + c);
        if (ImGui::Button(std::to_string(c + 1).c_str(), ImVec2(46.0f, 22.0f))) {
            comp.triggerColumn(c);
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Trigger ca cot %d", c + 1);
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
    if (ImGui::Button("Clear all")) comp.clearAll();
    ImGui::SameLine();
    ImGui::TextDisabled("Bam o clip = phat  |  Bam so cot = phat ca cot  |  Ctrl+bam o = chon file");
}

// ── A7 A8 A11: layer ───────────────────────────────────────────────────

void ControlPanel::drawLayerPanel(Project& p) {
    Composition& comp = p.composition;

    float master = static_cast<float>(comp.masterOpacity);
    if (ImGui::SliderFloat("Master", &master, 0.0f, 1.0f)) {
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
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Solo - an moi layer khac");

        ImGui::SameLine();
        ImGui::Checkbox("B", &layer.bypass);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Bypass - tat layer");

        ImGui::SameLine();
        ImGui::SetNextItemWidth(70.0f);
        float tr = static_cast<float>(layer.transitionDuration);
        if (ImGui::DragFloat("##tr", &tr, 0.01f, 0.0f, 10.0f, "%.2fs")) {
            layer.transitionDuration = std::max(0.0f, tr);
        }
        if (ImGui::IsItemHovered()) {
            ImGui::SetTooltip("A10 - thoi luong chuyen clip (crossfade). "
                              "0 = cat thang. Trong luc chuyen, layer nay "
                              "phat HAI luong video cung luc.");
        }

        ImGui::SameLine();
        if (ImGui::SmallButton("X")) layer.clear();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Dung layer");

        ImGui::SameLine();
        ImGui::Text("%s", layer.name.c_str());

        ImGui::PopID();
    }
}

// ── F1 F3 F5 F15 F16: screen & slice ───────────────────────────────────

void ControlPanel::drawScreenPanel(Project& p, EditState& edit, UiActions& a) {
    if (p.screens.empty()) { ImGui::TextDisabled("Chua co screen nao"); return; }

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

    ImGui::Checkbox("Hien overlay chinh sua", &edit.showOverlay);
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("TAT khi chay show - khan gia khong duoc thay handle");
    }
    ImGui::SameLine();
    ImGui::Checkbox("Luoi test", &edit.showGrid);

    if (ImGui::Button("+ Them slice")) a.addSliceToScreen = m_activeScreen;

    ImGui::Separator();
    ImGui::Text("Slice (tren cung o duoi danh sach):");

    for (int i = 0; i < sc.sliceCount(); ++i) {
        Slice& s = sc.slices[static_cast<size_t>(i)];
        ImGui::PushID(3000 + i);

        const bool selected = (i == edit.activeSliceIndex);
        if (ImGui::RadioButton("##sel", selected)) edit.activeSliceIndex = i;
        ImGui::SameLine();
        ImGui::Checkbox("##en", &s.enabled);
        ImGui::SameLine();
        ImGui::Checkbox("solo", &s.solo);
        ImGui::SameLine();

        char nameBuf[64];
        std::snprintf(nameBuf, sizeof(nameBuf), "%s", s.name.c_str());
        ImGui::SetNextItemWidth(120.0f);
        if (ImGui::InputText("##name", nameBuf, sizeof(nameBuf))) s.name = nameBuf;

        ImGui::SameLine();
        if (ImGui::SmallButton("Xoa")) a.removeSliceIndex = i;

        if (!s.warp()->isInvertible()) {
            ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                               "  ! Warp suy bien — khong dung cho sensor duoc");
        }
        ImGui::PopID();
    }

    if (edit.activeSliceIndex < 0 || edit.activeSliceIndex >= sc.sliceCount()) {
        ImGui::TextDisabled("Chon mot slice de chinh chi tiet");
        return;
    }

    Slice& s = sc.slices[static_cast<size_t>(edit.activeSliceIndex)];
    ImGui::Separator();
    ImGui::Text("Slice dang chon: %s", s.name.c_str());

    // F4 — vùng lấy trên canvas
    float ox[2] = {static_cast<float>(s.inputOrigin.x), static_cast<float>(s.inputOrigin.y)};
    if (ImGui::DragFloat2("Input origin", ox, 1.0f)) {
        s.inputOrigin = Vec2{ox[0], ox[1]};
    }
    float sz[2] = {static_cast<float>(s.inputSize.x), static_cast<float>(s.inputSize.y)};
    if (ImGui::DragFloat2("Input size", sz, 1.0f, 1.0f, 16384.0f)) {
        s.inputSize = Vec2{std::max(1.0f, sz[0]), std::max(1.0f, sz[1])};
    }

    // Đổi loại warp
    int warpType = (s.warp()->type() == WarpType::Mesh) ? 1 : 0;
    const char* warpNames[] = {"Corner pin", "Mesh"};
    if (ImGui::Combo("Loai warp", &warpType, warpNames, 2)) {
        a.convertWarpTo = warpType;
    }
    ImGui::SameLine();
    if (ImGui::Button("Reset")) a.resetActiveSliceWarp = true;

    // F15 — nhập toạ độ góc bằng SỐ, không chỉ kéo chuột.
    // Cần thiết khi căn chính xác theo bản vẽ, hoặc khi máy chiếu ở xa
    // không với tay tới được.
    if (s.warp()->type() == WarpType::CornerPin) {
        auto* cp = static_cast<WarpCornerPin*>(s.warp());
        ImGui::Text("Goc (nhap so):");
        const char* cornerNames[4] = {"Tren-trai", "Tren-phai", "Duoi-phai", "Duoi-trai"};
        for (int k = 0; k < 4; ++k) {
            ImGui::PushID(4000 + k);
            float c[2] = {static_cast<float>(cp->corner(k).x),
                          static_cast<float>(cp->corner(k).y)};
            ImGui::SetNextItemWidth(160.0f);
            if (ImGui::DragFloat2(cornerNames[k], c, 0.5f)) {
                if (!cp->setCorner(k, Vec2{c[0], c[1]})) {
                    // Bị từ chối vì tứ giác sẽ lõm hoặc tự cắt.
                    setStatusMessage("Vi tri goc bi tu choi: tu giac se lom hoac tu cat", true);
                }
            }
            ImGui::PopID();
        }
    } else {
        auto* mesh = static_cast<WarpMesh*>(s.warp());
        int dims[2] = {mesh->cols(), mesh->rows()};
        if (ImGui::DragInt2("Luoi (cot x hang)", dims, 0.2f, 1, 32)) {
            mesh->resize(dims[0], dims[1]);   // F11 — giữ nguyên hình đã kéo
        }
        ImGui::TextDisabled("Keo diem luoi trong cua so Output");
    }
}

// ── G4 G9 G13: sensor ──────────────────────────────────────────────────

void ControlPanel::drawSensorPanel(Project& p, const PerfStats& s, UiActions& a) {
    (void)p;

    ImGui::Text("Sensor");

    static int sensorType = 0;
    const char* types[] = {"Mock (gia lap)", "OSC / UDP"};
    ImGui::SetNextItemWidth(160.0f);
    if (ImGui::Combo("Nguon", &sensorType, types, 2)) a.sensorTypeIndex = sensorType;

    ImGui::SameLine();
    if (s.sensorConnected) {
        if (ImGui::Button("Dung")) a.stopSensor = true;
        ImGui::SameLine();
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.5f, 1.0f), "dang chay");
    } else {
        if (ImGui::Button("Chay")) a.startSensor = true;
        ImGui::SameLine();
        ImGui::TextDisabled("dang dung");
    }

    ImGui::Text("Diem cham: %d   (co ID ben vung: %d)", s.touchCount, s.trackedCount);
    ImGui::TextDisabled("Loc nhieu (phim F): %s", s.filterEnabled ? "BAT" : "TAT");
}

// ── G6 G10: wizard calibration ─────────────────────────────────────────

void ControlPanel::drawCalibrationPanel(Project& p, const PerfStats& s, UiActions& a) {
    (void)s;

    if (p.calibrations.empty()) {
        ImGui::TextDisabled("Chua co ho so calibration");
        return;
    }

    m_selectedCalib = std::clamp(m_selectedCalib, 0,
                                 static_cast<int>(p.calibrations.size()) - 1);
    CalibrationProfile& cal = p.calibrations[static_cast<size_t>(m_selectedCalib)];

    ImGui::Text("Calibration: %s", cal.name.c_str());

    // Hướng dẫn từng bước — người vận hành không cần đọc tài liệu.
    m_calibTarget = std::clamp(m_calibTarget, 0, 3);
    ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f),
                       "Buoc %d/4: cham vao dau thap tren VAT THE THAT",
                       m_calibTarget + 1);
    ImGui::TextDisabled("Diem muc tieu: (%.2f, %.2f) cua slice",
                        kCalibTargets[m_calibTarget].x, kCalibTargets[m_calibTarget].y);

    // ★ G6 — khong co dau thap thi wizard vo dung: nguoi van hanh khong
    //   biet phai cham vao dau tren vat the that.
    ImGui::Checkbox("Hien dau thap tren may chieu (phim C)", &m_calibShowTarget);
    a.calibShowTarget = m_calibShowTarget;

    if (ImGui::Button("Auto-calibrate (Mock)")) a.calibAutoMock = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Suy ra H_s tu pham vi da biet cua MockSource. "
                          "De thu ca chuoi ma khong can phan cung.");
    }
    ImGui::Separator();

    if (ImGui::Button("Ghi diem nay")) a.calibAddPoint = true;
    ImGui::SameLine();
    if (ImGui::Button("Bo qua")) a.calibNextTarget = true;
    ImGui::SameLine();
    if (ImGui::Button("Giai")) a.calibSolve = true;
    ImGui::SameLine();
    if (ImGui::Button("Xoa het")) a.calibClear = true;

    ImGui::Text("Da ghi: %d diem (%d dang bat)",
                static_cast<int>(cal.pairCount()),
                static_cast<int>(cal.enabledPairCount()));

    // ★ G10 — sai số phải hiện ra. Không có nó, người vận hành thấy hiệu
    //   ứng lệch chỗ mà đi chỉnh keystone (sai chỗ) thay vì calibrate lại.
    if (cal.isValid()) {
        const bool good = cal.isAccurate(3.0);
        ImGui::TextColored(good ? ImVec4(0.4f, 1.0f, 0.5f, 1.0f)
                                : ImVec4(1.0f, 0.5f, 0.3f, 1.0f),
                           "Sai so: %.2f px (max %.2f) - %s",
                           cal.rmsError(), cal.maxError(),
                           good ? "TOT" : "NEN CALIBRATE LAI");
        ImGui::Text("Inlier: %d / %d", cal.inlierCount(),
                    static_cast<int>(cal.enabledPairCount()));
    } else {
        ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.3f, 1.0f), "Chua calibrate");
        if (!cal.message().empty()) ImGui::TextWrapped("%s", cal.message().c_str());
    }

    // Danh sách điểm — tắt được điểm xấu mà không phải chạm lại từ đầu.
    if (ImGui::TreeNode("Cac diem da ghi")) {
        const auto& flags = cal.outlierFlags();
        for (size_t i = 0; i < cal.pairCount(); ++i) {
            ImGui::PushID(static_cast<int>(5000 + i));
            bool en = cal.pairs()[i].enabled;
            if (ImGui::Checkbox("##en", &en)) cal.setPairEnabled(i, en);
            ImGui::SameLine();

            const bool isOutlier = (i < flags.size() && flags[i]);
            if (isOutlier) ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.4f, 0.4f, 1.0f));
            ImGui::Text("#%d  sensor(%.0f, %.0f) -> out(%.0f, %.0f)%s",
                        static_cast<int>(i),
                        cal.pairs()[i].src.x, cal.pairs()[i].src.y,
                        cal.pairs()[i].dst.x, cal.pairs()[i].dst.y,
                        isOutlier ? "   [RAC]" : "");
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
    ImGui::Text("Frame: avg %.2f ms  p99 %.2f ms", s.frameAvgMs, s.frameP99Ms);
    ImGui::Separator();

    // ★ architecture.md §10.6 — do, khong doan.
    if (s.sensorConnected) {
        ImGui::Text("Do tre sensor:");
        ImGui::Text("  avg %.2f ms   p99 %.2f ms",
                    s.sensorLatencyAvgMs, s.sensorLatencyP99Ms);
        ImGui::TextDisabled("(chua ke input lag cua may chieu:");
        ImGui::TextDisabled(" 16-80ms - bat Low Latency Mode)");
    } else {
        ImGui::TextDisabled("Sensor chua chay");
    }

    ImGui::Separator();
    ImGui::Text("Frame sensor bo qua: %llu",
                static_cast<unsigned long long>(s.framesDropped));

    // Ring overflow khac 0 = render thread qua tai, su kien BI MAT.
    if (s.ringOverflow > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.35f, 0.35f, 1.0f),
                           "! Ring overflow: %llu — MAT su kien",
                           static_cast<unsigned long long>(s.ringOverflow));
    } else {
        ImGui::Text("Ring overflow: 0");
    }

    if (s.packetsMalformed > 0) {
        ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f),
                           "Goi OSC hong: %llu",
                           static_cast<unsigned long long>(s.packetsMalformed));
    }

    ImGui::Separator();
    ImGui::Text("Layer ve: %d   Slice ve: %d", s.layersDrawn, s.slicesDrawn);
    ImGui::Text("Media nap: %d   Da don: %d", s.mediaLoaded, s.mediaEvictions);
    ImGui::Text("VRAM (uoc tinh): %s", formatBytes(s.vramBytes).c_str());
}

// ── C3-C8 + D1-D6: thuoc tinh clip dang chon ───────────────────────────

void ControlPanel::drawClipPanel(Project& p) {
    Composition& comp = p.composition;

    if (m_selLayer < 0 || m_selColumn < 0) {
        ImGui::TextDisabled("Bam vao mot o clip de xem thuoc tinh");
        return;
    }

    Clip* c = comp.deck(comp.viewedDeck()).clipPtr(m_selLayer, m_selColumn);
    if (c == nullptr || c->isEmpty()) {
        ImGui::TextDisabled("O nay trong");
        return;
    }

    ImGui::Text("Layer %d  /  Cot %d", m_selLayer + 1, m_selColumn + 1);

    char nameBuf[96];
    std::snprintf(nameBuf, sizeof(nameBuf), "%s", c->name.c_str());
    if (ImGui::InputText("Ten", nameBuf, sizeof(nameBuf))) c->name = nameBuf;

    ImGui::TextWrapped("%s", c->media.path.c_str());
    ImGui::TextDisabled("%.0f x %.0f  |  %.2f s",
                        c->media.size.x, c->media.size.y, c->media.durationSec);

    // ── C1 C8: transport ───────────────────────────────────────────────
    ImGui::SeparatorText("Transport");

    if (ImGui::Button(c->transport.isPlaying() ? "Pause" : "Play")) {
        c->transport.togglePlay();
    }
    ImGui::SameLine();
    if (ImGui::Button("Stop")) c->transport.stop();

    // C8 — keo tua. Chi ghi khi nguoi dung THUC SU keo; neu ghi moi frame
    // thi se de len dau phat dang chay va clip dung yen tai cho.
    float pos = static_cast<float>(c->transport.position);
    if (ImGui::SliderFloat("Playhead", &pos, 0.0f, 1.0f, "%.3f")) {
        c->transport.seekNormalized(pos);
    }

    // ── C4: chieu phat ─────────────────────────────────────────────────
    const char* dirNames[] = {"Thuan", "Nguoc", "Ping-pong"};
    int dir = static_cast<int>(c->transport.direction);
    if (ImGui::Combo("Chieu", &dir, dirNames, 3)) {
        c->transport.direction = static_cast<PlayDirection>(dir);
    }

    // ── C5: toc do ─────────────────────────────────────────────────────
    float sp = static_cast<float>(c->transport.speed);
    if (ImGui::DragFloat("Toc do", &sp, 0.01f, -4.0f, 4.0f, "%.2fx")) {
        c->transport.speed = sp;
    }
    ImGui::SameLine();
    if (ImGui::SmallButton("1x")) c->transport.speed = 1.0;

    // ── C6: cat dau/cuoi ───────────────────────────────────────────────
    float trim[2] = {static_cast<float>(c->transport.inPoint),
                     static_cast<float>(c->transport.outPoint)};
    if (ImGui::DragFloat2("In / Out", trim, 0.005f, 0.0f, 1.0f, "%.3f")) {
        // setTrim tu hoan doi khi vao nguoc va giu khoang toi thieu, nen
        // keo hai handle chong len nhau khong gay chia cho 0.
        c->transport.setTrim(trim[0], trim[1]);
    }

    // ── C7: autopilot ──────────────────────────────────────────────────
    const char* endNames[] = {"Loop", "Dung", "Giu khung cuoi",
                              "Clip ke tiep", "Ngau nhien"};
    int ea = static_cast<int>(c->transport.endAction);
    if (ImGui::Combo("Het clip thi", &ea, endNames, 5)) {
        c->transport.endAction = static_cast<EndAction>(ea);
    }

    // ── C3: kieu kich hoat ─────────────────────────────────────────────
    const char* trigNames[] = {"Toggle", "Piano (giu)"};
    int ts = static_cast<int>(c->triggerStyle);
    if (ImGui::Combo("Kieu bam", &ts, trigNames, 2)) {
        c->triggerStyle = static_cast<TriggerStyle>(ts);
    }

    // ── D1-D6: bien doi ────────────────────────────────────────────────
    ImGui::SeparatorText("Transform");

    float posXY[2] = {static_cast<float>(c->transform.position.x),
                      static_cast<float>(c->transform.position.y)};
    if (ImGui::DragFloat2("Vi tri", posXY, 1.0f)) {
        c->transform.position = Vec2{posXY[0], posXY[1]};
    }

    float scXY[2] = {static_cast<float>(c->transform.scale.x),
                     static_cast<float>(c->transform.scale.y)};
    if (ImGui::DragFloat2("Ti le", scXY, 0.01f, -10.0f, 10.0f)) {
        c->transform.scale = Vec2{scXY[0], scXY[1]};
    }

    float rotDeg = static_cast<float>(c->transform.rotation * 180.0 / 3.14159265358979);
    if (ImGui::DragFloat("Xoay", &rotDeg, 0.5f, -360.0f, 360.0f, "%.1f do")) {
        c->transform.rotation = rotDeg * 3.14159265358979 / 180.0;
    }

    ImGui::Checkbox("Lat ngang", &c->transform.flipH);
    ImGui::SameLine();
    ImGui::Checkbox("Lat doc", &c->transform.flipV);

    if (ImGui::Button("Reset transform")) c->transform.reset();

    // ── D3 D4: hoa tron ────────────────────────────────────────────────
    ImGui::SeparatorText("Hoa tron");

    float op = static_cast<float>(c->opacity);
    if (ImGui::SliderFloat("Do mo", &op, 0.0f, 1.0f)) c->opacity = op;

    int bl = static_cast<int>(c->blend);
    if (ImGui::Combo("Blend", &bl, kBlendNames, IM_ARRAYSIZE(kBlendNames))) {
        c->blend = static_cast<BlendMode>(bl);
    }
}

// ── G17: vung cam ung ──────────────────────────────────────────────────

void ControlPanel::drawTriggerZonePanel(Project& p, UiActions& a) {
    ImGui::Text("Vung cam ung (G17)   -   da kich hoat: %d", m_triggerCount);
    if (!m_lastTriggerName.empty()) {
        ImGui::TextColored(ImVec4(0.4f, 1.0f, 0.6f, 1.0f),
                           "Gan nhat: %s", m_lastTriggerName.c_str());
    }

    if (ImGui::Button("+ Them vung")) a.addTriggerZone = true;
    if (ImGui::IsItemHovered()) {
        ImGui::SetTooltip("Vung dinh nghia trong khong gian CANVAS. "
                          "Chinh lai keystone khong lam lech vung.");
    }

    auto& zones = p.triggerZones.zones;
    if (zones.empty()) {
        ImGui::TextDisabled("Chua co vung nao. Cham vao vat the se khong lam gi.");
        return;
    }

    const char* actionNames[] = {"Khong", "Phat clip", "Phat ca cot",
                                 "Dung layer", "Dung het"};

    for (int i = 0; i < static_cast<int>(zones.size()); ++i) {
        TriggerZone& z = zones[static_cast<size_t>(i)];
        ImGui::PushID(7000 + i);

        // To sang khi dang co ngon tay trong vung — phan hoi truc quan
        // quan trong nhat khi can chinh.
        if (z.occupied) ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.1f, 0.6f, 0.35f, 1.0f));

        const bool open = ImGui::CollapsingHeader(
            (z.name + (z.occupied ? "   [DANG CHAM]" : "")).c_str());

        if (z.occupied) ImGui::PopStyleColor();

        if (open) {
            ImGui::Checkbox("Bat", &z.enabled);
            ImGui::SameLine();
            if (ImGui::SmallButton("Xoa vung")) a.removeTriggerZone = i;

            char nb[64];
            std::snprintf(nb, sizeof(nb), "%s", z.name.c_str());
            if (ImGui::InputText("Ten", nb, sizeof(nb))) z.name = nb;

            float o[2] = {static_cast<float>(z.origin.x), static_cast<float>(z.origin.y)};
            if (ImGui::DragFloat2("Vi tri", o, 1.0f)) z.origin = Vec2{o[0], o[1]};

            float sz[2] = {static_cast<float>(z.size.x), static_cast<float>(z.size.y)};
            if (ImGui::DragFloat2("Kich thuoc", sz, 1.0f, 1.0f, 16384.0f)) {
                z.size = Vec2{std::max(1.0f, sz[0]), std::max(1.0f, sz[1])};
            }

            int act = static_cast<int>(z.action);
            if (ImGui::Combo("Hanh dong", &act, actionNames, 5)) {
                z.action = static_cast<TriggerAction>(act);
            }

            if (z.action == TriggerAction::TriggerClip
                || z.action == TriggerAction::ClearLayer) {
                ImGui::DragInt("Layer", &z.targetLayer, 0.1f, 0,
                               std::max(0, p.composition.layerCount() - 1));
            }
            if (z.action == TriggerAction::TriggerClip
                || z.action == TriggerAction::TriggerColumn) {
                ImGui::DragInt("Cot", &z.targetColumn, 0.1f, 0,
                               std::max(0, p.composition.columnCount() - 1));
            }

            float cd = static_cast<float>(z.cooldownSec);
            if (ImGui::DragFloat("Chong doi", &cd, 0.01f, 0.0f, 5.0f, "%.2fs")) {
                z.cooldownSec = std::max(0.0f, cd);
            }
            if (ImGui::IsItemHovered()) {
                ImGui::SetTooltip("BAT BUOC voi sensor that. Mot cu cham sinh ra "
                                  "hang chuc su kien Down; khong chan doi thi clip "
                                  "bi trigger lai 30 lan/giay.");
            }
        }
        ImGui::PopID();
    }
}

void ControlPanel::drawPreview(const ofTexture* tex) {
    if (tex == nullptr || !tex->isAllocated()) {
        ImGui::TextDisabled("Chua co noi dung");
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

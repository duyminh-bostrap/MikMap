// ════════════════════════════════════════════════════════════════════════
//  render/RenderEngine.h — pipeline vẽ (architecture.md §5)
//
//    Pass 1  Layer → Composition FBO   (không gian canvas ảo)
//    Pass 2  Composition FBO → Slice đã warp  (không gian máy chiếu)
//    Pass 3  Overlay chỉnh sửa  — CHỈ khi edit mode bật
//
//  ── Vì sao dựng FBO canvas riêng thay vì vẽ thẳng ────────────────────
//  Canvas ảo ĐỘC LẬP với độ phân giải máy chiếu (A1). Nhiều slice cùng
//  lấy từ một canvas, và nhiều screen cùng dùng lại canvas đó. Vẽ thẳng
//  ra từng máy chiếu sẽ phải composite lại toàn bộ layer cho mỗi screen.
//
//  ── Ràng buộc: đây là RENDER THREAD ──────────────────────────────────
//  Không khoá, không cấp phát, không I/O đĩa trong hot path.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/model/Composition.h"
#include "core/model/Screen.h"
#include "core/model/TriggerZone.h"
#include "render/MediaCache.h"

#include "ofMain.h"

#include <vector>

namespace hexmap {

/// Trạng thái tương tác của overlay chỉnh sửa.
struct EditState {
    /// ★ Cua so output dang fullscreen tren may chieu.
    ///
    /// Khi bat, output CHI ve noi dung — khong vien slice, khong handle,
    /// khong luoi, khong vung cam ung, khong cham sensor. Day la thu
    /// khan gia nhin thay, nen bat cu thu gi khong phai noi dung deu la
    /// loi hien ra man anh.
    ///
    /// Co nay do AppController dat theo trang thai THAT cua cua so, khong
    /// phai do nguoi dung tich. Quen tat overlay truoc khi dien la loi
    /// rat de mac, nen de phan mem tu lo.
    bool outputIsFullscreen = false;

    bool showOverlay = true;     ///< I5 — tắt hoàn toàn khi chạy show
    bool showGrid = false;       ///< F14 — lưới test card
    int  activeSliceIndex = -1;

    /// Chỉ số điểm điều khiển (không phải "góc") — cùng chỉ số với
    /// IWarp::controlPointAt, nên dùng chung được cho corner pin và mesh.
    int  hoveredHandle = -1;
    int  draggedHandle = -1;

    // ── G6: wizard calibration ─────────────────────────────────────────
    /// Khi bật, vẽ dấu thập LỚN lên máy chiếu để người vận hành nhắm vào
    /// mà chạm. Không có nó thì wizard vô dụng: người ta không biết phải
    /// chạm vào đâu trên vật thể thật.
    bool calibrating = false;
    Vec2 calibTargetUV{0.15, 0.15};   ///< contentUV của slice đang chọn
    int  calibStep = 0;               ///< 0..3, để hiện "bước n/4"

    // ── G13: overlay điểm sensor ───────────────────────────────────────
    /// Toạ độ điểm chạm đã ánh xạ sang không gian OUTPUT (pixel máy chiếu).
    /// Vẽ trực tiếp lên output để kiểm chứng calibration bằng mắt: chạm
    /// vào vật thể, thấy chấm rơi đúng chỗ tay mình.
    std::vector<Vec2> sensorOutputPoints;
    bool showSensorPoints = true;
};

class RenderEngine {
public:
    bool setup(const Vec2& canvasSize);
    void resizeCanvas(const Vec2& canvasSize);

    /// Pass 1 — dựng nội dung canvas từ các layer đang hiện.
    void renderComposition(Composition& comp, MediaCache& cache);

    /// Pass 2 + 3 — vẽ canvas lên một screen qua các slice.
    /// Gọi trong ngữ cảnh cửa sổ output.
    void renderScreen(const Screen& screen, const EditState& edit);

    /// G17 — ve vung cam ung len may chieu.
    /// Vung dinh nghia trong khong gian CANVAS nen phai di qua slice de
    /// ra khong gian output. Goi SAU renderScreen.
    void drawTriggerZones(const Screen& screen,
                          const TriggerZoneSet& zones,
                          const EditState& edit) const;

    /// Vẽ preview canvas thu nhỏ (cho cửa sổ control — I3).
    void drawCanvasPreview(float x, float y, float w, float h) const;

    /// Overlay điểm sensor đã ánh xạ (G13).
    void drawSensorOverlay(const std::vector<Vec2>& canvasPoints,
                           float x, float y, float w, float h) const;

    const ofFbo& canvasFbo() const { return m_canvas; }
    Vec2 canvasSize() const { return m_canvasSize; }

    // ── Chẩn đoán (G9) ─────────────────────────────────────────────────
    int lastLayersDrawn() const { return m_lastLayersDrawn; }
    int lastSlicesDrawn() const { return m_lastSlicesDrawn; }

private:
    void drawClipToCanvas(const Clip& clip, double opacity,
                          BlendMode blend, MediaCache& cache);
    void applyBlendMode(BlendMode mode) const;
    void drawSliceGeometry(const Slice& slice) const;
    void drawEditOverlay(const Screen& screen, const EditState& edit) const;
    void drawTestGrid(const Screen& screen) const;
    void drawCalibTarget(const Screen& screen, const EditState& edit) const;
    void drawSensorPointsOnOutput(const EditState& edit) const;

    /// Shader ve slice, co hieu chinh mau (F19).
    ///
    /// Nhung SOURCE thang vao code chu khong doc tu bin/data/shaders:
    /// shader la mot phan cua CHUONG TRINH, khong phai tai san cua nguoi
    /// dung. Doc tu file nghia la app co the khoi dong voi shader thieu
    /// hoac lech phien ban — mot loi chi lo ra luc chay show.
    ofShader m_sliceShader;
    bool     m_shaderReady = false;

    /// Chuyen YCoCg -> RGB cho HAP Q, viet lai bang #version 150.
    ///
    /// ★ KHONG dung duoc shader co san cua ofxHapPlayer: no viet bang
    ///   GLSL 120 fixed-function (ftransform, gl_TexCoord, gl_FragColor,
    ///   texture2D). Tat ca deu bi LOAI BO trong GL 3.2 core profile —
    ///   ma app nay chay dung profile do. Shader do van link duoc nhung
    ///   khong ve ra gi, cho ra man hinh den.
    ofShader m_ycocgShader;
    bool     m_ycocgReady = false;

    bool buildSliceShader();
    bool buildYCoCgShader();

    ofFbo m_canvas;
    Vec2  m_canvasSize{1920.0, 1080.0};

    /// Lưới tam giác dùng lại giữa các frame — chỉ dựng lại khi warp dirty.
    mutable ofVboMesh m_sliceMesh;

    int m_lastLayersDrawn = 0;
    int m_lastSlicesDrawn = 0;
};

} // namespace hexmap

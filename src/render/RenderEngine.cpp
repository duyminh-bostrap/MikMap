#include "render/RenderEngine.h"

#include <algorithm>

namespace hexmap {
namespace {

constexpr float kHandleRadius = 9.0f;

/// Tra ve vec3: ofPolyline::addVertex chi nhan glm::vec3.
/// Truy cap .x/.y van dung binh thuong cho ofDrawCircle.
glm::vec3 toOf(const Vec2& v) {
    return {static_cast<float>(v.x), static_cast<float>(v.y), 0.0f};
}

} // namespace

bool RenderEngine::setup(const Vec2& canvasSize) {
    resizeCanvas(canvasSize);
    return m_canvas.isAllocated();
}

void RenderEngine::resizeCanvas(const Vec2& canvasSize) {
    const int w = std::max(1, static_cast<int>(canvasSize.x));
    const int h = std::max(1, static_cast<int>(canvasSize.y));

    if (m_canvas.isAllocated()
        && m_canvas.getWidth() == w && m_canvas.getHeight() == h) {
        return;
    }

    ofFbo::Settings s;
    s.width = w;
    s.height = h;
    s.internalformat = GL_RGBA8;
    s.useDepth = false;
    s.useStencil = true;      // Pass 3 (mask bezier — F12) sẽ cần
    s.numSamples = 0;         // KHÔNG khử răng cưa: slice tự lo, MSAA ở
                              // đây chỉ tốn băng thông mà không cải thiện
    m_canvas.allocate(s);

    m_canvas.begin();
    ofClear(0, 0, 0, 0);
    m_canvas.end();

    m_canvasSize = Vec2{static_cast<double>(w), static_cast<double>(h)};
}

// ═══════════════════════════════════════════════════════════════════════
//  Pass 1 — Layer → Composition FBO
// ═══════════════════════════════════════════════════════════════════════

void RenderEngine::applyBlendMode(BlendMode mode) const {
    switch (mode) {
    case BlendMode::Add:      ofEnableBlendMode(OF_BLENDMODE_ADD);      break;
    case BlendMode::Multiply: ofEnableBlendMode(OF_BLENDMODE_MULTIPLY); break;
    case BlendMode::Screen:   ofEnableBlendMode(OF_BLENDMODE_SCREEN);   break;
    case BlendMode::Subtract: ofEnableBlendMode(OF_BLENDMODE_SUBTRACT); break;
    default:                  ofEnableBlendMode(OF_BLENDMODE_ALPHA);    break;
    }
}

void RenderEngine::drawClipToCanvas(const Clip& clip, double opacity,
                                    BlendMode blend, MediaCache& cache) {
    MediaCache::Entry* e = cache.acquire(clip.media);
    if (e == nullptr || !e->loaded) return;

    ofTexture* tex = e->texture();
    if (tex == nullptr) return;

    const Vec2 contentSize = (e->size.x > 0.0) ? e->size : m_canvasSize;

    applyBlendMode(blend);
    ofSetColor(255, 255, 255, static_cast<int>(ofClamp(opacity, 0.0, 1.0) * 255.0));

    // Transform2D dựng ma trận trong không gian canvas; đẩy nó lên
    // ma trận model của GL thay vì tự nhân từng đỉnh.
    const Mat3 m = clip.transform.toMatrix(contentSize);

    // Mat3 row-major 3×3 → ma trận 4×4 column-major của GL.
    ofMatrix4x4 gl(
        static_cast<float>(m.at(0, 0)), static_cast<float>(m.at(1, 0)), 0.0f, static_cast<float>(m.at(2, 0)),
        static_cast<float>(m.at(0, 1)), static_cast<float>(m.at(1, 1)), 0.0f, static_cast<float>(m.at(2, 1)),
        0.0f,                            0.0f,                            1.0f, 0.0f,
        static_cast<float>(m.at(0, 2)), static_cast<float>(m.at(1, 2)), 0.0f, static_cast<float>(m.at(2, 2)));

    ofPushMatrix();
    ofMultMatrix(gl);
    tex->draw(0.0f, 0.0f,
              static_cast<float>(contentSize.x),
              static_cast<float>(contentSize.y));
    ofPopMatrix();
}

void RenderEngine::renderComposition(Composition& comp, MediaCache& cache) {
    resizeCanvas(comp.canvasSize);
    if (!m_canvas.isAllocated()) return;

    const std::vector<int> visible = comp.visibleLayers();
    m_lastLayersDrawn = static_cast<int>(visible.size());

    m_canvas.begin();
    ofClear(0, 0, 0, 0);

    // visibleLayers() trả về theo thứ tự z từ DƯỚI lên TRÊN — vẽ đúng
    // thứ tự đó, layer sau đè lên layer trước.
    for (const int layerIdx : visible) {
        const Composition::LayerRenderInfo info = comp.renderInfo(layerIdx);
        if (info.current == nullptr) continue;

        // ★ A10 — clip CU truoc (dang tat dan), roi clip MOI de len tren.
        //   Trong khoang nay layer chiem HAI luong video cung luc.
        if (info.previous != nullptr && info.previousOpacity > 0.0) {
            cache.syncTransport(*info.previous);
            drawClipToCanvas(*info.previous, info.previousOpacity, info.blend, cache);
        }

        cache.syncTransport(*info.current);
        drawClipToCanvas(*info.current, info.currentOpacity, info.blend, cache);
    }

    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    ofSetColor(255);
    m_canvas.end();
}

// ═══════════════════════════════════════════════════════════════════════
//  Pass 2 — Composition FBO → Slice đã warp
// ═══════════════════════════════════════════════════════════════════════

void RenderEngine::drawSliceGeometry(const Slice& slice) const {
    const IWarp* w = slice.warp();
    if (w == nullptr) return;

    const int sub = std::max(1, w->defaultSubdivisions());

    WarpGeometry geo;
    w->tessellate(sub, sub, geo);
    if (geo.vertices.empty()) return;

    // Đổi contentUV [0,1]² sang toạ độ texture của canvas FBO, có tính
    // vùng lấy (inputRect) của slice.
    const float texW = m_canvas.getWidth();
    const float texH = m_canvas.getHeight();

    m_sliceMesh.clear();
    m_sliceMesh.setMode(OF_PRIMITIVE_TRIANGLES);

    for (const WarpVertex& v : geo.vertices) {
        m_sliceMesh.addVertex(glm::vec3(static_cast<float>(v.position.x),
                                        static_cast<float>(v.position.y), 0.0f));

        const Vec2 canvasPt = slice.contentToCanvas(v.uv);
        // oF với ARB texture dùng toạ độ pixel; FBO ở đây là texture 2D
        // chuẩn hoá, nên chia cho kích thước.
        m_sliceMesh.addTexCoord(glm::vec2(
            static_cast<float>(canvasPt.x) / texW,
            static_cast<float>(canvasPt.y) / texH));
    }
    for (const unsigned int idx : geo.indices) {
        m_sliceMesh.addIndex(static_cast<ofIndexType>(idx));
    }

    m_canvas.getTexture().bind();
    m_sliceMesh.draw();
    m_canvas.getTexture().unbind();
}

void RenderEngine::renderScreen(const Screen& screen, const EditState& edit) {
    ofClear(0, 0, 0, 255);
    if (!m_canvas.isAllocated()) return;

    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    ofSetColor(255);

    const std::vector<int> visible = screen.visibleSlices();
    m_lastSlicesDrawn = static_cast<int>(visible.size());

    for (const int i : visible) {
        drawSliceGeometry(screen.slices[static_cast<size_t>(i)]);
    }

    // ★ Overlay CHỈ hiện khi edit mode bật. Lúc chạy show phải tắt hoàn
    //   toàn — khán giả không được thấy handle và lưới (I5).
    if (edit.showOverlay) {
        if (edit.showGrid) drawTestGrid(screen);
        drawEditOverlay(screen, edit);
    }

    // G6 va G13 ve NGAY CA khi overlay chinh sua bi tat.
    //   · dau thap calibration: dang calibrate thi phai thay no
    //   · diem sensor: de kiem chung calibration bang mat, va de debug
    //     giua show ma khong phai bat lai toan bo handle
    if (edit.calibrating)      drawCalibTarget(screen, edit);
    if (edit.showSensorPoints) drawSensorPointsOnOutput(edit);
}

// ── G6: dau thap calibration ───────────────────────────────────────────

void RenderEngine::drawCalibTarget(const Screen& screen, const EditState& edit) const {
    if (edit.activeSliceIndex < 0 || edit.activeSliceIndex >= screen.sliceCount()) return;

    const Slice& s = screen.slices[static_cast<size_t>(edit.activeSliceIndex)];
    const glm::vec3 p = toOf(s.contentToOutput(edit.calibTargetUV));

    ofPushStyle();

    // Nen toi hinh tron: dau thap phai noi bat KE CA khi chieu len noi
    // dung sang mau. Khong co no, nguoi van hanh khong nhin ra diem nham.
    ofFill();
    ofSetColor(0, 0, 0, 170);
    ofDrawCircle(p.x, p.y, 46.0f);

    ofNoFill();
    ofSetLineWidth(3.0f);
    ofSetColor(255, 235, 60);

    constexpr float kArm = 40.0f;
    constexpr float kGap = 8.0f;          // chua o giua de khong che diem
    ofDrawLine(p.x - kArm, p.y, p.x - kGap, p.y);
    ofDrawLine(p.x + kGap, p.y, p.x + kArm, p.y);
    ofDrawLine(p.x, p.y - kArm, p.x, p.y - kGap);
    ofDrawLine(p.x, p.y + kGap, p.x, p.y + kArm);

    ofDrawCircle(p.x, p.y, 22.0f);
    ofFill();
    ofSetColor(255, 235, 60);
    ofDrawCircle(p.x, p.y, 3.5f);

    // So buoc, de nguoi dung biet dang o dau trong quy trinh 4 buoc.
    ofSetColor(255, 235, 60);
    ofDrawBitmapString("CHAM VAO DAY  (" + std::to_string(edit.calibStep + 1) + "/4)",
                       p.x - 60.0f, p.y + 66.0f);

    ofPopStyle();
}

// ── G17: vung cam ung ──────────────────────────────────────────────────

void RenderEngine::drawTriggerZones(const Screen& screen,
                                    const TriggerZoneSet& zones,
                                    const EditState& edit) const {
    if (!edit.showOverlay || zones.zones.empty()) return;

    ofPushStyle();
    ofSetLineWidth(2.0f);

    for (const int si : screen.visibleSlices()) {
        const Slice& slice = screen.slices[static_cast<size_t>(si)];
        if (!slice.isUsable()) continue;

        for (const TriggerZone& z : zones.zones) {
            if (!z.enabled) continue;

            // Goc vung trong khong gian canvas -> contentUV cua slice này
            // -> pixel may chieu. Vung nam ngoai vung lay cua slice se co
            // UV ngoai [0,1] va bi bo qua — dung, vi slice do khong chieu
            // phan canvas do.
            const Vec2 corners[4] = {
                {z.origin.x,            z.origin.y},
                {z.origin.x + z.size.x, z.origin.y},
                {z.origin.x + z.size.x, z.origin.y + z.size.y},
                {z.origin.x,            z.origin.y + z.size.y},
            };

            ofPolyline poly;
            bool anyInside = false;
            for (int k = 0; k < 4; ++k) {
                const Vec2 uv = slice.canvasToContent(corners[k]);
                if (uv.x >= -0.02 && uv.x <= 1.02 && uv.y >= -0.02 && uv.y <= 1.02) {
                    anyInside = true;
                }
                poly.addVertex(toOf(slice.contentToOutput(uv)));
            }
            if (!anyInside) continue;
            poly.close();

            // Vung dang co ngon tay -> to sang. Day la phan hoi truc quan
            // quan trong nhat khi can chinh: nhin la biet cham co trung
            // vung khong.
            if (z.occupied) {
                ofFill();
                ofSetColor(0, 255, 150, 60);
                ofBeginShape();
                for (const auto& v : poly.getVertices()) ofVertex(v);
                ofEndShape(true);
                ofNoFill();
                ofSetColor(0, 255, 150);
            } else {
                ofNoFill();
                ofSetColor(120, 190, 255, 170);
            }
            poly.draw();

            if (!z.name.empty()) {
                const auto& v0 = poly.getVertices().front();
                ofDrawBitmapString(z.name, v0.x + 6.0f, v0.y + 16.0f);
            }
        }
    }

    ofPopStyle();
}

// ── G13: diem sensor tren output ───────────────────────────────────────

void RenderEngine::drawSensorPointsOnOutput(const EditState& edit) const {
    if (edit.sensorOutputPoints.empty()) return;

    ofPushStyle();
    for (const Vec2& v : edit.sensorOutputPoints) {
        const glm::vec3 p = toOf(v);

        ofFill();
        ofSetColor(0, 255, 150, 140);
        ofDrawCircle(p.x, p.y, 10.0f);

        ofNoFill();
        ofSetLineWidth(2.0f);
        ofSetColor(0, 255, 150);
        ofDrawCircle(p.x, p.y, 22.0f);
    }
    ofPopStyle();
}

void RenderEngine::drawTestGrid(const Screen& screen) const {
    // F14 — lưới calibration để căn máy chiếu vào vật thể.
    ofPushStyle();
    ofSetColor(70, 70, 70);
    ofSetLineWidth(1.0f);

    const float w = static_cast<float>(screen.resolution.x);
    const float h = static_cast<float>(screen.resolution.y);
    constexpr int kDiv = 16;

    for (int i = 0; i <= kDiv; ++i) {
        const float t = static_cast<float>(i) / kDiv;
        ofDrawLine(t * w, 0.0f, t * w, h);
        ofDrawLine(0.0f, t * h, w, t * h);
    }

    ofSetColor(120, 120, 120);
    ofDrawLine(w * 0.5f, 0.0f, w * 0.5f, h);
    ofDrawLine(0.0f, h * 0.5f, w, h * 0.5f);
    ofPopStyle();
}

void RenderEngine::drawEditOverlay(const Screen& screen, const EditState& edit) const {
    ofPushStyle();
    ofNoFill();
    ofSetLineWidth(2.0f);

    for (int i = 0; i < screen.sliceCount(); ++i) {
        const Slice& s = screen.slices[static_cast<size_t>(i)];
        const IWarp* w = s.warp();
        if (w == nullptr) continue;

        const bool active = (i == edit.activeSliceIndex);
        const bool broken = !w->isInvertible();

        // Viền ĐỎ khi warp hỏng — người dùng cần biết ngay vì sao kéo
        // góc bị "bật ngược" (xem test "keo goc thanh hinh LOM bi tu choi").
        if (broken)      ofSetColor(255, 60, 60);
        else if (active) ofSetColor(255, 200, 60);
        else             ofSetColor(90, 140, 200, 160);

        // Viền slice — đi theo mép của lưới đã tessellate để bề mặt cong
        // hiện đúng hình, không phải hình chữ nhật bao ngoài.
        const int sub = std::max(1, w->defaultSubdivisions());
        ofPolyline outline;
        for (int k = 0; k <= sub; ++k) outline.addVertex(toOf(w->forward({static_cast<double>(k) / sub, 0.0})));
        for (int k = 1; k <= sub; ++k) outline.addVertex(toOf(w->forward({1.0, static_cast<double>(k) / sub})));
        for (int k = sub - 1; k >= 0; --k) outline.addVertex(toOf(w->forward({static_cast<double>(k) / sub, 1.0})));
        for (int k = sub - 1; k >= 1; --k) outline.addVertex(toOf(w->forward({0.0, static_cast<double>(k) / sub})));
        outline.close();
        outline.draw();

        if (!active) continue;

        // Handle diem dieu khien — chi cho slice dang chon, de khong roi mat.
        //
        // Dung API tong quat IWarp::controlPointAt: corner pin cho 4 diem,
        // mesh cho (cols+1)x(rows+1). Code o day KHONG can biet la loai nao.
        ofFill();
        const int n = w->controlPointCount();

        // Luoi day thi handle nho lai, neu khong chung dinh vao nhau.
        const float r = (n > 25) ? kHandleRadius * 0.6f : kHandleRadius;

        for (int k = 0; k < n; ++k) {
            const glm::vec3 p = toOf(w->controlPointAt(k));
            if (k == edit.draggedHandle)      ofSetColor(255, 255, 255);
            else if (k == edit.hoveredHandle) ofSetColor(255, 230, 120);
            else                              ofSetColor(255, 200, 60);
            ofDrawCircle(p.x, p.y, r);
            ofSetColor(20);
            ofDrawCircle(p.x, p.y, r * 0.4f);
        }
        ofNoFill();
    }

    ofPopStyle();
}

// ═══════════════════════════════════════════════════════════════════════
//  Preview cho cửa sổ control
// ═══════════════════════════════════════════════════════════════════════

void RenderEngine::drawCanvasPreview(float x, float y, float w, float h) const {
    if (!m_canvas.isAllocated()) return;

    ofPushStyle();
    ofSetColor(30);
    ofDrawRectangle(x, y, w, h);
    ofSetColor(255);
    m_canvas.draw(x, y, w, h);
    ofNoFill();
    ofSetColor(80);
    ofDrawRectangle(x, y, w, h);
    ofPopStyle();
}

void RenderEngine::drawSensorOverlay(const std::vector<Vec2>& canvasPoints,
                                     float x, float y, float w, float h) const {
    if (canvasPoints.empty() || m_canvasSize.x <= 0.0 || m_canvasSize.y <= 0.0) return;

    ofPushStyle();
    for (const Vec2& p : canvasPoints) {
        const float px = x + static_cast<float>(p.x / m_canvasSize.x) * w;
        const float py = y + static_cast<float>(p.y / m_canvasSize.y) * h;

        ofFill();
        ofSetColor(0, 255, 140, 180);
        ofDrawCircle(px, py, 6.0f);
        ofNoFill();
        ofSetColor(0, 255, 140);
        ofDrawCircle(px, py, 12.0f);
    }
    ofPopStyle();
}

} // namespace hexmap

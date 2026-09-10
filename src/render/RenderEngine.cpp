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

namespace {

const char* kSliceVert = R"(#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
in vec2 texcoord;

// UV CUC BO cua slice [0,1]^2, dong goi vao kenh color.
// texcoord da dung cho toa do canvas (co tinh inputRect), nen soft edge
// can mot kenh khac. oF khong co attribute tuy y nen muon kenh color.
in vec4 color;

out vec2 vTexCoord;
out vec2 vSliceUV;

void main() {
    vTexCoord   = texcoord;
    vSliceUV    = color.rg;
    gl_Position = modelViewProjectionMatrix * position;
}
)";

const char* kSliceFrag = R"(#version 150
uniform sampler2D tex0;
uniform float uBrightness;
uniform float uContrast;
uniform float uInvGamma;
uniform vec3  uGain;
uniform float uOpacity;

// F20 — soft edge. (trai, phai, tren, duoi) tinh theo ti le slice.
uniform vec4  uEdge;
uniform float uEdgeGamma;
uniform float uEdgeLum;

// F12 — mat na bezier, nuong san ra texture o khong gian contentUV.
uniform sampler2D uMask;
uniform float uMaskOn;       // 0 = khong co mat na
uniform float uMaskInvert;
uniform vec2  uFeatherUV;    // ban kinh lam mo, theo UV moi truc

in  vec2 vTexCoord;
in  vec2 vSliceUV;
out vec4 fragColor;

// Lay mau mat na, co lam mo mep.
//
// Lam mo bang cach lay nhieu mau NGAY LUC VE chu khong blur san vao
// texture. Nho vay keo thanh truot feather la thay ngay, khong phai
// nuong lai texture moi frame — nuong lai giua luc keo se giat.
//
// Hop 5x5 = 25 mau. Tren output 1920x1080 la ~50 trieu luot doc texture
// moi frame — vai phan tram GPU hien dai, va CHI chay khi feather > 0.
float maskCoverage() {
    if (uMaskOn < 0.5) return 1.0;

    float m;
    if (uFeatherUV.x <= 0.0 && uFeatherUV.y <= 0.0) {
        m = texture(uMask, vSliceUV).r;
    } else {
        float acc = 0.0;
        for (int j = -2; j <= 2; ++j) {
            for (int i = -2; i <= 2; ++i) {
                vec2 o = vec2(float(i), float(j)) * 0.5 * uFeatherUV;
                acc += texture(uMask, vSliceUV + o).r;
            }
        }
        m = acc / 25.0;

        // Hop blur cho ra doc TUYEN TINH; smoothstep bo hai dau goc canh
        // de mep mo nhin lien mach hon.
        m = smoothstep(0.0, 1.0, m);
    }

    return (uMaskInvert > 0.5) ? (1.0 - m) : m;
}

// Duong cong hoa vien. Tong hai duong cong doi dien phai bang 1 o moi
// diem, neu khong vung chong se sang hon hoac toi hon phan con lai.
float edgeCurve(float d, float w) {
    if (w <= 0.0) return 1.0;
    float t = clamp(d / w, 0.0, 1.0);
    if (t < 0.5) return uEdgeLum * pow(2.0 * t, uEdgeGamma);
    return 1.0 - (1.0 - uEdgeLum) * pow(2.0 * (1.0 - t), uEdgeGamma);
}

void main() {
    vec4 c = texture(tex0, vTexCoord);
    vec3 rgb = c.rgb;

    // Tuong phan quanh diem giua 0.5, roi cong do sang.
    rgb = (rgb - 0.5) * uContrast + 0.5 + uBrightness;

    // Gamma. max() de pow() khong nhan so am -> NaN.
    rgb = pow(max(rgb, vec3(0.0)), vec3(uInvGamma));

    rgb *= uGain;

    float a = c.a * uOpacity;
    a *= maskCoverage();                          // F12
    a *= edgeCurve(vSliceUV.x,       uEdge.x);   // trai
    a *= edgeCurve(1.0 - vSliceUV.x, uEdge.y);   // phai
    a *= edgeCurve(vSliceUV.y,       uEdge.z);   // tren
    a *= edgeCurve(1.0 - vSliceUV.y, uEdge.w);   // duoi

    fragColor = vec4(clamp(rgb, 0.0, 1.0), a);
}
)";

// ── Shader YCoCg -> RGB cho HAP Q (#version 150) ───────────────────────
//
// Cong thuc lay dung tu ofxHapPlayer, chi viet lai theo cu phap core
// profile. Shader goc cua addon dung GLSL 120 fixed-function nen khong
// chay duoc o day.
const char* kYCoCgVert = R"(#version 150
uniform mat4 modelViewProjectionMatrix;
in vec4 position;
in vec2 texcoord;
out vec2 vTexCoord;
void main() {
    vTexCoord   = texcoord;
    gl_Position = modelViewProjectionMatrix * position;
}
)";

const char* kYCoCgFrag = R"(#version 150
uniform sampler2D cocgsy_src;
uniform vec4 globalColor;

in  vec2 vTexCoord;
out vec4 fragColor;

const vec4 offsets = vec4(-0.50196078431373, -0.50196078431373, 0.0, 0.0);

void main() {
    vec4 CoCgSY = texture(cocgsy_src, vTexCoord) + offsets;

    // Kenh Z mang he so ti le, khong phai mau. Giai nen truoc khi tach
    // Co/Cg, neu khong mau se lech han o vung bao hoa cao.
    float scale = (CoCgSY.z * (255.0 / 8.0)) + 1.0;
    float Co = CoCgSY.x / scale;
    float Cg = CoCgSY.y / scale;
    float Y  = CoCgSY.w;

    fragColor = vec4(Y + Co - Cg,
                     Y + Cg,
                     Y - Co - Cg,
                     1.0) * globalColor;
}
)";

} // namespace

bool RenderEngine::buildYCoCgShader() {
    if (m_ycocgReady) return true;

    m_ycocgShader.setupShaderFromSource(GL_VERTEX_SHADER, kYCoCgVert);
    m_ycocgShader.setupShaderFromSource(GL_FRAGMENT_SHADER, kYCoCgFrag);
    m_ycocgShader.bindDefaults();
    m_ycocgReady = m_ycocgShader.linkProgram();

    if (!m_ycocgReady) {
        ofLogError("RenderEngine") << "Khong link duoc shader YCoCg — "
                                      "video HAP Q se sai mau";
    }
    return m_ycocgReady;
}

bool RenderEngine::buildSliceShader() {
    if (m_shaderReady) return true;

    m_sliceShader.setupShaderFromSource(GL_VERTEX_SHADER, kSliceVert);
    m_sliceShader.setupShaderFromSource(GL_FRAGMENT_SHADER, kSliceFrag);
    m_sliceShader.bindDefaults();
    m_shaderReady = m_sliceShader.linkProgram();

    if (!m_shaderReady) {
        // Khong dung duoc shader thi van ve duoc, chi mat hieu chinh mau.
        // Mat mau con hon mat hinh giua show.
        ofLogError("RenderEngine") << "Khong link duoc slice shader — "
                                      "se ve khong co hieu chinh mau (F19)";
    }
    return m_shaderReady;
}

bool RenderEngine::setup(const Vec2& canvasSize) {
    resizeCanvas(canvasSize);
    buildSliceShader();
    buildYCoCgShader();
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

    // ★ HAP Q luu o khong gian mau YCoCg (HapTextureFormat_YCoCg_DXT5),
    //   KHONG phai RGB. Ve texture tho ma khong chuyen doi se cho mau
    //   sai hoan toan.
    //
    //   ofxHapPlayer::getShader() tra ve non-null DUNG KHI codec la HapY,
    //   nen dung no lam TIN HIEU nhan biet. Nhung KHONG dung chinh shader
    //   do: no viet bang GLSL 120 fixed-function, khong chay duoc trong
    //   GL 3.2 core profile — link duoc nhung khong ve ra gi.
    const bool needsYCoCg =
        (e->video != nullptr) && (e->video->getShader() != nullptr);

    if (needsYCoCg && m_ycocgReady) {
        m_ycocgShader.begin();
        m_ycocgShader.setUniformTexture("cocgsy_src", *tex, 0);
        m_ycocgShader.setUniform4f("globalColor",
            1.0f, 1.0f, 1.0f,
            static_cast<float>(ofClamp(opacity, 0.0, 1.0)));
    }

    tex->draw(0.0f, 0.0f,
              static_cast<float>(contentSize.x),
              static_cast<float>(contentSize.y));

    if (needsYCoCg && m_ycocgReady) m_ycocgShader.end();

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

// ── F12: nuong mat na ra texture ───────────────────────────────────────

const ofFbo* RenderEngine::maskTexture(const Slice& slice, int sliceIndex,
                                       int screenId) const {
    if (!slice.mask.isActive()) return nullptr;

    const uint64_t key = (static_cast<uint64_t>(static_cast<uint32_t>(screenId)) << 32)
                       |  static_cast<uint64_t>(static_cast<uint32_t>(sliceIndex));
    MaskTex& mt = m_maskCache[key];
    mt.lastUsedFrame = m_frameCounter;

    const uint64_t want = slice.mask.geometryHash();
    if (mt.fbo.isAllocated() && mt.hash == want) return &mt.fbo;

    // ── Kich thuoc texture ─────────────────────────────────────────────
    //
    // Mat na song o khong gian contentUV (0..1 ca hai truc), nhung slice
    // thuong khong vuong. Cap phat texture vuong cho mot slice 1920x200
    // thi chieu doc thua rat nhieu texel con chieu ngang lai thieu.
    // Chia theo TI LE cua inputSize — dai lien la kich thuoc vung LAY,
    // on dinh, khong doi khi nguoi dung keo goc keystone.
    const double sx = std::max(1.0, slice.inputSize.x);
    const double sy = std::max(1.0, slice.inputSize.y);
    const double longSide = std::max(sx, sy);

    constexpr int kMaxDim = 1024;
    constexpr int kMinDim = 64;
    const int tw = std::clamp(static_cast<int>(std::lround(kMaxDim * sx / longSide)),
                              kMinDim, kMaxDim);
    const int th = std::clamp(static_cast<int>(std::lround(kMaxDim * sy / longSide)),
                              kMinDim, kMaxDim);

    if (!mt.fbo.isAllocated()
        || mt.fbo.getWidth() != tw || mt.fbo.getHeight() != th) {
        ofFbo::Settings fs;
        fs.width  = tw;
        fs.height = th;
        fs.internalformat = GL_RGBA8;
        fs.useDepth   = false;
        fs.useStencil = false;

        // MSAA o day KHONG phai xa xi: mep mat na la mot duong cheo tuy y
        // tren mot texture co the thap hon do phan giai may chieu. Khong
        // khu rang cua thi mep hien ra thanh bac thang tren tuong that.
        // Chi phi tra MOT LAN moi khi hinh doi, khong phai moi frame.
        fs.numSamples = 4;

        mt.fbo.allocate(fs);

        // Bam vao mep: lay mau ngoai [0,1] (do offset feather) phai lay
        // gia tri o bien, khong duoc lap vong sang phia doi dien.
        mt.fbo.getTexture().setTextureWrap(GL_CLAMP_TO_EDGE, GL_CLAMP_TO_EDGE);
    }

    // ── Ve duong bien ──────────────────────────────────────────────────
    std::vector<Vec2> poly;
    slice.mask.flatten(poly);

    ofPushStyle();

    mt.fbo.begin();
    ofClear(0, 0, 0, 0);

    // Ghi DE, khong hoa tron: ta muon do phu tho cua hinh, con mep muot
    // la do MSAA lo khi resolve. Bat hoa tron o day chi lam gia tri mep
    // phu thuoc vao thu tu ve.
    ofDisableBlendMode();
    ofSetColor(255);

    if (poly.size() >= 3) {
        ofPath path;
        path.setFilled(true);
        path.setFillColor(ofColor(255, 255, 255, 255));

        // Chan-le, KHONG phai nonzero. Hinh tu cat khi do cho ra vung
        // rong nhin thay duoc thay vi to dac bat ngo — va no khop voi
        // BezierMask::containsUV, nen cai nhin thay va cai hit-test bao
        // luon la mot.
        path.setPolyWindingMode(OF_POLY_WINDING_ODD);

        for (size_t i = 0; i < poly.size(); ++i) {
            const float x = static_cast<float>(poly[i].x) * static_cast<float>(tw);
            const float y = static_cast<float>(poly[i].y) * static_cast<float>(th);
            if (i == 0) path.moveTo(x, y);
            else        path.lineTo(x, y);
        }
        path.close();
        path.draw();
    }
    mt.fbo.end();

    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    ofPopStyle();

    mt.hash = want;
    return &mt.fbo;
}

void RenderEngine::pruneMaskCache() const {
    // Slice bi xoa thi texture cua no khong con ai hoi toi. Doi mot lat
    // roi don, thay vi don ngay: doi qua lai giua hai slice trong luc
    // can chinh se lien tuc cap phat lai FBO.
    constexpr uint64_t kIdleFrames = 600;   // ~10 giay o 60fps

    for (auto it = m_maskCache.begin(); it != m_maskCache.end(); ) {
        if (m_frameCounter - it->second.lastUsedFrame > kIdleFrames) {
            it = m_maskCache.erase(it);
        } else {
            ++it;
        }
    }
}

void RenderEngine::drawSliceGeometry(const Slice& slice, int sliceIndex,
                                    int screenId) const {
    const IWarp* w = slice.warp();
    if (w == nullptr) return;

    // ★ Nuong texture mat na TRUOC MOI THU KHAC.
    //
    //   maskTexture() ve vao mot FBO khac. Goi no sau m_sliceShader.begin()
    //   thi ofPath::draw() se ve duong mat na BANG CHINH slice shader do —
    //   shader ay lay mau tex0 va uMask, cho ra texture den si, va mat na
    //   cat sach toan bo noi dung. Trieu chung nhin thay la man hinh den
    //   hoan toan, khong he giong "loi ve mat na".
    const ofFbo* maskFbo = maskTexture(slice, sliceIndex, screenId);

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

        // F20 — UV CUC BO cua slice, doi qua kenh color. Soft edge phai
        // tinh theo mep SLICE, khong phai theo toa do canvas.
        m_sliceMesh.addColor(ofFloatColor(static_cast<float>(v.uv.x),
                                          static_cast<float>(v.uv.y),
                                          0.0f, 1.0f));
    }
    for (const unsigned int idx : geo.indices) {
        m_sliceMesh.addIndex(static_cast<ofIndexType>(idx));
    }

    // F19 — ap hieu chinh mau qua shader. Neu shader khong dung duoc,
    // ve theo duong mac dinh: mat mau con hon mat hinh.
    if (m_shaderReady) {
        const ColorAdjust& c = slice.color;

        m_sliceShader.begin();
        m_sliceShader.setUniformTexture("tex0", m_canvas.getTexture(), 0);
        m_sliceShader.setUniform1f("uBrightness", static_cast<float>(c.brightness));
        m_sliceShader.setUniform1f("uContrast",   static_cast<float>(c.contrast));
        m_sliceShader.setUniform1f("uInvGamma",
            static_cast<float>(1.0 / std::max(0.05, c.gamma)));
        m_sliceShader.setUniform3f("uGain",
            static_cast<float>(c.gainR),
            static_cast<float>(c.gainG),
            static_cast<float>(c.gainB));
        m_sliceShader.setUniform1f("uOpacity", static_cast<float>(c.opacity));

        const SoftEdge& e = slice.softEdge;
        m_sliceShader.setUniform4f("uEdge",
            static_cast<float>(e.left),  static_cast<float>(e.right),
            static_cast<float>(e.top),   static_cast<float>(e.bottom));
        m_sliceShader.setUniform1f("uEdgeGamma", static_cast<float>(std::max(0.05, e.gamma)));
        m_sliceShader.setUniform1f("uEdgeLum",   static_cast<float>(e.luminance));

        // F12 — mat na. Shader luon co sampler uMask; khi khong co mat na
        // thi uMaskOn = 0 va sampler khong bao gio duoc doc. Van phai
        // GAN mot texture nao do: sampler chua rang buoc trong GL core
        // profile la hanh vi khong xac dinh, ke ca khi nhanh code khong
        // chay toi — driver van co the tra ve rac hoac bao loi.
        if (maskFbo != nullptr) {
            m_sliceShader.setUniformTexture("uMask", maskFbo->getTexture(), 1);
            m_sliceShader.setUniform1f("uMaskOn", 1.0f);
            m_sliceShader.setUniform1f("uMaskInvert", slice.mask.invert ? 1.0f : 0.0f);

            // Feather duoc khai theo canh NGAN cua slice. Doi sang UV thi
            // moi truc mot he so khac nhau, neu khong mep mo se bi keo
            // gian theo ti le slice — mot slice 1920x200 se co vien tren
            // duoi day gap 10 lan vien trai phai.
            const double sx = std::max(1.0, slice.inputSize.x);
            const double sy = std::max(1.0, slice.inputSize.y);
            const double shortSide = std::min(sx, sy);
            const double f = std::clamp(slice.mask.feather, 0.0, 0.5);

            m_sliceShader.setUniform2f("uFeatherUV",
                static_cast<float>(f * shortSide / sx),
                static_cast<float>(f * shortSide / sy));
        } else {
            m_sliceShader.setUniformTexture("uMask", m_canvas.getTexture(), 1);
            m_sliceShader.setUniform1f("uMaskOn", 0.0f);
            m_sliceShader.setUniform1f("uMaskInvert", 0.0f);
            m_sliceShader.setUniform2f("uFeatherUV", 0.0f, 0.0f);
        }

        m_sliceMesh.draw();
        m_sliceShader.end();
    } else {
        m_canvas.getTexture().bind();
        m_sliceMesh.draw();
        m_canvas.getTexture().unbind();
    }
}

void RenderEngine::renderScreen(const Screen& screen, const EditState& edit) {
    ofClear(0, 0, 0, 255);
    if (!m_canvas.isAllocated()) return;

    ofEnableBlendMode(OF_BLENDMODE_ALPHA);
    ofSetColor(255);

    const std::vector<int> visible = screen.visibleSlices();
    m_lastSlicesDrawn = static_cast<int>(visible.size());

    ++m_frameCounter;
    if ((m_frameCounter % 300ull) == 0ull) pruneMaskCache();

    for (const int i : visible) {
        drawSliceGeometry(screen.slices[static_cast<size_t>(i)], i, screen.id);
    }

    // ★ FULLSCREEN = dang chieu cho khan gia xem: CHI noi dung.
    //   Khong vien slice, khong handle, khong luoi, khong vung cam ung,
    //   khong cham sensor. Bat cu thu gi khac deu la loi hien ra man anh.
    //
    //   Hai NGOAI LE, va ca hai cung mot ly do: chung phai nhin thay
    //   TREN VAT THE THAT thi cong viec moi lam duoc.
    //
    //     · Dau thap calibration — nguoi van hanh phai cham vao no
    //     · Duong mat na (F12) — phai khop voi mep vat the that; nhin
    //       vao khung xem thu trong app thi khong can duoc
    //
    //   Ca hai deu co cong tac RIENG do nguoi dung chu dong bat (phim C,
    //   va o "Chinh mat na"), nen chung khong bao gio tu nhien hien ra
    //   giua buoi dien.
    if (edit.outputIsFullscreen) {
        if (edit.maskEditMode) drawMaskOutline(screen, edit);
        if (edit.calibrating)  drawCalibTarget(screen, edit);
        return;
    }

    if (edit.showOverlay) {
        if (edit.showGrid) drawTestGrid(screen);
        drawEditOverlay(screen, edit);
    }

    if (edit.maskEditMode)     drawMaskOutline(screen, edit);
    if (edit.calibrating)      drawCalibTarget(screen, edit);
    if (edit.showSensorPoints) drawSensorPointsOnOutput(edit);
}

// ── F12: duong mat na tren may chieu ───────────────────────────────────
//
// Chinh mat na phai NHIN VAO VAT THE THAT ma keo, khong phai nhin vao
// bang xem thu trong app: viec can lam la khop duong cat voi mep vat the.
// Vi vay duong mat na phai hien ca tren may chieu.

void RenderEngine::drawMaskOutline(const Screen& screen, const EditState& edit) const {
    if (edit.activeSliceIndex < 0 || edit.activeSliceIndex >= screen.sliceCount()) return;

    const Slice& s = screen.slices[static_cast<size_t>(edit.activeSliceIndex)];
    if (s.mask.nodes.size() < 2 || s.warp() == nullptr) return;

    std::vector<Vec2> poly;
    s.mask.flatten(poly);
    if (poly.size() < 2) return;

    ofPushStyle();

    ofNoFill();
    ofSetLineWidth(2.0f);
    ofSetColor(120, 255, 200);

    ofPolyline line;
    for (const Vec2& uv : poly) line.addVertex(toOf(s.contentToOutput(uv)));
    line.close();
    line.draw();

    // Diem neo + tay nam.
    const int n = static_cast<int>(s.mask.nodes.size());
    for (int i = 0; i < n; ++i) {
        const MaskNode& nd = s.mask.nodes[static_cast<size_t>(i)];
        const glm::vec3 p = toOf(s.contentToOutput(nd.point));

        if (!nd.isCorner()) {
            ofSetLineWidth(1.0f);
            ofSetColor(120, 255, 200, 140);
            const glm::vec3 a = toOf(s.contentToOutput(nd.inPoint()));
            const glm::vec3 b = toOf(s.contentToOutput(nd.outPoint()));
            ofDrawLine(p.x, p.y, a.x, a.y);
            ofDrawLine(p.x, p.y, b.x, b.y);
            ofFill();
            ofDrawCircle(a.x, a.y, 4.0f);
            ofDrawCircle(b.x, b.y, 4.0f);
        }

        ofFill();
        ofSetColor(i == edit.maskDraggedNode ? ofColor(255, 255, 255)
                 : i == edit.maskHoveredNode ? ofColor(200, 255, 230)
                                             : ofColor(120, 255, 200));
        ofDrawCircle(p.x, p.y, 7.0f);
        ofSetColor(20);
        ofDrawCircle(p.x, p.y, 3.0f);
    }

    ofPopStyle();
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

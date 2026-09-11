#include "ofApp.h"

// ════════════════════════════════════════════════════════════════════════
//  SPIKE R1 — đo khả năng phát video 4K HAP
//
//  ── Vì sao đo NHIỀU luồng chứ không chỉ 1 ────────────────────────────
//  Số luồng phát đồng thời = số LAYER đang bật, không phải số clip
//  trong lưới. Một lưới 32 clip với 1 layer chỉ phát 1 luồng.
//
//  Nhưng con số không bao giờ đúng bằng 1:
//    · lúc chuyển clip (dissolve) có 2 luồng cùng chạy
//    · thêm layer overlay → +1
//    · preload clip kế tiếp → +1
//
//  Vì vậy công cụ này chỉnh được 1..8 luồng. Mặc định 1 — trường hợp
//  thường gặp nhất. Bấm phím số để tăng và tìm ngưỡng trần thật.
// ════════════════════════════════════════════════════════════════════════

namespace {
constexpr int kMaxPlayers = 8;

/// Đường dẫn tuyệt đối tới file mẫu đã sinh bằng ffmpeg.
/// Dùng đường dẫn tuyệt đối để spike không phụ thuộc thư mục data riêng.
const std::string kMediaDir = "D:/2026/Mike/HexMapping/bin/data/media/";
} // namespace

void ofApp::setup() {
    ofSetVerticalSync(m_vsync);
    ofBackground(18);
    ofSetFrameRate(0);          // không giới hạn — muốn thấy trần thật
    ofDisableArbTex();

    // Chọn nội dung "xấu nhất": bản nhiễu, vì Snappy gần như không nén
    // được nó (237 MB/s — sát trần lý thuyết 249 MB/s của HAP Q 4K30).
    // Nếu bản này chạy được thì nội dung đồ hoạ chắc chắn chạy được.
    m_mediaPath  = kMediaDir + "4k_noisy_hap_q.mov";
    m_mediaLabel = "4k_noisy_hap_q.mov (truong hop XAU NHAT)";

    if (!ofFile::doesFileExist(m_mediaPath)) {
        m_mediaPath  = kMediaDir + "4k_detail_hap_q.mov";
        m_mediaLabel = "4k_detail_hap_q.mov";
    }

    if (m_benchMode) {
        m_benchCurrent = 1;
        setPlayerCount(1);
    } else {
        setPlayerCount(1);      // ← mặc định 1 luồng
    }
    m_lastFrameNs = ofGetElapsedTimeMicros() * 1000ull;
}

void ofApp::benchTick() {
    m_benchPhaseTimer += ofGetLastFrameTime();

    if (!m_benchMeasuring) {
        // Giai đoạn khởi động: decoder đang nạp buffer, chưa đo.
        if (m_benchPhaseTimer >= kBenchWarmup) {
            m_benchMeasuring  = true;
            m_benchPhaseTimer = 0.0f;
            m_frameTimes.clear();
            m_newFrameCount   = 0;
        }
        return;
    }

    if (m_benchPhaseTimer < kBenchMeasure) return;

    // Chốt kết quả cho mức hiện tại.
    const Stats s = frameStats();
    BenchRow row;
    row.players = m_benchCurrent;
    row.fps     = (s.avg > 0.0) ? (1000.0 / s.avg) : 0.0;
    row.avgMs   = s.avg;
    row.p99Ms   = s.p99;
    row.worstMs = s.worst;
    row.videoFps = (m_benchCurrent > 0)
        ? m_newFrameCount / (kBenchMeasure * m_benchCurrent) : 0.0;
    m_benchResults.push_back(row);

    ofLogNotice("bench") << row.players << " luong: "
                         << ofToString(row.fps, 1) << " fps, p99 "
                         << ofToString(row.p99Ms, 2) << " ms, video "
                         << ofToString(row.videoFps, 1) << " fps";

    if (m_benchCurrent >= kBenchMax || m_loadFailed) {
        writeBenchReport();
        ofExit();
        return;
    }

    ++m_benchCurrent;
    setPlayerCount(m_benchCurrent);
    m_benchMeasuring  = false;
    m_benchPhaseTimer = 0.0f;
}

void ofApp::writeBenchReport() {
    std::ostringstream out;
    out << "# HexMapping - Spike R1: ket qua phat 4K HAP\n";
    out << "# Media: " << m_mediaLabel << "\n";
    out << "# GPU  : " << glGetString(GL_RENDERER) << "\n";
    out << "# vsync: OFF (do tran that su)\n";
    out << "# Luu y: so luong = so LAYER dang phat, khong phai so clip trong luoi\n";
    out << "#\n";
    out << "luong,render_fps,avg_ms,p99_ms,worst_ms,VIDEO_fps,dat_60fps\n";

    for (const BenchRow& r : m_benchResults) {
        out << r.players << ","
            << ofToString(r.fps, 1) << ","
            << ofToString(r.avgMs, 2) << ","
            << ofToString(r.p99Ms, 2) << ","
            << ofToString(r.worstMs, 2) << ","
            << ofToString(r.videoFps, 1) << ","
            << ((r.p99Ms > 0.0 && r.p99Ms <= 16.7) ? "YES" : "no")
            << "\n";
    }

    const std::string path = "D:/2026/Mike/HexMapping/tools/spike_hap/spike_r1_result.csv";
    ofBufferFromFile(path);
    ofBuffer buf;
    buf.set(out.str());
    ofBufferToFile(path, buf);
    ofLogNotice("bench") << "Da ghi: " << path;
}

void ofApp::setPlayerCount(int n) {
    n = ofClamp(n, 0, kMaxPlayers);

    while (static_cast<int>(m_players.size()) > n) {
        m_players.pop_back();
    }

    while (static_cast<int>(m_players.size()) < n) {
        auto p = std::make_unique<ofxHapPlayer>();
        if (!p->load(m_mediaPath)) {
            m_loadFailed = true;
            ofLogError("spike") << "Khong load duoc: " << m_mediaPath;
            return;
        }
        p->setLoopState(OF_LOOP_NORMAL);
        p->play();
        m_players.push_back(std::move(p));
    }

    // Xoá lịch sử: số liệu của cấu hình cũ không còn ý nghĩa.
    m_frameTimes.clear();
    m_warmupTimer = 0.0f;
}

void ofApp::update() {
    const uint64_t nowNs = ofGetElapsedTimeMicros() * 1000ull;
    const double dtMs = static_cast<double>(nowNs - m_lastFrameNs) / 1.0e6;
    m_lastFrameNs = nowNs;

    // Bỏ qua ~1 giây đầu sau khi đổi cấu hình: lúc đó decoder còn đang
    // nạp buffer, số đo sẽ bôi nhọ kết quả một cách không công bằng.
    m_warmupTimer += ofGetLastFrameTime();
    if (m_warmupTimer > 1.0f && dtMs > 0.0 && dtMs < 1000.0) {
        m_frameTimes.push_back(dtMs);
        if (m_frameTimes.size() > kHistory) m_frameTimes.pop_front();
    }

    for (auto& p : m_players) {
        p->update();
        if (p->isFrameNew()) ++m_newFrameCount;
    }

    if (m_benchMode) benchTick();
}

ofApp::Stats ofApp::frameStats() const {
    Stats s;
    if (m_frameTimes.empty()) return s;

    std::vector<double> sorted(m_frameTimes.begin(), m_frameTimes.end());
    std::sort(sorted.begin(), sorted.end());

    double sum = 0.0;
    for (double v : sorted) sum += v;

    s.avg   = sum / static_cast<double>(sorted.size());
    s.p99   = sorted[static_cast<size_t>(sorted.size() * 0.99)];
    s.worst = sorted.back();
    return s;
}

void ofApp::draw() {
    if (m_drawVideo && !m_players.empty()) {
        // Xếp lưới để thấy được tất cả luồng cùng lúc.
        const int n    = static_cast<int>(m_players.size());
        const int cols = (n <= 1) ? 1 : (n <= 4 ? 2 : 3);
        const int rows = (n + cols - 1) / cols;
        const float w  = ofGetWidth()  / static_cast<float>(cols);
        const float h  = (ofGetHeight() - 150.0f) / static_cast<float>(rows);

        for (int i = 0; i < n; ++i) {
            const float x = (i % cols) * w;
            const float y = 150.0f + (i / cols) * h;
            m_players[static_cast<size_t>(i)]->draw(x, y, w, h);
        }
    }
    drawHud();
}

void ofApp::drawHud() {
    const Stats s = frameStats();
    const int n = static_cast<int>(m_players.size());

    ofSetColor(0, 0, 0, 210);
    ofDrawRectangle(0, 0, ofGetWidth(), 145);

    if (m_loadFailed) {
        ofSetColor(255, 90, 90);
        ofDrawBitmapString("LOI: khong load duoc " + m_mediaPath, 20, 30);
        ofDrawBitmapString("Chay ffmpeg de sinh file mau truoc.", 20, 50);
        return;
    }

    // 60fps = 16.67ms. Tô màu theo mức đạt/không đạt để đọc được ngay.
    const bool ok60  = (s.p99 > 0.0 && s.p99 <= 16.7);
    const bool ok30  = (s.p99 > 0.0 && s.p99 <= 33.4);
    if      (ok60) ofSetColor(120, 255, 140);
    else if (ok30) ofSetColor(255, 220, 120);
    else           ofSetColor(255, 110, 110);

    ofDrawBitmapString("SPIKE R1 — 4K HAP playback", 20, 26);

    ofSetColor(235);
    ofDrawBitmapString("Media : " + m_mediaLabel, 20, 46);
    ofDrawBitmapString("Luong dang phat : " + ofToString(n)
                       + "   (= so LAYER, khong phai so clip trong luoi)", 20, 64);

    ofDrawBitmapString("FPS   : " + ofToString(ofGetFrameRate(), 1), 20, 88);
    ofDrawBitmapString("Frame : avg " + ofToString(s.avg, 2)
                       + " ms   p99 " + ofToString(s.p99, 2)
                       + " ms   worst " + ofToString(s.worst, 2) + " ms", 20, 106);

    if      (ok60) { ofSetColor(120, 255, 140); ofDrawBitmapString(">> DAT 60 FPS", 480, 88); }
    else if (ok30) { ofSetColor(255, 220, 120); ofDrawBitmapString(">> chi dat 30 FPS", 480, 88); }
    else if (s.p99 > 0.0) { ofSetColor(255, 110, 110); ofDrawBitmapString(">> KHONG DAT", 480, 88); }

    ofSetColor(150);
    ofDrawBitmapString("[1-8] so luong  |  [v] vsync: "
                       + std::string(m_vsync ? "ON" : "OFF")
                       + "  |  [d] ve video: "
                       + std::string(m_drawVideo ? "ON" : "OFF")
                       + "  |  [r] reset do", 20, 130);
}

void ofApp::keyPressed(int key) {
    if (key >= '1' && key <= '8') {
        setPlayerCount(key - '0');
        return;
    }

    switch (key) {
    case 'v':
        m_vsync = !m_vsync;
        ofSetVerticalSync(m_vsync);
        m_frameTimes.clear();
        m_warmupTimer = 0.0f;
        break;

    case 'd':
        // Tắt vẽ để tách bạch: chi phí nằm ở GIẢI MÃ hay ở VẼ?
        // Nếu tắt vẽ mà vẫn chậm → nút thắt ở decode/upload.
        m_drawVideo = !m_drawVideo;
        m_frameTimes.clear();
        m_warmupTimer = 0.0f;
        break;

    case 'r':
        m_frameTimes.clear();
        m_warmupTimer = 0.0f;
        break;

    default:
        break;
    }
}

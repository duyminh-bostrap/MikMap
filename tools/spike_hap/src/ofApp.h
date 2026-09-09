#pragma once

#include "ofMain.h"
#include "ofxHapPlayer.h"

#include <deque>
#include <memory>
#include <vector>

class ofApp : public ofBaseApp {
public:
    void setup() override;
    void update() override;
    void draw() override;
    void keyPressed(int key) override;

private:
    void setPlayerCount(int n);
    void drawHud();

    struct Stats {
        double avg = 0.0;
        double p99 = 0.0;
        double worst = 0.0;
    };
    Stats frameStats() const;

    std::vector<std::unique_ptr<ofxHapPlayer>> m_players;
    std::string m_mediaPath;
    std::string m_mediaLabel;
    bool        m_loadFailed = false;
    bool        m_vsync = false;
    bool        m_drawVideo = true;

    /// Lịch sử frame time (ms) để tính p99 — trung bình che giấu hiện
    /// tượng khựng, mà khựng mới là thứ khán giả nhìn thấy.
    std::deque<double> m_frameTimes;
    static constexpr size_t kHistory = 600;   // ~10 giây @60fps

    uint64_t m_lastFrameNs = 0;
    float    m_warmupTimer = 0.0f;

    // ── Chế độ benchmark tự động ───────────────────────────────────────
    // Quét 1..kBenchMax luồng, mỗi mức đo một khoảng cố định, ghi kết quả
    // ra file rồi thoát. Cần thiết vì đây là app GUI — không có chế độ này
    // thì không lấy được số liệu ra ngoài để đưa vào báo cáo.
public:
    void enableBenchmark() { m_benchMode = true; }

private:
    void benchTick();
    void writeBenchReport();

    struct BenchRow {
        int    players = 0;
        double fps = 0.0;
        double avgMs = 0.0;
        double p99Ms = 0.0;
        double worstMs = 0.0;
        double videoFps = 0.0;   ///< khung hinh VIDEO moi/giay tren 1 luong
    };

    /// Dem khung hinh video moi thuc su duoc giai ma.
    /// Bat buoc phai co: FPS cua render loop KHONG chung minh duoc decoder
    /// theo kip. Neu decode thread doi, render loop van chay nhanh nhung
    /// ve lai cung mot texture -> video giat ma so do van dep.
    int m_newFrameCount = 0;

    bool   m_benchMode = false;
    int    m_benchCurrent = 0;
    float  m_benchPhaseTimer = 0.0f;
    bool   m_benchMeasuring = false;
    std::vector<BenchRow> m_benchResults;

    static constexpr int   kBenchMax     = 6;
    static constexpr float kBenchWarmup  = 1.5f;   // giây
    static constexpr float kBenchMeasure = 3.0f;   // giây
};

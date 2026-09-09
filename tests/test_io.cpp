#include "TestHarness.h"

#include "io/SensorFrame.h"
#include "io/SpscRingBuffer.h"
#include "io/TripleBuffer.h"
#include "io/sources/MockSource.h"

#include <atomic>
#include <chrono>
#include <thread>

using namespace hexmap;

// ═══════════════════════════════════════════════════════════════════════
//  SensorFrame — rang buoc POD
// ═══════════════════════════════════════════════════════════════════════

// Rang buoc kien truc: neu ai do them std::string hay std::vector vao
// SensorFrame, BUILD SE HONG — khong phai test do.
// Dung static_assert vi day la dieu kiem tra duoc luc bien dich; nhet vao
// REQUIRE() chi lam bien dich sinh canh bao "dieu kien la hang so".
static_assert(std::is_trivially_copyable_v<TouchPoint>,
              "TouchPoint phai POD — TripleBuffer phu thuoc dieu nay");
static_assert(std::is_trivially_copyable_v<SensorFrame>,
              "SensorFrame phai POD — TripleBuffer phu thuoc dieu nay");
static_assert(std::is_trivially_copyable_v<TouchEvent>,
              "TouchEvent phai POD — SpscRingBuffer phu thuoc dieu nay");

static_assert(sizeof(TouchPoint) == 24u, "TouchPoint phai giu kich thuoc co dinh");
static_assert(sizeof(SensorFrame) >= 24u * kMaxTouchPoints,
              "SensorFrame phai chua du kMaxTouchPoints diem");

TEST_CASE("SensorFrame: addPoint khong tran khi vuot gioi han") {
    SensorFrame f;
    TouchPoint p;
    for (int i = 0; i < kMaxTouchPoints; ++i) {
        CHECK(f.addPoint(p));
    }
    REQUIRE(f.count == kMaxTouchPoints);
    REQUIRE(!f.addPoint(p));            // lang le tu choi
    REQUIRE(f.count == kMaxTouchPoints); // khong tang qua gioi han
}

// ═══════════════════════════════════════════════════════════════════════
//  TripleBuffer — mot luong
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("TripleBuffer: chua co gi thi consume tra ve false") {
    TripleBuffer<int> tb;
    REQUIRE(!tb.hasNew());
    REQUIRE(!tb.consume());
}

TEST_CASE("TripleBuffer: ghi roi doc lai duoc") {
    TripleBuffer<int> tb;
    tb.write(42);
    REQUIRE(tb.hasNew());
    REQUIRE(tb.consume());
    REQUIRE(tb.readSlot() == 42);
}

TEST_CASE("★ TripleBuffer: ghi nhieu lan giua 2 lan doc -> chi thay CAI MOI NHAT") {
    // Day la hanh vi DUNG cho du lieu trang thai: vi tri ngon tay 5ms
    // truoc khong con gia tri.
    TripleBuffer<int> tb;
    tb.write(1);
    tb.write(2);
    tb.write(3);

    REQUIRE(tb.consume());
    REQUIRE(tb.readSlot() == 3);
    REQUIRE(!tb.consume());          // khong con gi moi
}

TEST_CASE("TripleBuffer: doc lai khi chua co du lieu moi van giu gia tri cu") {
    TripleBuffer<int> tb;
    tb.write(7);
    REQUIRE(tb.consume());
    REQUIRE(tb.readSlot() == 7);

    REQUIRE(!tb.consume());
    REQUIRE(tb.readSlot() == 7);     // van hop le
}

TEST_CASE("TripleBuffer: dem so frame bi bo qua") {
    TripleBuffer<int> tb;
    tb.write(1);
    tb.write(2);
    tb.write(3);
    tb.consume();

    REQUIRE(tb.publishedCount() == 3u);
    REQUIRE(tb.consumedCount() == 1u);
    REQUIRE(tb.droppedCount() == 2u);
}

TEST_CASE("TripleBuffer: hoat dong voi SensorFrame") {
    TripleBuffer<SensorFrame> tb;

    SensorFrame f;
    f.seq = 99;
    f.count = 2;
    f.points[0].x = 1.5f;
    f.points[1].x = 2.5f;
    tb.write(f);

    REQUIRE(tb.consume());
    REQUIRE(tb.readSlot().seq == 99u);
    REQUIRE(tb.readSlot().count == 2);
    REQUIRE_NEAR(tb.readSlot().points[1].x, 2.5, 1e-6);
}

// ═══════════════════════════════════════════════════════════════════════
//  ★ TripleBuffer — DA LUONG THAT
// ═══════════════════════════════════════════════════════════════════════

namespace {
/// Frame co the tu kiem tra: moi diem mang cung mot gia tri suy ra tu seq.
/// Neu doc phai frame ghi do (torn read), cac diem se khong khop nhau.
struct CheckedFrame {
    uint64_t seq = 0;
    uint32_t values[32]{};

    void fill(uint64_t s) {
        seq = s;
        for (uint32_t& v : values) v = static_cast<uint32_t>(s & 0xFFFFFFFFu);
    }

    bool isConsistent() const {
        const auto expect = static_cast<uint32_t>(seq & 0xFFFFFFFFu);
        for (uint32_t v : values) {
            if (v != expect) return false;
        }
        return true;
    }
};
} // namespace

TEST_CASE("★★ TripleBuffer: KHONG BAO GIO doc phai frame ghi do (2 thread that)") {
    // Day la test quan trong nhat cua tang io. Neu triple buffer sai,
    // loi se hiem, khong tai lap duoc, va chi lo ra giua show.
    TripleBuffer<CheckedFrame> tb;
    std::atomic<bool> stop{false};
    std::atomic<uint64_t> written{0};

    std::thread producer([&] {
        uint64_t s = 0;
        while (!stop.load(std::memory_order_relaxed)) {
            tb.writeSlot().fill(++s);
            tb.publish();
            written.store(s, std::memory_order_relaxed);
        }
    });

    int torn = 0;
    int reads = 0;
    uint64_t lastSeq = 0;
    int outOfOrder = 0;

    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(400);
    while (std::chrono::steady_clock::now() < deadline) {
        if (!tb.consume()) continue;
        const CheckedFrame& f = tb.readSlot();
        ++reads;
        if (!f.isConsistent()) ++torn;
        if (f.seq < lastSeq) ++outOfOrder;   // duoc phep nhay coc, khong duoc lui
        lastSeq = f.seq;
    }

    stop.store(true);
    producer.join();

    REQUIRE(reads > 100);        // co that su chay
    REQUIRE(torn == 0);          // ★ khong mot frame nao bi xe
    REQUIRE(outOfOrder == 0);    // ★ khong bao gio lui ve qua khu
    REQUIRE(written.load() > 0u);
}

// ═══════════════════════════════════════════════════════════════════════
//  SpscRingBuffer
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("SpscRing: push roi pop dung thu tu") {
    SpscRingBuffer<int, 8> ring;
    REQUIRE(ring.empty());

    REQUIRE(ring.push(1));
    REQUIRE(ring.push(2));
    REQUIRE(ring.push(3));
    REQUIRE(!ring.empty());

    int v = 0;
    REQUIRE(ring.pop(v)); CHECK(v == 1);
    REQUIRE(ring.pop(v)); CHECK(v == 2);
    REQUIRE(ring.pop(v)); CHECK(v == 3);
    REQUIRE(!ring.pop(v));
}

TEST_CASE("★ SpscRing: day thi tu choi va DEM lai, khong ghi de am tham") {
    // Ghi de am tham se lam mat su kien Up -> hieu ung ket vinh vien.
    // Tha tu choi va bao cho PerfPanel biet.
    SpscRingBuffer<int, 4> ring;
    REQUIRE(ring.capacity() == 3u);

    REQUIRE(ring.push(1));
    REQUIRE(ring.push(2));
    REQUIRE(ring.push(3));
    REQUIRE(!ring.push(4));          // day

    REQUIRE(ring.overflowCount() == 1u);

    int v = 0;
    REQUIRE(ring.pop(v));
    CHECK(v == 1);                   // phan tu DAU con nguyen
}

TEST_CASE("SpscRing: quay vong dung") {
    SpscRingBuffer<int, 4> ring;
    int v = 0;
    for (int round = 0; round < 10; ++round) {
        REQUIRE(ring.push(round));
        REQUIRE(ring.pop(v));
        CHECK(v == round);
    }
    REQUIRE(ring.empty());
}

TEST_CASE("SpscRing: popBatch gioi han so luong") {
    SpscRingBuffer<int, 32> ring;
    for (int i = 0; i < 10; ++i) ring.push(i);

    int out[4]{};
    const size_t n = ring.popBatch(out, 4);
    REQUIRE(n == 4u);
    CHECK(out[0] == 0);
    CHECK(out[3] == 3);
    REQUIRE(ring.size() == 6u);
}

TEST_CASE("★★ SpscRing: KHONG MAT su kien nao qua 2 thread that") {
    SpscRingBuffer<uint64_t, 1024> ring;
    constexpr uint64_t kTotal = 200000;

    std::thread producer([&] {
        for (uint64_t i = 1; i <= kTotal; ++i) {
            // Day toi khi thanh cong — su kien khong duoc phep roi.
            while (!ring.push(i)) {
                std::this_thread::yield();
            }
        }
    });

    uint64_t expected = 1;
    uint64_t received = 0;
    int gaps = 0;

    while (received < kTotal) {
        uint64_t v = 0;
        if (!ring.pop(v)) { std::this_thread::yield(); continue; }
        if (v != expected) ++gaps;
        expected = v + 1;
        ++received;
    }

    producer.join();

    REQUIRE(received == kTotal);   // ★ nhan du toan bo
    REQUIRE(gaps == 0);            // ★ dung thu tu, khong sot
}

// ═══════════════════════════════════════════════════════════════════════
//  MockSource (G2)
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("MockSource: sinh frame dong bo, tat dinh") {
    MockConfig cfg;
    cfg.pointCount = 5;
    cfg.pattern = MockPattern::Circle;
    MockSource src(cfg);

    const SensorFrame f = src.generateFrame(0.0);
    REQUIRE(f.count == 5);
    REQUIRE(f.seq == 1u);
    REQUIRE(f.tCaptureNs > 0);

    for (int i = 0; i < f.count; ++i) {
        CHECK(f.points[i].id == static_cast<uint32_t>(i + 1));
    }
}

TEST_CASE("MockSource: diem nam trong pham vi da cau hinh") {
    MockConfig cfg;
    cfg.pointCount = 8;
    cfg.minX = 100.0; cfg.maxX = 900.0;
    cfg.minY = 200.0; cfg.maxY = 800.0;
    cfg.pattern = MockPattern::Circle;
    MockSource src(cfg);

    for (double t = 0.0; t < 6.0; t += 0.37) {
        const SensorFrame f = src.generateFrame(t);
        for (int i = 0; i < f.count; ++i) {
            CHECK(f.points[i].x >= cfg.minX - 1.0);
            CHECK(f.points[i].x <= cfg.maxX + 1.0);
            CHECK(f.points[i].y >= cfg.minY - 1.0);
            CHECK(f.points[i].y <= cfg.maxY + 1.0);
        }
    }
}

TEST_CASE("MockSource: cung seed cho cung ket qua (tai lap duoc)") {
    MockConfig cfg;
    cfg.pointCount = 4;
    cfg.pattern = MockPattern::Random;
    cfg.seed = 4242;

    MockSource a(cfg), b(cfg);
    const SensorFrame fa = a.generateFrame(1.0);
    const SensorFrame fb = b.generateFrame(1.0);

    REQUIRE(fa.count == fb.count);
    for (int i = 0; i < fa.count; ++i) {
        CHECK_NEAR(fa.points[i].x, fb.points[i].x, 1e-6);
        CHECK_NEAR(fa.points[i].y, fb.points[i].y, 1e-6);
    }
}

TEST_CASE("MockSource: gioi han so diem o kMaxTouchPoints") {
    MockConfig cfg;
    cfg.pointCount = 999;
    MockSource src(cfg);
    REQUIRE(src.generateFrame(0.0).count == kMaxTouchPoints);
}

TEST_CASE("★ MockSource: chay thread that va day duoc du lieu qua buffer") {
    MockConfig cfg;
    cfg.pointCount = 3;
    cfg.rateHz = 500.0;
    MockSource src(cfg);

    REQUIRE(src.start());
    REQUIRE(src.status() == SourceStatus::Running);

    std::this_thread::sleep_for(std::chrono::milliseconds(120));

    int gotFrames = 0;
    for (int i = 0; i < 50; ++i) {
        if (src.frames().consume()) ++gotFrames;
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }

    src.stop();
    REQUIRE(src.status() == SourceStatus::Stopped);

    REQUIRE(gotFrames > 0);
    REQUIRE(src.frames().publishedCount() > 0u);
}

TEST_CASE("★ MockSource: khi dung PHAI phat su kien Up cho moi diem") {
    // Thieu mot Up = hieu ung ket vinh vien tren man hinh.
    MockConfig cfg;
    cfg.pointCount = 4;
    cfg.rateHz = 300.0;
    MockSource src(cfg);

    src.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    src.stop();

    int downs = 0, ups = 0;
    TouchEvent ev;
    while (src.events().pop(ev)) {
        if (ev.state == TouchState::Down) ++downs;
        if (ev.state == TouchState::Up)   ++ups;
    }

    REQUIRE(downs == 4);
    REQUIRE(ups == 4);   // ★ moi diem di xuong deu phai co luot di len
}

TEST_CASE("MockSource: stop hai lan khong gay loi") {
    MockSource src;
    src.start();
    src.stop();
    src.stop();
    REQUIRE(src.status() == SourceStatus::Stopped);
}

TEST_CASE("MockSource: dropoutRate = 1.0 thi khong phat gi") {
    MockConfig cfg;
    cfg.dropoutRate = 1.0;
    cfg.rateHz = 500.0;
    MockSource src(cfg);

    src.start();
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    src.stop();

    REQUIRE(src.frames().publishedCount() == 0u);
}

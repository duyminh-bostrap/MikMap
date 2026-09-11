#include "TestHarness.h"

#include "core/model/Composition.h"
#include "core/model/Transform2D.h"
#include "core/model/Transport.h"

using namespace hexmap;

// ═══════════════════════════════════════════════════════════════════════
//  C1 C2 — play / pause / stop / loop
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("C1: trang thai mac dinh la Stopped") {
    Transport t;
    REQUIRE(t.state == PlayState::Stopped);
    REQUIRE(!t.isPlaying());
}

TEST_CASE("C1: dang dung thi advance khong lam gi") {
    Transport t;
    t.durationSec = 10.0;
    t.advance(1.0);
    REQUIRE_NEAR(t.position, 0.0, 1e-12);
}

TEST_CASE("C1: phat thi dau phat tien theo thoi gian") {
    Transport t;
    t.durationSec = 10.0;
    t.play();
    t.advance(2.5);
    REQUIRE_NEAR(t.position, 0.25, 1e-12);   // 2.5 / 10
}

TEST_CASE("C1: pause dung dau phat lai") {
    Transport t;
    t.durationSec = 10.0;
    t.play();
    t.advance(3.0);
    t.pause();
    t.advance(5.0);
    REQUIRE_NEAR(t.position, 0.3, 1e-12);
}

TEST_CASE("C1: stop dua dau phat ve dau") {
    Transport t;
    t.durationSec = 10.0;
    t.play();
    t.advance(5.0);
    t.stop();
    REQUIRE_NEAR(t.position, 0.0, 1e-12);
    REQUIRE(!t.isPlaying());
}

TEST_CASE("★ C1: clip chay NGUOC thi stop dua ve outPoint") {
    // Neu dua ve inPoint, bam stop roi play lai se khong phat gi —
    // dau phat da o cuoi doan roi.
    Transport t;
    t.durationSec = 10.0;
    t.direction = PlayDirection::Reverse;
    t.setTrim(0.2, 0.8);
    t.play();
    t.advance(1.0);
    t.stop();
    REQUIRE_NEAR(t.position, 0.8, 1e-12);
}

TEST_CASE("C2: loop quay ve dau khi cham cuoi") {
    Transport t;
    t.durationSec = 1.0;
    t.endAction = EndAction::Loop;
    t.play();

    const TransportEvent ev = t.advance(1.25);
    REQUIRE(ev == TransportEvent::ReachedEnd);
    // Giu phan du: 1.25s tren clip 1s -> vi tri 0.25, khong phai 0.
    REQUIRE_NEAR(t.position, 0.25, 1e-9);
}

TEST_CASE("★ C2: loop giu phan du nen khong troi nhip khi he thong khung") {
    // Neu kep cung ve inPoint moi lan cham bien, moi lan khung se lam
    // mat mot doan -> nhip bi troi tich luy, thay ro khi dong bo nhac.
    Transport t;
    t.durationSec = 1.0;
    t.play();
    t.advance(3.4);          // vuot 3 vong tron
    REQUIRE_NEAR(t.position, 0.4, 1e-9);
}

// ═══════════════════════════════════════════════════════════════════════
//  C4 — chieu phat
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("C4: chay nguoc") {
    Transport t;
    t.durationSec = 10.0;
    t.direction = PlayDirection::Reverse;
    t.position = 0.5;
    t.play();
    t.advance(2.0);
    REQUIRE_NEAR(t.position, 0.3, 1e-12);
}

TEST_CASE("★ C4: ping-pong DOI CHIEU o bien, khong khung nhip") {
    Transport t;
    t.durationSec = 1.0;
    t.direction = PlayDirection::PingPong;
    t.position = 0.9;
    t.play();

    // Tien 0.2 -> cham 1.0 roi doi lai 0.1 -> ket qua 0.9
    t.advance(0.2);
    REQUIRE_NEAR(t.position, 0.9, 1e-9);
    REQUIRE(t.pingPongSign == -1);

    // Gio dang chay nguoc.
    t.advance(0.2);
    REQUIRE_NEAR(t.position, 0.7, 1e-9);
}

TEST_CASE("C4: ping-pong doi chieu o bien duoi") {
    Transport t;
    t.durationSec = 1.0;
    t.direction = PlayDirection::PingPong;
    t.pingPongSign = -1;
    t.position = 0.1;
    t.play();

    t.advance(0.2);
    REQUIRE_NEAR(t.position, 0.1, 1e-9);
    REQUIRE(t.pingPongSign == 1);
}

// ═══════════════════════════════════════════════════════════════════════
//  C5 C6 — toc do va cat dau/cuoi
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("C5: toc do x2 thi tien nhanh gap doi") {
    Transport t;
    t.durationSec = 10.0;
    t.speed = 2.0;
    t.play();
    t.advance(1.0);
    REQUIRE_NEAR(t.position, 0.2, 1e-12);
}

TEST_CASE("C5: toc do am chay nguoc") {
    Transport t;
    t.durationSec = 10.0;
    t.speed = -1.0;
    t.position = 0.5;
    t.play();
    t.advance(2.0);
    REQUIRE_NEAR(t.position, 0.3, 1e-12);
}

TEST_CASE("C6: loop ton trong in/out point") {
    Transport t;
    t.durationSec = 10.0;
    t.setTrim(0.25, 0.75);
    t.position = 0.7;
    t.play();

    t.advance(1.0);          // +0.1 -> 0.8, vuot outPoint
    REQUIRE(t.position >= 0.25);
    REQUIRE(t.position <= 0.75);
}

TEST_CASE("★ C6: setTrim tu hoan doi khi vao nguoc") {
    Transport t;
    t.setTrim(0.8, 0.2);
    REQUIRE_NEAR(t.inPoint, 0.2, 1e-12);
    REQUIRE_NEAR(t.outPoint, 0.8, 1e-12);
}

TEST_CASE("★ C6: in va out trung nhau khong gay chia cho 0") {
    // Nguoi dung keo hai handle chong len nhau la chuyen thuong.
    Transport t;
    t.durationSec = 10.0;
    t.setTrim(0.5, 0.5);
    REQUIRE(t.outPoint > t.inPoint);
    REQUIRE(t.trimmedLength() > 0.0);

    t.play();
    t.advance(1.0);
    REQUIRE(std::isfinite(t.position));
}

TEST_CASE("C6: setTrim kep vao [0,1]") {
    Transport t;
    t.setTrim(-5.0, 99.0);
    REQUIRE_NEAR(t.inPoint, 0.0, 1e-12);
    REQUIRE_NEAR(t.outPoint, 1.0, 1e-12);
}

TEST_CASE("seekNormalized kep vao doan cat") {
    Transport t;
    t.setTrim(0.3, 0.6);
    t.seekNormalized(0.9);
    REQUIRE_NEAR(t.position, 0.6, 1e-12);
    t.seekNormalized(0.0);
    REQUIRE_NEAR(t.position, 0.3, 1e-12);
}

// ═══════════════════════════════════════════════════════════════════════
//  C7 — autopilot
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("C7: EndAction::Stop thi dung lai o cuoi") {
    Transport t;
    t.durationSec = 1.0;
    t.endAction = EndAction::Stop;
    t.play();

    const TransportEvent ev = t.advance(1.5);
    REQUIRE(ev == TransportEvent::ReachedEnd);
    REQUIRE(!t.isPlaying());
    REQUIRE_NEAR(t.position, 1.0, 1e-12);
}

TEST_CASE("C7: EndAction::PlayNext bao ra ngoai") {
    Transport t;
    t.durationSec = 1.0;
    t.endAction = EndAction::PlayNext;
    t.play();
    REQUIRE(t.advance(1.5) == TransportEvent::RequestNext);
}

TEST_CASE("★ C7: Composition tu chuyen sang clip ke tiep") {
    Composition comp(1, 3);
    for (int c = 0; c < 3; ++c) {
        Clip clip;
        clip.name = "c" + std::to_string(c);
        clip.media.type = MediaType::Video;
        clip.media.durationSec = 1.0;
        clip.transport.endAction = EndAction::PlayNext;
        comp.deck(0).setClip(0, c, clip);
    }

    comp.triggerClip(0, 0);
    REQUIRE(comp.playingClip(0)->name == "c0");

    comp.update(1.5);        // clip 0 het
    REQUIRE(comp.playingClip(0)->name == "c1");

    comp.update(1.5);
    REQUIRE(comp.playingClip(0)->name == "c2");

    comp.update(1.5);        // quay vong
    REQUIRE(comp.playingClip(0)->name == "c0");
}

TEST_CASE("★ C7: PlayNext BO QUA o trong thay vi dung lai") {
    Composition comp(1, 4);
    for (int c : {0, 3}) {   // chi dien cot 0 va 3
        Clip clip;
        clip.name = "c" + std::to_string(c);
        clip.media.type = MediaType::Video;
        clip.media.durationSec = 1.0;
        clip.transport.endAction = EndAction::PlayNext;
        comp.deck(0).setClip(0, c, clip);
    }

    comp.triggerClip(0, 0);
    comp.update(1.5);
    REQUIRE(comp.playingClip(0)->name == "c3");   // nhay qua 1 va 2
}

TEST_CASE("C7: EndAction::Random tai lap duoc voi cung seed") {
    auto build = [](uint32_t seed) {
        Composition comp(1, 5);
        for (int c = 0; c < 5; ++c) {
            Clip clip;
            clip.name = "c" + std::to_string(c);
            clip.media.type = MediaType::Video;
            clip.media.durationSec = 1.0;
            clip.transport.endAction = EndAction::Random;
            comp.deck(0).setClip(0, c, clip);
        }
        comp.setRandomSeed(seed);
        comp.triggerClip(0, 0);
        for (int i = 0; i < 5; ++i) comp.update(1.5);
        return comp.playingClip(0)->name;
    };

    REQUIRE(build(777) == build(777));
}

// ═══════════════════════════════════════════════════════════════════════
//  Anh tinh / generator — khong co do dai
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ Anh tinh (duration = 0) khong lam treo hay chia cho 0") {
    Transport t;
    t.durationSec = 0.0;
    t.play();
    REQUIRE(t.advance(1.0) == TransportEvent::None);
    REQUIRE_NEAR(t.position, 0.0, 1e-12);
    REQUIRE(t.isPlaying());        // van coi la dang phat de duoc ve
}

// ═══════════════════════════════════════════════════════════════════════
//  D1 D2 D3 D5 D6 D8 — Transform2D
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("D1: position dich chuyen noi dung") {
    Transform2D tr;
    tr.position = Vec2{100.0, 50.0};
    const Mat3 m = tr.toMatrix(Vec2{200.0, 100.0});
    const Vec2 p = m.transformPoint(Vec2{0.0, 0.0});
    REQUIRE_VEC_NEAR(p, 100.0, 50.0, 1e-9);
}

TEST_CASE("D2 D8: scale quanh anchor giu anchor dung cho") {
    Transform2D tr;
    tr.scale = Vec2{2.0, 2.0};
    const Vec2 size{200.0, 100.0};
    const Mat3 m = tr.toMatrix(size);

    // anchor mac dinh (0.5,0.5) -> tam (100,50) khong duoc dich chuyen
    const Vec2 center = m.transformPoint(Vec2{100.0, 50.0});
    REQUIRE_VEC_NEAR(center, 100.0, 50.0, 1e-9);

    // goc thi bi day ra xa gap doi
    const Vec2 corner = m.transformPoint(Vec2{0.0, 0.0});
    REQUIRE_VEC_NEAR(corner, -100.0, -50.0, 1e-9);
}

TEST_CASE("D5: xoay quanh anchor") {
    const double pi = 3.14159265358979323846;
    Transform2D tr;
    tr.rotation = pi / 2.0;
    const Vec2 size{200.0, 200.0};
    const Mat3 m = tr.toMatrix(size);

    const Vec2 center = m.transformPoint(Vec2{100.0, 100.0});
    REQUIRE_VEC_NEAR(center, 100.0, 100.0, 1e-9);
}

TEST_CASE("D6: lat ngang") {
    Transform2D tr;
    tr.flipH = true;
    const Vec2 size{200.0, 100.0};
    const Mat3 m = tr.toMatrix(size);

    // Anchor o tam -> x=0 phai thanh x=200
    const Vec2 p = m.transformPoint(Vec2{0.0, 50.0});
    REQUIRE_VEC_NEAR(p, 200.0, 50.0, 1e-9);
}

TEST_CASE("★ Transform2D: nghich dao khu duoc bien doi") {
    Transform2D tr;
    tr.position = Vec2{300.0, 120.0};
    tr.scale    = Vec2{1.7, 0.8};
    tr.rotation = 0.6;

    const Vec2 size{640.0, 480.0};
    Mat3 inv;
    REQUIRE(tr.toInverseMatrix(size, inv));

    const Mat3 fwd = tr.toMatrix(size);
    const Vec2 p{123.0, 456.0};
    const Vec2 back = inv.transformPoint(fwd.transformPoint(p));
    REQUIRE_VEC_NEAR(back, p.x, p.y, 1e-8);
}

TEST_CASE("★ Transform2D: scale = 0 bao loi thay vi tra ve ma tran rac") {
    // Nguoi dung keo slider scale ve 0 la chuyen thuong.
    Transform2D tr;
    tr.scale = Vec2{0.0, 1.0};
    Mat3 inv;
    REQUIRE(!tr.toInverseMatrix(Vec2{100.0, 100.0}, inv));
}

TEST_CASE("BlendMode: ten va phan giai nguoc khop nhau") {
    for (int i = 0; i < static_cast<int>(BlendMode::Count); ++i) {
        const auto m = static_cast<BlendMode>(i);
        CHECK(blendModeFromName(blendModeName(m)) == m);
    }
}

TEST_CASE("BlendMode: ten la thi ve Normal, khong lam hong project") {
    REQUIRE(blendModeFromName("KhongTonTai") == BlendMode::Normal);
    REQUIRE(blendModeFromName(nullptr) == BlendMode::Normal);
}

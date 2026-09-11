#include "TestHarness.h"

#include "core/model/Composition.h"

using namespace hexmap;

namespace {

/// Tao clip video gia lap co do dai xac dinh.
Clip makeVideoClip(const std::string& name, double durationSec = 5.0) {
    Clip c;
    c.name = name;
    c.media.type = MediaType::Video;
    c.media.path = name + ".mov";
    c.media.size = Vec2{3840.0, 2160.0};
    c.media.durationSec = durationSec;
    c.transport.durationSec = durationSec;
    return c;
}

/// Composition 3 layer x 4 cot, deck 0 dien day clip.
Composition makeFilled(int layers = 3, int cols = 4, int decks = 1) {
    Composition comp(layers, cols, decks);
    for (int d = 0; d < decks; ++d) {
        for (int L = 0; L < layers; ++L) {
            for (int c = 0; c < cols; ++c) {
                comp.deck(d).setClip(L, c, makeVideoClip(
                    "d" + std::to_string(d) + "L" + std::to_string(L) + "c" + std::to_string(c)));
            }
        }
    }
    return comp;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  A1 — canvas & cau truc
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("A1: canvas mac dinh va doi duoc") {
    Composition comp;
    REQUIRE_VEC_NEAR(comp.canvasSize, 1920.0, 1080.0, 1e-9);

    comp.canvasSize = Vec2{3840.0, 2160.0};
    REQUIRE_VEC_NEAR(comp.canvasSize, 3840.0, 2160.0, 1e-9);
}

TEST_CASE("A2 A3: so layer va so cot") {
    Composition comp(4, 6);
    REQUIRE(comp.layerCount() == 4);
    REQUIRE(comp.columnCount() == 6);
    REQUIRE(comp.deckCount() == 1);
}

TEST_CASE("A2: doi so layer dong bo xuong moi deck") {
    Composition comp(2, 4, 3);
    comp.setLayerCount(5);

    REQUIRE(comp.layerCount() == 5);
    for (int d = 0; d < comp.deckCount(); ++d) {
        CHECK(comp.deck(d).layerCount() == 5);
    }
}

TEST_CASE("A3: them cot GIU NGUYEN clip da dat") {
    Composition comp = makeFilled(2, 3);
    comp.setColumnCount(6);

    REQUIRE(comp.columnCount() == 6);
    REQUIRE(comp.deck(0).clip(0, 0).name == "d0L0c0");
    REQUIRE(comp.deck(0).clip(1, 2).name == "d0L1c2");
    REQUIRE(comp.deck(0).clip(0, 5).isEmpty());   // cot moi thi rong
}

// ═══════════════════════════════════════════════════════════════════════
//  A5 — bam vao clip
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("A5: bam clip thi CHI layer do doi") {
    Composition comp = makeFilled();

    REQUIRE(comp.triggerClip(1, 2));

    // Layer 1 dang phat cot 2.
    REQUIRE(comp.layer(1).activeColumn == 2);
    REQUIRE(comp.layer(1).activeDeck == 0);

    // Cac layer khac KHONG bi dong toi.
    REQUIRE(!comp.layer(0).isPlayingSomething());
    REQUIRE(!comp.layer(2).isPlayingSomething());
}

TEST_CASE("A5: clip duoc dua ve dau va bat dau phat") {
    Composition comp = makeFilled();
    REQUIRE(comp.triggerClip(0, 1));

    const Clip* c = comp.playingClip(0);
    REQUIRE(c != nullptr);
    REQUIRE(c->name == "d0L0c1");
    REQUIRE(c->transport.isPlaying());
    REQUIRE_NEAR(c->transport.position, 0.0, 1e-12);
}

TEST_CASE("A5: bam o RONG khong lam gi ca") {
    Composition comp(2, 3);   // khong dien clip
    REQUIRE(!comp.triggerClip(0, 0));
    REQUIRE(!comp.layer(0).isPlayingSomething());
}

TEST_CASE("A5: chi so ngoai pham vi khong lam sap") {
    Composition comp = makeFilled();
    REQUIRE(!comp.triggerClip(-1, 0));
    REQUIRE(!comp.triggerClip(0, 99));
    REQUIRE(!comp.triggerClip(99, 0));
}

TEST_CASE("A5: doi clip trong cung layer thi thay the") {
    Composition comp = makeFilled();
    REQUIRE(comp.triggerClip(0, 1));
    REQUIRE(comp.triggerClip(0, 3));

    REQUIRE(comp.layer(0).activeColumn == 3);
    REQUIRE(comp.playingClip(0)->name == "d0L0c3");
}

// ═══════════════════════════════════════════════════════════════════════
//  A6 — bam vao cot
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ A6: bam cot kich hoat MOI layer cung luc") {
    Composition comp = makeFilled(3, 4);

    const int changed = comp.triggerColumn(2);
    REQUIRE(changed == 3);

    for (int L = 0; L < 3; ++L) {
        CHECK(comp.layer(L).activeColumn == 2);
        CHECK(comp.playingClip(L) != nullptr);
    }
}

TEST_CASE("★ A6: o TRONG thi layer do GIU NGUYEN clip dang phat") {
    // Hanh vi mac dinh (giong Resolume): cho phep dung cot chi doi vai
    // layer, giu nguyen layer nen.
    Composition comp = makeFilled(3, 4);

    comp.triggerColumn(0);              // moi layer phat cot 0
    comp.deck(0).clearClip(1, 2);       // duc lo o [layer1][cot2]

    comp.triggerColumn(2);

    CHECK(comp.layer(0).activeColumn == 2);   // doi
    CHECK(comp.layer(1).activeColumn == 0);   // GIU NGUYEN
    CHECK(comp.layer(2).activeColumn == 2);   // doi
}

TEST_CASE("★ A6: che do ClearLayer thi o trong lam DUNG layer") {
    Composition comp = makeFilled(3, 4);
    comp.emptyCellBehavior = EmptyCellBehavior::ClearLayer;

    comp.triggerColumn(0);
    comp.deck(0).clearClip(1, 2);
    comp.triggerColumn(2);

    CHECK(comp.layer(0).activeColumn == 2);
    CHECK(!comp.layer(1).isPlayingSomething());   // bi dung
    CHECK(comp.layer(2).activeColumn == 2);
}

// ═══════════════════════════════════════════════════════════════════════
//  ★ A9 — doi deck KHONG duoc ngat playback
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★★ A9: doi deck dang XEM khong ngat clip dang PHAT") {
    // Day la rang buoc dinh hinh ca cau truc du lieu. Neu Deck so huu
    // Layer thi test nay khong the pass.
    Composition comp = makeFilled(2, 3, /*decks*/ 2);

    comp.triggerClip(0, 1);
    REQUIRE(comp.playingClip(0)->name == "d0L0c1");
    REQUIRE(comp.layer(0).activeDeck == 0);

    // Nguoi dung chuyen sang xem deck 1.
    comp.setViewedDeck(1);
    REQUIRE(comp.viewedDeck() == 1);

    // Clip cua deck 0 VAN dang phat, khong he bi dong toi.
    REQUIRE(comp.layer(0).activeDeck == 0);
    REQUIRE(comp.playingClip(0) != nullptr);
    REQUIRE(comp.playingClip(0)->name == "d0L0c1");
    REQUIRE(comp.playingClip(0)->transport.isPlaying());
}

TEST_CASE("★ A9: sau khi doi deck, trigger moi lay clip cua deck MOI") {
    Composition comp = makeFilled(2, 3, 2);

    comp.triggerClip(0, 1);
    comp.setViewedDeck(1);
    comp.triggerClip(0, 2);

    REQUIRE(comp.layer(0).activeDeck == 1);
    REQUIRE(comp.playingClip(0)->name == "d1L0c2");
}

TEST_CASE("A9: hai layer co the phat clip tu HAI deck khac nhau") {
    Composition comp = makeFilled(2, 3, 2);

    comp.triggerClip(0, 0);      // layer 0 <- deck 0
    comp.setViewedDeck(1);
    comp.triggerClip(1, 1);      // layer 1 <- deck 1

    REQUIRE(comp.playingClip(0)->name == "d0L0c0");
    REQUIRE(comp.playingClip(1)->name == "d1L1c1");
}

// ═══════════════════════════════════════════════════════════════════════
//  A7 A8 — opacity, blend, solo, bypass
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("A7: opacity hieu dung = layer x clip x master") {
    Composition comp = makeFilled(1, 1);
    comp.triggerClip(0, 0);

    comp.layer(0).opacity = 0.5;
    comp.playingClip(0)->opacity = 0.4;
    comp.masterOpacity = 0.5;

    REQUIRE_NEAR(comp.effectiveOpacity(0), 0.5 * 0.4 * 0.5, 1e-12);
}

TEST_CASE("A8: bypass lam layer bien mat khoi danh sach ve") {
    Composition comp = makeFilled(3, 2);
    comp.triggerColumn(0);
    REQUIRE(comp.visibleLayers().size() == 3u);

    comp.layer(1).bypass = true;
    const auto vis = comp.visibleLayers();
    REQUIRE(vis.size() == 2u);
    CHECK(vis[0] == 0);
    CHECK(vis[1] == 2);
}

TEST_CASE("★ A8: solo an TAT CA layer khong solo") {
    Composition comp = makeFilled(3, 2);
    comp.triggerColumn(0);

    comp.layer(2).solo = true;
    REQUIRE(comp.anySolo());

    const auto vis = comp.visibleLayers();
    REQUIRE(vis.size() == 1u);
    REQUIRE(vis[0] == 2);
}

TEST_CASE("A8: nhieu layer solo thi hien tat ca chung") {
    Composition comp = makeFilled(4, 2);
    comp.triggerColumn(0);

    comp.layer(0).solo = true;
    comp.layer(3).solo = true;

    const auto vis = comp.visibleLayers();
    REQUIRE(vis.size() == 2u);
    CHECK(vis[0] == 0);
    CHECK(vis[1] == 3);
}

TEST_CASE("A8: opacity = 0 thi khong ve") {
    Composition comp = makeFilled(2, 2);
    comp.triggerColumn(0);
    comp.layer(0).opacity = 0.0;

    const auto vis = comp.visibleLayers();
    REQUIRE(vis.size() == 1u);
    REQUIRE(vis[0] == 1);
}

TEST_CASE("A8: clear lam layer ngung phat") {
    Composition comp = makeFilled(2, 2);
    comp.triggerColumn(0);
    comp.layer(0).clear();

    REQUIRE(!comp.layer(0).isPlayingSomething());
    REQUIRE(comp.playingClip(0) == nullptr);
    REQUIRE(comp.visibleLayers().size() == 1u);
}

TEST_CASE("visibleLayers tra ve dung thu tu z (duoi len tren)") {
    Composition comp = makeFilled(4, 2);
    comp.triggerColumn(0);

    const auto vis = comp.visibleLayers();
    REQUIRE(vis.size() == 4u);
    for (size_t i = 0; i < vis.size(); ++i) {
        CHECK(vis[i] == static_cast<int>(i));
    }
}

// ═══════════════════════════════════════════════════════════════════════
//  An toan khi doi kich thuoc luoi
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ Cat bot cot thi layer dang phat cot do phai DUNG") {
    // Neu khong xu ly, layer se tro vao o khong con ton tai.
    Composition comp = makeFilled(2, 6);
    comp.triggerClip(0, 5);
    REQUIRE(comp.layer(0).activeColumn == 5);

    comp.setColumnCount(3);

    REQUIRE(!comp.layer(0).isPlayingSomething());
    REQUIRE(comp.playingClip(0) == nullptr);
}

// ═══════════════════════════════════════════════════════════════════════
//  A10 — transition giua clip
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("A10: duration = 0 thi cat thang, khong transition") {
    Composition comp = makeFilled(1, 3);
    comp.layer(0).transitionDuration = 0.0;

    comp.triggerClip(0, 0);
    comp.triggerClip(0, 1);

    REQUIRE(!comp.layer(0).isTransitioning());
    REQUIRE(comp.transitioningClip(0) == nullptr);
    REQUIRE(comp.playingClip(0)->name == "d0L0c1");
}

TEST_CASE("★★ A10: dang transition thi CO HAI clip cung song") {
    // Day la ly do engine phai chiu duoc 2 luong video. Spike test R1 da
    // do: 2 luong 4K van giu 30fps goc.
    Composition comp = makeFilled(1, 3);
    comp.layer(0).transitionDuration = 1.0;

    comp.triggerClip(0, 0);
    comp.triggerClip(0, 1);

    REQUIRE(comp.layer(0).isTransitioning());

    const Clip* cur  = comp.playingClip(0);
    const Clip* prev = comp.transitioningClip(0);
    REQUIRE(cur  != nullptr);
    REQUIRE(prev != nullptr);          // ★ HAI clip cung luc
    CHECK(cur->name  == "d0L0c1");
    CHECK(prev->name == "d0L0c0");

    // Ca hai deu dang PHAT — clip cu dung hinh se lo ra la anh tinh mo dan.
    CHECK(cur->transport.isPlaying());
    CHECK(prev->transport.isPlaying());
}

TEST_CASE("★ A10: crossfade GIU TONG do mo, khong sang vot hay toi sup") {
    Composition comp = makeFilled(1, 3);
    comp.layer(0).transitionDuration = 1.0;

    comp.triggerClip(0, 0);
    comp.triggerClip(0, 1);

    for (double t : {0.0, 0.25, 0.5, 0.75}) {
        // Dat tien do truc tiep de kiem tra tung moc.
        comp.layer(0).transitionProgress = t;
        const auto info = comp.renderInfo(0);
        REQUIRE(info.current != nullptr);
        REQUIRE(info.previous != nullptr);
        CHECK_NEAR(info.currentOpacity + info.previousOpacity, 1.0, 1e-9);
    }
}

TEST_CASE("★ A10: transition ket thuc dung han thi nha clip cu") {
    Composition comp = makeFilled(1, 3);
    comp.layer(0).transitionDuration = 0.5;

    comp.triggerClip(0, 0);
    comp.triggerClip(0, 1);
    REQUIRE(comp.layer(0).isTransitioning());

    comp.update(0.3);
    REQUIRE(comp.layer(0).isTransitioning());     // chua xong

    comp.update(0.3);                             // tong 0.6 > 0.5
    REQUIRE(!comp.layer(0).isTransitioning());
    REQUIRE(comp.transitioningClip(0) == nullptr);
    REQUIRE_NEAR(comp.layer(0).transitionProgress, 1.0, 1e-12);

    // Chi con MOT luong — tra lai tai nguyen cho engine.
    const auto info = comp.renderInfo(0);
    REQUIRE(info.previous == nullptr);
    REQUIRE_NEAR(info.currentOpacity, 1.0, 1e-9);
}

TEST_CASE("★ A10: bam lai DUNG clip dang phat -> phat lai, KHONG crossfade") {
    // Crossfade mot clip voi chinh no chi tao ra hieu ung nhap nhay.
    Composition comp = makeFilled(1, 3);
    comp.layer(0).transitionDuration = 1.0;

    comp.triggerClip(0, 0);
    comp.update(0.2);
    comp.triggerClip(0, 0);           // bam lai chinh no

    REQUIRE(!comp.layer(0).isTransitioning());
    REQUIRE_NEAR(comp.playingClip(0)->transport.position, 0.0, 1e-12);  // da tua ve dau
}

TEST_CASE("★ A10: clip cu VAN TIEN dau phat trong luc tat dan") {
    Composition comp = makeFilled(1, 3);
    comp.layer(0).transitionDuration = 2.0;

    comp.triggerClip(0, 0);
    comp.triggerClip(0, 1);

    const Clip* prev = comp.transitioningClip(0);
    REQUIRE(prev != nullptr);
    const double before = prev->transport.position;

    comp.update(0.5);

    REQUIRE(comp.transitioningClip(0) != nullptr);
    REQUIRE(comp.transitioningClip(0)->transport.position > before);
}

TEST_CASE("A10: clear() huy luon transition dang chay") {
    Composition comp = makeFilled(1, 3);
    comp.layer(0).transitionDuration = 1.0;
    comp.triggerClip(0, 0);
    comp.triggerClip(0, 1);
    REQUIRE(comp.layer(0).isTransitioning());

    comp.layer(0).clear();
    REQUIRE(!comp.layer(0).isTransitioning());
    REQUIRE(comp.transitioningClip(0) == nullptr);
}

TEST_CASE("★ A10: trigger COT cung kich hoat transition tren moi layer") {
    Composition comp = makeFilled(3, 4);
    for (int L = 0; L < 3; ++L) comp.layer(L).transitionDuration = 1.0;

    comp.triggerColumn(0);
    comp.triggerColumn(2);

    for (int L = 0; L < 3; ++L) {
        CHECK(comp.layer(L).isTransitioning());
        CHECK(comp.transitioningClip(L) != nullptr);
    }
}

TEST_CASE("A10: renderInfo tra ve blend cua layer") {
    Composition comp = makeFilled(1, 2);
    comp.layer(0).blend = BlendMode::Screen;
    comp.triggerClip(0, 0);
    REQUIRE(comp.renderInfo(0).blend == BlendMode::Screen);
}

TEST_CASE("A10: layer khong phat gi thi renderInfo rong") {
    Composition comp = makeFilled(2, 2);
    const auto info = comp.renderInfo(0);
    REQUIRE(info.current == nullptr);
    REQUIRE(info.previous == nullptr);
    REQUIRE_NEAR(info.currentOpacity, 0.0, 1e-12);
}

TEST_CASE("clearAll dung moi layer") {
    Composition comp = makeFilled(3, 3);
    comp.triggerColumn(1);
    REQUIRE(comp.visibleLayers().size() == 3u);

    comp.clearAll();
    REQUIRE(comp.visibleLayers().empty());
}

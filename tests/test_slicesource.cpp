#include "TestHarness.h"

#include "core/model/ProjectIO.h"
#include "core/model/Screen.h"
#include "core/model/Slice.h"

#include <cstdio>

using namespace hexmap;

namespace {

Slice makeSlice(const char* name, Slice::SourceKind kind, int layer) {
    Slice s(Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0},
            Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0});
    s.name        = name;
    s.sourceKind  = kind;
    s.sourceLayer = layer;
    return s;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════
//  F22 — chọn nguồn cho slice
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("F22: mac dinh la Composition") {
    const Slice s;
    CHECK(s.sourceKind == Slice::SourceKind::Composition);
    CHECK(s.effectiveSourceLayer(3) == -1);
}

TEST_CASE("F22: chon layer hop le thi tra ve dung layer do") {
    const Slice s = makeSlice("sl", Slice::SourceKind::Layer, 1);
    CHECK(s.effectiveSourceLayer(3) == 1);
}

// ★ Day la cai bay that cua F22.
//
//   Nguoi dung tro slice vao Layer 3, sau do XOA bot layer. Chi so con
//   lai tro vao layer khong ton tai. Neu render cu the ma lay thi may
//   chieu do ra man DEN — khong thong bao, khong log, chi mot may chieu
//   tat ngom giua buoi dien.
//
//   Lui ve Composition van sai y nguoi dung, nhung SAI THAY DUOC: ho nhin
//   ra ngay la dang chieu nham nguon va sua duoc.
TEST_CASE("★ F22: chi so tro vao layer da bi xoa -> lui ve Composition") {
    const Slice s = makeSlice("sl", Slice::SourceKind::Layer, 5);
    CHECK(s.effectiveSourceLayer(3) == -1);   // chi co 3 layer
    CHECK(s.effectiveSourceLayer(6) == 5);    // du layer thi lay lai duoc
}

TEST_CASE("F22: chi so am bi tu choi") {
    const Slice s = makeSlice("sl", Slice::SourceKind::Layer, -1);
    CHECK(s.effectiveSourceLayer(3) == -1);
}

TEST_CASE("F22: sourceLayer bi BO QUA khi nguon la Composition") {
    // Giu lai chi so cu la co chu dich: nguoi dung doi qua lai giua hai
    // che do thi khong phai chon lai layer moi lan.
    const Slice s = makeSlice("sl", Slice::SourceKind::Composition, 2);
    CHECK(s.sourceLayer == 2);
    CHECK(s.effectiveSourceLayer(4) == -1);
}

// ═══════════════════════════════════════════════════════════════════════
//  F22 — danh sách layer cần nướng FBO riêng
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("F22: khong slice nao dung layer -> khong can FBO nao") {
    std::vector<Screen> screens;
    Screen sc(0, "S", Vec2{1920.0, 1080.0});
    sc.slices.push_back(makeSlice("a", Slice::SourceKind::Composition, -1));
    sc.slices.push_back(makeSlice("b", Slice::SourceKind::Composition, -1));
    screens.push_back(std::move(sc));

    CHECK(layersUsedAsSource(screens, 4).empty());
}

TEST_CASE("★ F22: gom layer tu NHIEU screen, sap xep va bo trung") {
    std::vector<Screen> screens;

    Screen a(0, "A", Vec2{1920.0, 1080.0});
    a.slices.push_back(makeSlice("a1", Slice::SourceKind::Layer, 2));
    a.slices.push_back(makeSlice("a2", Slice::SourceKind::Layer, 0));
    a.slices.push_back(makeSlice("a3", Slice::SourceKind::Composition, -1));
    screens.push_back(std::move(a));

    Screen b(1, "B", Vec2{1920.0, 1080.0});
    b.slices.push_back(makeSlice("b1", Slice::SourceKind::Layer, 2));   // trung
    b.slices.push_back(makeSlice("b2", Slice::SourceKind::Layer, 9));   // ngoai pham vi
    screens.push_back(std::move(b));

    const std::vector<int> used = layersUsedAsSource(screens, 4);
    REQUIRE(used.size() == 2u);
    CHECK(used[0] == 0);
    CHECK(used[1] == 2);
}

// ★ Slice dang TAT van duoc tinh vao danh sach.
//
//   Bo qua cho do ton nghe thi hop ly, nhung hau qua la bat slice len
//   giua buoi dien thi layer nguon chua duoc nuong o frame do — may chieu
//   loe MOT FRAME DEN roi moi co hinh. Giu FBO cho ca slice tat dat hon
//   mot chut, doi lai bat/tat khong bao gio chop.
TEST_CASE("★ F22: slice dang TAT van giu FBO de bat len khong chop den") {
    std::vector<Screen> screens;
    Screen sc(0, "S", Vec2{1920.0, 1080.0});

    Slice off = makeSlice("tat", Slice::SourceKind::Layer, 1);
    off.enabled = false;
    sc.slices.push_back(std::move(off));
    screens.push_back(std::move(sc));

    const std::vector<int> used = layersUsedAsSource(screens, 3);
    REQUIRE(used.size() == 1u);
    CHECK(used[0] == 1);
}

TEST_CASE("F22: screen dang tat van giu FBO, cung ly do") {
    std::vector<Screen> screens;
    Screen sc(0, "S", Vec2{1920.0, 1080.0});
    sc.enabled = false;
    sc.slices.push_back(makeSlice("sl", Slice::SourceKind::Layer, 0));
    screens.push_back(std::move(sc));

    CHECK(layersUsedAsSource(screens, 2).size() == 1u);
}

// ═══════════════════════════════════════════════════════════════════════
//  F22 — lưu / nạp
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★ F22: nguon Layer song sot qua luu / nap") {
    const std::string path = "hexmap_test_f22.hexmap";

    Project a;
    a.composition = Composition(3, 4, 1);
    Screen sc(0, "Projector 1", Vec2{1920.0, 1080.0});
    sc.addFullScreenSlice(a.composition.canvasSize);
    sc.slices[0].sourceKind  = Slice::SourceKind::Layer;
    sc.slices[0].sourceLayer = 2;
    a.screens.push_back(std::move(sc));

    std::string err;
    REQUIRE(projectio::save(path, a, err));

    Project b;
    REQUIRE(projectio::load(path, b).ok);
    REQUIRE(b.screens.size() == 1u);
    REQUIRE(b.screens[0].sliceCount() == 1);

    const Slice& s = b.screens[0].slices[0];
    CHECK(s.sourceKind == Slice::SourceKind::Layer);
    CHECK(s.sourceLayer == 2);
    CHECK(s.effectiveSourceLayer(b.composition.layerCount()) == 2);

    std::remove(path.c_str());
    std::remove((path + ".tmp").c_str());
}

// File cua ban CU khong co hai khoa nay — phai ra Composition, khong
// duoc thanh Layer voi chi so rac.
TEST_CASE("★ F22: file ban cu thieu khoa -> Composition, khong phai layer rac") {
    const char* text = R"({
      "version": 1,
      "screens": [{
        "name": "S", "resolution": [1920, 1080],
        "slices": [{ "name": "sl", "warp": { "type": "CornerPin" } }]
      }]
    })";

    Project b;
    const LoadResult r = projectio::fromJson(text, b);
    REQUIRE(r.ok);
    REQUIRE(b.screens.size() == 1u);

    const Slice& s = b.screens[0].slices[0];
    CHECK(s.sourceKind == Slice::SourceKind::Composition);
    CHECK(s.effectiveSourceLayer(4) == -1);
}

TEST_CASE("F22: file khai Layer nhung thieu chi so -> canh bao") {
    const char* text = R"({
      "version": 1,
      "screens": [{
        "name": "S", "resolution": [1920, 1080],
        "slices": [{ "name": "sl", "sourceKind": "Layer",
                     "warp": { "type": "CornerPin" } }]
      }]
    })";

    Project b;
    const LoadResult r = projectio::fromJson(text, b);
    REQUIRE(r.ok);
    CHECK(!r.warnings.empty());
    CHECK(b.screens[0].slices[0].effectiveSourceLayer(4) == -1);
}

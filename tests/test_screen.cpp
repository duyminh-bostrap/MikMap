#include "TestHarness.h"

#include "core/model/Screen.h"
#include "core/model/WarpCornerPin.h"
#include "core/model/WarpMesh.h"

using namespace mikmap;

// ═══════════════════════════════════════════════════════════════════════
//  F3 F4 — Slice
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("F3: slice mac dinh co warp corner pin san sang dung") {
    Slice s;
    REQUIRE(s.warp() != nullptr);
    REQUIRE(s.warp()->type() == WarpType::CornerPin);
    REQUIRE(s.isUsable());
}

TEST_CASE("F4: inputRect anh xa contentUV sang toa do canvas") {
    Slice s;
    s.inputOrigin = Vec2{200.0, 100.0};
    s.inputSize   = Vec2{800.0, 600.0};

    REQUIRE_VEC_NEAR(s.contentToCanvas({0.0, 0.0}), 200.0, 100.0, 1e-9);
    REQUIRE_VEC_NEAR(s.contentToCanvas({1.0, 1.0}), 1000.0, 700.0, 1e-9);
    REQUIRE_VEC_NEAR(s.contentToCanvas({0.5, 0.5}), 600.0, 400.0, 1e-9);
}

TEST_CASE("F4: canvasToContent la nghich dao cua contentToCanvas") {
    Slice s;
    s.inputOrigin = Vec2{200.0, 100.0};
    s.inputSize   = Vec2{800.0, 600.0};

    const Vec2 uv{0.37, 0.82};
    REQUIRE_VEC_NEAR(s.canvasToContent(s.contentToCanvas(uv)), uv.x, uv.y, 1e-9);
}

TEST_CASE("★ F4: inputSize = 0 khong gay chia cho 0") {
    // Nguoi dung keo vung lay thanh mot duong thang la chuyen co the xay ra.
    Slice s;
    s.inputSize = Vec2{0.0, 600.0};
    const Vec2 c = s.canvasToContent(Vec2{500.0, 300.0});
    REQUIRE(c.isFinite());
}

TEST_CASE("★ F3: slice dung constructor day du -> output dung vi tri") {
    Slice s(Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0},
            Vec2{100.0, 50.0}, Vec2{800.0, 600.0});

    REQUIRE_VEC_NEAR(s.contentToOutput({0.0, 0.0}), 100.0, 50.0, 1e-7);
    REQUIRE_VEC_NEAR(s.contentToOutput({1.0, 1.0}), 900.0, 650.0, 1e-7);
}

TEST_CASE("★ F3: outputToCanvas di het chuoi output -> canvas") {
    Slice s(Vec2{500.0, 200.0}, Vec2{400.0, 300.0},
            Vec2{0.0, 0.0}, Vec2{1000.0, 800.0});

    Vec2 canvas;
    REQUIRE(s.outputToCanvas(Vec2{500.0, 400.0}, canvas));   // giua output
    REQUIRE_VEC_NEAR(canvas, 500.0 + 200.0, 200.0 + 150.0, 1e-6);
}

TEST_CASE("F3: diem ngoai slice bi tu choi") {
    Slice s(Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0},
            Vec2{100.0, 100.0}, Vec2{200.0, 200.0});
    Vec2 canvas;
    REQUIRE(!s.outputToCanvas(Vec2{5000.0, 5000.0}, canvas));
}

TEST_CASE("★★ F3: SAO CHEP slice phai clone warp, khong chia se con tro") {
    // Neu quen viet copy constructor, hai slice se dung chung mot warp:
    // keo goc slice nay lam doi luon slice kia. Loi rat kho lan ra.
    Slice a(Vec2{0, 0}, Vec2{1920, 1080}, Vec2{0, 0}, Vec2{800, 600});
    Slice b = a;                       // sao chep

    auto* wa = static_cast<WarpCornerPin*>(a.warp());
    auto* wb = static_cast<WarpCornerPin*>(b.warp());
    REQUIRE(wa != wb);                 // ★ phai la hai doi tuong khac nhau

    REQUIRE(wa->setCorner(0, Vec2{50.0, 40.0}));

    // b KHONG duoc bi anh huong.
    REQUIRE_VEC_NEAR(wb->corner(0), 0.0, 0.0, 1e-9);
}

TEST_CASE("★ F3: GAN slice cung phai clone warp") {
    Slice a(Vec2{0, 0}, Vec2{1920, 1080}, Vec2{0, 0}, Vec2{800, 600});
    Slice b;
    b = a;

    REQUIRE(a.warp() != b.warp());
    auto* wa = static_cast<WarpCornerPin*>(a.warp());
    REQUIRE(wa->setCorner(2, Vec2{700.0, 500.0}));
    REQUIRE_VEC_NEAR(static_cast<WarpCornerPin*>(b.warp())->corner(2), 800.0, 600.0, 1e-9);
}

TEST_CASE("★ F3: doi corner pin sang mesh GIU vung output") {
    Slice s(Vec2{0, 0}, Vec2{1920, 1080}, Vec2{300.0, 200.0}, Vec2{600.0, 400.0});

    Vec2 loBefore, hiBefore;
    s.warp()->boundingBox(loBefore, hiBefore);

    s.convertWarp(WarpType::Mesh, 6, 6);
    REQUIRE(s.warp()->type() == WarpType::Mesh);

    Vec2 loAfter, hiAfter;
    s.warp()->boundingBox(loAfter, hiAfter);
    REQUIRE_VEC_NEAR(loAfter, loBefore.x, loBefore.y, 1e-6);
    REQUIRE_VEC_NEAR(hiAfter, hiBefore.x, hiBefore.y, 1e-6);
    REQUIRE(s.isUsable());
}

TEST_CASE("F3: doi sang cung loai warp thi khong lam gi") {
    Slice s;
    const IWarp* before = s.warp();
    s.convertWarp(WarpType::CornerPin);
    REQUIRE(s.warp() == before);       // khong tao lai vo ich
}

TEST_CASE("F16: slice bi tat thi khong con dung duoc") {
    Slice s;
    REQUIRE(s.isUsable());
    s.enabled = false;
    REQUIRE(!s.isUsable());
}

// ═══════════════════════════════════════════════════════════════════════
//  F1 F7 F16 F17 — Screen
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("F1: screen moi chua co slice nao") {
    Screen sc(0, "Projector 1", Vec2{1920.0, 1080.0});
    REQUIRE(sc.sliceCount() == 0);
    REQUIRE(sc.visibleSlices().empty());
    REQUIRE(sc.hitTest({500.0, 500.0}) == -1);
}

TEST_CASE("F7: them slice toan man hinh") {
    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    sc.addFullScreenSlice(Vec2{3840.0, 2160.0});

    REQUIRE(sc.sliceCount() == 1);
    REQUIRE_VEC_NEAR(sc.slices[0].inputSize, 3840.0, 2160.0, 1e-9);
    REQUIRE_VEC_NEAR(sc.slices[0].contentToOutput({1.0, 1.0}), 1920.0, 1080.0, 1e-6);
}

TEST_CASE("F7: nhieu slice tren mot screen") {
    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    for (int i = 0; i < 3; ++i) {
        Slice s(Vec2{0, 0}, Vec2{1920, 1080},
                Vec2{i * 600.0, 0.0}, Vec2{500.0, 400.0});
        sc.slices.push_back(std::move(s));
    }
    REQUIRE(sc.sliceCount() == 3);
    REQUIRE(sc.visibleSlices().size() == 3u);
}

TEST_CASE("★ F7: hitTest tim dung slice chua diem") {
    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    for (int i = 0; i < 3; ++i) {
        sc.slices.emplace_back(Vec2{0, 0}, Vec2{1920, 1080},
                               Vec2{i * 600.0, 0.0}, Vec2{500.0, 400.0});
    }

    CHECK(sc.hitTest({250.0, 200.0})  == 0);
    CHECK(sc.hitTest({850.0, 200.0})  == 1);
    CHECK(sc.hitTest({1450.0, 200.0}) == 2);
    CHECK(sc.hitTest({550.0, 200.0})  == -1);   // khe ho giua cac slice
    CHECK(sc.hitTest({250.0, 900.0})  == -1);   // duoi day
}

TEST_CASE("★★ F7: slice chong nhau -> slice TREN CUNG thang") {
    // Neu duyet xuoi thay vi nguoc, cham vao slice nho nam tren se kich
    // hoat nham slice nen phia duoi.
    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    sc.slices.emplace_back(Vec2{0, 0}, Vec2{1920, 1080},
                           Vec2{0.0, 0.0}, Vec2{1000.0, 800.0});     // [0] duoi
    sc.slices.emplace_back(Vec2{0, 0}, Vec2{1920, 1080},
                           Vec2{200.0, 200.0}, Vec2{300.0, 300.0});  // [1] tren

    REQUIRE(sc.hitTest({350.0, 350.0}) == 1);   // ★ vung chong -> slice tren
    REQUIRE(sc.hitTest({800.0, 700.0}) == 0);   // chi slice duoi phu
}

TEST_CASE("F16: slice bi tat thi hitTest bo qua no") {
    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    sc.slices.emplace_back(Vec2{0, 0}, Vec2{1920, 1080},
                           Vec2{0.0, 0.0}, Vec2{1000.0, 800.0});
    sc.slices.emplace_back(Vec2{0, 0}, Vec2{1920, 1080},
                           Vec2{200.0, 200.0}, Vec2{300.0, 300.0});

    REQUIRE(sc.hitTest({350.0, 350.0}) == 1);
    sc.slices[1].enabled = false;
    REQUIRE(sc.hitTest({350.0, 350.0}) == 0);   // roi xuong slice duoi
}

TEST_CASE("★ F16: solo an tat ca slice khong solo") {
    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    for (int i = 0; i < 3; ++i) {
        sc.slices.emplace_back(Vec2{0, 0}, Vec2{1920, 1080},
                               Vec2{i * 600.0, 0.0}, Vec2{500.0, 400.0});
    }
    REQUIRE(sc.visibleSlices().size() == 3u);

    sc.slices[1].solo = true;
    REQUIRE(sc.anySliceSolo());

    const auto vis = sc.visibleSlices();
    REQUIRE(vis.size() == 1u);
    REQUIRE(vis[0] == 1);

    // hitTest cung phai ton trong solo.
    REQUIRE(sc.hitTest({250.0, 200.0}) == -1);
    REQUIRE(sc.hitTest({850.0, 200.0}) == 1);
}

TEST_CASE("F7: xoa slice") {
    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    for (int i = 0; i < 3; ++i) {
        Slice s;
        s.name = "S" + std::to_string(i);
        sc.slices.push_back(std::move(s));
    }
    sc.removeSlice(1);
    REQUIRE(sc.sliceCount() == 2);
    CHECK(sc.slices[0].name == "S0");
    CHECK(sc.slices[1].name == "S2");

    sc.removeSlice(99);                 // ngoai pham vi -> khong lam gi
    REQUIRE(sc.sliceCount() == 2);
}

TEST_CASE("★ F7: doi thu tu z bang moveSlice") {
    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    for (int i = 0; i < 3; ++i) {
        Slice s;
        s.name = "S" + std::to_string(i);
        sc.slices.push_back(std::move(s));
    }

    REQUIRE(sc.moveSlice(0, 2));        // dua S0 len tren cung
    CHECK(sc.slices[0].name == "S1");
    CHECK(sc.slices[1].name == "S2");
    CHECK(sc.slices[2].name == "S0");

    REQUIRE(!sc.moveSlice(-1, 0));
    REQUIRE(!sc.moveSlice(0, 99));
}

TEST_CASE("★ F7: outputToCanvas tra ve du slice + contentUV + canvasPx") {
    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    sc.slices.emplace_back(Vec2{500.0, 200.0}, Vec2{400.0, 300.0},
                           Vec2{0.0, 0.0}, Vec2{1000.0, 800.0});

    int idx = -1;
    Vec2 uv, canvas;
    REQUIRE(sc.outputToCanvas(Vec2{500.0, 400.0}, idx, uv, canvas));

    CHECK(idx == 0);
    CHECK_NEAR(uv.x, 0.5, 1e-6);
    CHECK_NEAR(uv.y, 0.5, 1e-6);
    CHECK_NEAR(canvas.x, 700.0, 1e-5);
    CHECK_NEAR(canvas.y, 350.0, 1e-5);
}

TEST_CASE("★ F17: nhieu screen doc lap voi nhau") {
    Screen a(0, "Left", Vec2{1920.0, 1080.0});
    Screen b(1, "Right", Vec2{1920.0, 1080.0});

    // Hai screen lay hai NUA khac nhau cua canvas 3840x1080.
    a.slices.emplace_back(Vec2{0.0, 0.0},    Vec2{1920.0, 1080.0},
                          Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0});
    b.slices.emplace_back(Vec2{1920.0, 0.0}, Vec2{1920.0, 1080.0},
                          Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0});

    int ia = -1, ib = -1;
    Vec2 uva, uvb, ca, cb;
    REQUIRE(a.outputToCanvas(Vec2{960.0, 540.0}, ia, uva, ca));
    REQUIRE(b.outputToCanvas(Vec2{960.0, 540.0}, ib, uvb, cb));

    // Cung mot pixel tren MOI screen -> hai vung canvas KHAC NHAU.
    CHECK_NEAR(ca.x, 960.0, 1e-5);
    CHECK_NEAR(cb.x, 2880.0, 1e-5);
}

TEST_CASE("★ F7: slice dung MESH van hitTest duoc") {
    Screen sc(0, "Projector", Vec2{1920.0, 1080.0});
    Slice s;
    s.setWarp(std::make_unique<WarpMesh>(4, 4, Vec2{200.0, 150.0}, Vec2{800.0, 600.0}));
    sc.slices.push_back(std::move(s));

    REQUIRE(sc.hitTest({600.0, 450.0}) == 0);
    REQUIRE(sc.hitTest({50.0, 50.0}) == -1);
}

// ═══════════════════════════════════════════════════════════════════════
//  F19 — hieu chinh mau per-slice
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("F19: mac dinh la khong lam gi") {
    Slice s;
    REQUIRE(s.color.isIdentity());
}

TEST_CASE("F19: doi mot gia tri thi khong con la mac dinh") {
    Slice s;
    s.color.gamma = 2.2;
    REQUIRE(!s.color.isIdentity());
    s.color.reset();
    REQUIRE(s.color.isIdentity());
}

TEST_CASE("★ F19: SAO CHEP slice phai mang theo hieu chinh mau") {
    // Quen chep truong nay thi nhan doi slice se mat cong can mau —
    // cung loai loi voi viec quen clone() warp.
    Slice a;
    a.color.brightness = 0.3;
    a.color.gainR = 1.2;
    a.color.gamma = 1.8;

    Slice b = a;
    CHECK_NEAR(b.color.brightness, 0.3, 1e-12);
    CHECK_NEAR(b.color.gainR, 1.2, 1e-12);
    CHECK_NEAR(b.color.gamma, 1.8, 1e-12);

    Slice c;
    c = a;
    CHECK_NEAR(c.color.gamma, 1.8, 1e-12);
}

TEST_CASE("F20: soft edge mac dinh la tat") {
    Slice s;
    REQUIRE(s.softEdge.isIdentity());
}

TEST_CASE("★ F20: SAO CHEP slice phai mang theo soft edge") {
    Slice a;
    a.softEdge.right = 0.15;
    a.softEdge.gamma = 2.2;

    Slice b = a;
    CHECK_NEAR(b.softEdge.right, 0.15, 1e-12);
    CHECK_NEAR(b.softEdge.gamma, 2.2, 1e-12);

    Slice c;
    c = a;
    CHECK_NEAR(c.softEdge.right, 0.15, 1e-12);
}

TEST_CASE("F20: reset tra ve tat") {
    Slice s;
    s.softEdge.left = 0.2;
    REQUIRE(!s.softEdge.isIdentity());
    s.softEdge.reset();
    REQUIRE(s.softEdge.isIdentity());
}

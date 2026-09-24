#include "TestHarness.h"

#include "core/model/ProjectIO.h"
#include "core/model/WarpCornerPin.h"
#include "core/model/WarpMesh.h"
#include "core/model/WarpBezier.h"
#include "core/util/Json.h"

#include <cstdio>

using namespace mikmap;

// ═══════════════════════════════════════════════════════════════════════
//  JSON toi gian
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("Json: cac kieu co ban round-trip") {
    JsonValue o = JsonValue::object();
    o.set("b", JsonValue(true));
    o.set("n", JsonValue(3.5));
    o.set("i", JsonValue(42));
    o.set("s", JsonValue("hello"));

    JsonValue back;
    std::string err;
    REQUIRE(JsonValue::parse(o.dump(), back, err));
    CHECK(back["b"].asBool() == true);
    CHECK_NEAR(back["n"].asNumber(), 3.5, 1e-12);
    CHECK(back["i"].asInt() == 42);
    CHECK(back["s"].asString() == "hello");
}

TEST_CASE("★ Json: duong dan Windows duoc escape dung") {
    // Khong escape dau \ thi file project sinh ra se HONG va khong nap lai
    // duoc. Day la loi chac chan xay ra tren Windows.
    JsonValue o = JsonValue::object();
    o.set("path", JsonValue("D:\\2026\\media\\clip \"final\".mov"));

    const std::string text = o.dump();
    REQUIRE(text.find("\\\\") != std::string::npos);   // co escape

    JsonValue back;
    std::string err;
    REQUIRE(JsonValue::parse(text, back, err));
    REQUIRE(back["path"].asString() == "D:\\2026\\media\\clip \"final\".mov");
}

TEST_CASE("Json: chuoi UTF-8 tieng Viet round-trip") {
    JsonValue o = JsonValue::object();
    o.set("name", JsonValue("Lớp nền — Hiệu ứng"));

    JsonValue back;
    std::string err;
    REQUIRE(JsonValue::parse(o.dump(), back, err));
    REQUIRE(back["name"].asString() == "Lớp nền — Hiệu ứng");
}

TEST_CASE("Json: mang long nhau") {
    JsonValue arr = JsonValue::array();
    for (int i = 0; i < 3; ++i) {
        JsonValue inner = JsonValue::array();
        inner.push(JsonValue(i));
        inner.push(JsonValue(i * 2));
        arr.push(std::move(inner));
    }

    JsonValue back;
    std::string err;
    REQUIRE(JsonValue::parse(arr.dump(), back, err));
    REQUIRE(back.size() == 3u);
    CHECK(back.at(2).at(1).asInt() == 4);
}

TEST_CASE("Json: so am va so mu") {
    JsonValue back;
    std::string err;
    REQUIRE(JsonValue::parse("{\"a\": -1.5e-3, \"b\": 1e10}", back, err));
    CHECK_NEAR(back["a"].asNumber(), -0.0015, 1e-12);
    CHECK_NEAR(back["b"].asNumber(), 1e10, 1.0);
}

TEST_CASE("★ Json: doc truong THIEU tra ve gia tri mac dinh") {
    // Rat quan trong khi nap file cua ban cu: thieu mot truong khong
    // duoc lam hong ca project.
    JsonValue back;
    std::string err;
    REQUIRE(JsonValue::parse("{}", back, err));
    CHECK(back["khongcó"].asInt(99) == 99);
    CHECK(back["khongcó"].asString("mac dinh") == "mac dinh");
    CHECK(back["khongcó"].asBool(true) == true);
}

TEST_CASE("★ Json: cu phap sai -> bao loi CO VI TRI, khong sap") {
    JsonValue v;
    std::string err;
    REQUIRE(!JsonValue::parse("{\"a\": }", v, err));
    REQUIRE(!err.empty());
    REQUIRE(err.find("vi tri") != std::string::npos);
}

TEST_CASE("Json: du lieu thua phia sau bi tu choi") {
    JsonValue v;
    std::string err;
    REQUIRE(!JsonValue::parse("{} rac", v, err));
}

TEST_CASE("Json: chuoi khong dong bi tu choi") {
    JsonValue v;
    std::string err;
    REQUIRE(!JsonValue::parse("{\"a\": \"chua dong}", v, err));
}

TEST_CASE("Json: dump co thut le de nguoi doc duoc") {
    JsonValue o = JsonValue::object();
    o.set("a", JsonValue(1));
    const std::string s = o.dump(2);
    REQUIRE(s.find('\n') != std::string::npos);
}

// ═══════════════════════════════════════════════════════════════════════
//  ★ ProjectIO — round-trip day du
// ═══════════════════════════════════════════════════════════════════════

namespace {

Project makeRichProject() {
    Project p;
    p.name = "Show Tết 2026";

    // Composition: 3 layer x 4 cot x 2 deck
    p.composition = Composition(3, 4, 2);
    p.composition.canvasSize = Vec2{3840.0, 2160.0};
    p.composition.masterOpacity = 0.85;
    p.composition.emptyCellBehavior = EmptyCellBehavior::ClearLayer;

    p.composition.layer(0).name    = "Nền";
    p.composition.layer(0).opacity = 0.7;
    p.composition.layer(0).blend   = BlendMode::Multiply;
    p.composition.layer(1).solo    = true;
    p.composition.layer(2).bypass  = true;

    Clip c;
    c.name = "Rồng bay";
    c.media.type = MediaType::Video;
    c.media.path = "D:\\2026\\media\\rong \"bay\".mov";
    c.media.size = Vec2{3840.0, 2160.0};
    c.media.durationSec = 12.5;
    c.transport.direction = PlayDirection::PingPong;
    c.transport.endAction = EndAction::PlayNext;
    c.transport.speed = 1.5;
    c.transport.setTrim(0.1, 0.9);
    c.transform.position = Vec2{120.0, 45.0};
    c.transform.scale    = Vec2{1.2, 0.8};
    c.transform.rotation = 0.35;
    c.transform.flipH = true;
    c.blend = BlendMode::Screen;
    c.opacity = 0.6;
    c.triggerStyle = TriggerStyle::Piano;
    c.colorTag = 0xFF3366;
    p.composition.deck(0).setClip(1, 2, c);

    Clip img;
    img.name = "Logo";
    img.media.type = MediaType::Image;
    img.media.path = "logo.png";
    p.composition.deck(1).setClip(0, 0, img);

    // Screen 1: corner pin
    Screen sc0(0, "Máy chiếu trái", Vec2{1920.0, 1080.0});
    sc0.displayIndex = 1;
    Slice s0;
    s0.name = "Tường chính";
    s0.inputOrigin = Vec2{0.0, 0.0};
    s0.inputSize   = Vec2{1920.0, 2160.0};
    const Vec2 quad[4] = {{200, 150}, {1750, 100}, {1850, 950}, {150, 1000}};
    s0.setWarp(std::make_unique<WarpCornerPin>(quad));
    sc0.slices.push_back(std::move(s0));

    // Screen 2: mesh warp da bien dang
    Screen sc1(1, "Máy chiếu phải", Vec2{1920.0, 1080.0});
    Slice s1;
    s1.name = "Cột tròn";
    s1.solo = true;
    auto mesh = std::make_unique<WarpMesh>(3, 3, Vec2{100.0, 100.0}, Vec2{800.0, 600.0});
    mesh->setControlPoint(1, 1, Vec2{400.0, 250.0});
    mesh->setControlPoint(2, 2, Vec2{650.0, 500.0});
    s1.setWarp(std::move(mesh));
    sc1.slices.push_back(std::move(s1));
    sc1.slices.emplace_back(Vec2{0, 0}, Vec2{1920, 1080}, Vec2{900.0, 100.0}, Vec2{400.0, 300.0});

    p.screens.push_back(std::move(sc0));
    p.screens.push_back(std::move(sc1));

    // Calibration
    CalibrationProfile cal;
    cal.name = "IR Frame";
    cal.sourceId = 3;
    cal.targetScreenId = 0;
    cal.method = SolveMethod::LeastSquares;
    cal.addPair({100, 100},  {200, 150});
    cal.addPair({1900, 120}, {1750, 100});
    cal.addPair({1950, 1050},{1850, 950});
    cal.addPair({80, 1000},  {150, 1000});
    cal.addPair({900, 500},  {800, 400});
    cal.setPairEnabled(4, false);
    cal.solve();
    p.calibrations.push_back(std::move(cal));

    return p;
}

} // namespace

TEST_CASE("★★ ProjectIO: round-trip GIU NGUYEN toan bo project") {
    const Project a = makeRichProject();

    Project b;
    const LoadResult r = projectio::fromJson(projectio::toJson(a), b);
    REQUIRE(r.ok);

    // ── Composition ────────────────────────────────────────────────────
    CHECK(b.name == "Show Tết 2026");
    CHECK_NEAR(b.composition.canvasSize.x, 3840.0, 1e-9);
    CHECK_NEAR(b.composition.canvasSize.y, 2160.0, 1e-9);
    CHECK_NEAR(b.composition.masterOpacity, 0.85, 1e-12);
    CHECK(b.composition.emptyCellBehavior == EmptyCellBehavior::ClearLayer);
    CHECK(b.composition.layerCount() == 3);
    CHECK(b.composition.deckCount() == 2);
    CHECK(b.composition.columnCount() == 4);

    CHECK(b.composition.layer(0).name == "Nền");
    CHECK_NEAR(b.composition.layer(0).opacity, 0.7, 1e-12);
    CHECK(b.composition.layer(0).blend == BlendMode::Multiply);
    CHECK(b.composition.layer(1).solo);
    CHECK(b.composition.layer(2).bypass);

    // ── Clip ───────────────────────────────────────────────────────────
    const Clip& c = b.composition.deck(0).clip(1, 2);
    CHECK(c.name == "Rồng bay");
    CHECK(c.media.type == MediaType::Video);
    CHECK(c.media.path == "D:\\2026\\media\\rong \"bay\".mov");   // ★ escape
    CHECK_NEAR(c.media.durationSec, 12.5, 1e-12);
    CHECK(c.transport.direction == PlayDirection::PingPong);
    CHECK(c.transport.endAction == EndAction::PlayNext);
    CHECK_NEAR(c.transport.speed, 1.5, 1e-12);
    CHECK_NEAR(c.transport.inPoint, 0.1, 1e-12);
    CHECK_NEAR(c.transport.outPoint, 0.9, 1e-12);
    CHECK_NEAR(c.transform.position.x, 120.0, 1e-9);
    CHECK_NEAR(c.transform.rotation, 0.35, 1e-12);
    CHECK(c.transform.flipH);
    CHECK(c.blend == BlendMode::Screen);
    CHECK_NEAR(c.opacity, 0.6, 1e-12);
    CHECK(c.triggerStyle == TriggerStyle::Piano);
    CHECK(c.colorTag == 0xFF3366u);

    CHECK(b.composition.deck(1).clip(0, 0).name == "Logo");
    CHECK(b.composition.deck(0).clip(0, 0).isEmpty());   // o trong van trong

    // ── Screen + warp ──────────────────────────────────────────────────
    REQUIRE(b.screens.size() == 2u);
    CHECK(b.screens[0].name == "Máy chiếu trái");
    CHECK(b.screens[0].displayIndex == 1);
    REQUIRE(b.screens[0].sliceCount() == 1);
    CHECK(b.screens[0].slices[0].name == "Tường chính");
    CHECK_NEAR(b.screens[0].slices[0].inputSize.y, 2160.0, 1e-9);

    const auto* cp = static_cast<const WarpCornerPin*>(b.screens[0].slices[0].warp());
    REQUIRE(cp != nullptr);
    CHECK(cp->type() == WarpType::CornerPin);
    CHECK_NEAR(cp->corner(1).x, 1750.0, 1e-9);
    CHECK_NEAR(cp->corner(3).y, 1000.0, 1e-9);

    REQUIRE(b.screens[1].sliceCount() == 2);
    CHECK(b.screens[1].slices[0].solo);
    const auto* mesh = static_cast<const WarpMesh*>(b.screens[1].slices[0].warp());
    REQUIRE(mesh != nullptr);
    CHECK(mesh->type() == WarpType::Mesh);
    CHECK(mesh->cols() == 3);
    CHECK_NEAR(mesh->controlPoint(1, 1).x, 400.0, 1e-9);
    CHECK_NEAR(mesh->controlPoint(2, 2).y, 500.0, 1e-9);

    // ── Calibration ────────────────────────────────────────────────────
    REQUIRE(b.calibrations.size() == 1u);
    const CalibrationProfile& cal = b.calibrations[0];
    CHECK(cal.name == "IR Frame");
    CHECK(cal.sourceId == 3);
    CHECK(cal.pairCount() == 5u);
    CHECK(cal.enabledPairCount() == 4u);      // diem thu 5 bi tat
    CHECK(cal.isValid());                     // ★ tu giai lai khi nap
    CHECK(cal.rmsError() < 1e-6);
}

TEST_CASE("★ ProjectIO: ma tran calibration duoc GIAI LAI, khong luu san") {
    // Luu ca diem lan ma tran se tao ra kha nang chung lech nhau.
    // Diem la du lieu goc; ma tran suy ra duoc.
    const Project a = makeRichProject();
    const std::string text = projectio::toJson(a);

    // File KHONG duoc chua ma tran.
    REQUIRE(text.find("\"matrix\"") == std::string::npos);
    REQUIRE(text.find("\"pairs\"") != std::string::npos);

    Project b;
    REQUIRE(projectio::fromJson(text, b).ok);
    REQUIRE(b.calibrations[0].isValid());     // van dung duoc ngay
}

TEST_CASE("★ ProjectIO: KHONG luu trang thai dang phat") {
    // Mo lai project ma clip tu phat tiep tu giua chung la hanh vi
    // gay bat ngo cho nguoi van hanh.
    Project a = makeRichProject();
    a.composition.triggerClip(1, 2);
    REQUIRE(a.composition.playingClip(1) != nullptr);

    Project b;
    REQUIRE(projectio::fromJson(projectio::toJson(a), b).ok);

    for (int i = 0; i < b.composition.layerCount(); ++i) {
        CHECK(!b.composition.layer(i).isPlayingSomething());
    }
}

TEST_CASE("★ ProjectIO: JSON hong -> bao loi, KHONG sap") {
    Project b;
    const LoadResult r = projectio::fromJson("{ day khong phai json", b);
    REQUIRE(!r.ok);
    REQUIRE(!r.error.empty());
}

TEST_CASE("★ ProjectIO: file THIEU truong van nap duoc voi mac dinh") {
    // File cua ban cu, hoac nguoi dung sua tay va xoa nham.
    Project b;
    const LoadResult r = projectio::fromJson("{\"name\":\"Toi gian\"}", b);
    REQUIRE(r.ok);
    CHECK(b.name == "Toi gian");
    CHECK_NEAR(b.composition.canvasSize.x, 1920.0, 1e-9);   // mac dinh
    CHECK(b.screens.empty());
}

TEST_CASE("★ ProjectIO: file cua ban MOI HON -> canh bao chu khong tu choi") {
    Project b;
    const LoadResult r = projectio::fromJson(
        "{\"formatVersion\": 999, \"name\":\"Tuong lai\"}", b);
    REQUIRE(r.ok);                        // van nap duoc
    REQUIRE(!r.warnings.empty());         // nhung co canh bao
}

TEST_CASE("★ ProjectIO: warp suy bien trong file -> canh bao va dat lai") {
    // File bi sua tay hong. Phai cuu duoc, khong duoc de slice roi vao
    // trang thai khong nghich dao duoc.
    const std::string text = R"({
      "screens": [{
        "name": "S",
        "slices": [{
          "name": "hong",
          "warp": { "type": "CornerPin",
                    "corners": [[0,0],[100,0],[200,0],[300,0]] }
        }]
      }]
    })";

    Project b;
    const LoadResult r = projectio::fromJson(text, b);
    REQUIRE(r.ok);
    REQUIRE(!r.warnings.empty());
    REQUIRE(b.screens.size() == 1u);
    REQUIRE(b.screens[0].slices[0].isUsable());   // ★ da duoc cuu
}

TEST_CASE("ProjectIO: luoi mesh sai so diem -> canh bao, dung luoi mac dinh") {
    const std::string text = R"({
      "screens": [{ "slices": [{
        "warp": { "type": "Mesh", "cols": 4, "rows": 4, "points": [[0,0],[1,1]] }
      }]}]
    })";

    Project b;
    const LoadResult r = projectio::fromJson(text, b);
    REQUIRE(r.ok);
    REQUIRE(!r.warnings.empty());
    REQUIRE(b.screens[0].slices[0].warp()->type() == WarpType::Mesh);
}

TEST_CASE("ProjectIO: loai warp la -> canh bao va thay bang CornerPin") {
    Project b;
    const LoadResult r = projectio::fromJson(
        R"({"screens":[{"slices":[{"warp":{"type":"KhongTonTai"}}]}]})", b);
    REQUIRE(r.ok);
    REQUIRE(!r.warnings.empty());
    REQUIRE(b.screens[0].slices[0].warp()->type() == WarpType::CornerPin);
}

// ═══════════════════════════════════════════════════════════════════════
//  Ghi / doc file that
// ═══════════════════════════════════════════════════════════════════════

TEST_CASE("★★ ProjectIO: ghi ra file va nap lai duoc") {
    const std::string path = "mikmap_test_project.mikmap";
    const Project a = makeRichProject();

    std::string err;
    REQUIRE(projectio::save(path, a, err));
    REQUIRE(err.empty());

    Project b;
    const LoadResult r = projectio::load(path, b);
    REQUIRE(r.ok);
    CHECK(b.name == "Show Tết 2026");
    CHECK(b.screens.size() == 2u);
    CHECK(b.calibrations[0].isValid());

    std::remove(path.c_str());
    std::remove((path + ".tmp").c_str());
}

TEST_CASE("ProjectIO: nap file khong ton tai -> bao loi ro rang") {
    Project b;
    const LoadResult r = projectio::load("khong_ton_tai_12345.mikmap", b);
    REQUIRE(!r.ok);
    REQUIRE(r.error.find("Khong mo duoc file") != std::string::npos);
}

TEST_CASE("★ ProjectIO: ghi de len project cu van an toan") {
    const std::string path = "mikmap_test_overwrite.mikmap";
    Project a = makeRichProject();

    std::string err;
    REQUIRE(projectio::save(path, a, err));

    a.name = "Ban thu hai";
    REQUIRE(projectio::save(path, a, err));

    Project b;
    REQUIRE(projectio::load(path, b).ok);
    CHECK(b.name == "Ban thu hai");

    std::remove(path.c_str());
    std::remove((path + ".tmp").c_str());
}

// ═══════════════════════════════════════════════════════════════════════
//  F10 — warp Bezier phải sống sót qua lưu / nạp
// ═══════════════════════════════════════════════════════════════════════

// ★ Khong co test nay thi F10 la mot tinh nang BAY: nguoi van hanh uon
//   mat cong cho khop cot tron mat nua tieng, bam Luu, mo lai va thay
//   moi thu ve hinh chu nhat. Loi kieu do khong lam sap gi ca — no chi
//   lang le nuot cong viec.
TEST_CASE("★ F10: warp Bezier song sot qua luu / nap") {
    const std::string path = "mikmap_test_bezier.mikmap";

    Project a;
    a.composition = Composition(1, 1, 1);
    Screen sc(0, "Projector 1", Vec2{1920.0, 1080.0});
    sc.addFullScreenSlice(a.composition.canvasSize);
    sc.slices[0].convertWarp(WarpType::Bezier);

    auto* b0 = static_cast<WarpBezier*>(sc.slices[0].warp());
    for (int cy = 0; cy < WarpBezier::kDim; ++cy) {
        for (int cx = 0; cx < WarpBezier::kDim; ++cx) {
            const Vec2 p = b0->controlPoint(cx, cy);
            b0->setControlPoint(cx, cy, Vec2{p.x + cx * 11.0, p.y - cy * 7.0});
        }
    }
    a.screens.push_back(std::move(sc));

    std::string err;
    REQUIRE(projectio::save(path, a, err));

    Project b;
    const LoadResult r = projectio::load(path, b);
    REQUIRE(r.ok);
    REQUIRE(b.screens.size() == 1u);
    REQUIRE(b.screens[0].sliceCount() == 1);

    const IWarp* w = b.screens[0].slices[0].warp();
    REQUIRE(w != nullptr);
    REQUIRE(w->type() == WarpType::Bezier);
    REQUIRE(w->controlPointCount() == WarpBezier::kPointCount);

    for (int i = 0; i < WarpBezier::kPointCount; ++i) {
        const Vec2 want = b0->controlPointAt(i);
        const Vec2 got  = w->controlPointAt(i);
        CHECK_NEAR(got.x, want.x, 1e-9);
        CHECK_NEAR(got.y, want.y, 1e-9);
    }

    std::remove(path.c_str());
    std::remove((path + ".tmp").c_str());
}

// File sua tay thieu diem: phai canh bao va dung mat phang mac dinh,
// KHONG duoc nhet bua so diem co duoc thanh mot hinh dang rac.
TEST_CASE("F10: Bezier thieu diem -> canh bao, dung mat phang mac dinh") {
    const char* text = R"({
      "version": 1,
      "screens": [{
        "name": "S", "resolution": [1920, 1080],
        "slices": [{
          "name": "sl",
          "warp": { "type": "Bezier", "points": [[0,0],[1,0],[2,0]] }
        }]
      }]
    })";

    Project b;
    const LoadResult r = projectio::fromJson(text, b);
    REQUIRE(r.ok);
    REQUIRE(!r.warnings.empty());
    REQUIRE(b.screens.size() == 1u);

    const IWarp* w = b.screens[0].slices[0].warp();
    REQUIRE(w->type() == WarpType::Bezier);
    CHECK(w->isInvertible());       // ★ van dung duoc, khong phai hinh rac
}

#include "core/model/ProjectIO.h"

#include "core/model/WarpCornerPin.h"
#include "core/model/WarpMesh.h"
#include "core/util/Json.h"

#include <cstdio>
#include <fstream>
#include <sstream>

namespace hexmap {
namespace projectio {
namespace {

// ── Vec2 ↔ [x, y] ──────────────────────────────────────────────────────
JsonValue vecToJson(const Vec2& v) {
    JsonValue a = JsonValue::array();
    a.push(JsonValue(v.x));
    a.push(JsonValue(v.y));
    return a;
}

Vec2 jsonToVec(const JsonValue& j, const Vec2& def = {}) {
    if (!j.isArray() || j.size() < 2) return def;
    return Vec2{j.at(0).asNumber(def.x), j.at(1).asNumber(def.y)};
}

// ── Warp ───────────────────────────────────────────────────────────────
const char* warpTypeName(WarpType t) {
    switch (t) {
    case WarpType::CornerPin: return "CornerPin";
    case WarpType::Mesh:      return "Mesh";
    case WarpType::Bezier:    return "Bezier";
    default:                  return "CornerPin";
    }
}

JsonValue warpToJson(const IWarp* w) {
    JsonValue o = JsonValue::object();
    if (w == nullptr) {
        o.set("type", JsonValue("CornerPin"));
        return o;
    }

    o.set("type", JsonValue(warpTypeName(w->type())));

    if (w->type() == WarpType::CornerPin) {
        const auto* cp = static_cast<const WarpCornerPin*>(w);
        JsonValue corners = JsonValue::array();
        for (int i = 0; i < 4; ++i) corners.push(vecToJson(cp->corner(i)));
        o.set("corners", std::move(corners));

    } else if (w->type() == WarpType::Mesh) {
        const auto* m = static_cast<const WarpMesh*>(w);
        o.set("cols", JsonValue(m->cols()));
        o.set("rows", JsonValue(m->rows()));

        JsonValue pts = JsonValue::array();
        for (int cy = 0; cy <= m->rows(); ++cy) {
            for (int cx = 0; cx <= m->cols(); ++cx) {
                pts.push(vecToJson(m->controlPoint(cx, cy)));
            }
        }
        o.set("points", std::move(pts));
    }

    return o;
}

std::unique_ptr<IWarp> warpFromJson(const JsonValue& j, std::vector<std::string>& warn) {
    const std::string type = j["type"].asString("CornerPin");

    if (type == "Mesh") {
        const int cols = j["cols"].asInt(4);
        const int rows = j["rows"].asInt(4);
        auto m = std::make_unique<WarpMesh>(cols, rows,
                                            Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0});

        const JsonValue& pts = j["points"];
        const size_t expected = static_cast<size_t>(cols + 1) * static_cast<size_t>(rows + 1);
        if (pts.size() != expected) {
            warn.push_back("Mesh warp: so diem dieu khien khong khop ("
                           + std::to_string(pts.size()) + " thay vi "
                           + std::to_string(expected) + "), dung luoi mac dinh");
            return m;
        }

        size_t i = 0;
        for (int cy = 0; cy <= rows; ++cy) {
            for (int cx = 0; cx <= cols; ++cx) {
                m->setControlPoint(cx, cy, jsonToVec(pts.at(i++)));
            }
        }
        return m;
    }

    if (type != "CornerPin") {
        warn.push_back("Loai warp chua ho tro: '" + type + "', thay bang CornerPin");
    }

    auto cp = std::make_unique<WarpCornerPin>();
    const JsonValue& corners = j["corners"];
    if (corners.size() == 4) {
        Vec2 c[4];
        for (int i = 0; i < 4; ++i) c[i] = jsonToVec(corners.at(static_cast<size_t>(i)));
        cp->setCorners(c);
        if (!cp->isInvertible()) {
            warn.push_back("Corner pin trong file bi suy bien, dat lai hinh chu nhat");
            cp->resetToRect(Vec2{0.0, 0.0}, Vec2{1920.0, 1080.0});
        }
    }
    return cp;
}

// ── Transform2D ────────────────────────────────────────────────────────
JsonValue transformToJson(const Transform2D& t) {
    JsonValue o = JsonValue::object();
    o.set("position", vecToJson(t.position));
    o.set("scale",    vecToJson(t.scale));
    o.set("rotation", JsonValue(t.rotation));
    o.set("anchor",   vecToJson(t.anchor));
    o.set("flipH",    JsonValue(t.flipH));
    o.set("flipV",    JsonValue(t.flipV));
    return o;
}

Transform2D transformFromJson(const JsonValue& j) {
    Transform2D t;
    t.position = jsonToVec(j["position"], t.position);
    t.scale    = jsonToVec(j["scale"],    t.scale);
    t.rotation = j["rotation"].asNumber(0.0);
    t.anchor   = jsonToVec(j["anchor"],   t.anchor);
    t.flipH    = j["flipH"].asBool(false);
    t.flipV    = j["flipV"].asBool(false);
    return t;
}

// ── Transport ──────────────────────────────────────────────────────────
const char* directionName(PlayDirection d) {
    switch (d) {
    case PlayDirection::Reverse:  return "Reverse";
    case PlayDirection::PingPong: return "PingPong";
    default:                      return "Forward";
    }
}
PlayDirection directionFromName(const std::string& s) {
    if (s == "Reverse")  return PlayDirection::Reverse;
    if (s == "PingPong") return PlayDirection::PingPong;
    return PlayDirection::Forward;
}

const char* endActionName(EndAction a) {
    switch (a) {
    case EndAction::Stop:      return "Stop";
    case EndAction::HoldLast:  return "HoldLast";
    case EndAction::PlayNext:  return "PlayNext";
    case EndAction::Random:    return "Random";
    default:                   return "Loop";
    }
}
EndAction endActionFromName(const std::string& s) {
    if (s == "Stop")     return EndAction::Stop;
    if (s == "HoldLast") return EndAction::HoldLast;
    if (s == "PlayNext") return EndAction::PlayNext;
    if (s == "Random")   return EndAction::Random;
    return EndAction::Loop;
}

JsonValue transportToJson(const Transport& t) {
    JsonValue o = JsonValue::object();
    o.set("direction",    JsonValue(directionName(t.direction)));
    o.set("endAction",    JsonValue(endActionName(t.endAction)));
    o.set("speed",        JsonValue(t.speed));
    o.set("inPoint",      JsonValue(t.inPoint));
    o.set("outPoint",     JsonValue(t.outPoint));
    // KHÔNG lưu `state` và `position`: đó là trạng thái lúc chạy, không
    // phải cấu hình. Mở lại project mà clip tự phát tiếp từ giữa chừng
    // là hành vi gây bất ngờ.
    return o;
}

Transport transportFromJson(const JsonValue& j) {
    Transport t;
    t.direction = directionFromName(j["direction"].asString("Forward"));
    t.endAction = endActionFromName(j["endAction"].asString("Loop"));
    t.speed     = j["speed"].asNumber(1.0);
    t.setTrim(j["inPoint"].asNumber(0.0), j["outPoint"].asNumber(1.0));
    return t;
}

// ── Clip ───────────────────────────────────────────────────────────────
const char* mediaTypeName(MediaType t) {
    switch (t) {
    case MediaType::Video:     return "Video";
    case MediaType::Image:     return "Image";
    case MediaType::Generator: return "Generator";
    default:                   return "Empty";
    }
}
MediaType mediaTypeFromName(const std::string& s) {
    if (s == "Video")     return MediaType::Video;
    if (s == "Image")     return MediaType::Image;
    if (s == "Generator") return MediaType::Generator;
    return MediaType::Empty;
}

JsonValue clipToJson(const Clip& c) {
    JsonValue o = JsonValue::object();
    if (c.isEmpty()) return o;      // ô trống ghi thành {} — file gọn hơn nhiều

    o.set("name", JsonValue(c.name));

    JsonValue media = JsonValue::object();
    media.set("type",     JsonValue(mediaTypeName(c.media.type)));
    media.set("path",     JsonValue(c.media.path));
    media.set("size",     vecToJson(c.media.size));
    media.set("duration", JsonValue(c.media.durationSec));
    o.set("media", std::move(media));

    o.set("transport",    transportToJson(c.transport));
    o.set("transform",    transformToJson(c.transform));
    o.set("blend",        JsonValue(blendModeName(c.blend)));
    o.set("opacity",      JsonValue(c.opacity));
    o.set("triggerStyle", JsonValue(c.triggerStyle == TriggerStyle::Piano ? "Piano" : "Toggle"));
    o.set("colorTag",     JsonValue(static_cast<double>(c.colorTag)));
    return o;
}

Clip clipFromJson(const JsonValue& j) {
    Clip c;
    if (!j.isObject() || j.objectItems().empty()) return c;   // ô trống

    c.name = j["name"].asString();

    const JsonValue& m = j["media"];
    c.media.type        = mediaTypeFromName(m["type"].asString("Empty"));
    c.media.path        = m["path"].asString();
    c.media.size        = jsonToVec(m["size"]);
    c.media.durationSec = m["duration"].asNumber(0.0);

    c.transport = transportFromJson(j["transport"]);
    c.transport.durationSec = c.media.durationSec;
    c.transform = transformFromJson(j["transform"]);
    c.blend     = blendModeFromName(j["blend"].asString("Normal").c_str());
    c.opacity   = j["opacity"].asNumber(1.0);
    c.triggerStyle = (j["triggerStyle"].asString("Toggle") == "Piano")
                     ? TriggerStyle::Piano : TriggerStyle::Toggle;
    c.colorTag  = static_cast<uint32_t>(j["colorTag"].asNumber(0.0));
    return c;
}

// ── Slice / Screen ─────────────────────────────────────────────────────
JsonValue sliceToJson(const Slice& s) {
    JsonValue o = JsonValue::object();
    o.set("name",        JsonValue(s.name));
    o.set("enabled",     JsonValue(s.enabled));
    o.set("solo",        JsonValue(s.solo));
    o.set("inputOrigin", vecToJson(s.inputOrigin));
    o.set("inputSize",   vecToJson(s.inputSize));
    o.set("warp",        warpToJson(s.warp()));
    return o;
}

Slice sliceFromJson(const JsonValue& j, std::vector<std::string>& warn) {
    Slice s;
    s.name        = j["name"].asString("Slice");
    s.enabled     = j["enabled"].asBool(true);
    s.solo        = j["solo"].asBool(false);
    s.inputOrigin = jsonToVec(j["inputOrigin"], Vec2{0.0, 0.0});
    s.inputSize   = jsonToVec(j["inputSize"],   Vec2{1920.0, 1080.0});
    s.setWarp(warpFromJson(j["warp"], warn));
    return s;
}

const char* outputTypeName(ScreenOutputType t) {
    switch (t) {
    case ScreenOutputType::Virtual: return "Virtual";
    case ScreenOutputType::Spout:   return "Spout";
    case ScreenOutputType::NDI:     return "NDI";
    default:                        return "Display";
    }
}
ScreenOutputType outputTypeFromName(const std::string& s) {
    if (s == "Virtual") return ScreenOutputType::Virtual;
    if (s == "Spout")   return ScreenOutputType::Spout;
    if (s == "NDI")     return ScreenOutputType::NDI;
    return ScreenOutputType::Display;
}

JsonValue screenToJson(const Screen& sc) {
    JsonValue o = JsonValue::object();
    o.set("id",           JsonValue(sc.id));
    o.set("name",         JsonValue(sc.name));
    o.set("resolution",   vecToJson(sc.resolution));
    o.set("enabled",      JsonValue(sc.enabled));
    o.set("outputType",   JsonValue(outputTypeName(sc.outputType)));
    o.set("displayIndex", JsonValue(sc.displayIndex));

    JsonValue arr = JsonValue::array();
    for (const Slice& s : sc.slices) arr.push(sliceToJson(s));
    o.set("slices", std::move(arr));
    return o;
}

Screen screenFromJson(const JsonValue& j, std::vector<std::string>& warn) {
    Screen sc;
    sc.id           = j["id"].asInt(0);
    sc.name         = j["name"].asString("Screen");
    sc.resolution   = jsonToVec(j["resolution"], Vec2{1920.0, 1080.0});
    sc.enabled      = j["enabled"].asBool(true);
    sc.outputType   = outputTypeFromName(j["outputType"].asString("Display"));
    sc.displayIndex = j["displayIndex"].asInt(-1);

    const JsonValue& arr = j["slices"];
    for (size_t i = 0; i < arr.size(); ++i) {
        sc.slices.push_back(sliceFromJson(arr.at(i), warn));
    }
    return sc;
}

// ── Calibration ────────────────────────────────────────────────────────
JsonValue calibToJson(const CalibrationProfile& c) {
    JsonValue o = JsonValue::object();
    o.set("name",           JsonValue(c.name));
    o.set("sourceId",       JsonValue(static_cast<double>(c.sourceId)));
    o.set("targetScreenId", JsonValue(c.targetScreenId));
    o.set("method",         JsonValue(c.method == SolveMethod::Ransac ? "Ransac" : "LeastSquares"));
    o.set("inlierThreshold", JsonValue(c.ransacParams.inlierThreshold));

    // Lưu CÁC CẶP ĐIỂM, không lưu ma trận. Ma trận suy ra được từ điểm;
    // lưu cả hai sẽ tạo ra khả năng chúng lệch nhau. Điểm mới là dữ liệu
    // gốc — và nhờ giữ chúng, người vận hành sửa được một điểm xấu mà
    // không phải chạm lại từ đầu.
    JsonValue pairs = JsonValue::array();
    for (const CorrespondencePair& p : c.pairs()) {
        JsonValue po = JsonValue::object();
        po.set("sensor",  vecToJson(p.src));
        po.set("output",  vecToJson(p.dst));
        po.set("enabled", JsonValue(p.enabled));
        pairs.push(std::move(po));
    }
    o.set("pairs", std::move(pairs));
    return o;
}

CalibrationProfile calibFromJson(const JsonValue& j, std::vector<std::string>& warn) {
    CalibrationProfile c;
    c.name           = j["name"].asString("Calibration");
    c.sourceId       = static_cast<uint16_t>(j["sourceId"].asInt(0));
    c.targetScreenId = j["targetScreenId"].asInt(0);
    c.method = (j["method"].asString("Ransac") == "LeastSquares")
               ? SolveMethod::LeastSquares : SolveMethod::Ransac;
    c.ransacParams.inlierThreshold = j["inlierThreshold"].asNumber(3.0);

    const JsonValue& pairs = j["pairs"];
    for (size_t i = 0; i < pairs.size(); ++i) {
        const JsonValue& p = pairs.at(i);
        c.addPair(jsonToVec(p["sensor"]), jsonToVec(p["output"]));
        if (!p["enabled"].asBool(true)) {
            c.setPairEnabled(c.pairCount() - 1, false);
        }
    }

    // Giải lại ngay để ma trận sẵn sàng dùng.
    if (c.enabledPairCount() >= 4) {
        const auto r = c.solve();
        if (!r.ok) {
            warn.push_back("Calibration '" + c.name + "' khong giai duoc: " + r.message);
        } else if (!c.isAccurate(3.0)) {
            warn.push_back("Calibration '" + c.name + "' co sai so "
                           + std::to_string(c.rmsError()) + " px — nen calibrate lai");
        }
    }
    return c;
}

} // namespace

// ═══════════════════════════════════════════════════════════════════════

std::string toJson(const Project& p) {
    JsonValue root = JsonValue::object();
    root.set("formatVersion", JsonValue(kProjectFormatVersion));
    root.set("name", JsonValue(p.name));

    // ── Composition ────────────────────────────────────────────────────
    JsonValue comp = JsonValue::object();
    comp.set("canvasSize",    vecToJson(p.composition.canvasSize));
    comp.set("masterOpacity", JsonValue(p.composition.masterOpacity));
    comp.set("emptyCellBehavior",
             JsonValue(p.composition.emptyCellBehavior == EmptyCellBehavior::ClearLayer
                       ? "ClearLayer" : "KeepPlaying"));
    comp.set("viewedDeck", JsonValue(p.composition.viewedDeck()));

    JsonValue layers = JsonValue::array();
    for (int i = 0; i < p.composition.layerCount(); ++i) {
        const Layer& L = p.composition.layer(i);
        JsonValue lo = JsonValue::object();
        lo.set("name",    JsonValue(L.name));
        lo.set("opacity", JsonValue(L.opacity));
        lo.set("blend",   JsonValue(blendModeName(L.blend)));
        lo.set("bypass",  JsonValue(L.bypass));
        lo.set("solo",    JsonValue(L.solo));
        lo.set("transitionDuration", JsonValue(L.transitionDuration));
        layers.push(std::move(lo));
    }
    comp.set("layers", std::move(layers));

    JsonValue decks = JsonValue::array();
    for (int d = 0; d < p.composition.deckCount(); ++d) {
        const Deck& deck = p.composition.deck(d);
        JsonValue dj = JsonValue::object();
        dj.set("name",        JsonValue(deck.name));
        dj.set("columnCount", JsonValue(deck.columnCount()));

        JsonValue grid = JsonValue::array();
        for (int L = 0; L < deck.layerCount(); ++L) {
            JsonValue row = JsonValue::array();
            for (int col = 0; col < deck.columnCount(); ++col) {
                row.push(clipToJson(deck.clip(L, col)));
            }
            grid.push(std::move(row));
        }
        dj.set("grid", std::move(grid));
        decks.push(std::move(dj));
    }
    comp.set("decks", std::move(decks));
    root.set("composition", std::move(comp));

    // ── Screens ────────────────────────────────────────────────────────
    JsonValue screens = JsonValue::array();
    for (const Screen& sc : p.screens) screens.push(screenToJson(sc));
    root.set("screens", std::move(screens));

    // ── Calibrations ───────────────────────────────────────────────────
    JsonValue calibs = JsonValue::array();
    for (const CalibrationProfile& c : p.calibrations) calibs.push(calibToJson(c));
    root.set("calibrations", std::move(calibs));

    return root.dump(2);
}

LoadResult fromJson(const std::string& text, Project& out) {
    LoadResult res;

    JsonValue root;
    std::string err;
    if (!JsonValue::parse(text, root, err)) {
        res.error = "JSON sai cu phap: " + err;
        return res;
    }
    if (!root.isObject()) {
        res.error = "File project phai la mot JSON object";
        return res;
    }

    out = Project{};
    out.name = root["name"].asString("Untitled");
    out.formatVersion = root["formatVersion"].asInt(1);

    if (out.formatVersion > kProjectFormatVersion) {
        res.warnings.push_back(
            "File duoc tao boi ban MOI HON (version " + std::to_string(out.formatVersion)
            + "). Mot so thiet lap co the bi bo qua.");
    }

    // ── Composition ────────────────────────────────────────────────────
    const JsonValue& comp = root["composition"];

    const JsonValue& decksJ  = comp["decks"];
    const JsonValue& layersJ = comp["layers"];

    const int layerCount  = static_cast<int>(layersJ.size());
    const int deckCount   = static_cast<int>(decksJ.size());
    const int columnCount = (deckCount > 0) ? decksJ.at(0)["columnCount"].asInt(8) : 8;

    out.composition = Composition(std::max(0, layerCount),
                                  std::max(0, columnCount),
                                  std::max(1, deckCount));

    out.composition.canvasSize    = jsonToVec(comp["canvasSize"], Vec2{1920.0, 1080.0});
    out.composition.masterOpacity = comp["masterOpacity"].asNumber(1.0);
    out.composition.emptyCellBehavior =
        (comp["emptyCellBehavior"].asString("KeepPlaying") == "ClearLayer")
        ? EmptyCellBehavior::ClearLayer : EmptyCellBehavior::KeepPlaying;

    for (int i = 0; i < layerCount; ++i) {
        const JsonValue& lj = layersJ.at(static_cast<size_t>(i));
        Layer& L = out.composition.layer(i);
        L.name    = lj["name"].asString("Layer " + std::to_string(i + 1));
        L.opacity = lj["opacity"].asNumber(1.0);
        L.blend   = blendModeFromName(lj["blend"].asString("Normal").c_str());
        L.bypass  = lj["bypass"].asBool(false);
        L.solo    = lj["solo"].asBool(false);
        L.transitionDuration = lj["transitionDuration"].asNumber(0.0);
    }

    for (int d = 0; d < deckCount; ++d) {
        const JsonValue& dj = decksJ.at(static_cast<size_t>(d));
        Deck& deck = out.composition.deck(d);
        deck.name = dj["name"].asString("Deck " + std::to_string(d + 1));

        const JsonValue& grid = dj["grid"];
        for (size_t L = 0; L < grid.size(); ++L) {
            const JsonValue& row = grid.at(L);
            for (size_t col = 0; col < row.size(); ++col) {
                deck.setClip(static_cast<int>(L), static_cast<int>(col),
                             clipFromJson(row.at(col)));
            }
        }
    }

    out.composition.setViewedDeck(comp["viewedDeck"].asInt(0));

    // ── Screens ────────────────────────────────────────────────────────
    const JsonValue& screensJ = root["screens"];
    for (size_t i = 0; i < screensJ.size(); ++i) {
        out.screens.push_back(screenFromJson(screensJ.at(i), res.warnings));
    }

    // ── Calibrations ───────────────────────────────────────────────────
    const JsonValue& calibsJ = root["calibrations"];
    for (size_t i = 0; i < calibsJ.size(); ++i) {
        out.calibrations.push_back(calibFromJson(calibsJ.at(i), res.warnings));
    }

    res.ok = true;
    return res;
}

bool save(const std::string& path, const Project& p, std::string& outError) {
    outError.clear();

    // Ghi ra file tạm rồi mới đổi tên. Nếu mất điện hoặc đĩa đầy giữa
    // chừng, project CŨ vẫn còn nguyên thay vì biến thành file cụt —
    // mất project giữa lúc dựng show là thảm hoạ không cứu được.
    const std::string tmp = path + ".tmp";

    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        if (!f) {
            outError = "Khong mo duoc file de ghi: " + tmp;
            return false;
        }
        const std::string text = toJson(p);
        f.write(text.data(), static_cast<std::streamsize>(text.size()));
        if (!f) {
            outError = "Loi khi ghi du lieu: " + tmp;
            return false;
        }
    }

    std::remove(path.c_str());          // Windows: rename không ghi đè được
    if (std::rename(tmp.c_str(), path.c_str()) != 0) {
        outError = "Khong doi ten duoc " + tmp + " thanh " + path;
        return false;
    }
    return true;
}

LoadResult load(const std::string& path, Project& out) {
    LoadResult res;

    std::ifstream f(path, std::ios::binary);
    if (!f) {
        res.error = "Khong mo duoc file: " + path;
        return res;
    }

    std::ostringstream ss;
    ss << f.rdbuf();
    return fromJson(ss.str(), out);
}

} // namespace projectio
} // namespace hexmap

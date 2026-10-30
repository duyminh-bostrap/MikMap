#include "io/packs/PackManifest.h"

#include <cmath>
#include <set>
#include <unordered_map>

namespace mikmap::packs {

namespace {

constexpr size_t kMaxPathLen = 240, kMaxComponentLen = 100,
                 kMaxDepChain = 64, kMaxNeeds = 32, kMaxProvides = 32;

std::string lower(std::string s) {
    for (char& c : s) if (c >= 'A' && c <= 'Z') c = static_cast<char>(c - 'A' + 'a');
    return s;
}

bool isSafeRelPathFwd(const std::string& p);

// Id là tên thư mục: ngoài bộ ký tự còn phải qua luật tên tệp (chặn "...", "foo.", "con", "nul").
bool isSafeId(const std::string& s) {
    if (s.empty() || s.size() > 64) return false;
    for (char c : s)
        if (!((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') || c == '.' || c == '_' || c == '-')) return false;
    return isSafeRelPathFwd(s);
}

// N.N.N, mỗi N 1-9 chữ số, không số 0 đứng đầu ("01" bị từ chối).
bool parseVersion(const std::string& s, uint32_t out[3]) {
    size_t i = 0;
    for (int part = 0; part < 3; ++part) {
        const size_t start = i;
        uint32_t n = 0;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') { n = n * 10 + static_cast<uint32_t>(s[i] - '0'); ++i; }
        const size_t len = i - start;
        if (len == 0 || len > 9 || (len > 1 && s[start] == '0')) return false;
        out[part] = n;
        if (part < 2) { if (i >= s.size() || s[i] != '.') return false; ++i; }
    }
    return i == s.size();
}

bool isLowerHex64(const std::string& s) {
    if (s.size() != 64) return false;
    for (char c : s) if (!((c >= '0' && c <= '9') || (c >= 'a' && c <= 'f'))) return false;
    return true;
}

bool isReservedDeviceName(const std::string& comp) {
    // Windows: "con.txt.x" cũng là CON; dấu cách/chấm cuối của stem bị bỏ ("con .x" = CON).
    std::string stem = lower(comp.substr(0, comp.find('.')));
    while (!stem.empty() && (stem.back() == ' ' || stem.back() == '.')) stem.pop_back();
    if (stem == "conin$" || stem == "conout$" || stem == "clock$") return true;
    if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul") return true;
    return stem.size() == 4 && (stem.compare(0, 3, "com") == 0 || stem.compare(0, 3, "lpt") == 0)
           && stem[3] >= '1' && stem[3] <= '9';
}

// Parse vẫn mềm (chính sách thật ở HTTP layer, Task 5) nhưng không để lọt
// "http://localhost.evil.com", userinfo ("@") hay ký tự điều khiển.
bool isSafeUrl(const std::string& u) {
    if (u.empty() || u.size() > 2048) return false;
    for (unsigned char c : u) if (c <= 0x20 || c >= 0x7F) return false;
    const bool https = u.rfind("https://", 0) == 0;
    if (!https && u.rfind("http://", 0) != 0) return false;
    const size_t hs = https ? 8 : 7;
    const size_t he = u.find_first_of("/?#", hs);
    const std::string auth = u.substr(hs, he == std::string::npos ? std::string::npos : he - hs);
    if (auth.empty() || auth.find('@') != std::string::npos) return false;
    if (https) return auth[0] != ':';
    for (const char* host : {"127.0.0.1", "localhost"}) {
        const std::string h = host;
        if (auth.compare(0, h.size(), h) != 0) continue;
        if (auth.size() == h.size()) return true;
        if (auth[h.size()] != ':') return false;
        const std::string port = auth.substr(h.size() + 1);
        if (port.empty() || port.size() > 5) return false;
        unsigned n = 0;
        for (char c : port) { if (c < '0' || c > '9') return false; n = n * 10 + static_cast<unsigned>(c - '0'); }
        return n <= 65535;
    }
    return false;
}

bool readSize(const JsonValue& v, uint64_t& out) {
    if (!v.isNumber()) return false;
    const double d = v.asNumber();
    if (!(d >= 0.0 && d <= static_cast<double>(kMaxFileBytes)) || d != std::floor(d)) return false;
    out = static_cast<uint64_t>(d);
    return true;
}

bool fail(std::string& err, const std::string& m) { err = m; return false; }

bool isSafeRelPathFwd(const std::string& p) { return isSafeRelPath(p); }

} // namespace

bool isSafeRelPath(const std::string& p) {
    if (p.empty() || p.size() > kMaxPathLen) return false;
    size_t start = 0;
    while (true) {
        const size_t slash = p.find('/', start);
        const std::string comp = p.substr(start, slash == std::string::npos ? std::string::npos : slash - start);
        // ".." ⊂ trailing-dot rule below, kept explicit as the first line of defence.
        if (comp.empty() || comp == "." || comp == ".." || comp.size() > kMaxComponentLen) return false;
        if (comp.back() == '.' || comp.back() == ' ') return false;
        if (isReservedDeviceName(comp)) return false;
        // Bí danh 8.3 của Windows ("PROGRA~1"): '~' theo sau là chữ số.
        for (size_t i = 0; i + 1 < comp.size(); ++i)
            if (comp[i] == '~' && comp[i + 1] >= '0' && comp[i + 1] <= '9') return false;
        for (unsigned char c : comp)
            // ASCII in được, trừ ký tự cấm của Windows (gồm ':' ADS/ổ đĩa và '\\'); không UTF-8 đa byte.
            if (c < 0x20 || c >= 0x7F || c == '<' || c == '>' || c == ':' || c == '"' || c == '|' ||
                c == '?' || c == '*' || c == '\\') return false;
        if (slash == std::string::npos) break;
        start = slash + 1;
    }
    return true;
}

bool parsePackInfo(const JsonValue& v, PackInfo& out, std::string& err) {
    out = PackInfo{};
    if (!v.isObject()) return fail(err, "pack: not an object");
    auto str = [&](const char* k, std::string& dst) {
        if (!v.has(k) || !v[k].isString()) return false;
        dst = v[k].asString();
        return true;
    };
    if (!str("id", out.id) || !isSafeId(out.id)) return fail(err, "pack: bad id");
    const std::string who = "pack '" + out.id + "': ";
    if (!str("name", out.name) || out.name.empty() || out.name.size() > 200) return fail(err, who + "bad name");
    uint32_t ver[3];
    if (!str("version", out.version) || !parseVersion(out.version, ver)) return fail(err, who + "bad version (need N.N.N)");

    std::string kind;
    if (!str("kind", kind)) return fail(err, who + "bad kind");
    if (kind == "runtime") out.kind = Kind::Runtime;
    else if (kind == "plugin") out.kind = Kind::Plugin;
    else if (kind == "model") out.kind = Kind::Model;
    else return fail(err, who + "unknown kind '" + kind + "'");

    uint64_t declared = 0;
    if (!v.has("sizeBytes") || !readSize(v["sizeBytes"], declared)) return fail(err, who + "bad sizeBytes");

    if (!v.has("files") || !v["files"].isArray()) return fail(err, who + "files missing");
    const auto& files = v["files"].arrayItems();
    if (files.empty() || files.size() > kMaxFilesPerPack) return fail(err, who + "file count out of range");
    std::set<std::string> seen;
    uint64_t sum = 0;   // <= 64 * 2 GiB: không tràn
    for (const JsonValue& f : files) {
        FileEntry fe;
        if (!f.isObject()) return fail(err, who + "file entry not an object");
        if (!f.has("path") || !f["path"].isString() || !isSafeRelPath(f["path"].asString()))
            return fail(err, who + "unsafe or missing file path");
        fe.path = f["path"].asString();
        if (!f.has("sha256") || !f["sha256"].isString() || !isLowerHex64(f["sha256"].asString()))
            return fail(err, who + "bad sha256 for '" + fe.path + "' (64 lowercase hex)");
        fe.sha256Hex = f["sha256"].asString();
        if (!f.has("size") || !readSize(f["size"], fe.size)) return fail(err, who + "bad size for '" + fe.path + "'");
        if (f.has("url")) {
            if (!f["url"].isString() || !isSafeUrl(f["url"].asString()))
                return fail(err, who + "bad url for '" + fe.path + "'");
            fe.url = f["url"].asString();
        }
        if (!seen.insert(lower(fe.path)).second) return fail(err, who + "duplicate path '" + fe.path + "'");
        sum += fe.size;
        out.files.push_back(std::move(fe));
    }
    // Một đường dẫn không được vừa là tệp vừa là thư mục của tệp khác.
    for (const std::string& k : seen)
        for (size_t i = k.find('/'); i != std::string::npos; i = k.find('/', i + 1))
            if (seen.count(k.substr(0, i))) return fail(err, who + "path '" + k.substr(0, i) + "' is both file and folder");
    if (sum != declared) return fail(err, who + "sizeBytes does not equal sum of file sizes");
    out.sizeBytes = declared;

    if (v.has("requires")) {
        if (!v["requires"].isArray() || v["requires"].size() > kMaxNeeds) return fail(err, who + "bad requires");
        std::set<std::string> ids;
        for (const JsonValue& d : v["requires"].arrayItems()) {
            Dependency dep;
            uint32_t t[3];
            if (!d.isObject() || !d.has("id") || !d["id"].isString() || !isSafeId(d["id"].asString()) ||
                !d.has("min") || !d["min"].isString() || !parseVersion(d["min"].asString(), t))
                return fail(err, who + "bad requires entry");
            dep.id = d["id"].asString();
            dep.minVersion = d["min"].asString();
            if (dep.id == out.id) return fail(err, who + "requires itself");
            if (!ids.insert(dep.id).second) return fail(err, who + "duplicate requires '" + dep.id + "'");
            out.needs.push_back(std::move(dep));
        }
    }
    if (v.has("minAppVersion")) {
        if (!v["minAppVersion"].isString() || !parseVersion(v["minAppVersion"].asString(), ver))
            return fail(err, who + "bad minAppVersion (need N.N.N)");
        out.minAppVersion = v["minAppVersion"].asString();
    }
    if (v.has("provides")) {
        if (!v["provides"].isArray() || v["provides"].size() > kMaxProvides) return fail(err, who + "bad provides");
        for (const JsonValue& p : v["provides"].arrayItems()) {
            if (!p.isString() || !isSafeId(p.asString())) return fail(err, who + "bad provides entry");
            out.provides.push_back(p.asString());
        }
    }
    return true;
}

JsonValue toJson(const PackInfo& p) {
    JsonValue v = JsonValue::object();
    v.set("id", p.id);
    v.set("name", p.name);
    v.set("version", p.version);
    v.set("kind", p.kind == Kind::Runtime ? "runtime" : (p.kind == Kind::Model ? "model" : "plugin"));
    v.set("sizeBytes", static_cast<double>(p.sizeBytes));
    if (!p.minAppVersion.empty()) v.set("minAppVersion", p.minAppVersion);
    JsonValue files = JsonValue::array();
    for (const FileEntry& fe : p.files) {
        JsonValue f = JsonValue::object();
        f.set("path", fe.path);
        f.set("sha256", fe.sha256Hex);
        f.set("size", static_cast<double>(fe.size));
        if (!fe.url.empty()) f.set("url", fe.url);
        files.push(std::move(f));
    }
    v.set("files", std::move(files));
    if (!p.needs.empty()) {
        JsonValue req = JsonValue::array();
        for (const Dependency& d : p.needs) {
            JsonValue dv = JsonValue::object();
            dv.set("id", d.id);
            dv.set("min", d.minVersion);
            req.push(std::move(dv));
        }
        v.set("requires", std::move(req));
    }
    if (!p.provides.empty()) {
        JsonValue prov = JsonValue::array();
        for (const std::string& s : p.provides) prov.push(s);
        v.set("provides", std::move(prov));
    }
    return v;
}

bool parseCatalog(const std::string& text, Catalog& out, std::string& err) {
    out = Catalog{};
    if (text.size() > kMaxCatalogBytes) return fail(err, "catalog: larger than 4 MiB");
    // Độ sâu JSON do chính JsonValue::parse giới hạn.
    JsonValue root;
    std::string jerr;
    if (!JsonValue::parse(text, root, jerr)) return fail(err, "catalog: invalid JSON: " + jerr);
    if (!root.isObject() || !root.has("packs") || !root["packs"].isArray()) return fail(err, "catalog: 'packs' array missing");
    if (root["packs"].size() > kMaxPacks) return fail(err, "catalog: too many packs");
    std::set<std::string> ids;
    for (const JsonValue& pv : root["packs"].arrayItems()) {
        PackInfo p;
        if (!parsePackInfo(pv, p, err)) return false;
        if (!ids.insert(p.id).second) return fail(err, "catalog: duplicate pack id '" + p.id + "'");
        out.packs.push_back(std::move(p));
    }
    return true;
}

int compareVersions(const std::string& a, const std::string& b) {
    uint32_t x[3] = {0, 0, 0}, y[3] = {0, 0, 0};
    if (!parseVersion(a, x)) x[0] = x[1] = x[2] = 0;
    if (!parseVersion(b, y)) y[0] = y[1] = y[2] = 0;
    for (int i = 0; i < 3; ++i) if (x[i] != y[i]) return x[i] < y[i] ? -1 : 1;
    return 0;
}

const PackInfo* findPack(const Catalog& c, const std::string& id) {
    for (const PackInfo& p : c.packs) if (p.id == id) return &p;
    return nullptr;
}

bool resolveInstallOrder(const Catalog& c, const std::string& id,
                         std::vector<std::string>& order, std::string& err) {
    order.clear();
    const PackInfo* root = findPack(c, id);
    if (!root) return fail(err, "unknown pack '" + id + "'");
    struct Frame { const PackInfo* p; size_t next; };
    std::unordered_map<std::string, int> state;   // 1 = đang duyệt, 2 = xong
    std::vector<Frame> stack{{root, 0}};
    state[id] = 1;
    auto bail = [&](const std::string& m) { order.clear(); return fail(err, m); };
    while (!stack.empty()) {
        Frame& f = stack.back();
        if (f.next < f.p->needs.size()) {
            const PackInfo* owner = f.p;
            const Dependency& d = owner->needs[f.next++];
            const PackInfo* dp = findPack(c, d.id);
            if (!dp) return bail("'" + owner->id + "' needs missing pack '" + d.id + "'");
            if (compareVersions(dp->version, d.minVersion) < 0)
                return bail("'" + owner->id + "' needs '" + d.id + "' >= " + d.minVersion + " but catalog has " + dp->version);
            const int s = state[d.id];
            if (s == 1) return bail("dependency cycle at '" + d.id + "'");
            if (s == 2) continue;
            if (stack.size() >= kMaxDepChain) return bail("dependency chain too deep");
            state[d.id] = 1;
            stack.push_back({dp, 0});   // f dangling từ đây, không dùng lại
        } else {
            state[f.p->id] = 2;
            order.push_back(f.p->id);
            stack.pop_back();
        }
    }
    return true;
}

} // namespace mikmap::packs

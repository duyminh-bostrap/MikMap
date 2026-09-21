#include "core/util/Json.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>

namespace hexmap {
namespace {

const JsonValue& nullValue() {
    static const JsonValue kNull;
    return kNull;
}

void escapeInto(const std::string& s, std::string& out) {
    out += '"';
    for (const char c : s) {
        switch (c) {
        case '"':  out += "\\\""; break;
        case '\\': out += "\\\\"; break;   // ★ đường dẫn Windows phụ thuộc dòng này
        case '\b': out += "\\b";  break;
        case '\f': out += "\\f";  break;
        case '\n': out += "\\n";  break;
        case '\r': out += "\\r";  break;
        case '\t': out += "\\t";  break;
        default:
            if (static_cast<unsigned char>(c) < 0x20) {
                char buf[8];
                std::snprintf(buf, sizeof(buf), "\\u%04x", static_cast<unsigned char>(c));
                out += buf;
            } else {
                out += c;   // UTF-8 đi thẳng qua
            }
        }
    }
    out += '"';
}

void numberInto(double v, std::string& out) {
    if (!std::isfinite(v)) {
        // NaN/Inf không hợp lệ trong JSON. Ghi 0 còn hơn tạo ra file hỏng
        // mà lần sau không nạp lại được.
        out += '0';
        return;
    }

    // Số nguyên thì ghi không có phần thập phân — file dễ đọc hơn nhiều.
    if (v == static_cast<double>(static_cast<long long>(v))
        && std::abs(v) < 1e15) {
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(v));
        out += buf;
        return;
    }

    // %.17g giữ đủ độ chính xác để round-trip double không mất bit.
    char buf[40];
    std::snprintf(buf, sizeof(buf), "%.17g", v);
    out += buf;
}

// ── Bộ phân tích đệ quy xuống ──────────────────────────────────────────
class Parser {
public:
    Parser(const std::string& t) : m_text(t) {}

    bool parseValue(JsonValue& out) {
        skipWhitespace();
        if (m_pos >= m_text.size()) return fail("het du lieu bat ngo");

        switch (m_text[m_pos]) {
        case '{': return parseObject(out);
        case '[': return parseArray(out);
        case '"': {
            std::string s;
            if (!parseString(s)) return false;
            out = JsonValue(std::move(s));
            return true;
        }
        case 't':
            if (!literal("true")) return false;
            out = JsonValue(true);
            return true;
        case 'f':
            if (!literal("false")) return false;
            out = JsonValue(false);
            return true;
        case 'n':
            if (!literal("null")) return false;
            out = JsonValue();
            return true;
        default:
            return parseNumber(out);
        }
    }

    std::string error() const { return m_error; }

    bool atEndAfterWhitespace() {
        skipWhitespace();
        return m_pos >= m_text.size();
    }

private:
    bool fail(const std::string& msg) {
        if (m_error.empty()) {
            m_error = msg + " (vi tri " + std::to_string(m_pos) + ")";
        }
        return false;
    }

    void skipWhitespace() {
        while (m_pos < m_text.size()) {
            const char c = m_text[m_pos];
            if (c == ' ' || c == '\t' || c == '\n' || c == '\r') ++m_pos;
            else break;
        }
    }

    bool literal(const char* lit) {
        const size_t n = std::string(lit).size();
        if (m_text.compare(m_pos, n, lit) != 0) return fail("tu khoa khong hop le");
        m_pos += n;
        return true;
    }

    bool parseString(std::string& out) {
        if (m_pos >= m_text.size() || m_text[m_pos] != '"') return fail("mong doi chuoi");
        ++m_pos;

        out.clear();
        while (m_pos < m_text.size()) {
            const char c = m_text[m_pos++];

            if (c == '"') return true;

            if (c != '\\') { out += c; continue; }

            if (m_pos >= m_text.size()) return fail("escape bi cat cut");
            const char e = m_text[m_pos++];
            switch (e) {
            case '"':  out += '"';  break;
            case '\\': out += '\\'; break;
            case '/':  out += '/';  break;
            case 'b':  out += '\b'; break;
            case 'f':  out += '\f'; break;
            case 'n':  out += '\n'; break;
            case 'r':  out += '\r'; break;
            case 't':  out += '\t'; break;
            case 'u': {
                if (m_pos + 4 > m_text.size()) return fail("escape \\u bi cat cut");
                const std::string hex = m_text.substr(m_pos, 4);
                m_pos += 4;
                const auto code = static_cast<unsigned>(std::strtoul(hex.c_str(), nullptr, 16));
                // Mã hoá UTF-8. Cặp thay thế (surrogate) không xử lý —
                // ta không sinh ra chúng, và tên clip tiếng Việt nằm gọn
                // trong BMP nên không cần.
                if (code < 0x80) {
                    out += static_cast<char>(code);
                } else if (code < 0x800) {
                    out += static_cast<char>(0xC0 | (code >> 6));
                    out += static_cast<char>(0x80 | (code & 0x3F));
                } else {
                    out += static_cast<char>(0xE0 | (code >> 12));
                    out += static_cast<char>(0x80 | ((code >> 6) & 0x3F));
                    out += static_cast<char>(0x80 | (code & 0x3F));
                }
                break;
            }
            default:
                return fail("escape khong hop le");
            }
        }
        return fail("chuoi khong duoc dong");
    }

    bool parseNumber(JsonValue& out) {
        const size_t start = m_pos;
        if (m_pos < m_text.size() && (m_text[m_pos] == '-' || m_text[m_pos] == '+')) ++m_pos;
        while (m_pos < m_text.size()) {
            const char c = m_text[m_pos];
            if ((c >= '0' && c <= '9') || c == '.' || c == 'e' || c == 'E'
                || c == '+' || c == '-') {
                ++m_pos;
            } else {
                break;
            }
        }
        if (m_pos == start) return fail("mong doi so");

        const std::string tok = m_text.substr(start, m_pos - start);
        char* end = nullptr;
        const double v = std::strtod(tok.c_str(), &end);
        if (end == tok.c_str()) return fail("so khong hop le");

        out = JsonValue(v);
        return true;
    }

    bool parseArray(JsonValue& out) {
        ++m_pos;                       // '['
        out = JsonValue::array();

        skipWhitespace();
        if (m_pos < m_text.size() && m_text[m_pos] == ']') { ++m_pos; return true; }

        while (true) {
            JsonValue v;
            if (!parseValue(v)) return false;
            out.push(std::move(v));

            skipWhitespace();
            if (m_pos >= m_text.size()) return fail("mang khong duoc dong");
            if (m_text[m_pos] == ',') { ++m_pos; continue; }
            if (m_text[m_pos] == ']') { ++m_pos; return true; }
            return fail("mong doi ',' hoac ']'");
        }
    }

    bool parseObject(JsonValue& out) {
        ++m_pos;                       // '{'
        out = JsonValue::object();

        skipWhitespace();
        if (m_pos < m_text.size() && m_text[m_pos] == '}') { ++m_pos; return true; }

        while (true) {
            skipWhitespace();
            std::string key;
            if (!parseString(key)) return false;

            skipWhitespace();
            if (m_pos >= m_text.size() || m_text[m_pos] != ':') return fail("mong doi ':'");
            ++m_pos;

            JsonValue v;
            if (!parseValue(v)) return false;
            out.set(key, std::move(v));

            skipWhitespace();
            if (m_pos >= m_text.size()) return fail("object khong duoc dong");
            if (m_text[m_pos] == ',') { ++m_pos; continue; }
            if (m_text[m_pos] == '}') { ++m_pos; return true; }
            return fail("mong doi ',' hoac '}'");
        }
    }

    const std::string& m_text;
    size_t m_pos = 0;
    std::string m_error;
};

} // namespace

// ═══════════════════════════════════════════════════════════════════════

bool JsonValue::asBool(bool def) const {
    if (m_type == Type::Bool)   return m_bool;
    if (m_type == Type::Number) return m_num != 0.0;
    return def;
}

double JsonValue::asNumber(double def) const {
    return (m_type == Type::Number) ? m_num : def;
}

int JsonValue::asInt(int def) const {
    return (m_type == Type::Number) ? static_cast<int>(m_num) : def;
}

std::string JsonValue::asString(const std::string& def) const {
    return (m_type == Type::String) ? m_str : def;
}

bool JsonValue::has(const std::string& key) const {
    return m_type == Type::Object && m_obj.find(key) != m_obj.end();
}

const JsonValue& JsonValue::operator[](const std::string& key) const {
    if (m_type != Type::Object) return nullValue();
    const auto it = m_obj.find(key);
    return (it == m_obj.end()) ? nullValue() : it->second;
}

JsonValue& JsonValue::operator[](const std::string& key) {
    if (m_type != Type::Object) { m_type = Type::Object; m_obj.clear(); }
    return m_obj[key];
}

void JsonValue::set(const std::string& key, JsonValue v) {
    if (m_type != Type::Object) { m_type = Type::Object; m_obj.clear(); }
    m_obj[key] = std::move(v);
}

size_t JsonValue::size() const {
    if (m_type == Type::Array)  return m_arr.size();
    if (m_type == Type::Object) return m_obj.size();
    return 0;
}

const JsonValue& JsonValue::at(size_t i) const {
    if (m_type != Type::Array || i >= m_arr.size()) return nullValue();
    return m_arr[i];
}

void JsonValue::push(JsonValue v) {
    if (m_type != Type::Array) { m_type = Type::Array; m_arr.clear(); }
    m_arr.push_back(std::move(v));
}

void JsonValue::dumpTo(std::string& out, int indent, int depth) const {
    const bool pretty = indent > 0;
    const std::string pad    = pretty ? std::string(static_cast<size_t>(indent * (depth + 1)), ' ') : "";
    const std::string padEnd = pretty ? std::string(static_cast<size_t>(indent * depth), ' ') : "";
    const char* nl = pretty ? "\n" : "";

    switch (m_type) {
    case Type::Null:   out += "null"; break;
    case Type::Bool:   out += (m_bool ? "true" : "false"); break;
    case Type::Number: numberInto(m_num, out); break;
    case Type::String: escapeInto(m_str, out); break;

    case Type::Array:
        if (m_arr.empty()) { out += "[]"; break; }
        out += '['; out += nl;
        for (size_t i = 0; i < m_arr.size(); ++i) {
            out += pad;
            m_arr[i].dumpTo(out, indent, depth + 1);
            if (i + 1 < m_arr.size()) out += ',';
            out += nl;
        }
        out += padEnd; out += ']';
        break;

    case Type::Object:
        if (m_obj.empty()) { out += "{}"; break; }
        out += '{'; out += nl;
        {
            size_t i = 0;
            for (const auto& kv : m_obj) {
                out += pad;
                escapeInto(kv.first, out);
                out += pretty ? ": " : ":";
                kv.second.dumpTo(out, indent, depth + 1);
                if (++i < m_obj.size()) out += ',';
                out += nl;
            }
        }
        out += padEnd; out += '}';
        break;
    }
}

std::string JsonValue::dump(int indent) const {
    std::string out;
    out.reserve(1024);
    dumpTo(out, indent, 0);
    return out;
}

bool JsonValue::parse(const std::string& text, JsonValue& outValue, std::string& outError) {
    outError.clear();

    Parser p(text);
    if (!p.parseValue(outValue)) {
        outError = p.error();
        return false;
    }

    // Rác phía sau nghĩa là file hỏng — báo lỗi thay vì âm thầm bỏ qua.
    if (!p.atEndAfterWhitespace()) {
        outError = "con du lieu thua sau gia tri JSON";
        return false;
    }
    return true;
}

} // namespace hexmap

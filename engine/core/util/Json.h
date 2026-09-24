// ════════════════════════════════════════════════════════════════════════
//  core/util/Json.h — JSON tối giản, chỉ phụ thuộc STL
//
//  ── Vì sao tự viết thay vì dùng nlohmann/json ────────────────────────
//  Ràng buộc kiến trúc: core/ chỉ phụ thuộc STL, để build và test được
//  bằng CMake thuần trên máy trắng, không cần vcpkg. File project là
//  cấu trúc phẳng, không cần tính năng cao siêu — vài trăm dòng là đủ.
//
//  ── Yêu cầu bắt buộc ─────────────────────────────────────────────────
//  · Xuất có THỤT LỀ: file .mikmap là thứ người ta sẽ mở ra đọc và sửa
//    tay khi cần cứu một show. JSON một dòng thì vô dụng cho việc đó.
//  · Escape đúng dấu \ : đường dẫn Windows "D:\media\clip.mov" mà không
//    escape sẽ tạo ra JSON hỏng, và project không nạp lại được.
//  · Không ném ngoại lệ: lỗi trả về qua cờ, để nạp file hỏng không làm
//    sập ứng dụng giữa show.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <map>
#include <string>
#include <vector>

namespace mikmap {

class JsonValue {
public:
    enum class Type { Null, Bool, Number, String, Array, Object };

    JsonValue() = default;
    JsonValue(bool b)                : m_type(Type::Bool), m_bool(b) {}
    JsonValue(double n)              : m_type(Type::Number), m_num(n) {}
    JsonValue(int n)                 : m_type(Type::Number), m_num(static_cast<double>(n)) {}
    JsonValue(const char* s)         : m_type(Type::String), m_str(s ? s : "") {}
    JsonValue(std::string s)         : m_type(Type::String), m_str(std::move(s)) {}

    static JsonValue array()  { JsonValue v; v.m_type = Type::Array;  return v; }
    static JsonValue object() { JsonValue v; v.m_type = Type::Object; return v; }

    Type type() const { return m_type; }
    bool isNull()   const { return m_type == Type::Null; }
    bool isBool()   const { return m_type == Type::Bool; }
    bool isNumber() const { return m_type == Type::Number; }
    bool isString() const { return m_type == Type::String; }
    bool isArray()  const { return m_type == Type::Array; }
    bool isObject() const { return m_type == Type::Object; }

    // ── Đọc có giá trị mặc định ────────────────────────────────────────
    // Luôn dùng dạng này khi nạp project: file của bản cũ sẽ thiếu trường
    // mới, và thiếu một trường không đáng để làm hỏng cả project.
    bool        asBool(bool def = false) const;
    double      asNumber(double def = 0.0) const;
    int         asInt(int def = 0) const;
    std::string asString(const std::string& def = {}) const;

    // ── Truy cập object ────────────────────────────────────────────────
    bool has(const std::string& key) const;
    const JsonValue& operator[](const std::string& key) const;
    JsonValue& operator[](const std::string& key);
    void set(const std::string& key, JsonValue v);
    const std::map<std::string, JsonValue>& objectItems() const { return m_obj; }

    // ── Truy cập array ─────────────────────────────────────────────────
    size_t size() const;
    const JsonValue& at(size_t i) const;
    void push(JsonValue v);
    const std::vector<JsonValue>& arrayItems() const { return m_arr; }

    // ── Chuỗi hoá ──────────────────────────────────────────────────────
    /// @param indent số khoảng trắng mỗi cấp; 0 = một dòng (chỉ dùng khi
    ///        ghi log, không dùng cho file project)
    std::string dump(int indent = 2) const;

    // ── Phân tích ──────────────────────────────────────────────────────
    /// @param outError mô tả lỗi kèm vị trí, để hiện lên UI
    /// @return false nếu chuỗi không hợp lệ; outValue khi đó không dùng được
    static bool parse(const std::string& text, JsonValue& outValue,
                      std::string& outError);

private:
    void dumpTo(std::string& out, int indent, int depth) const;

    Type m_type = Type::Null;
    bool m_bool = false;
    double m_num = 0.0;
    std::string m_str;
    std::vector<JsonValue> m_arr;
    std::map<std::string, JsonValue> m_obj;
};

} // namespace mikmap

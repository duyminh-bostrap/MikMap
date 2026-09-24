// ════════════════════════════════════════════════════════════════════════
//  tests/test_localization.cpp — đối chiếu hai bảng ngôn ngữ
//
//  ── Vì sao cần test này ──────────────────────────────────────────────
//  Trước khi có i18n, mọi chuỗi định dạng là hằng số viết thẳng trong mã
//  và MSVC kiểm tra được "%d" có khớp với đối số hay không ngay lúc biên
//  dịch. Sau khi chuyển sang TR("key"), chuỗi được tra cứu LÚC CHẠY, nên
//  trình biên dịch không còn thấy nó nữa.
//
//  Hệ quả: một bản dịch viết nhầm "%s" ở chỗ đáng lẽ là "%d" sẽ khiến
//  ImGui::Text đọc một con trỏ rác từ stack. Và lỗi đó chỉ nổ khi người
//  dùng ĐỔI SANG ngôn ngữ đó — nghĩa là gần như không bao giờ xuất hiện
//  trong lúc phát triển, chỉ xuất hiện ở buổi diễn.
//
//  Test này đưa việc kiểm tra ấy trở lại thời điểm build.
// ════════════════════════════════════════════════════════════════════════
#include "TestHarness.h"

#include "i18n/Localization.h"

#include <map>
#include <string>
#include <vector>

using namespace mikmap;

namespace {

/// Rút chuỗi specifier printf khỏi một câu, ví dụ "Ghi %d điểm (%.2f)"
/// cho ra {"d", "f"}. "%%" là ký tự phần trăm nghĩa đen, bỏ qua.
std::vector<std::string> specifiers(const std::string& s) {
    std::vector<std::string> out;
    for (size_t i = 0; i < s.size(); ++i) {
        if (s[i] != '%') continue;
        if (i + 1 < s.size() && s[i + 1] == '%') { ++i; continue; }

        ++i;
        // Bỏ qua cờ, độ rộng, độ chính xác — chúng không đổi kiểu đối số.
        while (i < s.size() && (s[i] == '-' || s[i] == '+' || s[i] == ' '
                                || s[i] == '#' || s[i] == '0')) ++i;
        while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        if (i < s.size() && s[i] == '.') {
            ++i;
            while (i < s.size() && s[i] >= '0' && s[i] <= '9') ++i;
        }

        // Bổ từ độ dài (l, ll, h, hh, z) CÓ đổi kiểu đối số nên phải giữ.
        std::string spec;
        while (i < s.size() && (s[i] == 'l' || s[i] == 'h' || s[i] == 'z'
                                || s[i] == 'j' || s[i] == 't' || s[i] == 'L')) {
            spec += s[i];
            ++i;
        }
        if (i < s.size()) spec += s[i];
        out.push_back(spec);
    }
    return out;
}

std::map<std::string, std::string> asMap(Language l) {
    std::map<std::string, std::string> m;
    for (const auto& kv : i18n::dumpTable(l)) m[kv.first] = kv.second;
    return m;
}

} // namespace

TEST_CASE("localization_bang_khong_rong") {
    CHECK(i18n::dumpTable(Language::Vietnamese).size() > 100);
    CHECK(i18n::dumpTable(Language::English).size() > 100);
}

TEST_CASE("localization_hai_bang_cung_bo_khoa") {
    const auto vi = asMap(Language::Vietnamese);
    const auto en = asMap(Language::English);

    // Thiếu khoá ở một bên = chỗ đó hiện ra chính tên khoá trên màn hình.
    for (const auto& kv : vi) {
        CHECK_MSG(en.count(kv.first) == 1,
                  "khoa chi co o ban tieng Viet: " + kv.first);
    }
    for (const auto& kv : en) {
        CHECK_MSG(vi.count(kv.first) == 1,
                  "khoa chi co o ban tieng Anh: " + kv.first);
    }
    CHECK(vi.size() == en.size());
}

TEST_CASE("localization_specifier_khop_nhau") {
    const auto vi = asMap(Language::Vietnamese);
    const auto en = asMap(Language::English);

    for (const auto& kv : vi) {
        const auto it = en.find(kv.first);
        if (it == en.end()) continue;   // đã báo ở test trên

        const std::vector<std::string> a = specifiers(kv.second);
        const std::vector<std::string> b = specifiers(it->second);

        CHECK_MSG(a.size() == b.size(),
                  "so luong specifier khac nhau o khoa: " + kv.first);
        if (a.size() != b.size()) continue;

        for (size_t i = 0; i < a.size(); ++i) {
            CHECK_MSG(a[i] == b[i],
                      "specifier khac nhau o khoa " + kv.first + ": "
                      "vi='%" + a[i] + "' en='%" + b[i] + "'");
        }
    }
}

TEST_CASE("localization_thieu_khoa_tra_ve_chinh_khoa") {
    i18n::setLanguage(Language::Vietnamese);

    // Khoá không tồn tại phải hiện nguyên văn để nhìn thấy được trên màn
    // hình, thay vì rơi về chuỗi rỗng (lọt qua kiểm tra) hoặc nullptr.
    const char* r = i18n::t("khoa.khong.ton.tai.bao.gio");
    CHECK(std::string(r) == "khoa.khong.ton.tai.bao.gio");
    CHECK(i18n::missingCount() > 0);
}

TEST_CASE("localization_doi_ngon_ngu") {
    i18n::setLanguage(Language::English);
    CHECK(i18n::language() == Language::English);
    const std::string en = i18n::t("menu.project");

    i18n::setLanguage(Language::Vietnamese);
    CHECK(i18n::language() == Language::Vietnamese);
    const std::string vi = i18n::t("menu.project");

    CHECK(en != vi);
    CHECK(!en.empty() && !vi.empty());
}

TEST_CASE("localization_specifier_parser") {
    // Bản thân bộ tách cũng phải đúng, nếu không test trên vô nghĩa.
    CHECK(specifiers("khong co gi").empty());
    CHECK(specifiers("100%% xong").empty());

    const auto a = specifiers("Ghi %d diem (%.2f px) cua %s");
    CHECK(a.size() == 3);
    CHECK(a[0] == "d");
    CHECK(a[1] == "f");
    CHECK(a[2] == "s");

    const auto b = specifiers("Ring overflow: %llu");
    CHECK(b.size() == 1);
    CHECK(b[0] == "llu");

    const auto c = specifiers("%-8.8s");
    CHECK(c.size() == 1);
    CHECK(c[0] == "s");
}

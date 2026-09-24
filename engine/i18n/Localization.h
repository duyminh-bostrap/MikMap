// ════════════════════════════════════════════════════════════════════════
//  ui/Localization.h — chuỗi giao diện đa ngôn ngữ
//
//  Dùng: TR("menu.project")  →  "Project" / "Dự án"
//
//  ── Vì sao chuỗi tra theo KHOÁ, không phải theo tiếng Anh ────────────
//  Cách phổ biến khác là lấy chuỗi tiếng Anh làm khoá — `TR("Save")`.
//  Nhưng khi đó sửa một chữ trong bản tiếng Anh sẽ âm thầm làm mất bản
//  dịch, và hai chỗ dùng cùng một từ tiếng Anh với nghĩa khác nhau
//  ("Clear" = xoá lưới vs "Clear" = dừng layer) buộc phải dịch giống nhau.
//  Khoá phân cấp tránh cả hai.
//
//  ── Thiếu bản dịch thì sao ───────────────────────────────────────────
//  Trả về chính KHOÁ. Nhìn thấy "slice.softedge.gamma" trên màn hình thì
//  biết ngay là thiếu chuỗi và ở đâu — tốt hơn nhiều so với ô trống hoặc
//  âm thầm rơi về tiếng Anh, vì cả hai đều dễ lọt qua khâu kiểm tra.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/util/AppSettings.h"

#include <string>
#include <utility>
#include <vector>

namespace mikmap {
namespace i18n {

void     setLanguage(Language l);
Language language();

/// Chuỗi đã dịch. Con trỏ trỏ vào bảng tĩnh — dùng được cả frame,
/// không cần sao chép.
const char* t(const char* key);

/// Số khoá thiếu bản dịch ở ngôn ngữ hiện tại — hiện trong Settings để
/// biết bản dịch còn sót chỗ nào.
int missingCount();

/// Toan bo cap (khoa, chuoi) cua MOT ngon ngu.
///
/// Chi de KIEM THU doi chieu hai bang; UI khong dung. Ly do can no:
/// tu khi chuoi dinh dang duoc tra cuu luc chay, trinh bien dich khong
/// con kiem tra duoc "%d" trong chuoi co khop voi doi so hay khong. Mot
/// ban dich viet nham "%s" thay vi "%d" se doc bay ra ngoai stack —
/// va chi no khi nguoi dung DOI NGON NGU, tuc la khong bao gio thay
/// trong luc phat trien. Test doi chieu specifier chan dung viec do.
std::vector<std::pair<std::string, std::string>> dumpTable(Language l);

} // namespace i18n
} // namespace mikmap

/// Viết tắt. Cố ý ngắn vì nó xuất hiện ở mọi dòng UI.
// ★ Ten macro CO Y khong phai la `T`.
//
//   Mot macro mot chu cai `T` pha vo moi thu vien C++ co template: ImGui
//   viet `p->~T()` trong IM_DELETE, va bo tien xu ly bien no thanh
//   `p->~::mikmap::i18n::t()`. Loi bao o imgui.h chu khong phai o day, nen
//   rat kho lan ra. `TR` du ngan de viet nhung du hiem de khong dung do.
#define TR(key) ::mikmap::i18n::t(key)

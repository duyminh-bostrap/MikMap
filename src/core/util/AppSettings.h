// ════════════════════════════════════════════════════════════════════════
//  core/util/AppSettings.h — cấu hình ỨNG DỤNG (khác cấu hình project)
//
//  ── Vì sao tách khỏi .hexmap ─────────────────────────────────────────
//  Project mô tả MỘT show: canvas, clip, slice, calibration. Nó được
//  chép qua máy khác, gửi cho người khác, đưa vào git.
//
//  Settings mô tả MÁY NÀY và NGƯỜI DÙNG NÀY: ngôn ngữ giao diện, máy
//  chiếu nằm ở màn hình số mấy, có ghi log hiệu năng không. Nhét chúng
//  vào project là sai: mở project của đồng nghiệp sẽ đổi luôn ngôn ngữ
//  giao diện của mình, và output nhảy sang màn hình không tồn tại.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <string>

namespace hexmap {

enum class Language {
    Vietnamese = 0,
    English,
};

const char* languageCode(Language l);
Language    languageFromCode(const char* code);

struct AppSettings {
    Language language = Language::Vietnamese;

    // ── Output ─────────────────────────────────────────────────────────
    /// Màn hình mặc định để đưa output ra khi khởi động. −1 = không tự đưa.
    int  defaultOutputDisplay = -1;
    bool vsync = true;

    // ── Media ──────────────────────────────────────────────────────────
    /// Số media giữ trong cache. Cao thì đổi clip mượt, thấp thì đỡ VRAM.
    int  mediaCacheBudget = 12;

    /// Cảnh báo khi import file không phải `.mov` (I9).
    bool warnNonHapMedia = true;

    // ── Chẩn đoán ──────────────────────────────────────────────────────
    bool   perfLogEnabled = true;
    double perfLogIntervalSec = 5.0;

    // ── Khởi động ──────────────────────────────────────────────────────
    /// Tự phát clip đầu tiên khi tạo project mặc định.
    bool autoPlayFirstClip = true;

    /// Tự nạp project gần nhất. Rỗng = không có.
    std::string lastProjectPath;

    // ── Lưu / nạp ──────────────────────────────────────────────────────
    /// Nạp. File thiếu hoặc hỏng KHÔNG phải lỗi — dùng giá trị mặc định.
    /// Cấu hình hỏng không đáng để chặn người dùng mở phần mềm.
    /// @return false nếu file tồn tại nhưng đọc không được (outWarning nói rõ)
    bool load(const std::string& path, std::string& outWarning);

    bool save(const std::string& path, std::string& outError) const;
};

} // namespace hexmap

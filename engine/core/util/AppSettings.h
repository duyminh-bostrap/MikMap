// ════════════════════════════════════════════════════════════════════════
//  core/util/AppSettings.h — cấu hình ỨNG DỤNG (khác cấu hình project)
//
//  ── Vì sao tách khỏi .mikmap ─────────────────────────────────────────
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

namespace mikmap {

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

    // ── Sensor: bám điểm & khử nhiễu (G11 G12) ─────────────────────────
    //
    // ★ Bốn số này PHẢI chỉnh được lúc chạy, không phải hằng số biên dịch.
    //   Mỗi sensor và mỗi phòng cho ra mức nhiễu khác nhau, và cách chỉnh
    //   duy nhất đúng là vừa chỉnh vừa nhìn điểm chạy trên màn hình. Bắt
    //   người vận hành build lại phần mềm để thử một giá trị thì trên
    //   thực tế là không chỉnh được.
    //
    //   Giá trị mặc định = mặc định của OneEuroParams / PointTrackerParams;
    //   giữ hai nơi khớp nhau để "về mặc định" đúng nghĩa.

    /// Tần số cắt khi điểm đứng yên (Hz). Thấp = mượt hơn nhưng khởi động
    /// chuyển động chậm hơn.
    double sensorMinCutoff = 1.0;

    /// Mức nới lỏng bộ lọc theo tốc độ. Cao = bám tay tốt hơn khi vung
    /// nhanh, nhưng rung hơn lúc đứng yên.
    double sensorBeta = 0.007;

    /// Khoảng cách tối đa (đơn vị sensor) để coi hai điểm ở hai frame
    /// liên tiếp là CÙNG một vật.
    double trackMaxDistance = 120.0;

    /// Thời gian ân hạn khi sensor mất dấu một điểm, giây.
    double trackGraceSec = 0.15;

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

} // namespace mikmap

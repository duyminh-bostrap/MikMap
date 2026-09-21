// ════════════════════════════════════════════════════════════════════════
//  core/model/ProjectIO.h — lưu / nạp project .hexmap (F8 I2)
//
//  Một file .hexmap chứa TOÀN BỘ trạng thái show:
//    · Composition — canvas, layer, deck, clip
//    · Screen      — slice, warp, vùng lấy
//    · Calibration — H_s và các cặp điểm đã chạm
//
//  ── Nguyên tắc: nạp file hỏng KHÔNG được làm sập ứng dụng ────────────
//  Đây là phần mềm chạy show. Một trường thiếu, một số sai kiểu, một file
//  của bản cũ — tất cả phải nạp được ở mức tốt nhất có thể và báo cảnh
//  báo, thay vì từ chối hoặc crash. Mọi phép đọc đều có giá trị mặc định.
//
//  Ngoại lệ duy nhất: JSON sai cú pháp thì không đọc được gì cả — khi đó
//  trả về false kèm mô tả lỗi có vị trí.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/calib/CalibrationProfile.h"
#include "core/model/Composition.h"
#include "core/model/Screen.h"
#include "core/model/TriggerZone.h"

#include <string>
#include <vector>

namespace hexmap {

/// Phiên bản định dạng file. Tăng khi có thay đổi phá vỡ tương thích.
inline constexpr int kProjectFormatVersion = 1;

struct Project {
    std::string name = "Untitled";
    int formatVersion = kProjectFormatVersion;

    Composition composition;
    std::vector<Screen> screens;
    std::vector<CalibrationProfile> calibrations;

    /// G17 — vung cam ung tren canvas.
    TriggerZoneSet triggerZones;
};

/// Kết quả nạp — phân biệt "hỏng hoàn toàn" với "nạp được nhưng có vấn đề".
struct LoadResult {
    bool ok = false;
    std::string error;                  ///< chỉ có khi ok == false
    std::vector<std::string> warnings;  ///< nạp được nhưng cần người dùng biết
};

namespace projectio {

/// Chuỗi hoá ra JSON có thụt lề (người đọc và sửa tay được).
std::string toJson(const Project& p);

/// Đọc từ chuỗi JSON.
LoadResult fromJson(const std::string& text, Project& out);

/// Ghi ra file. Ghi vào file tạm rồi mới đổi tên — nếu mất điện giữa
/// chừng, project cũ vẫn còn nguyên thay vì thành file cụt.
bool save(const std::string& path, const Project& p, std::string& outError);

LoadResult load(const std::string& path, Project& out);

} // namespace projectio
} // namespace hexmap

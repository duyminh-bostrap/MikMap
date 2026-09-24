#include "core/calib/SensorRouting.h"

namespace mikmap {

const SensorRoute* SensorRoutingTable::find(uint16_t sourceId) const {
    for (const SensorRoute& r : routes) {
        if (r.sourceId == sourceId) return &r;
    }
    return nullptr;
}

SensorRoutingTable buildSensorRoutes(const std::vector<CalibrationProfile>& profiles,
                                     const std::vector<Screen>& screens) {
    SensorRoutingTable table;

    for (size_t pi = 0; pi < profiles.size(); ++pi) {
        const CalibrationProfile& p = profiles[pi];

        // ── Nguồn này đã có tuyến chưa? ────────────────────────────────
        //
        // ★ Hai hồ sơ cùng `sourceId` là cấu hình mâu thuẫn. Giữ hồ sơ
        //   ĐẦU TIÊN và nói rõ, thay vì lặng lẽ lấy cái sau: lấy cái sau
        //   nghĩa là hành vi phụ thuộc THỨ TỰ TRONG FILE, và người vận
        //   hành sẽ thấy hiệu ứng "tự nhiên nhảy sang chỗ khác" sau khi
        //   lưu lại project mà không đổi gì cả.
        if (table.find(p.sourceId) != nullptr) {
            table.warnings.push_back(
                "Hai ho so hieu chinh cung nguon " + std::to_string(p.sourceId)
                + " ('" + p.name + "' bi bo qua)");
            continue;
        }

        // ── Hồ sơ đã giải xong chưa? ───────────────────────────────────
        //
        // Hồ sơ chưa đủ điểm hoặc giải thất bại thì không map được. Vẫn
        // phải nói ra: người vận hành cắm cảm biến vào, thấy không có gì
        // xảy ra, và không có cách nào biết là vì chưa calibrate.
        if (!p.isValid()) {
            table.warnings.push_back(
                "Ho so '" + p.name + "' chua calibrate xong — nguon "
                + std::to_string(p.sourceId) + " se khong hoat dong");
            continue;
        }

        // ── Screen đích có tồn tại không? ──────────────────────────────
        //
        // ★ Tìm theo `Screen::id`, KHÔNG theo vị trí trong mảng. Xoá một
        //   screen ở giữa sẽ làm mọi vị trí sau nó dịch đi một, và khi đó
        //   mọi cảm biến lặng lẽ chiếu sang máy chiếu bên cạnh — sai theo
        //   kiểu trông vẫn "có chạy", nên rất lâu mới bị phát hiện.
        size_t screenIndex = 0;
        bool   found = false;
        for (size_t si = 0; si < screens.size(); ++si) {
            if (screens[si].id == p.targetScreenId) {
                screenIndex = si;
                found = true;
                break;
            }
        }

        if (!found) {
            // ★ KHÔNG lùi về screen 0.
            //
            //   Lùi về screen 0 thì điểm chạm vẫn xuất hiện — chỉ là ở
            //   SAI MÁY CHIẾU. Trông như phần mềm đang chạy, nên người ta
            //   sẽ đi tìm lỗi ở chỗ khác (calibration? sensor?) thay vì
            //   thấy ngay là screen đích đã bị xoá. Bỏ tuyến thì triệu
            //   chứng khớp với nguyên nhân: cảm biến đó im lặng.
            table.warnings.push_back(
                "Ho so '" + p.name + "' tro toi screen id "
                + std::to_string(p.targetScreenId) + " khong ton tai");
            continue;
        }

        SensorRoute r;
        r.sourceId     = p.sourceId;
        r.profileIndex = pi;
        r.screenIndex  = screenIndex;
        table.routes.push_back(r);
    }

    return table;
}

} // namespace mikmap

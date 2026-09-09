// ════════════════════════════════════════════════════════════════════════
//  core/calib/CalibrationProfile.h — hồ sơ căn chỉnh sensor (G8 G10)
//
//  Chứa ma trận H_s: [SENSOR SPACE] → [OUTPUT SPACE của một screen]
//
//  ★ H_s TÁCH RỜI hoàn toàn với H_w (corner pin của slice).
//    architecture.md §4.2①: chỉnh lại keystone chỉ đổi H_w; H_s giữ
//    nguyên nên KHÔNG phải calibrate lại sensor. Test
//    "★ Chinh lai keystone KHONG lam hong calibration sensor" khoá
//    ràng buộc này.
//
//  ── Vì sao chỉ số chất lượng là công dân hạng nhất ───────────────────
//  Ma trận luôn "giải được" kể cả khi dữ liệu tệ. Nếu không hiển thị sai
//  số tái chiếu, người vận hành sẽ thấy hiệu ứng lệch chỗ mà không hiểu
//  vì sao — và sẽ đi chỉnh keystone (sai chỗ) thay vì calibrate lại.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Homography.h"
#include "core/math/Mat3.h"

#include <string>
#include <vector>

namespace hexmap {

/// Cách giải ma trận từ các cặp điểm.
enum class SolveMethod {
    /// Bình phương tối thiểu trên toàn bộ điểm. Nhanh, dùng khi dữ liệu sạch.
    LeastSquares = 0,

    /// RANSAC — loại điểm rác trước. Dùng khi sensor có nhiễu: chạm nhầm,
    /// nhiễu hồng ngoại, blob depth bắt sai. Chậm hơn nhưng chỉ chạy lúc
    /// calibrate, không phải mỗi frame.
    Ransac,
};

class CalibrationProfile {
public:
    std::string name = "Calibration";

    /// Sensor nào và screen nào — một hệ có thể có nhiều cặp (G18).
    uint16_t sourceId = 0;
    int      targetScreenId = 0;

    SolveMethod  method = SolveMethod::Ransac;
    RansacParams ransacParams;

    // ── Thu thập điểm ──────────────────────────────────────────────────
    /// Thêm một cặp tương ứng: người vận hành chạm vào sensor tại `sensor`
    /// trong khi phần mềm đang hiện dấu thập tại `output` trên máy chiếu.
    void addPair(const Vec2& sensor, const Vec2& output);

    void removePair(size_t index);
    void clearPairs();

    /// Bật/tắt một điểm mà không xoá — người vận hành thấy điểm nào xấu
    /// thì tắt đi rồi giải lại, không mất công chạm lại từ đầu.
    void setPairEnabled(size_t index, bool enabled);

    size_t pairCount() const { return m_pairs.size(); }
    size_t enabledPairCount() const;
    const std::vector<CorrespondencePair>& pairs() const { return m_pairs; }

    // ── Giải ───────────────────────────────────────────────────────────
    /// Giải H_s từ các cặp đang bật.
    /// @return kết quả kèm sai số — HIỂN THỊ CHO NGƯỜI VẬN HÀNH.
    HomographyResult solve();

    bool isValid() const { return m_valid; }

    /// H_s: sensor → output px. Chỉ dùng khi isValid().
    const Mat3& sensorToOutput() const { return m_toOutput; }

    /// H_s⁻¹: output px → sensor. Tính sẵn — dùng khi muốn biết "muốn
    /// hiệu ứng ở pixel này thì sensor phải đọc được giá trị nào".
    const Mat3& outputToSensor() const { return m_toSensor; }

    // ── Chất lượng (G10) ───────────────────────────────────────────────
    double rmsError() const { return m_rmsError; }
    double maxError() const { return m_maxError; }
    int    inlierCount() const { return m_inlierCount; }
    const std::string& message() const { return m_message; }

    /// Sai số theo pixel có chấp nhận được không.
    /// Ngưỡng mặc định 3px — dưới mức này mắt thường không thấy lệch.
    bool isAccurate(double thresholdPx = 3.0) const {
        return m_valid && m_rmsError <= thresholdPx;
    }

    /// Điểm nào bị RANSAC loại — UI tô đỏ để người vận hành chạm lại.
    const std::vector<bool>& outlierFlags() const { return m_outliers; }

private:
    void invalidate();

    std::vector<CorrespondencePair> m_pairs;

    Mat3 m_toOutput;
    Mat3 m_toSensor;
    bool m_valid = false;

    double m_rmsError = 0.0;
    double m_maxError = 0.0;
    int    m_inlierCount = 0;
    std::string m_message;
    std::vector<bool> m_outliers;
};

} // namespace hexmap

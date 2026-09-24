#include "core/calib/CalibrationProfile.h"

#include <algorithm>

namespace mikmap {

void CalibrationProfile::addPair(const Vec2& sensor, const Vec2& output) {
    CorrespondencePair p;
    p.src = sensor;
    p.dst = output;
    p.enabled = true;
    m_pairs.push_back(p);
    invalidate();
}

void CalibrationProfile::removePair(size_t index) {
    if (index >= m_pairs.size()) return;
    m_pairs.erase(m_pairs.begin() + static_cast<ptrdiff_t>(index));
    invalidate();
}

void CalibrationProfile::clearPairs() {
    m_pairs.clear();
    invalidate();
}

void CalibrationProfile::setPairEnabled(size_t index, bool enabled) {
    if (index >= m_pairs.size()) return;
    m_pairs[index].enabled = enabled;
    invalidate();
}

size_t CalibrationProfile::enabledPairCount() const {
    return static_cast<size_t>(
        std::count_if(m_pairs.begin(), m_pairs.end(),
                      [](const CorrespondencePair& p) { return p.enabled; }));
}

void CalibrationProfile::invalidate() {
    // Đổi tập điểm ⇒ ma trận cũ không còn đúng. Vô hiệu hoá NGAY thay vì
    // để nó tiếp tục được dùng — dùng ma trận lỗi thời còn tệ hơn không
    // có ma trận, vì nó sai một cách âm thầm.
    m_valid = false;
    m_message.clear();
}

HomographyResult CalibrationProfile::solve() {
    HomographyResult r = (method == SolveMethod::Ransac)
        ? homography::solveRANSAC(m_pairs, ransacParams)
        : homography::solveLeastSquares(m_pairs);

    m_valid       = r.ok;
    m_rmsError    = r.rmsError;
    m_maxError    = r.maxError;
    m_inlierCount = r.inlierCount;
    m_message     = r.message;

    // Cờ ngoại lai — chỉ RANSAC mới điền. Với least-squares, coi mọi
    // điểm đang bật là inlier.
    if (!r.inliers.empty()) {
        m_outliers.resize(m_pairs.size());
        for (size_t i = 0; i < m_pairs.size(); ++i) {
            m_outliers[i] = m_pairs[i].enabled && !r.inliers[i];
        }
    } else {
        m_outliers.assign(m_pairs.size(), false);
    }

    if (!m_valid) {
        m_toOutput = Mat3::identity();
        m_toSensor = Mat3::identity();
        return r;
    }

    m_toOutput = r.H;

    // Tính sẵn nghịch đảo. Nếu H_s không nghịch đảo được thì hồ sơ này
    // vô dụng — báo hỏng luôn, đừng để nó có vẻ hợp lệ.
    if (!m_toOutput.invert(m_toSensor)) {
        m_valid = false;
        m_message = "H_s khong nghich dao duoc (diem suy bien?)";
        m_toOutput = Mat3::identity();
        m_toSensor = Mat3::identity();
        r.ok = false;
        r.message = "H_s khong nghich dao duoc";
    }

    return r;
}

} // namespace mikmap

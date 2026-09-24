// ════════════════════════════════════════════════════════════════════════
//  core/filter/PointTracker.h — gán ID bền vững qua các frame (G12)
//
//  ── Bài toán ─────────────────────────────────────────────────────────
//  Nhiều sensor KHÔNG cho ID ổn định. Kinect/LiDAR trả về một danh sách
//  blob mỗi frame, thứ tự có thể đổi tuỳ thuật toán quét. Khung IR rẻ
//  tiền đánh số lại từ đầu mỗi frame.
//
//  Không có ID bền vững thì:
//    · không biết đâu là "cùng một ngón tay" giữa hai frame
//    · OneEuroFilter lọc nhầm quỹ đạo của hai người thành một
//    · không phát hiện được điểm nào vừa xuất hiện / vừa biến mất
//
//  ── Cách làm ─────────────────────────────────────────────────────────
//  Ghép cặp theo láng giềng gần nhất, có ngưỡng khoảng cách tối đa.
//  Tham lam theo thứ tự khoảng cách tăng dần — không tối ưu toàn cục
//  như thuật toán Hungary, nhưng O(n·m·log) với n,m ≤ 64 là đủ nhanh
//  và cho kết quả giống hệt trong thực tế, vì các điểm chạm cách nhau
//  xa hơn nhiều so với quãng đường chúng đi trong một frame.
//
//  ── Vì sao có thời gian ân hạn ───────────────────────────────────────
//  Sensor hay mất dấu điểm một hai frame rồi bắt lại. Xoá ID ngay sẽ
//  làm hiệu ứng đang chạy bị ngắt rồi khởi động lại. Giữ điểm "mất tích"
//  thêm một khoảng ngắn rồi mới bỏ hẳn.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"

#include <cstdint>
#include <vector>

namespace mikmap {

struct TrackedPoint {
    uint32_t id = 0;
    Vec2     position;
    Vec2     velocity;        ///< đơn vị/giây, ước lượng từ frame trước
    double   firstSeenSec = 0.0;
    double   lastSeenSec = 0.0;

    /// Vừa xuất hiện ở frame này.
    bool isNew = false;

    /// Không thấy ở frame này nhưng còn trong thời gian ân hạn.
    bool isCoasting = false;

    double ageSec(double nowSec) const { return nowSec - firstSeenSec; }
};

struct PointTrackerParams {
    /// Khoảng cách tối đa để coi hai điểm ở hai frame là cùng một điểm.
    /// Đơn vị giống toạ độ đầu vào (mm, px...). Đặt lớn hơn quãng đường
    /// một ngón tay đi trong một frame, nhưng nhỏ hơn khoảng cách giữa
    /// hai ngón tay.
    double maxMatchDistance = 120.0;

    /// Giữ điểm mất dấu thêm bao lâu trước khi bỏ hẳn.
    double graceSec = 0.15;
};

class PointTracker {
public:
    explicit PointTracker(PointTrackerParams p = {}) : m_params(p) {}

    void setParams(const PointTrackerParams& p) { m_params = p; }
    const PointTrackerParams& params() const { return m_params; }

    /// Nạp các điểm quan sát được ở frame này và cập nhật danh sách theo dõi.
    void update(const std::vector<Vec2>& observations, double nowSec);

    /// Các điểm đang theo dõi, KỂ CẢ điểm đang trong thời gian ân hạn.
    const std::vector<TrackedPoint>& tracks() const { return m_tracks; }

    /// Chỉ các điểm thực sự thấy ở frame này.
    std::vector<TrackedPoint> activeTracks() const;

    /// Các điểm vừa biến mất hẳn ở lần update gần nhất — để phát sự kiện Up.
    const std::vector<uint32_t>& justLost() const { return m_justLost; }

    void reset();

private:
    PointTrackerParams m_params;
    std::vector<TrackedPoint> m_tracks;
    std::vector<uint32_t> m_justLost;
    uint32_t m_nextId = 1;
};

} // namespace mikmap

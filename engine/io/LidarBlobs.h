// ════════════════════════════════════════════════════════════════════════
//  io/LidarBlobs.h — scan → cụm vật → điểm chạm có ID bền vững
//
//  Tách phần "hiểu dữ liệu lidar" khỏi phần TCP: Hokuyo hôm nay và plugin
//  lidar ngày mai dùng chung đúng đoạn này.
//
//  · extractBlobs: gom các tia liền kề (khoảng cách Euclid ≤ maxGapMm)
//    thành cụm; cụm chỉ được nhận khi bề rộng dây cung nằm trong
//    [minBlobMm, maxBlobMm]. Tường dài bị loại, người/vật nhỏ được giữ.
//  · LidarTouchPublisher: PointTracker gán ID qua frame, phát Down/Up và
//    công bố SensorFrame chứa các track đang thấy.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/filter/PointTracker.h"
#include "core/math/Vec2.h"
#include "io/ISensorSource.h"
#include "io/LaserScan.h"

#include <vector>

namespace mikmap {

struct LidarParams {
    /// 4 đỉnh tứ giác ROI trong hệ lidar (mm), theo thứ tự vòng.
    /// Cả 4 điểm = (0,0) ⇒ không lọc ROI.
    Vec2 roi[4]{};

    double minBlobMm = 15.0;    ///< bề rộng dây cung nhỏ nhất
    double maxBlobMm = 600.0;   ///< lớn hơn ⇒ coi là tường/nền
    double maxGapMm = 50.0;     ///< hai tia liền kề xa hơn ⇒ cụm mới
};

/// Cắt cụm từ một scan. out[] nhận trọng tâm (mm), tối đa maxOut.
/// @return số cụm đã ghi.
int extractBlobs(const LaserScan& s, const LidarParams& p, Vec2* out, int maxOut);

class LidarTouchPublisher {
public:
    explicit LidarTouchPublisher(PointTrackerParams tp = {100.0, 0.15});

    /// Cập nhật tracker từ scan, phát Down/Up rồi công bố frame vào dst.
    void publish(const LaserScan& s, const LidarParams& p, double nowSec, ISensorSource& dst);

    /// Mất kết nối: Up cho mọi track + frame rỗng.
    void clear(double nowSec, ISensorSource& dst);

private:
    PointTracker      m_tracker;
    std::vector<Vec2> m_obs;   // reserve một lần trong constructor
};

} // namespace mikmap

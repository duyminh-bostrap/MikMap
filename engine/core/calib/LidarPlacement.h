#pragma once
// Đặt một lidar 2D lên bề mặt (tường/sàn) theo vị trí thật tính bằng mm.
// Bề mặt: gốc ở góc trên-trái screen, x sang phải, y xuống dưới.
// angleDeg = hướng "phía trước" của lidar trên bề mặt: 0 = +x, tăng theo chiều kim đồng hồ (vì y hướng xuống).
// mirror = lidar lắp úp ngược (trục "trái" của sensor bị lật).
#include "core/math/Vec2.h"

namespace mikmap {

struct LidarPose { double x = 0, y = 0, angleDeg = 0; bool mirror = false; };

Vec2 lidarToSurface(const LidarPose& p, const Vec2& sensorMm);
Vec2 surfaceToLidar(const LidarPose& p, const Vec2& surfaceMm);

}  // namespace mikmap

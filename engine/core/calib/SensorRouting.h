// ════════════════════════════════════════════════════════════════════════
//  core/calib/SensorRouting.h — nhiều sensor cho nhiều screen (G18)
//
//  Một sân khấu lớn thường có HAI hoặc BA cảm biến: một LiDAR quét sàn
//  trước, một khung IR trên bức tường bên, một Kinect nhìn xuống bục.
//  Mỗi cái phủ một vùng khác nhau và chiếu lên một máy chiếu khác nhau.
//
//  Trước G18, `AppController` gắn cứng `calibrations[0]` và `screens[0]`
//  ở sáu chỗ — tức là chỉ dùng được đúng một sensor, và cảm biến thứ hai
//  cắm vào sẽ đi qua phép hiệu chỉnh của cảm biến thứ nhất.
//
//  ── ★ Vì sao trả về BẢNG CHỈ SỐ chứ không phải con trỏ ───────────────
//  `SensorMapper` giữ con trỏ thô tới `CalibrationProfile` và `Screen`.
//  Hai thứ đó nằm trong `std::vector` của Project — nên chỉ cần thêm một
//  screen là vector cấp phát lại và MỌI con trỏ đang giữ thành treo.
//
//  Đây không phải lo xa: đúng lỗi đó đã xảy ra khi thêm nút "Thêm Screen"
//  (F22) và phải gán lại con trỏ cho mapper ngay sau `push_back`. Bảng
//  chỉ số thì không có hạng mục đó — chỉ số sai thì phát hiện được bằng
//  kiểm tra biên, còn con trỏ treo thì im lặng cho tới lúc sập.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/calib/CalibrationProfile.h"
#include "core/model/Screen.h"

#include <cstdint>
#include <string>
#include <vector>

namespace mikmap {

struct SensorRoute {
    uint16_t sourceId = 0;
    size_t   profileIndex = 0;
    size_t   screenIndex = 0;
};

struct SensorRoutingTable {
    std::vector<SensorRoute> routes;

    /// Vấn đề phát hiện được lúc dựng bảng. Phải hiện ra cho người vận
    /// hành — mọi mục ở đây đều nghĩa là "có cảm biến sẽ không hoạt động".
    std::vector<std::string> warnings;

    /// @return nullptr nếu nguồn này không có tuyến nào.
    const SensorRoute* find(uint16_t sourceId) const;
};

/// Dựng bảng định tuyến từ danh sách hiệu chỉnh và danh sách screen.
///
/// `targetScreenId` khớp theo `Screen::id`, KHÔNG phải theo vị trí trong
/// mảng: xoá một screen ở giữa sẽ làm mọi vị trí sau nó dịch đi, và khi
/// đó mọi cảm biến lặng lẽ chiếu sang máy chiếu bên cạnh.
SensorRoutingTable buildSensorRoutes(const std::vector<CalibrationProfile>& profiles,
                                     const std::vector<Screen>& screens);

} // namespace mikmap

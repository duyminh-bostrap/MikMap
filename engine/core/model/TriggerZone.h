// ════════════════════════════════════════════════════════════════════════
//  core/model/TriggerZone.h — vùng cảm ứng trên canvas (G17)
//
//  ★ ĐÂY LÀ ĐIỂM ĐẾN CỦA CẢ DỰ ÁN.
//  Mọi thứ đã xây — homography, mesh warp, thread sensor, slice mapping —
//  tồn tại để phục vụ đúng một câu: chạm vào vật thể thật thì có gì đó
//  xảy ra đúng chỗ đó.
//
//      chạm ──H_s──▶ máy chiếu ──slice.inverse()──▶ canvas
//                                                     │
//                                          zone nào chứa điểm này?
//                                                     ▼
//                                          phát clip / cột / dừng layer
//
//  Vùng được định nghĩa trong KHÔNG GIAN CANVAS, không phải không gian
//  máy chiếu. Nhờ vậy chỉnh lại keystone hay đổi máy chiếu không làm
//  lệch vùng cảm ứng — cùng lý do H_s và H_w phải tách rời (§4.2①).
//
//  ── ★ Kích hoạt theo "ĐI VÀO VÙNG", không phải theo sự kiện Down ────
//  Đây là điểm tôi làm sai lúc đầu và phải sửa.
//
//  Sensor tracking liên tục (Kinect, LiDAR, MockSource) chỉ phát Down
//  MỘT LẦN khi điểm xuất hiện, sau đó toàn là Move. Nếu vùng chỉ nghe
//  Down thì một điểm di chuyển vào vùng sẽ KHÔNG BAO GIỜ kích hoạt.
//
//  Phát hiện cạnh lên (không có điểm → có điểm) phủ được cả hai loại:
//    · sensor chạm  : Down bên trong vùng ⇒ vùng chuyển sang "có điểm"
//    · sensor tracking: điểm đi vào vùng ⇒ cũng vậy
//
//  Giá phải trả: trễ tối đa một frame (~16ms). Không đáng kể so với
//  input lag 16–80ms của máy chiếu.
//
//  ── Vì sao BẮT BUỘC có cooldown ──────────────────────────────────────
//  Sensor thật rất nhiễu. Ngón tay rung ngay mép vùng sẽ tạo ra chuỗi
//  vào-ra-vào-ra hàng chục lần mỗi giây. Không chống dội thì clip bị
//  trigger lại liên tục và hiệu ứng đứng hình ở khung đầu tiên.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"

#include <string>
#include <vector>

namespace mikmap {

enum class TriggerAction {
    None = 0,
    TriggerClip,     ///< phát một clip cụ thể (layer + column)
    TriggerColumn,   ///< phát cả cột trên mọi layer
    ClearLayer,      ///< dừng một layer
    ClearAll,
};

const char* triggerActionName(TriggerAction a);
TriggerAction triggerActionFromName(const char* name);

struct TriggerZone {
    std::string name = "Zone";
    bool enabled = true;

    /// Hình chữ nhật trong không gian COMPOSITION CANVAS.
    Vec2 origin{0.0, 0.0};
    Vec2 size{200.0, 200.0};

    TriggerAction action = TriggerAction::TriggerClip;
    int targetLayer  = 0;
    int targetColumn = 0;

    /// Khoảng chống dội, giây. Xem ghi chú ở đầu file — không có nó thì
    /// hệ thống không dùng được với sensor thật.
    double cooldownSec = 0.35;

    // ── Trạng thái lúc chạy (không lưu vào project) ────────────────────
    double lastFiredSec = -1.0e9;

    /// Đang có điểm chạm nằm trong vùng — để UI tô sáng.
    bool   occupied = false;

    /// Trạng thái frame TRƯỚC, để phát hiện cạnh lên.
    bool   wasOccupied = false;

    bool contains(const Vec2& canvasPt) const {
        return canvasPt.x >= origin.x && canvasPt.x <= origin.x + size.x
            && canvasPt.y >= origin.y && canvasPt.y <= origin.y + size.y;
    }

    bool isReady(double nowSec) const {
        return (nowSec - lastFiredSec) >= cooldownSec;
    }
};

/// Kết quả một lần kích hoạt — AppController đọc rồi thực thi.
/// core/ KHÔNG tự gọi Composition: giữ nó là dữ liệu thuần, test được.
struct TriggerHit {
    bool          fired = false;
    int           zoneIndex = -1;
    TriggerAction action = TriggerAction::None;
    int           layer = 0;
    int           column = 0;
};

class TriggerZoneSet {
public:
    std::vector<TriggerZone> zones;

    int  count() const { return static_cast<int>(zones.size()); }

    /// Cập nhật từ toàn bộ điểm chạm của frame này và trả về các vùng
    /// vừa được kích hoạt.
    ///
    /// Vùng kích hoạt khi chuyển từ KHÔNG có điểm sang CÓ điểm, và
    /// cooldown đã hết.
    ///
    /// Vùng chồng nhau: chỉ vùng TRÊN CÙNG chứa điểm được tính là có
    /// điểm — giống quy tắc z-order của slice và cách xử lý click chuột.
    ///
    /// @param nowSec đồng hồ đơn điệu; quyết định cooldown. Truyền vào
    ///        thay vì tự đọc để core/ không phụ thuộc nguồn thời gian
    ///        và test tái lập được.
    std::vector<TriggerHit> update(const std::vector<Vec2>& canvasPoints,
                                   double nowSec);

    /// Xoá trạng thái cooldown — dùng khi nạp project hoặc dừng sensor.
    void resetRuntimeState();
};

} // namespace mikmap

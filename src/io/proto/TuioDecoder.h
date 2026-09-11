// ════════════════════════════════════════════════════════════════════════
//  io/proto/TuioDecoder.h — giải mã TUIO 1.1 (G14)
//
//  TUIO là thứ hầu hết phần mềm tracking nói: Community Core Vision, các
//  bộ theo dõi LiDAR thương mại, khung IR, ứng dụng multitouch. Nói được
//  TUIO nghĩa là cắm được vào phần lớn hệ thống có sẵn mà không phải viết
//  driver riêng cho từng loại.
//
//  TUIO chồng lên OSC nên bộ này KHÔNG tự phân tích byte — nó nhận
//  `OscMessage` đã phân tích sẵn (xem `OscMessage.h`) và chỉ lo phần ngữ
//  nghĩa. Tách vậy để test được đầy đủ mà không cần mở cổng mạng.
//
//  ── Một frame TUIO gồm ba loại message ───────────────────────────────
//    /tuio/2Dcur set   <id> <x> <y> <vx> <vy> <accel>
//    /tuio/2Dcur alive <id> <id> ...        ← ai còn sống
//    /tuio/2Dcur fseq  <n>                  ← đóng frame
//
//  ── ★ Vì sao `alive` mới là thứ quan trọng nhất ──────────────────────
//  TUIO KHÔNG có message "ngón tay này nhấc lên". Cách duy nhất biết một
//  điểm đã biến mất là nó VẮNG MẶT trong danh sách `alive` của frame kế.
//
//  Hệ quả: ai chỉ xử lý `set` sẽ có một bộ theo dõi mà điểm không bao giờ
//  chết — chạm rồi nhấc tay, dấu chạm vẫn nằm đó mãi mãi. Trong một tác
//  phẩm tương tác thì đó là hiệu ứng kẹt cứng cho tới khi khởi động lại.
//
//  ── ★ Vì sao phải tôn trọng `fseq` ───────────────────────────────────
//  UDP không bảo đảm thứ tự. Gói đến muộn mang trạng thái CŨ; áp nó vào
//  sẽ làm điểm chạm nhảy giật về sau rồi lại nhảy tới. `fseq` cho biết
//  frame nào mới hơn — frame cũ bị bỏ.
//
//  Ngoại lệ: `fseq = -1` nghĩa là bên gửi không đánh số, phải nhận tất.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "io/proto/OscMessage.h"

#include <cstdint>
#include <vector>

namespace hexmap {

/// Một điểm chạm đã giải mã, toạ độ CHUẨN HOÁ [0,1] như TUIO quy định.
struct TuioCursor {
    int32_t id = 0;
    float   x = 0.0f;
    float   y = 0.0f;
};

/// Kết quả sau khi nạp xong một frame TUIO.
struct TuioFrame {
    std::vector<TuioCursor> cursors;   ///< các điểm còn sống, đã lọc theo `alive`
    std::vector<int32_t>    ended;     ///< id vừa biến mất ở frame này
    int32_t                 fseq = -1;
};

class TuioDecoder {
public:
    /// Nạp các message của MỘT gói UDP.
    ///
    /// @param outFrame  chỉ ghi khi trả về true
    /// @return true nếu gói này đóng một frame (có `fseq`, hoặc có `alive`
    ///         mà bên gửi không đánh số frame)
    bool feed(const std::vector<OscMessage>& messages, TuioFrame& outFrame);

    /// Quên toàn bộ trạng thái — dùng khi kết nối lại nguồn.
    void reset();

    /// Các id đang sống theo lần nạp gần nhất.
    const std::vector<int32_t>& aliveIds() const { return m_alive; }

private:
    /// Điểm nhận được từ `set` nhưng chưa được `alive` xác nhận.
    std::vector<TuioCursor> m_pending;

    /// Vị trí gần nhất của từng điểm còn sống.
    ///
    /// ★ Cần giữ vì bên gửi CHỈ gửi `set` khi vị trí thay đổi. Một ngón
    ///   tay đặt yên chỉ xuất hiện trong `alive`, không kèm `set` — không
    ///   nhớ vị trí cũ thì nó biến mất rồi hiện lại mỗi khi người ta ngừng
    ///   di chuyển, đúng lúc cần nó ổn định nhất.
    std::vector<TuioCursor> m_cursors;

    std::vector<int32_t> m_alive;
    int32_t m_lastFseq = -1;
    bool    m_haveFseq = false;
};

} // namespace hexmap

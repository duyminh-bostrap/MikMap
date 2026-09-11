#include "io/proto/TuioDecoder.h"

#include <algorithm>

namespace hexmap {
namespace {

/// TUIO 1.1 định nghĩa nhiều hồ sơ; ta nhận hai loại điểm.
///
/// `/tuio/2Dcur` là con trỏ (ngón tay, đốm sáng). `/tuio/25Dcur` giống
/// hệt nhưng thêm toạ độ z — nhiều bộ theo dõi LiDAR gửi loại này, và ba
/// đối số đầu vẫn là id, x, y nên đọc chung một đường được.
///
/// KHÔNG nhận `/tuio/2Dobj` (thẻ fiducial) và `/tuio/2Dblb` (vùng): chúng
/// có ngữ nghĩa khác (góc xoay, kích thước) và gộp bừa vào đây sẽ cho ra
/// điểm chạm ở vị trí đúng nhưng ý nghĩa sai.
bool isCursorAddress(const std::string& a) {
    return a == "/tuio/2Dcur" || a == "/tuio/25Dcur";
}

} // namespace

void TuioDecoder::reset() {
    m_pending.clear();
    m_cursors.clear();
    m_alive.clear();
    m_lastFseq = -1;
    m_haveFseq = false;
}

bool TuioDecoder::feed(const std::vector<OscMessage>& messages, TuioFrame& outFrame) {
    bool sawAlive = false;
    bool sawFseq  = false;
    int32_t fseq  = -1;
    std::vector<int32_t> alive;

    for (const OscMessage& m : messages) {
        if (!isCursorAddress(m.address)) continue;
        if (m.args.empty() || m.args[0].type != OscArg::Type::String) continue;

        const std::string& cmd = m.args[0].s;

        if (cmd == "set") {
            // set <id> <x> <y> ...  — các đối số sau (vận tốc, gia tốc)
            // bỏ qua: ta đã có OneEuroFilter (G11) làm mượt tốt hơn, và
            // vận tốc do bên gửi tính thường nhiễu hơn hẳn.
            if (m.args.size() < 4) continue;

            TuioCursor c;
            c.id = m.argInt(1);
            c.x  = static_cast<float>(m.argDouble(2));
            c.y  = static_cast<float>(m.argDouble(3));

            // Cùng một id xuất hiện hai lần trong một gói: giữ bản SAU.
            auto it = std::find_if(m_pending.begin(), m_pending.end(),
                                   [&](const TuioCursor& p) { return p.id == c.id; });
            if (it != m_pending.end()) *it = c;
            else                        m_pending.push_back(c);

        } else if (cmd == "alive") {
            sawAlive = true;
            for (size_t i = 1; i < m.args.size(); ++i) {
                alive.push_back(m.args[i].asInt());
            }

        } else if (cmd == "fseq") {
            sawFseq = true;
            fseq = m.argInt(1, -1);
        }
    }

    // Gói chỉ có `set` (bundle bị chia nhỏ) — chưa đóng frame, giữ lại
    // chờ gói mang `alive`.
    if (!sawAlive) return false;

    // ── ★ Bỏ frame ĐẾN MUỘN ────────────────────────────────────────────
    //
    //   UDP không bảo đảm thứ tự. Gói tới muộn mang trạng thái CŨ; áp vào
    //   sẽ làm điểm chạm nhảy giật về sau rồi nhảy tới. Triệu chứng nhìn
    //   thấy là hiệu ứng "rung" mà đổi bộ lọc bao nhiêu cũng không hết,
    //   vì nguyên nhân không nằm ở nhiễu.
    //
    //   fseq = -1 nghĩa là bên gửi không đánh số ⇒ phải nhận tất, không
    //   có cách nào biết thứ tự.
    if (sawFseq && fseq >= 0) {
        if (m_haveFseq && fseq <= m_lastFseq) {
            // Vẫn phải dọn `set` của gói bị bỏ, nếu không chúng sẽ rò
            // sang frame kế và xuất hiện như điểm ma.
            m_pending.clear();
            return false;
        }
        m_lastFseq = fseq;
        m_haveFseq = true;
    }

    // ── Điểm nào vừa biến mất ──────────────────────────────────────────
    //
    // ★ TUIO không có message "nhấc tay". Vắng mặt trong `alive` LÀ tín
    //   hiệu kết thúc — thiếu bước này thì điểm chạm không bao giờ chết.
    outFrame.ended.clear();
    for (const int32_t old : m_alive) {
        if (std::find(alive.begin(), alive.end(), old) == alive.end()) {
            outFrame.ended.push_back(old);
        }
    }

    // ── Điểm còn sống ──────────────────────────────────────────────────
    //
    // Vị trí lấy từ `set` của gói này; id nào có trong `alive` mà không
    // kèm `set` là điểm ĐỨNG YÊN — bên gửi chỉ gửi `set` khi có thay đổi.
    // Giữ nguyên vị trí cũ cho những id đó.
    std::vector<TuioCursor> next;
    next.reserve(alive.size());

    for (const int32_t id : alive) {
        auto fresh = std::find_if(m_pending.begin(), m_pending.end(),
                                  [&](const TuioCursor& c) { return c.id == id; });
        if (fresh != m_pending.end()) {
            next.push_back(*fresh);
            continue;
        }

        auto prev = std::find_if(m_cursors.begin(), m_cursors.end(),
                                 [&](const TuioCursor& c) { return c.id == id; });
        if (prev != m_cursors.end()) {
            next.push_back(*prev);
            continue;
        }

        // Có trong `alive` nhưng chưa từng thấy `set`: bỏ qua thay vì bịa
        // vị trí (0,0) — góc trên-trái là một toạ độ HỢP LỆ, nên điểm ma
        // ở đó trông y như một cú chạm thật.
    }

    m_cursors = next;
    m_alive   = alive;
    m_pending.clear();

    outFrame.cursors = m_cursors;
    outFrame.fseq    = sawFseq ? fseq : -1;
    return true;
}

} // namespace hexmap

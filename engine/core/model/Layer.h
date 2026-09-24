// ════════════════════════════════════════════════════════════════════════
//  core/model/Layer.h — một hàng trong lưới (A2 A7 A8)
//
//  ★ QUYẾT ĐỊNH CẤU TRÚC QUAN TRỌNG
//  Layer là TOÀN CỤC của composition, KHÔNG thuộc về deck.
//  Deck chỉ là "trang clip"; layer giữ thuộc tính (opacity, blend, solo)
//  và trạng thái ĐANG PHÁT.
//
//  Vì sao: yêu cầu A9 nói "chuyển deck không được ngắt playback".
//  Nếu Deck sở hữu Layer, đổi deck sẽ vứt luôn trạng thái phát → clip
//  đang chiếu bị cắt giữa chừng. Đó là lỗi không sửa được bằng vá víu,
//  phải đúng từ cấu trúc.
//
//  Hệ quả: layer trỏ tới clip đang phát bằng CẶP (deck, column) — clip
//  đó có thể nằm ở deck khác với deck người dùng đang xem.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/model/BlendMode.h"

#include <string>

namespace mikmap {

class Layer {
public:
    std::string name;
    double      opacity = 1.0;
    BlendMode   blend   = BlendMode::Normal;

    /// Tắt layer hoàn toàn (A8).
    bool bypass = false;

    /// Chỉ hiện layer này. Nếu BẤT KỲ layer nào solo, mọi layer không
    /// solo đều bị ẩn — quyết định này thuộc về Composition, không phải
    /// từng layer, vì nó phụ thuộc trạng thái của cả nhóm.
    bool solo = false;

    // ── Trạng thái đang phát ───────────────────────────────────────────
    /// −1 nghĩa là layer không phát gì.
    int activeDeck   = -1;
    int activeColumn = -1;

    // ── A10: transition ────────────────────────────────────────────────
    //
    // ★ ĐÂY LÀ LÝ DO ENGINE PHẢI CHỊU ĐƯỢC 2 LUỒNG VIDEO.
    // Trong lúc chuyển clip, clip CŨ vẫn đang giải mã và vẽ trong khi
    // clip MỚI đã bắt đầu. Một layer, nhưng hai luồng — suốt thời gian
    // transition. Spike test R1 đã đo: 2 luồng 4K vẫn giữ 30fps gốc.

    /// Clip đang tắt dần. −1 = không có transition nào đang chạy.
    int prevDeck   = -1;
    int prevColumn = -1;

    /// Tiến độ [0,1]. 1.0 = xong, clip cũ đã tắt hẳn.
    double transitionProgress = 1.0;

    /// Thời lượng chuyển, giây. 0 = cắt thẳng (không crossfade).
    double transitionDuration = 0.0;

    bool isPlayingSomething() const {
        return activeDeck >= 0 && activeColumn >= 0;
    }

    bool isTransitioning() const {
        return prevDeck >= 0 && prevColumn >= 0 && transitionProgress < 1.0;
    }

    /// Ngừng phát (A8 — nút Clear).
    void clear() {
        activeDeck   = -1;
        activeColumn = -1;
        prevDeck     = -1;
        prevColumn   = -1;
        transitionProgress = 1.0;
    }

    /// Layer có được vẽ không, xét riêng bypass và opacity.
    /// KHÔNG xét solo — xem ghi chú ở trên.
    bool isRenderable() const {
        return !bypass && opacity > 0.0 && isPlayingSomething();
    }
};

} // namespace mikmap

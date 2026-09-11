// ════════════════════════════════════════════════════════════════════════
//  core/model/Composition.h — gốc của cây dữ liệu (A1 A5 A6 A11)
//
//  Sở hữu:  canvas ảo · các layer (toàn cục) · các deck (trang clip)
//
//  ── Cấu trúc ──────────────────────────────────────────────────────────
//      Composition
//        ├─ canvasSize          không gian ảo, độc lập máy chiếu (A1)
//        ├─ masterOpacity                                        (A11)
//        ├─ layers[]            thuộc tính + trạng thái ĐANG PHÁT
//        └─ decks[]             trang clip; mỗi deck là lưới [layer][col]
//
//  Layer trỏ tới clip đang phát bằng cặp (deck, column). Nhờ vậy đổi deck
//  đang XEM không đụng tới clip đang PHÁT — yêu cầu A9.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/math/Vec2.h"
#include "core/model/Deck.h"
#include "core/model/Layer.h"

#include <cstdint>
#include <random>
#include <vector>

namespace hexmap {

/// Trigger cả cột thì các layer có ô TRỐNG ở cột đó xử lý thế nào?
enum class EmptyCellBehavior {
    /// Giữ nguyên clip đang phát. Mặc định, và giống Resolume.
    /// Cho phép dựng cột chỉ đổi một vài layer, giữ nguyên layer nền.
    KeepPlaying = 0,

    /// Dừng layer đó lại. Hữu ích khi muốn cột hoạt động như một "cảnh"
    /// xác định hoàn toàn: cái gì không có trong cột thì phải tắt.
    ClearLayer,
};

class Composition {
public:
    Composition();
    Composition(int layerCount, int columnCount, int deckCount = 1);

    // ── A1: canvas ảo ──────────────────────────────────────────────────
    Vec2   canvasSize{1920.0, 1080.0};

    // ── A11: master ────────────────────────────────────────────────────
    double masterOpacity = 1.0;

    EmptyCellBehavior emptyCellBehavior = EmptyCellBehavior::KeepPlaying;

    // ── Layer (toàn cục) ───────────────────────────────────────────────
    int    layerCount() const { return static_cast<int>(m_layers.size()); }
    Layer&       layer(int i);
    const Layer& layer(int i) const;
    void   setLayerCount(int n);   ///< đồng bộ xuống mọi deck

    // ── Deck ───────────────────────────────────────────────────────────
    int    deckCount() const { return static_cast<int>(m_decks.size()); }
    Deck&        deck(int i);
    const Deck&  deck(int i) const;
    void   addDeck(const std::string& name = "Deck");

    /// Deck đang XEM trên UI. Đổi giá trị này KHÔNG ảnh hưởng playback.
    int  viewedDeck() const { return m_viewedDeck; }
    void setViewedDeck(int i);

    int    columnCount() const;
    void   setColumnCount(int n);   ///< áp cho mọi deck

    // ── A5 / A6: kích hoạt ─────────────────────────────────────────────
    /// Bấm vào một ô clip. Lấy clip từ deck ĐANG XEM.
    /// @return false nếu ô không hợp lệ hoặc rỗng.
    bool triggerClip(int layerIdx, int column);

    /// Bấm vào tiêu đề cột — kích hoạt cột đó trên MỌI layer.
    /// @return số layer thực sự bị đổi.
    int triggerColumn(int column);

    /// Dừng toàn bộ layer.
    void clearAll();

    // ── Truy vấn khi render ────────────────────────────────────────────
    /// Clip đang phát của layer — CÓ THỂ nằm ở deck khác deck đang xem.
    /// nullptr nếu layer không phát gì.
    Clip*       playingClip(int layerIdx);
    const Clip* playingClip(int layerIdx) const;

    /// Có layer nào đang solo không (A8).
    bool anySolo() const;

    /// Chỉ số các layer cần vẽ, theo z-order từ DƯỚI lên TRÊN.
    /// Đã xử lý bypass, opacity, solo và trạng thái phát.
    std::vector<int> visibleLayers() const;

    /// Độ mờ hiệu dụng của layer = layer.opacity × clip.opacity × master.
    double effectiveOpacity(int layerIdx) const;

    // ── A10: thông tin để render một layer, kể cả khi đang transition ──
    /// Gói đủ những gì renderer cần cho MỘT layer. Trả về cả clip cũ lẫn
    /// clip mới cùng độ mờ tương ứng, nên renderer không phải tự suy luận
    /// về transition — nó chỉ việc vẽ những gì được đưa.
    struct LayerRenderInfo {
        const Clip* current  = nullptr;
        const Clip* previous = nullptr;   ///< chỉ khác null khi đang transition
        double currentOpacity  = 0.0;
        double previousOpacity = 0.0;
        BlendMode blend = BlendMode::Normal;
    };
    LayerRenderInfo renderInfo(int layerIdx) const;

    /// Clip đang tắt dần của layer. nullptr nếu không transition.
    Clip*       transitioningClip(int layerIdx);
    const Clip* transitioningClip(int layerIdx) const;

    // ── Cập nhật ───────────────────────────────────────────────────────
    /// Tiến toàn bộ transport và xử lý autopilot (C7).
    void update(double dtSec);

    /// Đặt seed RNG — để test tái lập được EndAction::Random.
    void setRandomSeed(uint32_t seed) { m_rng.seed(seed); }

private:
    /// Chuyển layer sang clip không rỗng kế tiếp trong cùng deck (C7).
    void advanceToNextClip(int layerIdx);
    void advanceToRandomClip(int layerIdx);

    /// Bắt đầu phát ô (deck, column) trên layer.
    bool startClip(int layerIdx, int deckIdx, int column);

    std::vector<Layer> m_layers;
    std::vector<Deck>  m_decks;
    int m_viewedDeck = 0;
    std::mt19937 m_rng{12345};
};

} // namespace hexmap

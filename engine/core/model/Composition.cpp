#include "core/model/Composition.h"

#include <algorithm>

namespace hexmap {
namespace {
Layer& dummyLayer() {
    static Layer kDummy{};
    return kDummy;
}
Deck& dummyDeck() {
    static Deck kDummy{};
    return kDummy;
}
} // namespace

Composition::Composition()
    : Composition(3, 8, 1) {}

Composition::Composition(int layerCount, int columnCount, int deckCount) {
    m_decks.reserve(static_cast<size_t>(std::max(1, deckCount)));
    for (int i = 0; i < std::max(1, deckCount); ++i) {
        m_decks.emplace_back(std::max(0, layerCount),
                             std::max(0, columnCount),
                             "Deck " + std::to_string(i + 1));
    }
    setLayerCount(layerCount);
}

// ── Layer ──────────────────────────────────────────────────────────────

Layer& Composition::layer(int i) {
    if (i < 0 || i >= layerCount()) return dummyLayer();
    return m_layers[static_cast<size_t>(i)];
}

const Layer& Composition::layer(int i) const {
    if (i < 0 || i >= layerCount()) return dummyLayer();
    return m_layers[static_cast<size_t>(i)];
}

void Composition::setLayerCount(int n) {
    n = std::max(0, n);
    const int old = layerCount();

    m_layers.resize(static_cast<size_t>(n));
    for (int i = old; i < n; ++i) {
        m_layers[static_cast<size_t>(i)].name = "Layer " + std::to_string(i + 1);
    }

    // Layer là toàn cục ⇒ mọi deck phải có cùng số hàng.
    for (Deck& d : m_decks) {
        d.resize(n, d.columnCount());
    }
}

// ── Deck ───────────────────────────────────────────────────────────────

Deck& Composition::deck(int i) {
    if (i < 0 || i >= deckCount()) return dummyDeck();
    return m_decks[static_cast<size_t>(i)];
}

const Deck& Composition::deck(int i) const {
    if (i < 0 || i >= deckCount()) return dummyDeck();
    return m_decks[static_cast<size_t>(i)];
}

void Composition::addDeck(const std::string& name) {
    m_decks.emplace_back(layerCount(), columnCount(), name);
}

void Composition::setViewedDeck(int i) {
    if (i < 0 || i >= deckCount()) return;
    // ★ CHỈ đổi deck đang xem. Tuyệt đối không đụng tới
    //   layer.activeDeck / activeColumn — đó là yêu cầu A9.
    m_viewedDeck = i;
}

int Composition::columnCount() const {
    return m_decks.empty() ? 0 : m_decks.front().columnCount();
}

void Composition::setColumnCount(int n) {
    n = std::max(0, n);
    for (Deck& d : m_decks) {
        d.resize(layerCount(), n);
    }

    // Layer đang phát cột vừa bị cắt mất thì phải dừng, nếu không nó
    // sẽ trỏ vào ô không còn tồn tại.
    for (Layer& L : m_layers) {
        if (L.activeColumn >= n) L.clear();
    }
}

// ── Kích hoạt ──────────────────────────────────────────────────────────

bool Composition::startClip(int layerIdx, int deckIdx, int column) {
    if (layerIdx < 0 || layerIdx >= layerCount()) return false;
    if (deckIdx  < 0 || deckIdx  >= deckCount())  return false;

    Clip* c = m_decks[static_cast<size_t>(deckIdx)].clipPtr(layerIdx, column);
    if (c == nullptr || c->isEmpty()) return false;

    Layer& L = m_layers[static_cast<size_t>(layerIdx)];

    // Bấm lại đúng clip đang phát → phát lại từ đầu, KHÔNG transition.
    // Crossfade một clip với chính nó chỉ tạo ra hiệu ứng nhấp nháy.
    const bool sameClip = (L.activeDeck == deckIdx && L.activeColumn == column);

    if (!sameClip && L.isPlayingSomething() && L.transitionDuration > 0.0) {
        // ★ A10 — clip CŨ tiếp tục phát và tắt dần. Đây chính là lúc
        //   layer này chiếm HAI luồng video cùng lúc.
        L.prevDeck   = L.activeDeck;
        L.prevColumn = L.activeColumn;
        L.transitionProgress = 0.0;
    } else {
        L.prevDeck   = -1;
        L.prevColumn = -1;
        L.transitionProgress = 1.0;
    }

    c->rewindAndPlay();
    L.activeDeck   = deckIdx;
    L.activeColumn = column;
    return true;
}

bool Composition::triggerClip(int layerIdx, int column) {
    return startClip(layerIdx, m_viewedDeck, column);
}

int Composition::triggerColumn(int column) {
    int changed = 0;

    for (int i = 0; i < layerCount(); ++i) {
        const Clip& c = m_decks[static_cast<size_t>(m_viewedDeck)].clip(i, column);

        if (!c.isEmpty()) {
            if (startClip(i, m_viewedDeck, column)) ++changed;
            continue;
        }

        // Ô trống — hành vi tuỳ cấu hình. Mặc định KeepPlaying (giống
        // Resolume): cho phép dựng cột chỉ đổi vài layer, giữ layer nền.
        if (emptyCellBehavior == EmptyCellBehavior::ClearLayer) {
            Layer& L = m_layers[static_cast<size_t>(i)];
            if (L.isPlayingSomething()) {
                L.clear();
                ++changed;
            }
        }
    }

    return changed;
}

void Composition::clearAll() {
    for (Layer& L : m_layers) L.clear();
}

// ── Truy vấn khi render ────────────────────────────────────────────────

Clip* Composition::playingClip(int layerIdx) {
    if (layerIdx < 0 || layerIdx >= layerCount()) return nullptr;
    const Layer& L = m_layers[static_cast<size_t>(layerIdx)];
    if (!L.isPlayingSomething()) return nullptr;
    if (L.activeDeck >= deckCount()) return nullptr;

    return m_decks[static_cast<size_t>(L.activeDeck)].clipPtr(layerIdx, L.activeColumn);
}

const Clip* Composition::playingClip(int layerIdx) const {
    return const_cast<Composition*>(this)->playingClip(layerIdx);
}

Clip* Composition::transitioningClip(int layerIdx) {
    if (layerIdx < 0 || layerIdx >= layerCount()) return nullptr;
    const Layer& L = m_layers[static_cast<size_t>(layerIdx)];
    if (!L.isTransitioning()) return nullptr;
    if (L.prevDeck >= deckCount()) return nullptr;

    return m_decks[static_cast<size_t>(L.prevDeck)].clipPtr(layerIdx, L.prevColumn);
}

const Clip* Composition::transitioningClip(int layerIdx) const {
    return const_cast<Composition*>(this)->transitioningClip(layerIdx);
}

Composition::LayerRenderInfo Composition::renderInfo(int layerIdx) const {
    LayerRenderInfo info;
    if (layerIdx < 0 || layerIdx >= layerCount()) return info;

    const Layer& L = m_layers[static_cast<size_t>(layerIdx)];
    info.blend   = L.blend;
    info.current = playingClip(layerIdx);
    if (info.current == nullptr) return info;

    const double base = L.opacity * masterOpacity;
    const double t = std::clamp(L.transitionProgress, 0.0, 1.0);

    info.currentOpacity = base * info.current->opacity * t;

    info.previous = transitioningClip(layerIdx);
    if (info.previous != nullptr) {
        // Crossfade tuyến tính. Tổng độ mờ giữ nguyên = base, nên layer
        // không bị sáng vọt hay tối sụp ở giữa transition.
        info.previousOpacity = base * info.previous->opacity * (1.0 - t);
    }
    return info;
}

bool Composition::anySolo() const {
    return std::any_of(m_layers.begin(), m_layers.end(),
                       [](const Layer& L) { return L.solo; });
}

std::vector<int> Composition::visibleLayers() const {
    const bool solo = anySolo();

    std::vector<int> out;
    out.reserve(m_layers.size());

    // Chỉ số 0 là layer DƯỚI CÙNG — trả về theo đúng thứ tự vẽ.
    for (int i = 0; i < layerCount(); ++i) {
        const Layer& L = m_layers[static_cast<size_t>(i)];
        if (!L.isRenderable()) continue;
        if (solo && !L.solo)   continue;   // có solo ⇒ chỉ vẽ layer solo
        if (playingClip(i) == nullptr) continue;
        out.push_back(i);
    }
    return out;
}

double Composition::effectiveOpacity(int layerIdx) const {
    const Clip* c = playingClip(layerIdx);
    if (c == nullptr) return 0.0;
    return layer(layerIdx).opacity * c->opacity * masterOpacity;
}

// ── Autopilot (C7) ─────────────────────────────────────────────────────

void Composition::advanceToNextClip(int layerIdx) {
    Layer& L = m_layers[static_cast<size_t>(layerIdx)];
    if (!L.isPlayingSomething()) return;

    const Deck& d = m_decks[static_cast<size_t>(L.activeDeck)];
    const int n = d.columnCount();
    if (n <= 0) return;

    // Quét vòng tìm ô không rỗng kế tiếp. Bỏ qua ô trống thay vì dừng —
    // lưới thưa là chuyện bình thường.
    for (int step = 1; step <= n; ++step) {
        const int col = (L.activeColumn + step) % n;
        if (!d.clip(layerIdx, col).isEmpty()) {
            startClip(layerIdx, L.activeDeck, col);
            return;
        }
    }
    // Không tìm được ô nào khác (lưới chỉ có 1 clip) → phát lại chính nó.
    startClip(layerIdx, L.activeDeck, L.activeColumn);
}

void Composition::advanceToRandomClip(int layerIdx) {
    Layer& L = m_layers[static_cast<size_t>(layerIdx)];
    if (!L.isPlayingSomething()) return;

    const Deck& d = m_decks[static_cast<size_t>(L.activeDeck)];

    std::vector<int> candidates;
    for (int col = 0; col < d.columnCount(); ++col) {
        if (!d.clip(layerIdx, col).isEmpty()) candidates.push_back(col);
    }
    if (candidates.empty()) return;

    std::uniform_int_distribution<size_t> pick(0, candidates.size() - 1);
    startClip(layerIdx, L.activeDeck, candidates[pick(m_rng)]);
}

void Composition::update(double dtSec) {
    for (int i = 0; i < layerCount(); ++i) {
        Layer& L = m_layers[static_cast<size_t>(i)];

        // ── A10: tiến độ transition ────────────────────────────────────
        if (L.isTransitioning()) {
            if (L.transitionDuration > 0.0) {
                L.transitionProgress += dtSec / L.transitionDuration;
            } else {
                L.transitionProgress = 1.0;
            }

            if (L.transitionProgress >= 1.0) {
                L.transitionProgress = 1.0;
                L.prevDeck   = -1;
                L.prevColumn = -1;
            } else {
                // Clip cũ VẪN PHẢI chạy trong lúc tắt dần — nếu đứng hình
                // thì crossfade sẽ lộ ra là một ảnh tĩnh mờ dần, rất xấu.
                if (Clip* prev = transitioningClip(i)) {
                    prev->transport.advance(dtSec);
                }
            }
        }

        Clip* c = playingClip(i);
        if (c == nullptr) continue;

        const TransportEvent ev = c->transport.advance(dtSec);

        switch (ev) {
        case TransportEvent::RequestNext:   advanceToNextClip(i);   break;
        case TransportEvent::RequestRandom: advanceToRandomClip(i); break;
        case TransportEvent::ReachedEnd:
        case TransportEvent::None:
        default:
            break;
        }
    }
}

} // namespace hexmap

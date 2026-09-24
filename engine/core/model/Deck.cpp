#include "core/model/Deck.h"

#include <algorithm>

namespace mikmap {
namespace {
/// Ô rỗng dùng chung, trả về khi truy cập ngoài phạm vi.
const Clip& emptyClip() {
    static const Clip kEmpty{};
    return kEmpty;
}
} // namespace

Deck::Deck(int layerCount, int columnCount, std::string deckName)
    : name(std::move(deckName)) {
    resize(layerCount, columnCount);
}

void Deck::resize(int layerCount, int columnCount) {
    layerCount  = std::max(0, layerCount);
    columnCount = std::max(0, columnCount);

    m_grid.resize(static_cast<size_t>(layerCount));
    for (auto& row : m_grid) {
        // resize giữ nguyên phần tử cũ ở đầu — clip người dùng đã đặt
        // không bị mất khi thêm cột.
        row.resize(static_cast<size_t>(columnCount));
    }
    m_columnCount = columnCount;
}

bool Deck::isValidCell(int layerIdx, int column) const {
    return layerIdx >= 0 && layerIdx < layerCount()
        && column   >= 0 && column   < m_columnCount;
}

const Clip& Deck::clip(int layerIdx, int column) const {
    if (!isValidCell(layerIdx, column)) return emptyClip();
    return m_grid[static_cast<size_t>(layerIdx)][static_cast<size_t>(column)];
}

Clip* Deck::clipPtr(int layerIdx, int column) {
    if (!isValidCell(layerIdx, column)) return nullptr;
    return &m_grid[static_cast<size_t>(layerIdx)][static_cast<size_t>(column)];
}

void Deck::setClip(int layerIdx, int column, Clip c) {
    if (!isValidCell(layerIdx, column)) return;
    m_grid[static_cast<size_t>(layerIdx)][static_cast<size_t>(column)] = std::move(c);
}

void Deck::clearClip(int layerIdx, int column) {
    if (!isValidCell(layerIdx, column)) return;
    m_grid[static_cast<size_t>(layerIdx)][static_cast<size_t>(column)] = Clip{};
}

bool Deck::columnHasAnyClip(int column) const {
    if (column < 0 || column >= m_columnCount) return false;
    for (const auto& row : m_grid) {
        if (!row[static_cast<size_t>(column)].isEmpty()) return true;
    }
    return false;
}

} // namespace mikmap

// ════════════════════════════════════════════════════════════════════════
//  core/model/Deck.h — một trang lưới clip (A3 A9)
//
//  Deck sở hữu các CLIP, không sở hữu layer. Xem Layer.h để biết lý do.
//
//  Lưới lưu theo [layer][column]. Số layer do Composition quyết định và
//  đồng bộ xuống mọi deck — vì layer là toàn cục.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/model/Clip.h"

#include <string>
#include <vector>

namespace mikmap {

class Deck {
public:
    Deck() = default;
    Deck(int layerCount, int columnCount, std::string deckName = "Deck");

    std::string name = "Deck";

    int layerCount()  const { return static_cast<int>(m_grid.size()); }
    int columnCount() const { return m_columnCount; }

    /// Đổi kích thước lưới, GIỮ NGUYÊN clip đã có ở phần giao nhau.
    void resize(int layerCount, int columnCount);

    bool isValidCell(int layerIdx, int column) const;

    /// Truy cập ô. Chỉ số ngoài phạm vi → trả về ô rỗng dùng chung
    /// (const) hoặc bị bỏ qua (non-const) — không bao giờ ném ngoại lệ,
    /// vì UI và sensor có thể hỏi ô không tồn tại một cách hợp lệ.
    const Clip& clip(int layerIdx, int column) const;
    Clip*       clipPtr(int layerIdx, int column);

    void setClip(int layerIdx, int column, Clip c);
    void clearClip(int layerIdx, int column);

    /// Cột này có clip nào không rỗng không? Dùng cho A6 (trigger cột).
    bool columnHasAnyClip(int column) const;

private:
    std::vector<std::vector<Clip>> m_grid;   ///< [layer][column]
    int m_columnCount = 0;
};

} // namespace mikmap

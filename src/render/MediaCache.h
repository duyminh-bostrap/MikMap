// ════════════════════════════════════════════════════════════════════════
//  render/MediaCache.h — quản lý tài nguyên GPU của media (B1 B2 C11)
//
//  ── Vì sao cần một lớp riêng cho việc này ────────────────────────────
//  Clip trong core/ là DỮ LIỆU THUẦN — nó chỉ giữ đường dẫn file. Lưới
//  4 layer × 8 column = 32 clip, mỗi clip trỏ tới một file 4K.
//
//  Nếu nạp hết: 32 × ~9 MB/frame texture + buffer giải mã ⇒ VRAM nổ.
//  Nếu nạp lúc trigger: mỗi lần bấm clip bị khựng vài trăm ms.
//
//  Giải pháp: cache có giới hạn. Clip đang phát được giữ; clip lâu không
//  dùng bị giải phóng. Đây chính là bài toán Resolume giải rất kỹ và nó
//  KHÔNG hiển nhiên — xem features.md, mục C11.
//
//  ── Ràng buộc luồng ──────────────────────────────────────────────────
//  CHỈ render thread được gọi lớp này. Nó chạm vào GL context.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "core/model/Clip.h"

#include "ofMain.h"
#include "ofxHapPlayer.h"

#include <map>
#include <memory>
#include <string>

namespace hexmap {

class MediaCache {
public:
    struct Entry {
        std::unique_ptr<ofxHapPlayer> video;
        std::unique_ptr<ofImage>      image;

        bool   loaded = false;
        bool   failed = false;
        Vec2   size{0.0, 0.0};
        double durationSec = 0.0;
        int    lastUsedFrame = 0;
        std::string errorText;

        ofTexture* texture();
    };

    /// Giới hạn số media giữ trong cache. Vượt quá thì cái lâu không
    /// dùng nhất bị giải phóng.
    void setBudget(int maxEntries) { m_budget = std::max(1, maxEntries); }
    int  budget() const { return m_budget; }

    /// Lấy (nạp nếu cần) media của clip. nullptr nếu clip rỗng.
    /// Đánh dấu là "vừa dùng" để không bị dọn.
    Entry* acquire(const MediaRef& ref);

    /// Chỉ tra cứu, KHÔNG nạp. Dùng khi vẽ thumbnail: không nên vì hiện
    /// thumbnail mà kéo cả file 4K vào VRAM.
    Entry* peek(const MediaRef& ref);

    /// Gọi mỗi frame TRƯỚC khi vẽ: cập nhật decoder và đồng bộ đầu phát.
    void update();

    /// Đồng bộ vị trí đầu phát của player thật theo Transport của clip.
    /// core/ tính "đầu phát nên ở đâu"; đây là nơi ra lệnh cho player.
    void syncTransport(const Clip& clip);

    /// Gọi cuối mỗi frame: giải phóng media lâu không dùng.
    void collectGarbage();

    void clear();

    // ── Chẩn đoán cho PerfPanel (G9) ───────────────────────────────────
    int  entryCount() const { return static_cast<int>(m_entries.size()); }
    int  loadedCount() const;
    size_t estimatedVramBytes() const;
    int  evictionCount() const { return m_evictions; }

private:
    Entry* load(const MediaRef& ref);

    std::map<std::string, std::unique_ptr<Entry>> m_entries;
    int m_frameCounter = 0;
    int m_budget = 12;
    int m_evictions = 0;
};

} // namespace hexmap

// ════════════════════════════════════════════════════════════════════════
//  io/TripleBuffer.h — cầu nối WAIT-FREE giữa sensor thread và render thread
//  (G1)
//
//  ── Bài toán ─────────────────────────────────────────────────────────
//  Sensor ghi tới 1000 Hz. Render đọc 60 Hz. Dùng mutex thì render thread
//  CÓ THỂ bị chặn bởi writer → rơi frame → phá vỡ ràng buộc C1 (>60fps).
//
//  ── Giải pháp ────────────────────────────────────────────────────────
//  3 ô nhớ. Mỗi bên sở hữu riêng một ô; ô thứ ba là "hộp thư" ở giữa,
//  hoán đổi bằng MỘT lệnh atomic exchange.
//
//      writer:  ghi vào slot[write] → exchange(middle, write | FRESH)
//      reader:  nếu middle có cờ FRESH → exchange(middle, read)
//
//  Không bên nào chờ bên nào. Không cấp phát. Không bao giờ đọc phải
//  frame đang bị ghi dở (torn read), vì hai bên không bao giờ chạm cùng
//  một ô.
//
//  ── Ngữ nghĩa "được phép rơi" ────────────────────────────────────────
//  Nếu writer publish 3 lần giữa 2 lần đọc, reader chỉ thấy lần cuối.
//  ĐÓ LÀ HÀNH VI ĐÚNG cho dữ liệu trạng thái: vị trí ngón tay 5ms trước
//  không còn giá trị. Với dữ liệu KHÔNG được rơi (sự kiện down/up),
//  dùng SpscRingBuffer.
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <atomic>
#include <cstdint>

namespace mikmap {

template <typename T>
class TripleBuffer {
public:
    TripleBuffer() : m_middle(2u) {}

    TripleBuffer(const TripleBuffer&) = delete;
    TripleBuffer& operator=(const TripleBuffer&) = delete;

    // ── Phía WRITER (chỉ sensor thread được gọi) ───────────────────────

    /// Ô để ghi. Writer sở hữu riêng ô này cho tới khi gọi publish().
    T& writeSlot() { return m_slots[m_writeIdx]; }

    /// Công bố ô vừa ghi. Sau lệnh này writer nhận một ô khác để ghi tiếp.
    void publish() {
        // release: mọi thao tác ghi vào slot phải hiển thị với reader
        // TRƯỚC khi reader nhìn thấy chỉ số mới.
        const uint32_t prev =
            m_middle.exchange(m_writeIdx | kFreshBit, std::memory_order_acq_rel);
        m_writeIdx = prev & kIdxMask;
        m_published.fetch_add(1, std::memory_order_relaxed);
    }

    /// Tiện ích: ghi một giá trị rồi công bố ngay.
    void write(const T& value) {
        writeSlot() = value;
        publish();
    }

    // ── Phía READER (chỉ render thread được gọi) ───────────────────────

    /// Có dữ liệu mới kể từ lần đọc trước không?
    bool hasNew() const {
        return (m_middle.load(std::memory_order_acquire) & kFreshBit) != 0u;
    }

    /// Lấy dữ liệu mới nhất nếu có.
    /// @return false nếu chưa có gì mới — ô đọc hiện tại vẫn hợp lệ.
    bool consume() {
        if (!hasNew()) return false;

        // acquire: sau lệnh này ta thấy được toàn bộ nội dung writer đã ghi.
        const uint32_t prev =
            m_middle.exchange(m_readIdx, std::memory_order_acq_rel);
        m_readIdx = prev & kIdxMask;
        m_consumed.fetch_add(1, std::memory_order_relaxed);
        return true;
    }

    /// Ô đang đọc. Luôn hợp lệ, kể cả khi consume() trả về false —
    /// khi đó nó giữ giá trị của lần đọc thành công gần nhất.
    const T& readSlot() const { return m_slots[m_readIdx]; }
    T&       readSlot()       { return m_slots[m_readIdx]; }

    // ── Chẩn đoán (cho PerfPanel — G9) ─────────────────────────────────
    uint64_t publishedCount() const { return m_published.load(std::memory_order_relaxed); }
    uint64_t consumedCount()  const { return m_consumed.load(std::memory_order_relaxed); }

    /// Số frame writer công bố mà reader không bao giờ thấy.
    /// KHÔNG phải lỗi — đây là thiết kế. Nhưng con số quá lớn nghĩa là
    /// sensor chạy nhanh hơn render rất nhiều, hữu ích để chỉnh tần số.
    uint64_t droppedCount() const {
        const uint64_t p = publishedCount();
        const uint64_t c = consumedCount();
        return (p > c) ? (p - c) : 0u;
    }

private:
    static constexpr uint32_t kIdxMask  = 0x3u;
    static constexpr uint32_t kFreshBit = 0x4u;

    T m_slots[3]{};

    /// Chỉ writer chạm vào.
    uint32_t m_writeIdx = 0;
    /// Chỉ reader chạm vào.
    uint32_t m_readIdx = 1;

    /// Ô thứ ba + cờ FRESH. Đây là điểm đồng bộ DUY NHẤT.
    std::atomic<uint32_t> m_middle;

    std::atomic<uint64_t> m_published{0};
    std::atomic<uint64_t> m_consumed{0};
};

} // namespace mikmap

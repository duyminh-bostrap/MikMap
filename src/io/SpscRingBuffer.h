// ════════════════════════════════════════════════════════════════════════
//  io/SpscRingBuffer.h — hàng đợi một-ghi-một-đọc, không khoá (G1)
//
//  Dùng cho SỰ KIỆN, không dùng cho trạng thái.
//
//  ── Vì sao cần kênh thứ hai bên cạnh TripleBuffer ────────────────────
//  TripleBuffer cố tình VỨT frame cũ — đúng cho vị trí ngón tay, vì
//  vị trí 5ms trước không còn giá trị.
//
//  Nhưng sự kiện thì khác: nếu bỏ lỡ một TOUCH_UP, hiệu ứng sẽ KẸT
//  VĨNH VIỄN trên màn hình. Người vận hành phải khởi động lại phần mềm
//  giữa show. Vì vậy sự kiện phải đi qua hàng đợi giữ đủ thứ tự.
//
//  ── Vì sao đệm giữa các chỉ số (cache line padding) ──────────────────
//  head và tail bị hai lõi CPU khác nhau ghi. Nếu chúng nằm chung một
//  cache line, mỗi lần ghi sẽ vô hiệu hoá cache của lõi kia (false
//  sharing) — chậm hơn nhiều lần dù thuật toán vẫn "không khoá".
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>

namespace hexmap {

// C4324 "structure was padded due to alignment specifier" — MSVC cảnh báo
// đúng, nhưng phần đệm ở đây là CHỦ ĐÍCH: alignas(64) tách head/tail sang
// hai cache line khác nhau để tránh false sharing. Tắt riêng cảnh báo này
// thay vì bỏ alignas, và thay vì hạ mức cảnh báo toàn dự án.
#if defined(_MSC_VER)
#  pragma warning(push)
#  pragma warning(disable : 4324)
#endif

/// @tparam Capacity PHẢI là luỹ thừa của 2 (để dùng phép AND thay cho chia dư).
template <typename T, size_t Capacity = 1024>
class SpscRingBuffer {
    static_assert(Capacity >= 2, "Capacity phai >= 2");
    static_assert((Capacity & (Capacity - 1)) == 0, "Capacity phai la luy thua cua 2");
    static_assert(std::is_trivially_copyable_v<T>, "T phai trivially copyable");

public:
    SpscRingBuffer() = default;
    SpscRingBuffer(const SpscRingBuffer&) = delete;
    SpscRingBuffer& operator=(const SpscRingBuffer&) = delete;

    // ── Phía PRODUCER (sensor thread) ──────────────────────────────────

    /// @return false nếu hàng đợi đầy. Khi đó overflowCount() tăng lên —
    ///         PerfPanel hiển thị con số này; khác 0 nghĩa là render
    ///         thread đang quá tải và ta biết ngay thay vì đoán.
    bool push(const T& item) {
        const size_t head = m_head.load(std::memory_order_relaxed);
        const size_t next = (head + 1) & kMask;

        if (next == m_tail.load(std::memory_order_acquire)) {
            m_overflow.fetch_add(1, std::memory_order_relaxed);
            return false;
        }

        m_buffer[head] = item;
        m_head.store(next, std::memory_order_release);
        return true;
    }

    // ── Phía CONSUMER (render thread) ──────────────────────────────────

    bool pop(T& out) {
        const size_t tail = m_tail.load(std::memory_order_relaxed);
        if (tail == m_head.load(std::memory_order_acquire)) return false;   // rỗng

        out = m_buffer[tail];
        m_tail.store((tail + 1) & kMask, std::memory_order_release);
        return true;
    }

    /// Lấy tối đa maxItems phần tử. Trả về số phần tử thực sự lấy được.
    /// Render thread nên dùng hàm này với giới hạn, để một trận bão sự
    /// kiện không kéo dài một frame vô hạn.
    size_t popBatch(T* out, size_t maxItems) {
        size_t n = 0;
        while (n < maxItems && pop(out[n])) ++n;
        return n;
    }

    // ── Truy vấn ───────────────────────────────────────────────────────
    bool empty() const {
        return m_head.load(std::memory_order_acquire)
            == m_tail.load(std::memory_order_acquire);
    }

    size_t size() const {
        const size_t h = m_head.load(std::memory_order_acquire);
        const size_t t = m_tail.load(std::memory_order_acquire);
        return (h - t) & kMask;
    }

    static constexpr size_t capacity() { return Capacity - 1; }   // 1 ô làm lính canh

    uint64_t overflowCount() const { return m_overflow.load(std::memory_order_relaxed); }
    void resetOverflowCount() { m_overflow.store(0, std::memory_order_relaxed); }

private:
    static constexpr size_t kMask = Capacity - 1;
    static constexpr size_t kCacheLine = 64;

    T m_buffer[Capacity]{};

    alignas(kCacheLine) std::atomic<size_t> m_head{0};   // chỉ producer ghi
    alignas(kCacheLine) std::atomic<size_t> m_tail{0};   // chỉ consumer ghi
    alignas(kCacheLine) std::atomic<uint64_t> m_overflow{0};
};

#if defined(_MSC_VER)
#  pragma warning(pop)
#endif

} // namespace hexmap

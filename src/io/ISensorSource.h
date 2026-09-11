// ════════════════════════════════════════════════════════════════════════
//  io/ISensorSource.h — giao diện chung cho mọi nguồn sensor (G1)
//
//  Mọi nguồn (Serial, OSC, TUIO, Kinect, Mock) đều tuân theo hợp đồng này.
//  Nhờ vậy Composition/UI không cần biết dữ liệu đến từ đâu — và
//  MockSource thay thế được phần cứng thật trong mọi tình huống test.
//
//  ── Hợp đồng luồng ───────────────────────────────────────────────────
//    · start()/stop()  gọi từ thread điều khiển (main)
//    · nguồn tự tạo THREAD RIÊNG bên trong, chặn ở read()/recvfrom()
//    · công bố qua TripleBuffer (trạng thái) và SpscRingBuffer (sự kiện)
//    · render thread CHỈ đọc qua frames()/events(), không bao giờ khoá
// ════════════════════════════════════════════════════════════════════════
#pragma once

#include "io/SensorFrame.h"
#include "io/SpscRingBuffer.h"
#include "io/TripleBuffer.h"

#include <string>

namespace hexmap {

enum class SourceStatus {
    Stopped = 0,
    Connecting,
    Running,
    Error,
};

class ISensorSource {
public:
    virtual ~ISensorSource() = default;

    virtual const char* typeName() const = 0;

    virtual bool start() = 0;
    virtual void stop() = 0;

    virtual SourceStatus status() const = 0;

    /// Mô tả lỗi để hiện lên UI. Rỗng nếu không có lỗi.
    virtual std::string lastError() const { return {}; }

    uint16_t sourceId() const { return m_sourceId; }
    void setSourceId(uint16_t id) { m_sourceId = id; }

    /// Kênh TRẠNG THÁI — được phép rơi frame cũ.
    TripleBuffer<SensorFrame>&       frames()       { return m_frames; }
    const TripleBuffer<SensorFrame>& frames() const { return m_frames; }

    /// Kênh SỰ KIỆN — không được rơi.
    SpscRingBuffer<TouchEvent, 1024>&       events()       { return m_events; }
    const SpscRingBuffer<TouchEvent, 1024>& events() const { return m_events; }

protected:
    TripleBuffer<SensorFrame>       m_frames;
    SpscRingBuffer<TouchEvent, 1024> m_events;
    uint16_t m_sourceId = 0;
};

} // namespace hexmap
